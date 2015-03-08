#ifndef DISRUPTOR2_SEQUENCER_H_
#define DISRUPTOR2_SEQUENCER_H_

#include <vector>

#include <boost/utility.hpp>

#include <disruptor/interface.h>
#include <disruptor/claim_strategy.h>
#include <disruptor/wait_strategy.h>
#include <disruptor/sequence_barrier.h>

namespace disruptor {

inline size_t ceilToPow2(size_t x)
{
    // From http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    for (size_t i = 1; i < sizeof(size_t); i <<= 1) {
        x |= x >> (i << 3);
    }
    ++x;
    return x;
}

// Coordinator for claiming sequences for access to a data structures while
// tracking dependent {@link Sequence}s
class Sequencer : public boost::noncopyable
{
public:
    // Construct a Sequencer with the selected strategies.
    //
    // @param buffer_size over which sequences are valid.
    // @param claim_strategy_option for those claiming sequences.
    // @param wait_strategy_option for those waiting on sequences.
    Sequencer(int buffer_size,
              ClaimStrategyOption claim_strategy_option,
              WaitStrategyOption wait_strategy_option,
              const TimeConfig& timeConfig = TimeConfig())
        : buffer_size_(ceilToPow2(buffer_size))
        , claim_strategy_(CreateClaimStrategy(claim_strategy_option, buffer_size_))
        , wait_strategy_(CreateWaitStrategy(wait_strategy_option, timeConfig))
    {
    }

    virtual ~Sequencer()
    {
    }

    // Set the sequences that will gate publishers to prevent the buffer
    // wrapping.
    //
    // @param sequences to be gated on.
    void setGatingSequences(const DependentSequences& sequences)
    {
        gating_sequences_ = sequences;
    }

    // Create a {@link SequenceBarrier} that gates on the cursor and a list of
    // {@link Sequence}s.
    //
    // @param sequences_to_track this barrier will track.
    // @return the barrier gated as required.
    SequenceBarrierPtr newBarrier(const DependentSequences& sequences_to_track)
    {
        return boost::make_shared<ProcessingSequenceBarrier>(
                wait_strategy_.get(), &cursor_, sequences_to_track );
    }

    // Create a new {@link BatchDescriptor} that is the minimum of the
    // requested size and the buffer_size.
    //
    // @param size for the new batch.
    // @return the new {@link BatchDescriptor}.
    BatchDescriptorPtr newBatchDescriptor(const int& size)
    {
        return boost::make_shared<BatchDescriptor>(size<buffer_size_?size:buffer_size_);
    }

    // The capacity of the data structure to hold entries.
    //
    // @return capacity of the data structure.
    int capacity() const { return buffer_size_; }


    // Get the value of the cursor indicating the published sequence.
    //
    // @return value of the cursor for events that have been published.
    int64_t getCursor() const { return cursor_.get(); }

    // Has the buffer capacity left to allocate another sequence. This is a
    // concurrent method so the response should only be taken as an indication
    // of available capacity.
    //
    // @return true if the buffer has the capacity to allocated another event.
    bool hasAvailableCapacity() const
    {
        return claim_strategy_->hasAvailableCapacity(gating_sequences_);
    }

    // Get the remaining capacity for this sequencer.
    //
    // @return The number of slots remaining.
    int remainingCapacity() const
    {
        return this->capacity() - this->occupiedCapacity();
    }

    // Get the slots taken for this sequencer.
    //
    // @return The number of slots taken.
    int occupiedCapacity() const
    {
        int64_t consumed = GetMinimumSequence(gating_sequences_);
        int64_t produced = cursor_.get();
        return static_cast<int>((buffer_size_ + produced - consumed) % buffer_size_);
    }

    // Claim the next event in sequence for publishing to the {@link RingBuffer}.
    //
    // @return the claimed sequence.
    int64_t next()
    {
        // TODO: check gatingSequence, throw exception if it's empty
        return claim_strategy_->incrementAndGet(gating_sequences_);
    }

    // Claim the next batch of sequence numbers for publishing.
    //
    // @param batch_descriptor to be updated for the batch range.
    // @return the updated batch_descriptor.
    BatchDescriptor* next(BatchDescriptor* batch_descriptor)
    {
        int64_t sequence = claim_strategy_->incrementAndGet(batch_descriptor->size(), gating_sequences_);
        batch_descriptor->set_end(sequence);
        return batch_descriptor;
    }

    // Claim a specific sequence when only one publisher is involved.
    //
    // @param sequence to be claimed.
    // @return sequence just claime.
    int64_t claim(const int64_t& sequence)
    {
        claim_strategy_->setSequence(sequence, gating_sequences_);
        return sequence;
    }

    // Publish an event and make it visible to {@link EventProcessor}s.
    //
    // @param sequence to be published.
    void publish(const int64_t& sequence)
    {
        this->publish(sequence, 1); } // Publish the batch of events in sequence.
    //
    // @param sequence to be published.
    void publish(const BatchDescriptor& batch_descriptor)
    {
        this->publish(batch_descriptor.end(), batch_descriptor.size());
    }

    // Force the publication of a cursor sequence.
    //
    // Only use this method when forcing a sequence and you are sure only one
    // publisher exists. This will cause the cursor to advance to this
    // sequence.
    //
    // @param sequence to which is to be forced for publication.
    void forcePublish(const int64_t& sequence)
    {
        cursor_.set(sequence);
        wait_strategy_->signalAllWhenBlocking();
    }

protected:
    const int buffer_size_;

    void publish(const int64_t& sequence, const int64_t& batch_size)
    {
        claim_strategy_->serialisePublishing(sequence, cursor_, batch_size);
        wait_strategy_->signalAllWhenBlocking();
    }

    Sequence cursor_;
    DependentSequences gating_sequences_;

    ClaimStrategyPtr claim_strategy_;
    WaitStrategyPtr wait_strategy_;
};

};  // namespace disruptor

#endif

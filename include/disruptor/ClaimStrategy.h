#ifndef DISRUPTOR_CLAIM_STRATEGY_H
#define DISRUPTOR_CLAIM_STRATEGY_H

#include <vector>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <disruptor/Interface.h>

namespace disruptor {

const int DEFAULT_PENDING_BUFFER_SIZE = 1024;
const int DEFAULT_RETRY_TIMES = 1000;

enum ClaimStrategyOption {
    eSingleThreadedStrategy,
    eMultiThreadedStrategy,
    eMultiThreadedLowContentionStrategy
};

// Optimised strategy can be used when there is a single publisher thread
// claiming {@link AbstractEvent}s.
class SingleThreadedStrategy : public IClaimStrategy
{
public:
    SingleThreadedStrategy(const int& buffer_size)
        : buffer_size_(buffer_size)
        , sequence_(INITIAL_CURSOR_VALUE)
        , min_gating_sequence_(INITIAL_CURSOR_VALUE)
    {}

    virtual int64_t incrementAndGet(
            const DependentSequences& dependent_sequences)
    {
        int64_t next_sequence = sequence_.incrementAndGet(1L);
        waitForFreeSlotAt(next_sequence, dependent_sequences);
        return next_sequence;
    }

    virtual int64_t incrementAndGet(const int& delta,
            const DependentSequences& dependent_sequences)
    {
        int64_t next_sequence = sequence_.incrementAndGet(delta);
        waitForFreeSlotAt(next_sequence, dependent_sequences);
        return next_sequence;
    }

    virtual bool hasAvailableCapacity(
            const DependentSequences& dependent_sequences)
    {
        int64_t wrap_point = sequence_.get() + 1L - buffer_size_;
        if (wrap_point > min_gating_sequence_.get()) {
            int64_t min_sequence = getMinimumSequence(dependent_sequences);
            min_gating_sequence_.set(min_sequence);
            if (wrap_point > min_sequence)
                return false;
        }
        return true;
    }

    virtual void setSequence(const int64_t& sequence,
            const DependentSequences& dependent_sequences)
    {
        sequence_.set(sequence);
        waitForFreeSlotAt(sequence, dependent_sequences);
    }

    virtual void serialisePublishing(const int64_t& sequence,
            Sequence& cursor,
            const int64_t& batch_size)
    {
        cursor.set(sequence);
    }

private:
    SingleThreadedStrategy();

    void waitForFreeSlotAt(const int64_t& sequence,
            const DependentSequences& dependent_sequences)
    {
        int64_t wrap_point = sequence - buffer_size_;
        if (wrap_point > min_gating_sequence_.get()) {
            int64_t min_sequence;
            while (wrap_point > (min_sequence = getMinimumSequence(dependent_sequences))) {
                boost::this_thread::yield();
            }
            min_gating_sequence_.set(min_sequence);
        }
    }

    const int buffer_size_;
    PaddedLong sequence_;
    PaddedLong min_gating_sequence_;
};

// Strategy to be used when there are multiple publisher threads claiming
// {@link AbstractEvent}s.
class MultiThreadedLowContentionStrategy : public IClaimStrategy
{
public:
    MultiThreadedLowContentionStrategy(const int& buffer_size)
        : buffer_size_(buffer_size)
        , sequence_(INITIAL_CURSOR_VALUE)
        , min_gating_sequence_(INITIAL_CURSOR_VALUE)
        , retries_(DEFAULT_RETRY_TIMES)
    {
    }

    virtual int64_t incrementAndGet(
            const DependentSequences& dependent_sequences)
    {
        int64_t next_sequence = sequence_.incrementAndGet(1L);
        waitForFreeSlotAt(next_sequence, dependent_sequences);
        return next_sequence;
    }

    virtual int64_t incrementAndGet(const int& delta,
            const DependentSequences& dependent_sequences) 
    {
        int64_t next_sequence = sequence_.incrementAndGet(delta);
        waitForFreeSlotAt(next_sequence, dependent_sequences);
        return next_sequence;
    }

    virtual void setSequence(const int64_t& sequence,
            const DependentSequences& dependent_sequences)
    {
        sequence_.set(sequence);
        waitForFreeSlotAt(sequence, dependent_sequences);
    }

    virtual bool hasAvailableCapacity(
            const DependentSequences& dependent_sequences)
    {
        const int64_t wrap_point = sequence_.get() + 1L - buffer_size_;
        if (wrap_point > min_gating_sequence_.get()) {
            int64_t min_sequence = getMinimumSequence(dependent_sequences);
            min_gating_sequence_.set(min_sequence);
            if (wrap_point > min_sequence)
                return false;
        }
        return true;
    }

    virtual void serialisePublishing(const int64_t& sequence,
                                     Sequence& cursor,
                                     const int64_t& batch_size)
    {
        int64_t expected_sequence = sequence - batch_size;

        while (expected_sequence != cursor.get()) {
        }

        cursor.set(sequence);
    }

protected:
    void waitForFreeSlotAt(const int64_t& sequence,
                           const DependentSequences& dependent_sequences) 
    {
        const int64_t wrap_point = sequence - buffer_size_;
        if (wrap_point > min_gating_sequence_.get()) {
            int counter = retries_;
            int64_t min_sequence;
            while (wrap_point > (min_sequence = getMinimumSequence(dependent_sequences))) {
                if (counter > 0) {
                    counter--;
                }
                else {
                    boost::this_thread::sleep(boost::posix_time::milliseconds(1));
                }
            }
            min_gating_sequence_.set(min_sequence);
        }
    }

    int applyBackPressure(int counter) 
    {
        if (counter > 0) {
            --counter;
        }
        else {
            boost::this_thread::sleep(boost::posix_time::milliseconds(1));
        }

        return counter;
    }

    const int buffer_size_;
    Sequence sequence_;
    MutableLong min_gating_sequence_; // not atomic, but is safe enough for wrap checking
    const int retries_;
};


class MultiThreadedStrategy : public MultiThreadedLowContentionStrategy
{
public:
    MultiThreadedStrategy(const int& buffer_size)
        : MultiThreadedLowContentionStrategy(buffer_size)
        , pending_size_(0)
        , pending_mask_(0)
    {
    }

    /**
     * Construct a new multi-threaded publisher {@link ClaimStrategy} for a given buffer size.
     *
     * @param buffer_size for the underlying data structure.
     * @param pending_buffer_size number of item that can be pending for serialisation
     */
    MultiThreadedStrategy(const int& buffer_size,
            int pending_buffer_size = DEFAULT_PENDING_BUFFER_SIZE)
        : MultiThreadedLowContentionStrategy(buffer_size)
        , pending_size_(pending_buffer_size)
        , pending_publication_(new Sequence[pending_buffer_size])
        , pending_mask_(pending_buffer_size - 1)
    {
    }

    virtual void serialisePublishing(const int64_t& sequence,
                                     Sequence& cursor,
                                     const int64_t& batch_size)
    {
        // Guard condition, limit the number of pending publications
        int counter = retries_;
        while (sequence - cursor.get() > pending_size_) {
            counter = applyBackPressure(counter);
        }

        // Transition from unpublished -> pending
        int64_t expected_sequence = sequence - batch_size;
        for (int64_t pending_sequence = expected_sequence + 1;
             pending_sequence <= sequence;
             ++pending_sequence) {
            pending_publication_[pending_sequence & pending_mask_].set(pending_sequence);
        }

        // Optimisation, if the cursor is not at the expected
        // value there is no point joining the race.
        int64_t cursor_sequence = cursor.get();
        if (cursor_sequence >= sequence) {
            return;
        }

        expected_sequence = std::max(expected_sequence, cursor_sequence);

        // Transition pending -> published
        int64_t next_sequence = expected_sequence + 1;
        while (cursor.compare_exchange(expected_sequence, next_sequence)) {
            expected_sequence = next_sequence;
            ++next_sequence;
            if (pending_publication_[next_sequence & pending_mask_].get() != next_sequence) {
                break;
            }
        }
    }

private:
    const int64_t pending_size_; // must be power of 2, TBA checking
    Sequence* pending_publication_;
    const int64_t pending_mask_;
};


inline ClaimStrategyPtr createClaimStrategy(ClaimStrategyOption option,
                                            const int& buffer_size)
{
    switch (option) {
        case eSingleThreadedStrategy:
            return boost::make_shared<SingleThreadedStrategy>(buffer_size);
         case eMultiThreadedStrategy:
            return boost::make_shared<MultiThreadedStrategy>(buffer_size, DEFAULT_PENDING_BUFFER_SIZE);
         case eMultiThreadedLowContentionStrategy:
            return boost::make_shared<MultiThreadedLowContentionStrategy>(buffer_size);
        default:
            return ClaimStrategyPtr();
    }
};

}

#endif 

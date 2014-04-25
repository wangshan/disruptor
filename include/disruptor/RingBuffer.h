#ifndef DISRUPTOR_RING_BUFFER_H
#define DISRUPTOR_RING_BUFFER_H

#include <vector>

#include <disruptor/Sequencer.h>

namespace disruptor {

// Ring based store of reusable entries containing the data representing an
// event beign exchanged between publisher and {@link EventProcessor}s.
//
// @param <T> implementation storing the data for sharing during exchange
// or parallel coordination of an event.
template<typename T>
class RingBuffer : public Sequencer
{
public:
    // Construct a RingBuffer with the full option set.
    //
    // @param event_factory to instance new entries for filling the RingBuffer.
    // @param buffer_size of the RingBuffer, must be a power of 2.
    // @param claim_strategy_option threading strategy for publishers claiming
    // entries in thS ring.
    // @param wait_strategy_option waiting strategy employed by
    // processors_to_track waiting in entries becoming available.
    //
    RingBuffer(IEventFactory<T>* event_factory,
               int buffer_size,
               ClaimStrategyOption claim_strategy_option,
               WaitStrategyOption wait_strategy_option,
               const TimeConfig& timeConfig = TimeConfig())
        : Sequencer(buffer_size,
                    claim_strategy_option,
                    wait_strategy_option,
                    timeConfig)
        , mask_(buffer_size - 1)
        , events_(new T[buffer_size])
    {
        if (event_factory) {
            this->fill(event_factory);
        }
    }

    RingBuffer(int buffer_size,
               ClaimStrategyOption claim_strategy_option,
               WaitStrategyOption wait_strategy_option,
               const TimeConfig& timeConfig) 
        : Sequencer(buffer_size,
                    claim_strategy_option,
                    wait_strategy_option,
                    timeConfig)
        , mask_(buffer_size - 1)
        , events_(new T[buffer_size])
    {
    }

    ~RingBuffer()
    {
    }

    // Get the event for a given sequence in the RingBuffer.
    //
    // @param sequence for the event
    // @return event pointer at the specified sequence position.
    T* get(const int64_t& sequence)
    {
        return &events_[sequence & mask_];
    }

private:
    void fill( IEventFactory<T>* factory)
    {
        for (int i = 0; i < capacity(); ++i) {
            events_[i] = *(factory->newInstance());
        }
    }

private:
    int mask_;
    boost::scoped_array<T> events_;
};

}

#endif

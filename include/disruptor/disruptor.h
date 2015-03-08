#ifndef DISRUPTOR2_DISRUPTOR_H
#define DISRUPTOR2_DISRUPTOR_H

#include <exception>
#include <iostream>
#include <memory>

#include <boost/scoped_ptr.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>

#include <disruptor/ring_buffer.h>
#include <disruptor/event_publisher.h>
#include <disruptor/event_processor.h>
#include <disruptor/dynamic_ring_buffer.h>
#include <disruptor/dynamic_event_processor.h>

namespace disruptor {

const int DEFAULT_MAX_IDLE_TIME_US = 10;

template<typename T>
class Disruptor
{
    public:
        // will start after construct
        Disruptor(int size,
                  ClaimStrategyOption claimStrategy,
                  WaitStrategyOption waitStrategy,
                  IEventHandler<T> * handler,
                  IExceptionHandler<T> * exceptHandler,
                  const TimeConfig& timeConfig = TimeConfig())
            : ring_buffer_(size, claimStrategy, waitStrategy, timeConfig)
            , barrier_(ring_buffer_.newBarrier(DependentSequences()))
            , processor_(&ring_buffer_, barrier_, handler, exceptHandler,
                         GetTimeConfig(timeConfig, kMaxIdle,
                                       boost::posix_time::microseconds(DEFAULT_MAX_IDLE_TIME_US)))
            , publisher_(&ring_buffer_)
            , consumer_thread_(boost::ref< BatchEventProcessor<T> >(processor_))
            , stopped_(false)
        {
            ring_buffer_.setGatingSequences(DependentSequences(1, processor_.getSequence()));
        }

        virtual ~Disruptor()
        {
            if(!stopped_) {
                stop();
            }
        }

        void publishEvent(IEventTranslator<T>* translator)
        {
            publisher_.publishEvent(translator);
        }

        bool tryPublishEvent(IEventTranslator<T>* translator)
        {
            return publisher_.tryPublishEvent(translator);
        }

        bool full() const
        {
            return !publisher_.hasAvailableCapacity();
        }

        BatchEventProcessor<T>& processor()
        {
            return processor_;
        }

        void stop()
        {
            processor_.halt();
            consumer_thread_.join();
            stopped_ = true;
        }

        int occupiedCapacity() const
        {
            return ring_buffer_.occupiedCapacity();
        }

    private:
        RingBuffer<T>           ring_buffer_;
        SequenceBarrierPtr      barrier_;
        BatchEventProcessor<T>  processor_;
        EventPublisher<T>       publisher_;
        boost::thread           consumer_thread_;
        bool                    stopped_;
};


// has similar interface as the normal Disruptor, but with the following differences:
// - it's strictly single producer single consumer
// - claim strategy is ignored, claim never fails or blocks, unless we run out of memory
// - T must define a copy constructor and assignment operator, or have trivial ones

template<typename T>
class DynamicDisruptor
{
    public:
        // will start after construct
        DynamicDisruptor(size_t size,
                  ClaimStrategyOption claimStrategy, // not useful here
                  WaitStrategyOption waitStrategy,
                  IEventHandler<T> * handler,
                  IExceptionHandler<T> * exceptHandler,
                  const TimeConfig& timeConfig = TimeConfig())
            : ring_buffer_(size, claimStrategy, waitStrategy, timeConfig)
            , processor_(&ring_buffer_, waitStrategy, handler, exceptHandler,
                         GetTimeConfig(timeConfig, kMaxIdle,
                                       boost::posix_time::microseconds(DEFAULT_MAX_IDLE_TIME_US)))
            , consumer_thread_(boost::ref< DynamicProcessor<T> >(processor_))
            , stopped_(false)
        {
        }

        virtual ~DynamicDisruptor()
        {
            if (!stopped_) {
                stop();
            }
        }

        void publishEvent(const T& event)
        {
            ring_buffer_.enqueue(event);
        }

        bool full() const
        {
            return !ring_buffer_.has_available_capacity();
        }

        DynamicProcessor<T>& processor()
        {
            return processor_;
        }

        void stop()
        {
            processor_.halt();
            stopped_ = true;
            consumer_thread_.join();
        }

        int occupiedCapacity() const
        {
            return ring_buffer_.occupied_approx();
        }

    private:
        DynamicRingBuffer<T>    ring_buffer_;
        DynamicProcessor<T>     processor_;
        boost::thread           consumer_thread_;
        bool                    stopped_;
};

}

#endif

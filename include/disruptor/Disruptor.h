#ifndef DISRUPTOR_DISRUPTOR_H
#define DISRUPTOR_DISRUPTOR_H

#include <exception>
#include <iostream>
#include <memory>

#include <boost/scoped_ptr.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>

#include <disruptor/RingBuffer.h>
#include <disruptor/EventPublisher.h>
#include <disruptor/EventProcessor.h>

namespace disruptor {

const int DEDAULT_MAX_IDLE_TIME_US = 10;

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
                         getTimeConfig(timeConfig, eMaxIdle,
                                       boost::posix_time::microseconds(DEDAULT_MAX_IDLE_TIME_US)))
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

}

#endif

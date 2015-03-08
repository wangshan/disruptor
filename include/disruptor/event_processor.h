#ifndef DISRUPTOR2_EVENT_PROCESSOR_H_
#define DISRUPTOR2_EVENT_PROCESSOR_H_

#include <exception>

#include <boost/atomic.hpp>
#include <boost/utility.hpp>

#include <disruptor/ring_buffer.h>

namespace disruptor {


template <typename T>
class BatchEventProcessor : public IEventProcessor<T>
                          , public boost::noncopyable
{
public:
    BatchEventProcessor(RingBuffer<T>* ring_buffer,
                        SequenceBarrierPtr sequence_barrier,
                        IEventHandler<T>* event_handler,
                        IExceptionHandler<T>* exception_handler,
                        const boost::posix_time::time_duration& max_idle_time)
        : running_(false)
        , ring_buffer_(ring_buffer)
        , sequence_barrier_(sequence_barrier)
        , event_handler_(event_handler)
        , exception_handler_(exception_handler)
        , wait_(max_idle_time)
    {}

    virtual Sequence* getSequence() { return &sequence_; }
    //virtual PaddedLong* getSequence() { return &sequence_; }

    virtual void halt()
    {
        running_.store(false);
        sequence_barrier_->alert();
    }

    virtual void run()
    {
        bool expected = false;
        if ( !running_.compare_exchange_strong(expected, true) ) {
            throw std::runtime_error("Thread is already running");
        }

        // Note: must keep this line commented, otherwise, if Halt is called
        // before clearAlert, then the alert status will never be caught
        //sequence_barrier_->clearAlert();
        event_handler_->onStart();

        T* event = NULL;
        int64_t next_sequence = sequence_.get() + 1L;

        while (true) {
            try {
                int64_t available_sequence =
                    sequence_barrier_->waitFor(next_sequence, wait_);

                int64_t batch_size = available_sequence - next_sequence + 1;

                while (next_sequence <= available_sequence) {
                    event = ring_buffer_->get(next_sequence);
                    event_handler_->onEvent(next_sequence,
                            batch_size,
                            next_sequence == available_sequence, event);
                    next_sequence++;
                }

                if (wait_.ticks() != 0) {
                    // not matter there was events or not, always notify handler
                    // with NULL event for special handling
                    event_handler_->onEvent(next_sequence,
                            0,
                            next_sequence == available_sequence, NULL);
                }

                sequence_.set(next_sequence - 1L);
            }
            catch(const AlertException& e) {
                break;
            }
            catch(const std::exception& e) {
                if (exception_handler_) {
                    exception_handler_->handle(e, next_sequence, event);
                }
                sequence_.set(next_sequence);
                next_sequence++;
            }
        }

        event_handler_->onShutdown();
        running_.store(false);
    }

    void operator()() { run(); }

private:
    boost::atomic<bool> running_;
    Sequence sequence_;

    RingBuffer<T>* ring_buffer_;
    SequenceBarrierPtr sequence_barrier_; // barrier is (share)owned by processors
    IEventHandler<T>* event_handler_;
    IExceptionHandler<T>* exception_handler_;
    boost::posix_time::time_duration wait_; 
};

};  // namespace disruptor

#endif

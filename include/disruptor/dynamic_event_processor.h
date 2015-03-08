#ifndef DISRUPTOR2_DYNAMIC_EVENT_PROCESSOR_H_
#define DISRUPTOR2_DYNAMIC_EVENT_PROCESSOR_H_

#include <exception>

#include <boost/atomic.hpp>
#include <boost/utility.hpp>

#include <disruptor/ring_buffer.h>
#include <disruptor/dynamic_ring_buffer.h>

namespace disruptor {

#define MAX_RETRIES_TIMES 1

namespace dynamic {

typedef boost::function<bool (int&)> WaitStrategy;

inline bool sleepFor(const boost::posix_time::time_duration& max_idle, int& retries)
{
    if (retries <= 0) {
        boost::this_thread::sleep(max_idle);
        return true;
    }
    else {
        --retries;
        return false;
    }
}

inline bool yieldThis(int& retries)
{
    if (retries <= 0) {
        boost::this_thread::yield();
        return true;
    }
    else {
        --retries;
        return false;
    }
}

}

template <typename T>
class DynamicProcessor : public IEventProcessor<T>
                       , public boost::noncopyable
{
public:
    DynamicProcessor(DynamicRingBuffer<T>* ring_buffer,
                     WaitStrategyOption waitStrategy,
                     IEventHandler<T>* event_handler,
                     IExceptionHandler<T>* exception_handler,
                     const boost::posix_time::time_duration& max_idle_time)
        : running_(false)
        , ring_buffer_(ring_buffer)
        , event_handler_(event_handler)
        , exception_handler_(exception_handler)
        , wait_(max_idle_time)
        , slept_(0)
        , retries_(MAX_RETRIES_TIMES)
    {
        switch (waitStrategy) {
            case kSleepingStrategy:
                wait_strategy_ = boost::bind(&dynamic::sleepFor, wait_, _1);
                break;
            case kYieldingStrategy:
                wait_strategy_ = boost::bind(&dynamic::yieldThis, _1);
                break;
            case kBlockingStrategy:
            case kBusySpinStrategy:
                // not supported, fall through
            default:
                wait_strategy_ = boost::bind(&dynamic::yieldThis, _1);
                break;
        }
    }

    virtual Sequence* getSequence() { return &sequence_; }

    virtual void halt()
    {
        bool expected = true;
        int retries = 100;
        while (!running_.compare_exchange_strong(expected, false) && retries > 0) {
            expected = true;
            --retries;
            boost::this_thread::sleep(boost::posix_time::milliseconds(10));
        }
        running_.store(false);
    }

    virtual void run()
    {
        bool expected = false;
        if ( !running_.compare_exchange_strong(expected, true) ) {
            throw std::runtime_error("Thread is already running");
        }

        event_handler_->onStart();

        T event;
        size_t next_sequence(0);

        while (true) {
            try {
                next_sequence = 0;
                size_t available_sequence = ring_buffer_->occupied_approx();

                if (available_sequence == 0) {
                    if (wait_strategy_(retries_)) {
                        ++slept_;
                        retries_ = MAX_RETRIES_TIMES;
                        if (!running_) {
                            break;
                        }
                    }
                }
                else {
                    while (next_sequence < available_sequence
                            && ring_buffer_->dequeue(event)) {
                        event_handler_->onEvent(next_sequence,
                                available_sequence,
                                next_sequence + 1 == available_sequence,
                                &event);
                        ++next_sequence;
                    }
                    // FIXME: this is only useful for debugging now,
                    // but still potentially expensive and inaccurate,
                    // need to remove it!
                    sequence_.incrementAndGet(next_sequence, boost::memory_order_relaxed);
                    retries_ = MAX_RETRIES_TIMES;
                }

                if (wait_.ticks() != 0 && retries_ == MAX_RETRIES_TIMES) {
                    // no matter there was events or not, always notify handler
                    // with NULL event for special handling
                    event_handler_->onEvent(0, 0, false, NULL);
                }
            }
            catch(const AlertException& e) {
                break;
            }
            catch(const std::exception& e) {
                if (exception_handler_) {
                    exception_handler_->handle(e, next_sequence, &event);
                }
            }
        }

        event_handler_->onShutdown();
        running_.store(false);
        //std::cout<<"slept for "<<wait_<<" "<<slept_<<" times"<<std::endl;
    }

    void operator()() { run(); }

private:
    boost::atomic<bool> running_;
    Sequence sequence_;

    DynamicRingBuffer<T>* ring_buffer_;

    dynamic::WaitStrategy wait_strategy_;
    IEventHandler<T>* event_handler_;
    IExceptionHandler<T>* exception_handler_;
    boost::posix_time::time_duration wait_;
    int slept_;
    int retries_;
};

};  // namespace disruptor

#endif

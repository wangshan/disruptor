#ifndef DISRUPTOR_TEST_UTILS_H
#define DISRUPTOR_TEST_UTILS_H

#include <boost/date_time/posix_time/posix_time.hpp>
#include <disruptor/interface.h>
#include <sstream>

#include "time.h"

namespace disruptor {
namespace test {

const int64_t ONE_SEC_IN_NANO = 1000L * 1000L * 1000L;

template <typename T>
class NoOpEventProcessor : public IEventProcessor<T>
{
    // NOTE: Sequence does NOT have virtual function
    class SequencerFollowingSequence : public Sequence
    {
        public:
            SequencerFollowingSequence(Sequencer* following)
                : following_(following) {}

            int64_t get() const { return following_->getCursor(); }

        private:
            Sequencer* following_;
    };

public:
    NoOpEventProcessor(Sequencer* ring_buffer)
        : sequence_(ring_buffer)
    {
    }

    virtual Sequence* getSequence() { return &sequence_; } // will be sliced

    virtual void halt() {}

    virtual void run() {}

private:
    SequencerFollowingSequence sequence_;
};


class StubEvent
{
    public:
        explicit StubEvent(int i) : value_(i) {}

        StubEvent() : value_(0) {}

        void set_value(int i) { value_ = i; }
        const int value() const { return value_; }

    private:
        int value_;
};

class StubEventFactory : public IEventFactory<StubEvent>
{
     public:
         virtual ~StubEventFactory() {};

         boost::shared_ptr<StubEvent> newInstance() const {
             return boost::make_shared<StubEvent>();
         }
};

// used for performance test
class TimestampEvent
{
    public:
        explicit TimestampEvent(long i) : value_(i) {}

        TimestampEvent(long i, const disruptor::MonoTime& time) : value_(0), time_(time) {}
        TimestampEvent() : value_(0) {}

        void set_value(long i, const disruptor::MonoTime& time) { value_ = i; time_ = time; }
        const long value() const { return value_; }
        const disruptor::MonoTime& time() const { return time_; }

    private:
        long value_;
        disruptor::MonoTime time_;
};

class TimestampEventFactory : public IEventFactory<TimestampEvent>
{
     public:
         virtual ~TimestampEventFactory() {};

         boost::shared_ptr<TimestampEvent> newInstance() const {
             return boost::make_shared<TimestampEvent>();
         }
};


class TimestampBatchHandler : public IEventHandler<TimestampEvent>
{
    public:
        TimestampBatchHandler(int64_t benchmark = 0, int64_t sampling = 1)
            : total_latency_(0)
            , benchmark_(benchmark)
            , latency_below_benchmark_(0)
            , min_latency_(std::numeric_limits<int64_t>::max())
            , max_latency_(0)
            , count_(0)
            , alarm_called_(0)
            , sampling_(sampling)
            , sampled_(0)
        {
        }

        virtual void onEvent(const int64_t& sequence,
                             const int64_t& batch_size,
                             const bool& end_of_batch,
                             TimestampEvent* event)
        {
            if (event == NULL) {
                ++alarm_called_;
                return;
            }
            // process a new event as it becomes available.
            ++count_;

            if (sampling_ > 0 && count_ % sampling_ == 0) {
                ++sampled_;

                int64_t latency
                    = boost::chrono::duration_cast<disruptor::Nanoseconds>(
                            disruptor::MonoTime::clock::now() - event->time()).count();
//                std::cout<<"latency = "<<latency<<" ns"<<std::endl;
//                std::cout<<"batch_size = "<<batch_size<<std::endl;

                if (latency > 0) {
                    min_latency_ = (latency<min_latency_)?latency:min_latency_;
                    max_latency_ = (latency>max_latency_)?latency:max_latency_;
                }
                else if (latency < 0) {
                    std::ostringstream oss;
                    oss <<"latency less than 0? " << "latency = " << latency;
                    throw std::runtime_error(oss.str());
                }

                if( latency <= benchmark_ ) {
                    ++latency_below_benchmark_;
                }

                total_latency_ += latency;
            }

//            if( sequence != event->value() ) {
//                std::ostringstream oss;
//                oss << "sequence not the same as value!! sequence = " << sequence
//                    << " event->value = " << event->value();
//                throw std::runtime_error(oss.str());
//            }
            /*else {
                std::ostringstream oss;
                oss << "sequence = " << sequence
                    << " event->value = " << event->value();
                std::cout<<oss.str()<<std::endl;
            }*/
        }

        virtual void onStart() {}

        virtual void onShutdown() {}

        int64_t total_latency() const { return total_latency_; }

        int64_t max_latency() const { return max_latency_; }

        int64_t min_latency() const { return min_latency_; }

        int64_t latency_below_benchmark() const { return latency_below_benchmark_; }

        uint64_t count() const { return count_; }

        uint64_t alarm_called() const { return alarm_called_; }

        uint64_t sampled() const { return sampled_; }

    private:
        int64_t total_latency_;
        int64_t benchmark_;
        int64_t latency_below_benchmark_;
        int64_t min_latency_;
        int64_t max_latency_;
        uint64_t count_;
        uint64_t alarm_called_;
        const int64_t sampling_;
        uint64_t sampled_;
};

class TimestampEventTranslator : public IEventTranslator<TimestampEvent>
{
    public:
        TimestampEventTranslator()
            : m_stamp(disruptor::MonoTime::clock::now())
        {
        }

        explicit TimestampEventTranslator(const disruptor::MonoTime& stamp)
            : m_stamp(stamp)
        {
        }

        virtual TimestampEvent* translateTo(const int64_t& sequence, TimestampEvent* event)
        {
            event->set_value(sequence, m_stamp);
            return event;
        }

    private:
        disruptor::MonoTime m_stamp;
};

// NOT THREAD SAFE
class IgnoreExceptionHandler : public IExceptionHandler<TimestampEvent>
{
    public:
        virtual void handle(const std::exception& exception,
                            const int64_t& sequence,
                            TimestampEvent* event)
        {
            std::cerr<<"exception caught when processing event at sequence "<<sequence
                <<", "<<exception.what()<<std::endl;
        }
};

}
}

#endif

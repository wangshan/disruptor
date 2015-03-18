#ifndef DISRUPTOR_TEST_FIXTURE_H
#define DISRUPTOR_TEST_FIXTURE_H

#include <sys/time.h>

#include <exception>
#include <iostream>
#include <memory>
#include <boost/scoped_ptr.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>

#include <disruptor/disruptor.h>
#include <gtest/gtest.h>

#include "TestUtils.h"

namespace disruptor {
namespace test {

static const int64_t BENCHMARK_NS = 1000UL * 1UL;
static const int64_t SAMPLING_BY = 100;
static const size_t BUFFER_SIZE = 1024 * 8 * 8;
static const int DEFAULT_SENDING_BATCH_SIZE = 10;
static const int COST_OF_A_TIME_FUNCTION_CALL_NS = 30;

class Producer
{
    private:
        long iterations_;
        Disruptor<test::TimestampEvent>& disruptor_;
        int throttle_;
        const int batch_;
//        std::vector<test::TimestampEventTranslator> events_;

    public:
        explicit Producer(long i
                , Disruptor<test::TimestampEvent>& disruptor
                , int throttle)
            : iterations_(i)
            , disruptor_(disruptor)
            , throttle_(throttle)
            , batch_(DEFAULT_SENDING_BATCH_SIZE)
        {
//            events_.reserve(batch_);
//            for (int i=0; i<batch_; ++i) {
//                events_.push_back(test::TimestampEventTranslator());
//            }
        }

        void throttle(int i)
        {
            if(i % throttle_ == 0) {
                struct timespec tm;
                tm.tv_sec = 0;
                tm.tv_nsec = 50*1000;
                ::clock_nanosleep(CLOCK_MONOTONIC, 0, &tm, NULL);
            }
        }

        void operator() ()
        {
            using namespace disruptor;
            using namespace boost::chrono;
            assert(batch_ > 0);
            int num_batches = iterations_ / batch_;
            const Nanoseconds interval_between_batch(
                    Nanoseconds(ONE_SEC_IN_NANO / num_batches - COST_OF_A_TIME_FUNCTION_CALL_NS));

            disruptor::MonoTime start;
            for (int j=0; j<num_batches; ++j) {
                start = MonoTime::clock::now();
                // stamp a batch of events in advance, esstentially mimic a 
                // burst of messages
                test::TimestampEventTranslator translator;

                // publish this batch
                for (int i=0; i<batch_; ++i) {
                    disruptor_.publishEvent(&translator);
                    if (throttle_ > 0) {
                        this->throttle(i);
                    }
                }

                while (duration_cast<Nanoseconds>(MonoTime::clock::now() - start) < interval_between_batch) {
                }
            }
        }
};

class DynamicProducer
{
    private:
        long iterations_;
        DynamicDisruptor<test::TimestampEvent>& disruptor_;
        int throttle_;
        const int batch_;
//        std::vector<test::TimestampEvent> events_;

    public:
        explicit DynamicProducer(long i
                , DynamicDisruptor<test::TimestampEvent>& disruptor
                , int throttle)
            : iterations_(i)
            , disruptor_(disruptor)
            , throttle_(throttle)
            , batch_(DEFAULT_SENDING_BATCH_SIZE)
        {
//            events_.reserve(batch_);
//            for (int i=0; i<batch_; ++i) {
//                events_.push_back(test::TimestampEvent());
//            }
        }

        void throttle(int i)
        {
            if(i % throttle_ == 0) {
                struct timespec tm;
                tm.tv_sec = 0;
                tm.tv_nsec = 10*1000;
                ::clock_nanosleep(CLOCK_MONOTONIC, 0, &tm, NULL);
            }
        }

        void operator() ()
        {
            using namespace disruptor;
            using namespace boost::chrono;
            assert(batch_ > 0);
            int num_batches = iterations_ / batch_;
            const Nanoseconds interval_between_batch(
                    Nanoseconds(ONE_SEC_IN_NANO / num_batches - COST_OF_A_TIME_FUNCTION_CALL_NS));

            MonoTime start;
            for (int j=0; j<num_batches; ++j) {
                // stamp a batch of events in advance, esstentially mimic a 
                // burst of messages
                start = MonoTime::clock::now();

                // publish this batch
                for (int i=0; i<batch_; ++i) {
                    disruptor_.publishEvent(TimestampEvent(j * batch_ + i, start));
                    if (throttle_ > 0) {
                        this->throttle(i);
                    }
                }

                while (duration_cast<Nanoseconds>(MonoTime::clock::now() - start) < interval_between_batch) {
                }
            }
        }
};


template<int NumProducer>
class SingleSleeping : public Disruptor<test::TimestampEvent>
{
    public:
        SingleSleeping(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kSingleThreadedStrategy, kSleepingStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer>
class SingleYielding : public Disruptor<test::TimestampEvent>
{
    public:
        SingleYielding(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kSingleThreadedStrategy, kYieldingStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer>
class SingleBlocking : public Disruptor<test::TimestampEvent>
{
    public:
        SingleBlocking(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kSingleThreadedStrategy, kBlockingStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer>
class MultiSleeping : public Disruptor<test::TimestampEvent>
{
    public:
        MultiSleeping(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kMultiThreadedStrategy, kSleepingStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer>
class MultiYielding : public Disruptor<test::TimestampEvent>
{
    public:
        MultiYielding(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kMultiThreadedStrategy, kYieldingStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer>
class MultiLowContentionYielding : public Disruptor<test::TimestampEvent>
{
    public:
        MultiLowContentionYielding(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kMultiThreadedLowContentionStrategy, kYieldingStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer>
class SingleBusySpin : public Disruptor<test::TimestampEvent>
{
    public:
        SingleBusySpin(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kSingleThreadedStrategy, kBusySpinStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer>
class MultiBusySpin : public Disruptor<test::TimestampEvent>
{
    public:
        MultiBusySpin(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kMultiThreadedStrategy, kBusySpinStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer>
class MultiLowContentionBusySpin : public Disruptor<test::TimestampEvent>
{
    public:
        MultiLowContentionBusySpin(int buffer_size, test::TimestampBatchHandler* handler) :
            Disruptor<test::TimestampEvent>(buffer_size, kMultiThreadedLowContentionStrategy, kBusySpinStrategy, handler, NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef Producer producer_type;
};

template<int NumProducer, WaitStrategyOption WaitStrategy>
class DynamicSingleWith : public DynamicDisruptor<test::TimestampEvent>
{
    public:
        DynamicSingleWith(int buffer_size, test::TimestampBatchHandler* handler)
            : DynamicDisruptor<test::TimestampEvent>(
                    buffer_size,
                    kSingleThreadedStrategy,
                    WaitStrategy,
                    handler,
                    NULL)
        {
        }

        int supportedProducerNum() const
        {
            return NumProducer;
        }
        typedef DynamicProducer producer_type;
};


template <typename DisruptorType>
class DisruptorPerfFixture : public ::testing::Test
{
    protected:
        int                              sampling_;
        test::TimestampBatchHandler      tm_handler_;
        DisruptorType                    disruptor_;

        virtual ~DisruptorPerfFixture()
        {
        }

        DisruptorPerfFixture()
            : sampling_(SAMPLING_BY)
            , tm_handler_(BENCHMARK_NS, sampling_)
            , disruptor_(BUFFER_SIZE, &tm_handler_)
        {
        }

        virtual void SetUp()
        {
        }

        virtual void TearDown()
        {
        }

};

}
}

#endif

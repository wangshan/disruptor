#include <exception>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/ref.hpp>

#include <disruptor/event_processor.h>
#include <disruptor/ring_buffer.h>
#include <disruptor/dynamic_event_processor.h>
#include <disruptor/dynamic_ring_buffer.h>

#include <gtest/gtest.h>

#include "utils.h"

static const unsigned int BUFFER_SIZE = 8;


namespace disruptor {
namespace test {

class DynamicProcessorFixture : public ::testing::Test
{
protected:
    DynamicProcessorFixture()
        : handler(50, 1)
        , ring_buffer(BUFFER_SIZE,
                    disruptor::kSingleThreadedStrategy,
                    disruptor::kSleepingStrategy)
        , processor(&ring_buffer
                , disruptor::kSleepingStrategy
                , &handler
                , &except_handler
                , GetTimeConfig(
                    timeConfig, kMaxIdle, boost::posix_time::microseconds(10)))
    {
    }

    TimestampBatchHandler handler;
    IgnoreExceptionHandler except_handler;
    DynamicRingBuffer<TimestampEvent> ring_buffer;
    DynamicProcessor<TimestampEvent> processor;
    TimeConfig timeConfig;
};

TEST_F(DynamicProcessorFixture, testConstruct)
{
    EXPECT_EQ(BUFFER_SIZE, ring_buffer.available_approx());
    EXPECT_EQ(0UL, ring_buffer.occupied_approx());

    boost::thread consumer_thread(boost::ref< DynamicProcessor<TimestampEvent> >(processor));

    //ASSERT_NO_THROW(ring_buffer.enqueue(TimestampEvent(42)));

    //sleepFor(Microseconds(100));

    //EXPECT_EQ(BUFFER_SIZE, ring_buffer.available_approx());
    //EXPECT_EQ(0UL, ring_buffer.occupied_approx());

    processor.halt();
    consumer_thread.join();
}

}
}

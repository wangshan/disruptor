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

class DynamicRingBufferFixture : public ::testing::Test
{
protected:
    DynamicRingBufferFixture()
        : ring_buffer(BUFFER_SIZE,
                    disruptor::kSingleThreadedStrategy,
                    disruptor::kSleepingStrategy)
    {
    }

    ~DynamicRingBufferFixture() {}

    DynamicRingBuffer<StubEvent> ring_buffer;
};


TEST_F(DynamicRingBufferFixture, testEnqueueAndDequeue)
{
    StubEvent expected_event(1234);
    EXPECT_EQ(BUFFER_SIZE, ring_buffer.available_approx());

    ASSERT_NO_THROW(ring_buffer.enqueue(expected_event));
    EXPECT_EQ(1UL, ring_buffer.num_blocks());
    EXPECT_EQ(BUFFER_SIZE - 1, ring_buffer.available_approx());

    StubEvent received_event;
    EXPECT_TRUE(ring_buffer.dequeue(received_event));
    EXPECT_EQ(expected_event.value(), received_event.value());
    EXPECT_EQ(1UL, ring_buffer.num_blocks());
    EXPECT_EQ(BUFFER_SIZE, ring_buffer.available_approx());
    EXPECT_EQ(0UL, ring_buffer.occupied_approx());
}

TEST_F(DynamicRingBufferFixture, testEnqueueAndDequeueWithMoreThanOneBlock)
{
    EXPECT_EQ(1UL, ring_buffer.num_blocks());
    EXPECT_EQ(BUFFER_SIZE, ring_buffer.available_approx());

    unsigned expected_blocks = 3;
    unsigned total_event = BUFFER_SIZE * (expected_blocks - 1) + 3;
    for (unsigned i = 0; i < total_event; ++i) {
        StubEvent expected_event(i);

        ASSERT_NO_THROW(ring_buffer.enqueue(expected_event));
    }
    EXPECT_EQ(expected_blocks, ring_buffer.num_blocks());
    EXPECT_EQ(total_event, ring_buffer.occupied_approx());

    StubEvent received_event;
    unsigned dequeued_so_far = 0;
    while (ring_buffer.dequeue(received_event)) {
        ++dequeued_so_far;
        if (dequeued_so_far % BUFFER_SIZE == 0) {
            EXPECT_EQ(total_event-=BUFFER_SIZE, ring_buffer.occupied_approx());
        }
        // block never get removed!
        EXPECT_EQ(expected_blocks, ring_buffer.num_blocks());
        EXPECT_EQ((int)dequeued_so_far-1, received_event.value());
    }
    EXPECT_EQ(expected_blocks, ring_buffer.num_blocks());
    EXPECT_EQ(BUFFER_SIZE * expected_blocks, ring_buffer.available_approx());
    EXPECT_EQ(0UL, ring_buffer.occupied_approx());
}

std::vector<StubEvent> consume(DynamicRingBuffer<StubEvent>& ring_buffer,
        unsigned expected_total,
        unsigned sleep_us,
        unsigned sleep_limit)
{
    std::vector<StubEvent> results;
    StubEvent received;
    unsigned received_so_far(0);
    //std::cout<<"expected_total="<<expected_total<<std::endl;
    //std::cout<<"sleep_us="<<sleep_us<<std::endl;

    while (received_so_far != expected_total) {
        while(received_so_far != expected_total
                && !ring_buffer.dequeue(received)) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(1));
            if (--sleep_limit == 0) {
                std::cout<<"deadlock, received_so_far="<<received_so_far<<std::endl;
                return results;
            }
        }

        ++received_so_far;
        //std::cout<<"received_so_far="<<received_so_far<<std::endl;
        results.push_back(received);
        if (sleep_us != 0) {
            boost::this_thread::sleep(boost::posix_time::microseconds(sleep_us));
        }
    }

    return results;
};

TEST_F(DynamicRingBufferFixture, testEnqueueAndDequeueInSeperateThread)
{
    unsigned expected_blocks = 3;
    unsigned total_event = BUFFER_SIZE * (expected_blocks - 1) + 3;

    boost::packaged_task< std::vector<StubEvent> > consumer(
            boost::bind(&consume, boost::ref(ring_buffer), total_event, 0, 2000));
    boost::unique_future<std::vector<StubEvent> > future = consumer.get_future();
    boost::thread thread(boost::ref(consumer));

    std::vector<StubEvent> expected;
    for (unsigned i = 0; i < total_event; ++i) {
        StubEvent expected_event(i);
        expected.push_back(expected_event);

        ASSERT_NO_THROW(ring_buffer.enqueue(expected_event));
    }

    std::vector<StubEvent> results = future.get();

    ASSERT_TRUE(total_event == expected.size() && total_event == results.size());
    for (unsigned i = 0; i < total_event; ++i) {
        EXPECT_EQ(expected[i].value(), results[i].value());
    }
}

// minimum frequency is 1HZ
unsigned freqToMicrosecondInterval(unsigned freq, unsigned order)
{
    static const unsigned pow10[10] = {
        1,
        10,
        100,
        1000,
        10000,
        100000,
        1000000,
        10000000,
        100000000,
        1000000000
    };

    if (freq < 1 || order > 9) {
        // will return 0 if frequency is too high
        return 0;
    }

    unsigned significant = pow10[order] / freq;
    if (significant == 0) {
        return freqToMicrosecondInterval(freq, ++order);
    }
    else {
        return significant * 1000000 / pow10[order];
    }
}

TEST(DynamicRingBufferUtilityTest, freqToMicrosecondInterval)
{
    EXPECT_EQ(1000000U, freqToMicrosecondInterval(1, 0));
    EXPECT_EQ(200U, freqToMicrosecondInterval(5000, 0));
    EXPECT_EQ(30U, freqToMicrosecondInterval(30000, 0));
    EXPECT_EQ(1U, freqToMicrosecondInterval(1000000, 0));
}

struct TestParams
{
    // 0 frequency means saturated test - publish and consume as fast as we can
    unsigned producer_freq;
    unsigned consumer_freq;
    unsigned total_event;
    unsigned expected_blocks;
};

class ParamedDynamicRingBufferTest : public DynamicRingBufferFixture
                                   , public ::testing::WithParamInterface<TestParams>
{
public:
};

static TestParams params[] = {
    {0,         0,          BUFFER_SIZE*0 + 3,     1},
    {0,         0,          BUFFER_SIZE-1,         1},
    {0,         0,          BUFFER_SIZE*1 + 0,     2},
    {0,         0,          BUFFER_SIZE*3 + 3,     4},
    {0,         0,          BUFFER_SIZE*1000L + 3,     4},
    {10000,     5000,       BUFFER_SIZE*3 + 3,     4},
    {10000,     10000,      BUFFER_SIZE*3 + 3,     1},
    {5000,      10000,      BUFFER_SIZE*3 + 3,     1},
};

INSTANTIATE_TEST_CASE_P(VariousSizeAndUpdateFrequency,
                        ParamedDynamicRingBufferTest,
                        ::testing::ValuesIn(params)
                        );

TEST_P(ParamedDynamicRingBufferTest, testEnqueueAndDequeueAtVariousFrequency)
{
    unsigned total_event = this->GetParam().total_event;
    //unsigned expected_blocks = this->GetParam().expected_blocks;
    const unsigned produce_freq = this->GetParam().producer_freq;
    const unsigned consume_freq = this->GetParam().consumer_freq;

    unsigned sleep_times = produce_freq >= consume_freq ? 2000 : total_event;
    boost::packaged_task< std::vector<StubEvent> > consumer(
            boost::bind(
                &consume,
                boost::ref(ring_buffer),
                total_event,
                freqToMicrosecondInterval(consume_freq, 0),
                sleep_times
                )
            );
    boost::unique_future<std::vector<StubEvent> > future = consumer.get_future();
    boost::thread thread(boost::ref(consumer));

    std::vector<StubEvent> expected;
    for (unsigned i = 0; i < total_event; ++i) {
        StubEvent expected_event(i);
        expected.push_back(expected_event);

        ASSERT_NO_THROW(ring_buffer.enqueue(expected_event));
        //std::cout<<"sent="<<expected_event.value()<<std::endl;
        if (produce_freq != 0) {
            boost::this_thread::sleep(boost::posix_time::microseconds(
                        freqToMicrosecondInterval(produce_freq, 0)));
        }
    }

    std::vector<StubEvent> results = future.get();

    if (total_event != results.size()) {
        for (unsigned i = 0; i < results.size(); ++i) {
            std::cout<<"received="<<results[i].value()<<std::endl;
        }
    }

    ASSERT_EQ(total_event, expected.size());
    ASSERT_EQ(total_event, results.size());
    for (unsigned i = 0; i < total_event; ++i) {
        EXPECT_EQ(expected[i].value(), results[i].value());
    }

    std::cout<<"num blocks="<<ring_buffer.num_blocks()<<std::endl;
    // can't really check this reliably, depends on how the two threads proceed
    // EXPECT_EQ(expected_blocks, ring_buffer.num_blocks());
}


}; // namespace test
}; // namespace disruptor

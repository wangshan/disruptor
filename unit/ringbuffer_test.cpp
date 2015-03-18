#include <exception>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/ref.hpp>

#include <disruptor/event_processor.h>
#include <disruptor/ring_buffer.h>

#include <gtest/gtest.h>

#include "utils.h"

#define BUFFER_SIZE 64

namespace disruptor {
namespace test {

class RingBufferFixture : public ::testing::Test
{
protected:
    RingBufferFixture()
        : factory(new StubEventFactory())
        , ring_buffer(factory.get(),
                    BUFFER_SIZE,
                    disruptor::kSingleThreadedStrategy,
                    disruptor::kSleepingStrategy)
        , stub_processor(&ring_buffer)
        , barrier( ring_buffer.newBarrier(std::vector<Sequence*>(0)) )
    {
        std::vector<Sequence*> sequences;
        sequences.push_back(stub_processor.getSequence());
        ring_buffer.setGatingSequences(sequences);
    }

    ~RingBufferFixture() {}

    void fillBuffer()
    {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            int64_t sequence = ring_buffer.next();
            ring_buffer.publish(sequence);
        }
    }


    boost::shared_ptr<StubEventFactory> factory;
    RingBuffer<StubEvent> ring_buffer;
    NoOpEventProcessor<StubEvent> stub_processor;
    SequenceBarrierPtr barrier;
};

std::vector<StubEvent> Waiter(RingBuffer<StubEvent>* ring_buffer,
                              ISequenceBarrier* barrier,
                              int64_t initial_sequence,
                              int64_t to_wait_for_sequence)
{
    barrier->waitFor(to_wait_for_sequence);

    std::vector<StubEvent> results;
    for (int64_t i = initial_sequence; i <= to_wait_for_sequence; i++) {
        results.push_back(*ring_buffer->get(i));
    }

    return results;
};



class TestEventProcessor : public IEventProcessor<StubEvent>
{
    public:
        TestEventProcessor(SequenceBarrierPtr barrier)
            : barrier_(barrier)
            , sequence_(INITIAL_CURSOR_VALUE)
        {}

        virtual Sequence* getSequence() { return &sequence_; }

        virtual void halt() {}

        virtual void run()
        {
            try {
                barrier_->waitFor(0L);
            }
            catch(...) {
                throw std::runtime_error("catched exception in TestEventProcessor::Run()");
            }

            sequence_.set(sequence_.get() + 1L);
        }

    private:
        SequenceBarrierPtr barrier_;
        Sequence sequence_;
};


TEST_F(RingBufferFixture, testClaimAndGet)
{
    EXPECT_EQ(disruptor::INITIAL_CURSOR_VALUE, ring_buffer.getCursor());
    StubEvent expected_event(1234);

    int64_t claim_sequence = ring_buffer.next();
    StubEvent* old_event = ring_buffer.get(claim_sequence);
    old_event->set_value(expected_event.value());
    ring_buffer.publish(claim_sequence);

    int64_t sequence = barrier->waitFor(0);
    EXPECT_EQ(0, sequence);

    StubEvent* event = ring_buffer.get(sequence);
    EXPECT_EQ(expected_event.value(), event->value());

    EXPECT_EQ(0, ring_buffer.getCursor());
}

TEST_F(RingBufferFixture, testClaimAndGetWithTimeout)
{
    EXPECT_EQ(disruptor::INITIAL_CURSOR_VALUE, ring_buffer.getCursor());
    StubEvent expected_event(1234);

    int64_t claim_sequence = ring_buffer.next();
    StubEvent* old_event = ring_buffer.get(claim_sequence);
    old_event->set_value(expected_event.value());
    ring_buffer.publish(claim_sequence);

    int64_t sequence = barrier->waitFor(0, boost::posix_time::milliseconds(5000));
    EXPECT_EQ(0, sequence);

    StubEvent* event = ring_buffer.get(sequence);
    EXPECT_EQ(expected_event.value(), event->value());

    EXPECT_EQ(0, ring_buffer.getCursor());
}

TEST_F(RingBufferFixture, testGetWithTimeout)
{
    int64_t sequence = barrier->waitFor(0, boost::posix_time::milliseconds(5000));
    EXPECT_EQ(INITIAL_CURSOR_VALUE, sequence);
}

TEST_F(RingBufferFixture, testClaimAndGetInSeperateThread)
{
    boost::packaged_task<std::vector<StubEvent> > consumer(boost::bind(&Waiter, &ring_buffer, barrier.get(), 0LL, 0LL));
    boost::unique_future<std::vector<StubEvent> > future = consumer.get_future();
    boost::thread thread(boost::ref(consumer));

    StubEvent expected_event(1234);

    int64_t sequence = ring_buffer.next();
    StubEvent* old_event = ring_buffer.get(sequence);
    old_event->set_value(expected_event.value());
    ring_buffer.publish(sequence);

    std::vector<StubEvent> results = future.get();

    EXPECT_EQ(expected_event.value(), results[0].value());
}

TEST_F(RingBufferFixture, DISABLED_testWrap)
{
    int n_messages = BUFFER_SIZE;
    int offset = 1000;

    for (int i = 0; i < n_messages + offset; i++) {
        int64_t sequence = ring_buffer.next();
        StubEvent* event = ring_buffer.get(sequence);
        event->set_value(i);
        ring_buffer.publish(sequence);
    }

    int expected_sequence = n_messages + offset - 1;
    int64_t available= barrier->waitFor(expected_sequence);
    EXPECT_EQ(expected_sequence, available);

    for (int i = offset; i < n_messages + offset; i++) {
        EXPECT_EQ(i, ring_buffer.get(i)->value());
    }
}

TEST_F(RingBufferFixture, testGetAtSpecificSequence)
{
    int64_t expected_sequence = 5;

    ring_buffer.claim(expected_sequence);
    StubEvent* expected_event = ring_buffer.get(expected_sequence);
    expected_event->set_value((int) expected_sequence);
    ring_buffer.forcePublish(expected_sequence);

    int64_t sequence = barrier->waitFor(expected_sequence);
    EXPECT_EQ(expected_sequence, sequence);

    StubEvent* event = ring_buffer.get(sequence);
    EXPECT_EQ(expected_event->value(), event->value());

    EXPECT_EQ(ring_buffer.getCursor(), expected_sequence);
}

// Publisher will try to publish BUFFER_SIZE + 1 events. The last event
// should wait for at least one consume before publishing, thus preventing
// an overwrite. After the single consume, the publisher should resume and 
// publish the last event.

class TestPublishThread
{
    private:
        RingBuffer<StubEvent>* ring_buffer_;
        boost::atomic<bool> publisher_completed_;
        boost::atomic<int> counter_;

    public:
        TestPublishThread(RingBuffer<StubEvent>* ring_buffer_)
            : ring_buffer_(ring_buffer_)
            , publisher_completed_(false)
            , counter_(0)
        {
        }

        void operator() ()
        {
            for (int i = 0; i <= BUFFER_SIZE; i++) {
                int64_t sequence = ring_buffer_->next();
                StubEvent* event = ring_buffer_->get(sequence);
                event->set_value(i);
                ring_buffer_->publish(sequence);
                counter_.fetch_add(1L);
            }

            publisher_completed_.store(true);
        }

        bool PublisherCompleted() const { return publisher_completed_.load(); }
        int  Counter() const { return counter_.load(); }
};


TEST_F(RingBufferFixture, testPreventPublishersOvertakingEventProcessorWrapPoint)
{
    std::vector<Sequence*> dependency(0);
    TestEventProcessor processor(ring_buffer.newBarrier(dependency));
    dependency.push_back(processor.getSequence());
    ring_buffer.setGatingSequences(dependency);

    TestPublishThread publisher(&ring_buffer);

    // Publisher in a seperate thread
    boost::thread thread(boost::ref(publisher));

    while (publisher.Counter() < BUFFER_SIZE) {}

    int64_t sequence = ring_buffer.getCursor();
    EXPECT_EQ((BUFFER_SIZE - 1), sequence);
    EXPECT_FALSE(publisher.PublisherCompleted());

    processor.run();
    thread.join();

    EXPECT_TRUE(publisher.PublisherCompleted());
}


}; // namespace test
}; // namespace disruptor

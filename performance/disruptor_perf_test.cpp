#include "fixture.h"

namespace disruptor {
namespace test {

using namespace ::testing;

// Define typed tests
typedef ::testing::Types<
        SingleSleeping<1>,
        SingleYielding<1>,
        SingleBusySpin<1>,
        MultiSleeping<1>,
        MultiYielding<1>,
        MultiYielding<3>,
        MultiLowContentionYielding<3>,
        MultiBusySpin<1>,
        MultiBusySpin<3>,
        MultiLowContentionBusySpin<3>,
        DynamicSingleWith<1, kSleepingStrategy>,
        DynamicSingleWith<1, kYieldingStrategy>
    > DisruptorTypes;
TYPED_TEST_CASE(DisruptorPerfFixture, DisruptorTypes);

TYPED_TEST(DisruptorPerfFixture, PushThroughTimeEventWithSingleComsumer)
{
    unsigned grace_period_sec = 3;
    long iterations = 1000L * 1000 * 10;
    int throttle_per = 0; // sleeps 10us every x messages;
    // Note: we are inside a derived class template, use this-> to access fixture memebers
    const int num_producers = this->disruptor_.supportedProducerNum();
    std::vector< typename TypeParam::producer_type > producerObjs(
            num_producers,
            typename TypeParam::producer_type(iterations, (this->disruptor_), throttle_per));
    boost::thread_group producers;

    struct timespec start_time, end_time;
    // +----- start timer -----+
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < num_producers; ++i ) {
        producers.create_thread(boost::ref< typename TypeParam::producer_type >(producerObjs[i]));
    }

    iterations = num_producers * iterations;
    int64_t expected_sequence = iterations - 1;
    while (this->disruptor_.processor().getSequence()->get() < expected_sequence) {}
    sleep(grace_period_sec);

    this->disruptor_.stop();
    producers.join_all();

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    // +----- stop timer -----+

    double start, end;
    start = start_time.tv_sec + ((double) start_time.tv_nsec / (ONE_SEC_IN_NANO));
    end = end_time.tv_sec + ((double) end_time.tv_nsec / (ONE_SEC_IN_NANO));
    double duration = end - start - grace_period_sec;

    std::cout.precision(15);
    std::cout << num_producers << "-Publisher-1-Processor performance: ";
    std::cout << (iterations * 1.0) / duration << " ops/secs" << std::endl;
    std::cout << "iterations = " << this->tm_handler_.count() << std::endl;
    std::cout << "duration = " << duration << " secs" << std::endl;
    uint64_t sampled = this->tm_handler_.sampled();
    ASSERT_TRUE(sampled > 0);
    std::cout << "sampled = " << sampled << std::endl;
    std::cout << "ns per op = " << duration * ONE_SEC_IN_NANO /iterations << std::endl;
    double mean = (this->tm_handler_.total_latency() / sampled);
    std::cout << "mean latency = " << mean << " ns" << std::endl;
    std::cout << "max latency = " << this->tm_handler_.max_latency() << " ns" << std::endl;
    std::cout << "min latency = " << this->tm_handler_.min_latency() << " ns" << std::endl;
    std::cout << "\% latency below " << BENCHMARK_NS << " ns = "
              << this->tm_handler_.latency_below_benchmark()/1.0/sampled*100
              << "\%" << std::endl;
    std::cout << "alarm called = " << this->tm_handler_.alarm_called() << " times" << std::endl;

    //RecordProperty("mean_latency", mean);

}

}
}

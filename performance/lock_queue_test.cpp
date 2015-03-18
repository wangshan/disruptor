#include <sys/time.h>

#include <exception>
#include <iostream>
#include <memory>
#include <boost/scoped_ptr.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>

#include <gtest/gtest.h>

#include "lock_queue.h"
#include "queue_tester.h"


static const uint64_t ONE_SEC_IN_NANO = 1000UL * 1000UL * 1000UL;
static const uint64_t BENCHMARK_NS = 1000UL * 50UL;

class LockDequeTest : public ::testing::Test,
                      public ::testing::WithParamInterface<int> 
{
};

INSTANTIATE_TEST_CASE_P(VariableProducers,
        LockDequeTest,
        ::testing::Values(1, 3));

TEST_P(LockDequeTest, DISABLED_MultiProducerSingleConsumer)
{
    long iterations = 1000L * 1000L * 10;
    const int num_producers = this->GetParam();

    iterations = num_producers * iterations;

    struct timespec start_time, end_time;
    // +----- start timer -----+
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    QueueTester<LockQueue> lock_queue_test(num_producers, iterations);
    lock_queue_test.run();

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    // +----- stop timer -----+

    double start, end;
    start = start_time.tv_sec + ((double) start_time.tv_nsec / (ONE_SEC_IN_NANO));
    end = end_time.tv_sec + ((double) end_time.tv_nsec / (ONE_SEC_IN_NANO));
    double duration = end - start;

    std::cout.precision(15);
    std::cout << num_producers << "-Publisher-1-Processor performance: ";
    std::cout << (iterations * 1.0) / duration << " ops/secs" << std::endl;
    std::cout << "iterations = " << lock_queue_test.received_count() << std::endl;
    std::cout << "duration = " << duration << " secs" << std::endl;
    std::cout << "ns per op = " << duration * ONE_SEC_IN_NANO /iterations << std::endl;
    std::cout << "mean latency = " << lock_queue_test.mean_latency_us()/1000.0 << " ns" << std::endl;
}

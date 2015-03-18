#ifndef LOCKFREE_QUEUETESTER_H_
#define LOCKFREE_QUEUETESTER_H_

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "message.h"
#include "lock_queue.h"


typedef IG::LockQueue<Message*> LockQueue;

template<typename T>
class QueueTester
{ 
    public:
        explicit QueueTester(unsigned int num_producers, uint64_t size)
            : m_total_latency(0)
            , m_min_latency(std::numeric_limits<uint64_t>::max())
            , m_max_latency(0)
            , m_received_count(0)
            , m_num_producers(num_producers)
            , m_size(size)
            , m_messages(size)
        {}

        void produce(unsigned int id)
        {
            uint64_t total_production = m_size/m_num_producers;
            uint64_t begin_index = id*total_production;
            uint64_t end_index = (id+1)*total_production;
            uint64_t total = 0;
            for( uint64_t i = begin_index; i < end_index; ++i ) {
                Message* m = &m_messages[i];
                m->stamptime( boost::posix_time::microsec_clock::universal_time() );
                m_queue.put(m);
                ++total;
            }
        }

        void consume()
        {
            while( m_received_count <  m_size ) {
                Message* msg = m_queue.get();
                if( msg ) {
                    boost::posix_time::ptime get_time = boost::posix_time::microsec_clock::universal_time();
                    boost::posix_time::time_duration latency_duration = get_time - msg->timestamp();
                    uint64_t latency = latency_duration.total_microseconds();
                    m_total_latency += latency; 
                    m_min_latency = (latency<m_min_latency)?latency:m_min_latency;
                    m_max_latency = (latency>m_max_latency)?latency:m_max_latency;
                    ++m_received_count;
                }
            }
        }

        void run()
        {
            boost::thread consumer(&QueueTester::consume, this);
            boost::thread_group producers;
            boost::posix_time::ptime begin_time = boost::posix_time::microsec_clock::universal_time();
            for( unsigned int producer_id = 0; producer_id < m_num_producers; ++producer_id ) {
                producers.create_thread( boost::bind(&QueueTester::produce, this, producer_id) );
            }
            consumer.join();
            producers.join_all();
            boost::posix_time::ptime end_time = boost::posix_time::microsec_clock::universal_time();
            m_duration = end_time - begin_time;
        }

        const uint64_t duration() const
        { return m_duration.total_microseconds(); }

        const uint64_t mean_latency_us() const
        { return m_total_latency/m_received_count; }

        const uint64_t received_count() const
        { return m_received_count; }

    private:
        T m_queue;
        boost::posix_time::time_duration m_duration;
        uint64_t m_total_latency;
        uint64_t m_min_latency;
        uint64_t m_max_latency;
        uint64_t m_received_count;
        uint64_t m_num_producers;
        uint64_t m_size;

        // only used by producers
        std::vector<Message> m_messages;
};

#endif

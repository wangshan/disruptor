#ifndef LOCKFREE_LOCK_QUEUE_H_
#define LOCKFREE_LOCK_QUEUE_H_

#include <boost/thread.hpp>
#include <iostream>

namespace disruptor
{

// T has to be a pointer
template<typename T>
class LockQueue
{
    public:
        LockQueue() 
        {
        }

        ~LockQueue() 
        {
        }

        void put(T msg) 
        {
            boost::unique_lock<boost::mutex> lock(m_mutex);
            q.push_back(msg);
        }

        T get()
        {
            boost::unique_lock<boost::mutex> lock(m_mutex);
            if( q.empty() ) {
                return NULL;
            }
            T res = q.front();
            q.pop_front();
            return res;
        }

    private:
        // TODO: q should be bounded
        std::deque<T> q;
        boost::mutex m_mutex;
};

}

#endif

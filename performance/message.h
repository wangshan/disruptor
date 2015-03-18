#ifndef LOCKFREE_TYPES_H_
#define LOCKFREE_TYPES_H_

#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>

static const unsigned int MESSAGE_SIZE = 128;

struct Message
{
    private:
        //char* m_data;
        char m_data[MESSAGE_SIZE];
        boost::posix_time::ptime m_timestamp;

    public:
        Message()
        {
            //m_data = new char[MESSAGE_SIZE];
        }

        void stamptime(boost::posix_time::ptime t)
        {
            m_timestamp = t;
        }

        const boost::posix_time::ptime& timestamp() const
        {
            return m_timestamp;
        }

        /*
        void set(char* data, size_t len)
        {
        len = len > MESSAGE_SIZE ? MESSAGE_SIZE : len;
        ::memcpy(m_data, data, len);
        m_data[len]='\0';
        }
        */

        ~Message()
        {
            //delete[] m_data;
        }
};

#endif

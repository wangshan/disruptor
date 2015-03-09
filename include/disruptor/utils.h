#ifndef DISRUPTOR_UTILS_H_
#define DISRUPTOR_UTILS_H_

#include <climits>
#include <vector>
#include <map>
#include <memory>

// According to Stroustrup, in C++11 the macro __cplusplus will be set to a
// value that differs from (is greater than) the current 199711L.
#if __cplusplus <= 199711L
#define stdext boost
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/make_shared.hpp>
#include <boost/chrono.hpp>
#include <boost/atomic.hpp>
#include <boost/utility.hpp>
#include <boost/thread.hpp>
#else
#define stdext std
#include <chrono>
#include <thread>
#endif


namespace disruptor {

enum TimeConfigKey {
    kSleep,
    kMaxIdle
};

typedef std::map<TimeConfigKey, stdext::chrono::microseconds> TimeConfig;

inline stdext::chrono::microseconds getTimeConfig(
        const TimeConfig& timeConfig,
        TimeConfigKey key,
        const stdext::chrono::microseconds& defVal)
{
    TimeConfig::const_iterator it = timeConfig.find(key);
    if (it != timeConfig.end()) {
        return it->second;
    }
    else {
        return defVal;
    }
}

inline size_t ceilToPow2(size_t x)
{
    // From http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    for (size_t i = 1; i < sizeof(size_t); i <<= 1) {
        x |= x >> (i << 3);
    }
    ++x;
    return x;
}

}

#endif

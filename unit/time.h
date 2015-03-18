#ifndef DISRUPTOR2_TIME_H
#define DISRUPTOR2_TIME_H

#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <errno.h>

#include <iomanip>
#include <sstream>

#include <boost/chrono/include.hpp>

namespace disruptor {

extern const time_t NANOSECONDS_IN_SECOND;
extern const time_t MICROSECONDS_IN_SECOND;
extern const time_t NANOSECONDS_IN_MILLISECOND;
extern const time_t MILLISECONDS_IN_SECOND;
extern const time_t MICROSECONDS_IN_MILLISECOND;
extern const time_t NANOSECONDS_IN_MICROSECOND;
extern const time_t SECONDS_IN_DAY;
extern const time_t SECONDS_IN_HOUR;
extern const time_t SECONDS_IN_MINUTE;
extern const time_t EPOCH_OF_STARTUP;


typedef boost::chrono::hours Hours;
typedef boost::chrono::minutes Minutes;
typedef boost::chrono::seconds Seconds;
typedef boost::chrono::milliseconds Milliseconds;
typedef boost::chrono::microseconds Microseconds;
typedef boost::chrono::nanoseconds Nanoseconds;

typedef boost::chrono::system_clock RealClock;
typedef boost::chrono::steady_clock MonoClock;
typedef RealClock::time_point Time;
typedef MonoClock::time_point MonoTime;

#define NotATime Time::min()
#define NotAMonoTime MonoTime::min()


// helper functions

// TODO: handle both Time and MonoTime

inline time_t secondsOf(const Time& time)
{
    return RealClock::to_time_t(time);
}

template <typename Duration>
int64_t countOf(const Time& time)
{
    return (
            boost::chrono::duration_cast<Duration>(time.time_since_epoch())
           ).count();
}

inline time_t millisecondsOf(const Time& time)
{
    return (
            boost::chrono::duration_cast<Milliseconds>(
                time.time_since_epoch()
                )
            ).count() % MILLISECONDS_IN_SECOND;
}

inline time_t microsecondsOf(const Time& time)
{
    return (
            boost::chrono::duration_cast<Microseconds>(
                time.time_since_epoch()
                )
            ).count() % MICROSECONDS_IN_SECOND;
}

inline time_t nanosecondsOf(const Time& time)
{
    return (
            boost::chrono::duration_cast<Nanoseconds>(
                time.time_since_epoch()
                )
            ).count() % NANOSECONDS_IN_SECOND;
}

inline double toDouble(const Time& time)
{
    // convert to double duration of seconds
    typedef boost::chrono::duration<double> Dseconds;
    const Dseconds& dseconds = time.time_since_epoch();
    return dseconds.count();
}

inline timespec toTimespec(const Time& time)
{
    timespec ts;
    Nanoseconds xt = time.time_since_epoch();
    ts.tv_sec = xt.count() / NANOSECONDS_IN_SECOND;
    ts.tv_nsec = xt.count() % NANOSECONDS_IN_SECOND;
    if (ts.tv_nsec < 0) {
        ts.tv_nsec += NANOSECONDS_IN_SECOND;
        --ts.tv_sec;
    }
    return ts;
}

inline std::string toString(const Time& time)
{
    char timeBuf[32];
    const size_t timeLen = sizeof(timeBuf);

    time_t tv_sec = secondsOf(time);
    time_t tv_nsec = nanosecondsOf(time);
    int written = ::snprintf(timeBuf, timeLen, "%ld.%09ld",
            tv_sec, tv_nsec);
    if (0 < written && (size_t)written < timeLen) {
        return timeBuf;
    }

    return "<Invalid Time>";
}

inline Time fromTimespec(const timespec& ts)
{
    return Time() + Seconds(ts.tv_sec) + Nanoseconds(ts.tv_nsec);
}

inline bool isElapsed(const Time& time)
{
    return time <= Time::clock::now();
}

inline int sleepFor(const Nanoseconds& duration) // can't sleep less than 1 nano
{
    timespec ts;
    ts.tv_sec = duration.count() / NANOSECONDS_IN_SECOND;
    ts.tv_nsec = duration.count() % NANOSECONDS_IN_SECOND;
    return ::clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
}

inline int sleepUntil(const Time& time)
{
    timespec ts = toTimespec(time);
    return ::clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
}

inline int sleepUntil(const Time& time, void(*interruption_point)())
{
    int status;
    do {
        status = sleepUntil(time);
        interruption_point();
    } while (status == EINTR);
    return status;
}

inline int sleepNoInterruptUntil(const Time& time)
{
    int status;
    do {
        status = sleepUntil(time);
    } while (status == EINTR);
    return status;
}

inline Milliseconds millisecondsSinceMidnight()
{
    Time now = RealClock::now();
    time_t secsSinceEpoch = secondsOf(now);

    struct tm wallclock;
    ::localtime_r(&secsSinceEpoch, &wallclock);
    Milliseconds res(millisecondsOf(now));
    res += Seconds(wallclock.tm_sec) + Minutes(wallclock.tm_min) + Hours(wallclock.tm_hour);

    return res;
}


}

#endif

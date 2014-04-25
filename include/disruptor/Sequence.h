#ifndef DISRUPTOR_SEQUENCE_H
#define DISRUPTOR_SEQUENCE_H

#include <boost/atomic.hpp>
#include <boost/utility.hpp>

#ifndef CACHE_LINE_SIZE_IN_BYTES
#define CACHE_LINE_SIZE_IN_BYTES 64
#endif
#define ATOMIC_SEQUENCE_PADDING_LENGTH \
    (CACHE_LINE_SIZE_IN_BYTES - sizeof(boost::atomic<int64_t>))/8
#define SEQUENCE_PADDING_LENGTH \
    (CACHE_LINE_SIZE_IN_BYTES - sizeof(int64_t))/8

namespace disruptor {

const int64_t INITIAL_CURSOR_VALUE = -1L;

class ISequence
{
    public:
        virtual ~ISequence() {}

        virtual int64_t get() const = 0;

        virtual void set(const int64_t& sequence) = 0;

        virtual int64_t incrementAndGet(const int64_t& delta) = 0;
};

// Cache line padded sequence counter.
//
// Can be used across threads without worrying about false sharing if a
// located adjacent to another counter in memory.
class Sequence : public ISequence
               , public boost::noncopyable
{
public:
    // Construct a sequence counter that can be tracked across threads.
    //
    // @param initial_value for the counter.
    explicit Sequence(int64_t initial_value = INITIAL_CURSOR_VALUE)
        : value_(initial_value)
    {
    }

    virtual ~Sequence()
    {
    }

    // Get the current value of the {@link Sequence}.
    //
    // @return the current value.
    int64_t get() const
    {
        return value_.load(boost::memory_order_acquire);
    }

    // Set the current value of the {@link Sequence}.
    //
    // @param the value to which the {@link Sequence} will be set.
    void set(const int64_t& value)
    {
        value_.store(value, boost::memory_order_release);
    }

    // Increment and return the value of the {@link Sequence}.
    //
    // @param increment the {@link Sequence}.
    // @return the new value incremented.
    int64_t incrementAndGet(const int64_t& increment)
    {
        return value_.fetch_add(increment, boost::memory_order_release) + increment;
    }

    bool compare_exchange(int64_t expected, int64_t desired)
    {
        return value_.compare_exchange_strong(expected, desired);
    }

private:
    // Not sure this can completely prevent false sharing:
    // Sequence is not CACHE_LINE_SIZE_IN_BYTES-aligned!
    boost::atomic<int64_t> value_;
    int64_t padding_[ATOMIC_SEQUENCE_PADDING_LENGTH];
};

// Non-atomic sequence counter.
//
// This counter is not thread safe.
class MutableLong : public ISequence
{
 public:
     explicit MutableLong(int64_t initial_value = INITIAL_CURSOR_VALUE) 
         : sequence_(initial_value) 
     {
     }

     int64_t get() const { return sequence_; }

     void set(const int64_t& sequence) { sequence_ = sequence; };

     int64_t incrementAndGet(const int64_t& delta)
     {
         sequence_ += delta;
         return sequence_;
     }

 private:
     volatile int64_t sequence_;
};

// Cache line padded non-atomic sequence counter.
//
// This counter is not thread safe.
class PaddedLong : public MutableLong
{
 public:
     explicit PaddedLong(int64_t initial_value = INITIAL_CURSOR_VALUE)
         : MutableLong(initial_value)
     {
     }

 private:
     int64_t padding_[SEQUENCE_PADDING_LENGTH];
};


typedef std::vector<Sequence*> DependentSequences;


inline int64_t getMinimumSequence(const DependentSequences& sequences)
{
    int64_t minimum = LONG_MAX;

    DependentSequences::const_iterator seq_itr = sequences.begin();
    DependentSequences::const_iterator seq_end = sequences.end();
    for ( ; seq_itr != seq_end; ++seq_itr ) {
        int64_t sequence = (*seq_itr)->get();
        minimum = minimum < sequence ? minimum : sequence;
    }

    return minimum;
};

}

#endif

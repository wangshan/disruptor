#ifndef DISRUPTOR_SEQUENCE_H_
#define DISRUPTOR_SEQUENCE_H_

#include <disruptor/utils.h>

#ifndef CACHE_LINE_SIZE_IN_BYTES
#define CACHE_LINE_SIZE_IN_BYTES 64
#endif
#define ATOMIC_SEQUENCE_PADDING_LENGTH \
    (CACHE_LINE_SIZE_IN_BYTES - sizeof(stdext::atomic<int64_t>))
#define SEQUENCE_PADDING_LENGTH \
    (CACHE_LINE_SIZE_IN_BYTES - sizeof(int64_t))

// only works for gcc
#define ALIGN(x) __attribute__((aligned(x)))

namespace disruptor {

const int64_t INITIAL_CURSOR_VALUE = -1L;

// Cache line padded sequence counter.
//
// Can be used across threads without worrying about false sharing if a
// located adjacent to another counter in memory.
class Sequence
{
public:
    // Construct a sequence counter that can be tracked across threads.
    //
    // @param initial_value for the counter.
    explicit Sequence(int64_t initial_value=INITIAL_CURSOR_VALUE)
        : value_(initial_value)
    {
    }

    ~Sequence()
    {
    }

    // Get the current value of the {@link Sequence}.
    //
    // @return the current value.
    int64_t get(
            stdext::memory_order order=stdext::memory_order_acquire) const
    {
        return value_.load(order);
    }

    // Set the current value of the {@link Sequence}.
    //
    // @param the value to which the {@link Sequence} will be set.
    void set(const int64_t& value,
            stdext::memory_order order=stdext::memory_order_release)
    {
        value_.store(value, order);
    }

    // Increment and return the value of the {@link Sequence}.
    //
    // @param increment the {@link Sequence}.
    // @return the new value incremented.
    int64_t incrementAndGet(
            const int64_t& increment,
            stdext::memory_order order=stdext::memory_order_release)
    {
        return value_.fetch_add(increment, order) + increment;
    }

    bool compareAndExchange(
            int64_t expected,
            int64_t desired,
            stdext::memory_order order=stdext::memory_order_release)
    {
        return value_.compare_exchange_strong(expected, desired, order);
    }

private:
    ALIGN(CACHE_LINE_SIZE_IN_BYTES);
    stdext::atomic<int64_t> value_;
    char padding_[ATOMIC_SEQUENCE_PADDING_LENGTH];
};

// Non-atomic sequence counter.
//
// This counter is not thread safe.
class MutableLong
{
public:
    explicit MutableLong(int64_t initial_value=INITIAL_CURSOR_VALUE) 
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
    ALIGN(CACHE_LINE_SIZE_IN_BYTES);
    volatile int64_t sequence_;
};

// Cache line padded non-atomic sequence counter.
//
// This counter is not thread safe.
class PaddedLong : public MutableLong
{
public:
    explicit PaddedLong(int64_t initial_value=INITIAL_CURSOR_VALUE)
        : MutableLong(initial_value)
    {
    }

private:
    char padding_[SEQUENCE_PADDING_LENGTH];
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
}

}

#endif

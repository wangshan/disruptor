#ifndef DISRUPTOR_INTERFACE_H
#define DISRUPTOR_INTERFACE_H

#include <climits>
#include <vector>
#include <map>

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/make_shared.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <disruptor/Sequence.h>
#include <disruptor/BatchDescriptor.h>

namespace disruptor {

enum TimeConfigKey {
    eSleep,
    eMaxIdle
};
typedef std::map<TimeConfigKey, boost::posix_time::time_duration> TimeConfig;

// Strategies employed for claiming the sequence of events in the
// {@link Seqencer} by publishers.
// TODO: add getSequence and getBufferSize
class IClaimStrategy : public boost::noncopyable
{
public:
    virtual ~IClaimStrategy() {};

    // Is there available capacity in the buffer for the requested sequence.
    //
    // @param dependent_sequences to be checked for range.
    // @return true if the buffer has capacity for the requested sequence.
    virtual bool hasAvailableCapacity(
        const DependentSequences& dependent_sequences) = 0;

    // Claim the next sequence in the {@link Sequencer}.
    //
    // @param dependent_sequences to be checked for range.
    // @return the index to be used for the publishing.
    virtual int64_t incrementAndGet(
            const DependentSequences& dependent_sequences) = 0;

    // Claim the next sequence in the {@link Sequencer}.
    //
    // @param delta to increment by.
    // @param dependent_sequences to be checked for range.
    // @return the index to be used for the publishing.
    virtual int64_t incrementAndGet(const int& delta,
            const DependentSequences& dependent_sequences) = 0;

    // Set the current sequence value for claiming an event in the
    // {@link Sequencer}.
    //
    // @param sequence to be set as the current value.
    // @param dependent_sequences to be checked for range.
    virtual void setSequence(const int64_t& sequence,
            const DependentSequences& dependent_sequences) = 0;

    // Serialise publishing in sequence.
    //
    // @param sequence to be applied.
    // @param cursor to be serialise against.
    // @param batch_size of the sequence.
    virtual void serialisePublishing(const int64_t& sequence,
                                     Sequence& cursor,
                                     const int64_t& batch_size) = 0;
};

typedef boost::shared_ptr<IClaimStrategy> ClaimStrategyPtr;

// Coordination barrier for tracking the cursor for publishers and sequence of
// dependent {@link EventProcessor}s for processing a data structure.
class ISequenceBarrier
{
public:
    virtual ~ISequenceBarrier() {};
    // Wait for the given sequence to be available for consumption.
    //
    // @param sequence to wait for.
    // @return the sequence up to which is available.
    //
    // @throws AlertException if a status change has occurred for the
    // Disruptor.
    virtual int64_t waitFor(const int64_t& sequence) = 0;

    // Wait for the given sequence to be available for consumption with a
    // time out.
    //
    // @param sequence to wait for.
    // @param timeout in microseconds.
    // @return the sequence up to which is available.
    //
    // @throws AlertException if a status change has occurred for the
    // Disruptor.
    virtual int64_t waitFor(const int64_t& sequence,
                            const boost::posix_time::time_duration& timeout) = 0;

    // Delegate a call to the {@link Sequencer#getCursor()}
    //
    //  @return value of the cursor for entries that have been published.
    virtual int64_t getCursor() const = 0;

    // The current alert status for the barrier.
    //
    // @return true if in alert otherwise false.
    virtual bool isAlerted() const = 0;

    // Alert the {@link EventProcessor}s of a status change and stay in this
    // status until cleared.
    virtual void alert() = 0;

    // Clear the current alert status.
    virtual void clearAlert() = 0;

    // Check if barrier is alerted, if so throws an AlertException
    //
    // @throws AlertException if barrier is alerted
    virtual void checkAlert() const = 0;
};

typedef boost::shared_ptr<ISequenceBarrier> SequenceBarrierPtr;

// Called by the {@link RingBuffer} to pre-populate all the events to fill the
// RingBuffer.
//
// @param <T> event implementation storing the data for sharing during exchange
// or parallel coordination of an event.
template<typename T>
class IEventFactory
{
public:
     virtual ~IEventFactory() {};
     virtual boost::shared_ptr<T> newInstance() const = 0;
};

// Callback interface to be implemented for processing events as they become
// available in the {@link RingBuffer}.
//
// @param <T> event implementation storing the data for sharing during exchange
// or parallel coordination of an event.
template<typename T>
class IEventHandler
{
public:
    virtual ~IEventHandler() {};

    // Called when a publisher has published an event to the {@link RingBuffer}
    //
    // @param event published to the {@link RingBuffer}
    // @param sequence of the event being processed
    // @param end_of_batch flag to indicate if this is the last event in a batch
    // from the {@link RingBuffer}
    //
    // @throws Exception if the EventHandler would like the exception handled
    // further up the chain.
    virtual void onEvent(const int64_t& sequence,
                         const bool& end_of_batch,
                         T* event) = 0;

    // Called once on thread start before processing the first event.
    virtual void onStart() = 0;

    // Called once on thread stop just before shutdown.
    virtual void onShutdown() = 0;
};

// Implementations translate another data representations into events claimed
// for the {@link RingBuffer}.
//
// @param <T> event implementation storing the data for sharing during exchange
// or parallel coordination of an event.
template<typename T>
class IEventTranslator
{
public:
     virtual ~IEventTranslator() {};

     // Translate a data representation into fields set in given event
     //
     // @param event into which the data should be translated.
     // @param sequence that is assigned to events.
     // @return the resulting event after it has been translated.
     virtual T* translateTo(const int64_t& sequence, T* event) = 0;
};

// EventProcessors wait for events to become available for consumption from
// the {@link RingBuffer}. An event processor should be associated with a
// thread.
//
// @param <T> event implementation storing the data for sharing during exchange
// or parallel coordination of an event.
template<typename T>
class IEventProcessor
{
public:
     virtual ~IEventProcessor() {};

     // Get a pointer to the {@link Sequence} being used by this
     // {@link EventProcessor}.
     //
     // @return pointer to the {@link Sequence} for this
     // {@link EventProcessor}
    virtual Sequence* getSequence() = 0;

    // Signal that this EventProcessor should stop when it has finished
    // consuming at the next clean break.
    // It will call {@link DependencyBarrier#alert()} to notify the thread to
    // check status.
    virtual void halt() = 0;
};

// Callback handler for uncaught exception in the event processing cycle
// of the {@link BatchEventProcessor}.
//
// @param <T> event type stored in the {@link RingBuffer}.
template<typename T>
class IExceptionHandler
{
public:
    virtual ~IExceptionHandler() {};

    // Strategy for handling uncaught exceptions when processing an event.
    // If the strategy wishes to suspend further processing by the
    // {@link BatchEventProcessor} then it should throw a std::runtime_error.
    //
    // @param exception that propagated from the {@link EventHandler}.
    // @param sequence of the event which caused the exception.
    // @param event being processed when the exception occured.
    virtual void handle(const std::exception& exception,
                        const int64_t& sequence,
                        T* event) = 0;
};

// Strategy employed for making {@link EventProcessor}s wait on a cursor
// {@link Sequence}.
class IWaitStrategy : public boost::noncopyable
{
public:
    virtual ~IWaitStrategy() {};

    //  Wait for the given sequence to be available for consumption.
    //
    //  @param dependents further back the chain that must advance first.
    //  @param cursor on which to wait.
    //  @param barrier the consumer is waiting on.
    //  @param sequence to be waited on.
    //  @return the sequence that is available which may be greater than the
    //  requested sequence.
    //
    //  @throws AlertException if the status of the Disruptor has changed.
    virtual int64_t waitFor(const int64_t& sequence,
                            const Sequence& cursor,
                            const DependentSequences& dependents,
                            const ISequenceBarrier& barrier
                            ) = 0;

    //  Wait for the given sequence to be available for consumption in a
    //  {@link RingBuffer} with a timeout specified.
    //
    //  @param dependents further back the chain that must advance first
    //  @param cursor on which to wait.
    //  @param barrier the consumer is waiting on.
    //  @param sequence to be waited on.
    //  @param timeout value in micro seconds to abort after.
    //  @return the sequence that is available which may be greater than the
    //  requested sequence.
    //
    //  @throws AlertException if the status of the Disruptor has changed.
    //  @throws InterruptedException if the thread is interrupted.
    virtual int64_t waitFor(const int64_t& sequence,
                            const Sequence& cursor,
                            const DependentSequences& dependents,
                            const ISequenceBarrier& barrier,
                            const boost::posix_time::time_duration& timeout) = 0;

    // Signal those waiting that the cursor has advanced.
    virtual void signalAllWhenBlocking() = 0;
};

typedef boost::shared_ptr<IWaitStrategy> WaitStrategyPtr;


inline boost::posix_time::time_duration getTimeConfig(const TimeConfig& timeConfig,
                                                      TimeConfigKey key,
                                                      const boost::posix_time::time_duration& defVal)
{
    TimeConfig::const_iterator it = timeConfig.find(key);
    if (it != timeConfig.end()) {
        return it->second;
    }
    else {
        return defVal;
    }
}


};  // namespace disruptor

#endif

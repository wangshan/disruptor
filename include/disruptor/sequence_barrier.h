#ifndef DISRUPTOR2_SEQUENCE_BARRIER_H_
#define DISRUPTOR2_SEQUENCE_BARRIER_H_

#include <vector>
#include <boost/atomic.hpp>

#include <disruptor/exceptions.h>
#include <disruptor/interface.h>

namespace disruptor {

class ProcessingSequenceBarrier : public ISequenceBarrier
{
    public:
        ProcessingSequenceBarrier(IWaitStrategy* wait_strategy,
                Sequence* sequence,
                const DependentSequences& dependent_sequences)
            : wait_strategy_(wait_strategy)
            , cursor_sequence_(sequence)
            , dependent_sequences_(dependent_sequences)
            , alerted_(false)
        {
        }

        ProcessingSequenceBarrier(IWaitStrategy* wait_strategy,
                Sequence* sequence)
            : wait_strategy_(wait_strategy)
            , cursor_sequence_(sequence)
            , alerted_(false)
        {
        }

        virtual int64_t waitFor(const int64_t& sequence)
        {
            return wait_strategy_->waitFor(sequence,
                    *cursor_sequence_, dependent_sequences_, *this);
        }

        virtual int64_t waitFor(const int64_t& sequence,
                                const boost::posix_time::time_duration& timeout)
        {
            return wait_strategy_->waitFor(sequence,
                    *cursor_sequence_, dependent_sequences_, *this, timeout);
        }

        virtual int64_t getCursor() const
        {
            return cursor_sequence_->get();
        }

        virtual bool isAlerted() const
        {
            return alerted_.load(boost::memory_order_acquire);
        }

        virtual void alert()
        {
            alerted_.store(true, boost::memory_order_release);
        }

        virtual void clearAlert()
        {
            alerted_.store(false, boost::memory_order_release);
        }

        virtual void checkAlert() const
        {
            if (isAlerted()) {
                throw AlertException();
            }
        }

    private:
        IWaitStrategy* wait_strategy_;
        Sequence* cursor_sequence_;
        DependentSequences dependent_sequences_;
        boost::atomic<bool> alerted_;
};

};  // namespace disruptor

#endif

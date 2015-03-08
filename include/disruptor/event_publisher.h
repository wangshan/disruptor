#ifndef DISRUPTOR2_EVENT_PUBLISHER_H_
#define DISRUPTOR2_EVENT_PUBLISHER_H_

#include <disruptor/ring_buffer.h>

namespace disruptor {

template<typename T>
class EventPublisher
{
public:
    EventPublisher(RingBuffer<T>* ring_buffer)
        : ring_buffer_(ring_buffer)
    {
    }

    void publishEvent(IEventTranslator<T>* translator)
    {
        int64_t sequence = ring_buffer_->next();
        // this is where time stamp generated
        translator->translateTo(sequence, ring_buffer_->get(sequence));
        ring_buffer_->publish(sequence);
    }


    bool tryPublishEvent(IEventTranslator<T>* translator)
    {
        if (ring_buffer_->hasAvailableCapacity()) {
            int64_t sequence = ring_buffer_->next();
            translator->translateTo(sequence, ring_buffer_->get(sequence));
            ring_buffer_->publish(sequence);
            return true;
        }

        return false;
    }

    bool hasAvailableCapacity() const 
    {
        return ring_buffer_->hasAvailableCapacity();
    }

private:
    RingBuffer<T>* ring_buffer_;
};

};  // namespace disruptor

#endif

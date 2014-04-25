#ifndef DISRUPTOR_EVENT_PUBLISHER_H
#define DISRUPTOR_EVENT_PUBLISHER_H

#include <disruptor/RingBuffer.h>

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
        translator->translateTo(sequence, ring_buffer_->get(sequence));
        ring_buffer_->publish(sequence);
    }

private:
    RingBuffer<T>* ring_buffer_;
};

}

#endif

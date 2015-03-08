#ifndef DISRUPTOR2_EXCEPTIONS_H_
#define DISRUPTOR2_EXCEPTIONS_H_

#include <exception>

namespace disruptor {

class AlertException : public std::exception
{
};

};  // namespace disruptor

#endif

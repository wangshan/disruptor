#include "time.h"

namespace disruptor {

const time_t NANOSECONDS_IN_SECOND       = 1000000000;
const time_t MICROSECONDS_IN_SECOND      = 1000000;
const time_t NANOSECONDS_IN_MILLISECOND  = 1000000;
const time_t MILLISECONDS_IN_SECOND      = 1000;
const time_t MICROSECONDS_IN_MILLISECOND = 1000;
const time_t NANOSECONDS_IN_MICROSECOND  = 1000;
const time_t SECONDS_IN_DAY              = 86400;
const time_t SECONDS_IN_HOUR             = 3600;
const time_t SECONDS_IN_MINUTE           = 60;
const time_t EPOCH_OF_STARTUP            = ::time(NULL);

}

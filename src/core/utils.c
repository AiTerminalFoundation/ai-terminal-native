#include <time.h>
#include <stdlib.h>
#include <stdint.h>

char * generate_uuid_v7(void);

struct uuid_v7 {
    uint64_t unix_timestamp_ms : 48;
    uint64_t microseconds_fraction: 12;
    uint64_t version: 4;
    uint64_t variant: 2;
    uint64_t random_bits: 62;
};


char * generate_uuid_v7(void) {

    struct timespec time_specification;
    // getting the time at microsecond precision
    clock_gettime(CLOCK_REALTIME, &time_specification);

    // generating a random number
}

#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "utils.h"


uuid_v7 generate_uuid_v7(void);
uint64_t generate_random_value_64_bits(void);
char * generates_string_uuid_v7(void);

/* unsafe and unpredictable as some things are left to the compiler (padding, low-high disposition of the bits)
struct {
    uint64_t unix_timestamp_ms : 48;
    uint64_t version: 4; 
    uint64_t microseconds_fraction: 12;
    uint64_t variant: 2;
    uint64_t random_bits: 62; 
} uuid_v7;
*/

uuid_v7 generate_uuid_v7(void) {

    struct timespec time_specification;
    uuid_v7 uuid_v7;

    // getting the time at microsecond precision
    clock_gettime(CLOCK_REALTIME, &time_specification);

    uint64_t milliseconds = (uint64_t)((time_specification.tv_sec * 1000ULL) + (time_specification.tv_nsec / 1000000ULL));

    // milliseconds since epoch uses 41 bits (now) so it fits well into 48 bits
    uint64_t milliseconds_masked_48_bits = milliseconds & 0xFFFFFFFFFFFF; // 12 Fs in exadecimal means 48 bits of 1s, as exa uses 4 bits

    // microseconds are from 0-999, so they use 10 bit (2^10 = 1024), so it fits in 12 bits dedicated to the microseconds fraction
    // we get rid of the milliseconds by using modulo, so what remains is the sub-millisecond part (defined in nanosecons), and we divide by 1000
    // to get the microseconds from milliseconds
    uint64_t microseconds = (uint64_t)((time_specification.tv_nsec % 1000000ULL) / 1000ULL);
    uint64_t microseconds_masked_12_bits = microseconds & 0xFFF;

    uint64_t version_masked_4_bits = 7 & 0xF; // the uuid version
    uint64_t variant_masked_2_bits = 2 & 0b11; // variant is 2 bits and in our case is 2, that corresponds to RFC 4122. It's a standard for all uuids
    
    // 62 random bits
    uint64_t random_value_masked_62_bits = generate_random_value_64_bits() & 0x3FFFFFFFFFFFFFFF;

    uuid_v7.upper = (milliseconds_masked_48_bits << 16) | (version_masked_4_bits << 12) | microseconds_masked_12_bits;
    uuid_v7.lower = (variant_masked_2_bits << 62) | random_value_masked_62_bits;

    return uuid_v7;
}


char * generate_string_uuid_v7(void) {
    char *uuid_str = malloc(37); // 32 bytes for the values, 4 dashes, 1 null terminator
                            
    uuid_v7 uuid_v7 = generate_uuid_v7();

    uint32_t ts_first_part = (uint32_t)(uuid_v7.upper >> 32);
    uint16_t ts_second_part = (uint16_t)((uuid_v7.upper >> 16) & 0xFFFF);
    uint16_t ver_and_micros = (uint16_t)(uuid_v7.upper & 0xFFFF);
    uint16_t variant_and_rand = (uint16_t)(uuid_v7.lower >> 48);
    uint64_t rand = uuid_v7.lower & 0xFFFFFFFFFFFF;

    // writing formatted string
    snprintf(uuid_str, 37, "%08x-%04x-%04x-%04x-%012llx", ts_first_part, ts_second_part, ver_and_micros, variant_and_rand, rand);
    return uuid_str;
}


uint64_t generate_random_value_64_bits(void) {
    uint64_t value;

    int urandom_file_descriptor = open("/dev/urandom", O_RDONLY);
    read(urandom_file_descriptor, &value, sizeof(value));
    close(urandom_file_descriptor);

    return value;
}

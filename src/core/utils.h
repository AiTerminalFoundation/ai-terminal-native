#ifndef utils_h
#define utils_h

#include <stdint.h>

typedef struct {
    uint64_t upper; // timestamp and version
    uint64_t lower; // variant and random
} uuid_v7;


uuid_v7 generate_uuid_v7(void);
uint64_t generate_random_value_64_bits(void);
char * generate_string_uuid_v7(void);

#endif

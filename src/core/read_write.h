#ifndef read_write_h
#define read_write_h

#include <unistd.h>

ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes);
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context);

#endif

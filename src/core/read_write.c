#include "read_write.h"
#include "logger.h"
#include <stdlib.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>


#define BUFFER_SIZE 4096


ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes);
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context);

/*
 * Send input to the master_fd that sends it to the slave, and the slave shell elaborates
 * and sends to the STDOUT of the slave, that will be read by the master, and then by the user app
 */
ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes) {
    terminal_logger *logger = terminal_logger_find(master_file_descriptor);
    if (logger != NULL) {
        terminal_logger_log(logger, "INFO", "INPUT", bytes, n_bytes);
    }
    return write(master_file_descriptor, bytes, n_bytes);
}

/*
 * Reading the STDOUT connected to the slave connected to the given master
 */
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context) {
    char buffer[BUFFER_SIZE];
    terminal_logger *logger = terminal_logger_find(master_file_descriptor);

    struct pollfd poll_file_descriptor = { .fd = master_file_descriptor, .events = POLLIN };

    // we pass to poll() the fds to check and the event,
    // the number of the fds to check (in our case 1)
    // and the timeout, that we don't want, so -1 for us
    // if poll returns -1 there is an error, if 0 it means timeout
    // so we check for values > 0, that is the count of the fds that got some operation
    // in our case we just have 1, so we could simplify to == 1, but i don't know if in the future i want
    // to put more fds in this function
    while(poll(&poll_file_descriptor, 1, -1) > 0) {
        // we need to use bitwise AND with POLLIN, becasue there might be also the POLLHUP events (that means connection close) with some output
        // if we use == we will lose this edge case
        if((poll_file_descriptor.revents & POLLIN) > 0) {
            ssize_t n_bytes_read = read(master_file_descriptor, buffer, BUFFER_SIZE);
            
            // some error, as we expect output here given that the bitwise operation is true
            // TODO: add a counter for the error to have some sort of reliability before exiting the loop
            if(n_bytes_read <= 0) break;

            if (logger != NULL) {
                terminal_logger_log(logger, "INFO", "OUTPUT", buffer, (size_t)n_bytes_read);
            }
            on_output(buffer, n_bytes_read, context);
        }
    }

    terminal_logger_close(master_file_descriptor, "SESSION_CLOSED");
}


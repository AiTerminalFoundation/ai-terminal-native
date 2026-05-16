#ifndef pty_session_h
#define pty_session_h

#include <unistd.h>

int create_pseudoterminal(int *master_fd, int *slave_fd, char **session_id);
int fork_and_exec_shell(int master_fd, int slave_fd);
ssize_t send_input(char *command, int master_file_descriptor, size_t command_n_bytes);
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context);
void close_terminal_session(int master_file_descriptor);

#endif

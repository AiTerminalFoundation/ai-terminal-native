#ifndef terminal_h
#define terminal_h

int create_pseudoterminal(int *master_fd, int *slave_fd);
int fork_and_exec_shell(int master_fd, int slave_fd);
ssize_t send_input(char *command, int master_file_descriptor, size_t command_n_bytes);
char * read_loop(int master_file_descriptor, void (*on_output(const char *buffer, ssize_t n_bytes_read);

#endif

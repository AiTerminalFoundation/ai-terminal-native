#ifndef terminal_h
#define terminal_h

int create_pseudoterminal(int *master_fd, int *slave_fd);
int fork_and_exec_shell(int master_fd, int slave_fd);

#endif

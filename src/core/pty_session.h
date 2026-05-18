#ifndef pty_session_h
#define pty_session_h

int create_pseudoterminal(int *master_fd, int *slave_fd, char **session_id);
int fork_and_exec_shell(int master_fd, int slave_fd);
void close_terminal_session(int master_file_descriptor);

#endif

#ifndef signals_h
#define signals_h

void handle_terminal_window_size_change_signal(int signal);
int get_terminal_window_size(struct winsize *window_size);
int set_terminal_window_size(struct winsize *window_size);

#endif

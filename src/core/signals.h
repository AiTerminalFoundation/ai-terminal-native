#ifndef signals_h
#define signals_h

#include <sys/ioctl.h>
#include <termios.h>


void handle_terminal_window_size_change_signal(int signal);
int get_terminal_window_size(struct winsize *window_size);
int set_terminal_window_size(unsigned int horizontal_pixels, unsigned int vertical_pixels);

#endif

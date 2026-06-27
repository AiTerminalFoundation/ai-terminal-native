#ifndef signals_h
#define signals_h

#include <termios.h>

int get_terminal_window_size(int master_file_descriptor, struct winsize *window_size);
int set_terminal_window_size(int master_file_descriptor, struct winsize window_size);
int resize_terminal_window(int master_file_descriptor, int rows, int columns, int width_pixels, int height_pixels);

#endif

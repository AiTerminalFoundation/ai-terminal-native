#include "signals.h"
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <stdlib.h>

int get_terminal_window_size(int master_file_descriptor, struct winsize *window_size);
int set_terminal_window_size(int master_file_descriptor, struct winsize window_size);
int resize_terminal_window(int master_file_descriptor, int rows, int columns, int width_pixels, int height_pixels);


int get_terminal_window_size(int master_file_descriptor, struct winsize *window_size) {
    return ioctl(master_file_descriptor, TIOCGWINSZ, window_size);
}

int set_terminal_window_size(int master_file_descriptor, struct winsize window_size) {
    return ioctl(master_file_descriptor, TIOCSWINSZ, &window_size);
}

/*
 * This function just get the windowsize attributes from the client app directly and
 * applies the changes, the ws_xpixel and ws_ypixel are usally not used in terminals
 * but some cli or tui app might use them
 * rows = y pixels / character height
 * cols = x pixels / character width
 * the ws struct is sent by the app client
 */
int resize_terminal_window(int master_file_descriptor, int rows, int columns, int width_pixels, int height_pixels) {

    if (rows <= 0 || columns <= 0 || width_pixels < 0 || height_pixels < 0 ||
        rows > USHRT_MAX || columns > USHRT_MAX || width_pixels > USHRT_MAX || height_pixels > USHRT_MAX) {
        return -1;
    }

    struct winsize window_size = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)columns,
        .ws_xpixel = (unsigned short)width_pixels,
        .ws_ypixel = (unsigned short)height_pixels
    };

    return set_terminal_window_size(master_file_descriptor, window_size);
}

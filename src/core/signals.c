#include "signals.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

void handle_terminal_window_size_change_signal(int signal);
int get_terminal_window_size(struct winsize *window_size);
int set_terminal_window_size(unsigned int horizontal_pixels, unsigned int vertical_pixels);

/* winsize struct for refernce
 *
 * unsigned short 	ws_row
 * unsigned short 	ws_col
 * unsigned short 	ws_xpixel
 * unsigned short 	ws_ypixel
 */

int get_terminal_window_size(struct winsize *window_size) {
    return ioctl(STDIN_FILENO, TIOCGWINSZ, window_size);
}

// for now we don't care about rows and cols and we send pixels, or we might just calculate the rows by ws_xpixel/font size, but the cols?
/* 
 * This function is directly called by SwiftUI when it detects a change in the window size, 
 * so it can tell the kernel that the new terminal size is changed, and update the shell output by consequence
 * that's very useful mostly for interactive apps like vim
 */
int set_terminal_window_size(unsigned int horizontal_pixels, unsigned int vertical_pixels) {
    struct winsize *window_size = NULL;
    get_terminal_window_size(window_size);
    
    window_size->ws_xpixel = horizontal_pixels;
    window_size->ws_ypixel = vertical_pixels;
    return ioctl(STDIN_FILENO, TIOCSWINSZ, window_size);
}

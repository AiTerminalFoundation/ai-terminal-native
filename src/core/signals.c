#include "signals.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>

int get_terminal_window_size(int master_file_descriptor, struct winsize *window_size);
int set_terminal_window_size(int master_file_descriptor, struct winsize window_size);

/* winsize struct for refernce
 *
 * unsigned short 	ws_row
 * unsigned short 	ws_col
 * unsigned short 	ws_xpixel
 * unsigned short 	ws_ypixel
 */

int get_terminal_window_size(int master_file_descriptor, struct winsize *window_size) {
    return ioctl(STDIN_FILENO, TIOCGWINSZ, window_size);
}

/* 
 * This function just get the windowsize attributes from the client app directly and
 * applies the changes, the ws_xpixel and ws_ pixel are usally not used in terminals so not really required
 * but some cli or tui app might use them
 * rows = y pixels / character height
 * cols = x pixels / character width
 */
int set_terminal_window_size(int master_file_descriptor, struct winsize window_size) {
    return ioctl(master_file_descriptor, TIOCSWINSZ, window_size);
}

#include "terminal.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

static struct winsize window_size;

/* terminal window resizing operations */
void handle_terminal_window_size_change_signal(int signal);
int get_terminal_window_size(struct winsize *window_size);
int set_terminal_window_size(struct winsize *window_size);


int main(void) {
    //register various signals to listen
    signal(SIGWINCH, handle_terminal_window_size_change_signal);

    //getting initial window size
    if (get_terminal_window_size(&window_size) == -1) return -1;
    printf("Initial size: %d x %d px\n", window_size.ws_xpixel, window_size.ws_ypixel);

    while (1) {
        pause();
    }


    puts("Terminal resizing detected");
    return 0;
}

void handle_terminal_window_size_change_signal(int signal) {
    if (get_terminal_window_size(&window_size) == -1) {
        puts("Error getting window size");
    } else {
        printf("size: %d x %d px\n", window_size.ws_xpixel, window_size.ws_ypixel);
    }
}

int get_terminal_window_size(struct winsize *window_size) {
    return ioctl(STDIN_FILENO, TIOCGWINSZ, window_size);
}

int set_terminal_window_size(struct winsize *window_size) {
    return ioctl(STDIN_FILENO, TIOCSWINSZ, window_size);
}

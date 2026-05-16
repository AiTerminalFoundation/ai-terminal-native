#include "signals.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void handle_terminal_window_size_change_signal(int signal);
int get_terminal_window_size(struct winsize *window_size);
int set_terminal_window_size(struct winsize *window_size);


void handle_terminal_window_size_change_signal(int signal) {
    (void)signal;
    //TODO: to implement, the window size changes will be sent by the client app directly
}

int get_terminal_window_size(struct winsize *window_size) {
    return ioctl(STDIN_FILENO, TIOCGWINSZ, window_size);
}

int set_terminal_window_size(struct winsize *window_size) {
    return ioctl(STDIN_FILENO, TIOCSWINSZ, window_size);
}

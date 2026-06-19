#include "signals.h"
#include "logger.h"
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <stdlib.h>

int get_terminal_window_size(int master_file_descriptor, struct winsize *window_size);
int set_terminal_window_size(int master_file_descriptor, struct winsize window_size);
int resize_terminal_window(int master_file_descriptor, int rows, int columns, int width_pixels, int height_pixels);

/* winsize struct for refernce
 *
 * unsigned short 	ws_row
 * unsigned short 	ws_col
 * unsigned short 	ws_xpixel
 * unsigned short 	ws_ypixel
 */

int get_terminal_window_size(int master_file_descriptor, struct winsize *window_size) {
    return ioctl(master_file_descriptor, TIOCGWINSZ, window_size);
}

/*
 * This function just get the windowsize attributes from the client app directly and
 * applies the changes, the ws_xpixel and ws_ pixel are usally not used in terminals so not really required
 * but some cli or tui app might use them
 * rows = y pixels / character height
 * cols = x pixels / character width
 */
int set_terminal_window_size(int master_file_descriptor, struct winsize window_size) {
    return ioctl(master_file_descriptor, TIOCSWINSZ, &window_size);
}

int resize_terminal_window(int master_file_descriptor, int rows, int columns, int width_pixels, int height_pixels) {
    terminal_logger *logger = terminal_logger_find(master_file_descriptor);

    int len_set = snprintf(NULL, 0, "Resize terminal window to rows=%d cols=%d width=%d height=%d", rows, columns, width_pixels, height_pixels);
    if (len_set > 0) {
        char *payload = malloc((size_t)len_set + 1);

        snprintf(
            payload,
            (size_t)len_set + 1,
            "Resize terminal window to rows=%d cols=%d width=%d height=%d",
            rows,
            columns,
            width_pixels,
            height_pixels
        );

        terminal_logger_log(logger, "INFO", "RESIZE_SET", payload, len_set);

        free(payload);
    }

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

    int result = set_terminal_window_size(master_file_descriptor, window_size);

    get_terminal_window_size(master_file_descriptor, &window_size);

    int len_get = snprintf(
        NULL,
        0,
        "Terminal window size from shell after resize: rows=%d cols=%d width=%d height=%d",
        window_size.ws_row, window_size.ws_col, window_size.ws_xpixel, window_size.ws_ypixel
    );
    if (len_get > 0) {
        char *payload = malloc((size_t)len_get + 1);

        snprintf(
            payload,
            (size_t)len_get + 1,
            "Terminal window size from shell after resize: rows=%d cols=%d width=%d height=%d",
            window_size.ws_row, window_size.ws_col, window_size.ws_xpixel, window_size.ws_ypixel
        );

        terminal_logger_log(logger, "INFO", "RESIZE_GET", payload, len_get);

        free(payload);
    }

    return result;
}

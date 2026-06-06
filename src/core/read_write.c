#include "read_write.h"
#include "logger.h"
#include <stdlib.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096
#define CSI_BUFFER_SIZE 128
#define OSC_BUFFER_SIZE 1024


typedef enum {
    ACTION_PRINT,
    ACTION_CARRIAGE_RETURN,
    ACTION_LINE_FEED,
    ACTION_BACKSPACE,
    ACTION_SET_GRAPHICS,
    ACTION_CURSOR_UP,
    ACTION_CLEAR_LINE,
    ACTION_SET_MODE,
    ACTION_OSC_TITLE,
    ACTION_IGNORE
} TerminalActionType;

typedef struct {
    int row;
    int column;
} grid_position;

typedef struct {
    bool is_bold;
} cell_properties;

typedef struct {
    bool is_empty_cell;
    char character;
    grid_position position;
    cell_properties properties;
} screen_cell;

typedef enum {
    GROUND_STATE,                       /* normal printing output state */
    ESCAPE_STATE,                       /* an escape sequence is started */
    CONTROL_SEQUENCE_INTRODUCER_STATE,  /* the start of a an escape sequence = [ */
    OPERATING_SYSTEM_COMMAND_STATE      /* useful for tab renaming, hyperlink = ] */
} TerminalState;                  /* Actually there are other states to handle but for a minimal working version is enough */

typedef enum {
    ESC_ASCII = 0x1b,
    CONTROL_SEQUENCE_INTRODUCER_ASCII = 0x5b,  /* [ = 5b exa, 93 dec on ascii */
    OPERATING_SYSTEM_COMMAND_ASCII = 0x5d,      /* ] = 5d exa, 91 dec on ascii */
    SEMICOLON_ASCII = 0x3b
} ASCII;


struct csi_sequence {
    uint8_t params[CSI_BUFFER_SIZE];
    int params_len;
    uint8_t intermediate_bytes[8]; // intermediate bytes are very rare usually 1 or 2, we will use 8 for safety, later we will remove all caps and use malloc
    int intermediate_bytes_len;
    uint8_t final_byte;
};


static TerminalState terminal_state = GROUND_STATE;
static cell_properties current_properties;
static struct csi_sequence *csi_sequence_ptr = NULL;


ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes);
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context);
void clear_csi_sequence(void);
int parse_csi(uint8_t c);
void parse(int master_fd, uint8_t c);
void clear_buffer(uint8_t *buffer, int *length);
void parse_osc(uint8_t c);
char to_utf8(uint8_t bytes[]);
void to_screen_cell(TerminalActionType *terminalActionType);
TerminalActionType * evaluate_csi_sequence(struct csi_sequence *csi_sequence_ptr);


/*
 * Send input to the master_fd that sends it to the slave, and the slave shell elaborates
 * and sends to the STDOUT of the slave, that will be read by the master, and then by the user app
 */
ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes) {
    terminal_logger *logger = terminal_logger_find(master_file_descriptor);
    if (logger != NULL) {
        terminal_logger_log(logger, "INFO", "INPUT", bytes, n_bytes);
    }
    return write(master_file_descriptor, bytes, n_bytes);
}


/*
 * Reading the STDOUT connected to the slave connected to the given master
 */
// the on_output is going to change, it will basically send a grid, and each entry a[m][n] of the matrix will have what is going to change, so for instance if we have a matrix of 
// mxn (let's say 1092x728) and we reduce the window size we will send a new matrix of new_m x new_n with in each of the matrix[i][j] the new chars. so then swiftui should just render a matrix.
// to get the window_size we get it from termios (already implementd in the main one)
// we will need something to parse escape sequences in real characters (it's going to be a big switch?)
// how do we send the cursor position? we are going to send an entire struct to the screen that will be build with (int cursor_i = x, int cursor_j, int cursor_type(i want to be able to render a normal text editor like cursor, char[][] screen) we will send already parsed sequences
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context) {
    char buffer[BUFFER_SIZE];

    terminal_logger *logger = terminal_logger_find(master_file_descriptor);
    csi_sequence_ptr = malloc(sizeof(struct csi_sequence));

    struct pollfd poll_file_descriptor = {
        .fd = master_file_descriptor,
        .events = POLLIN
    };

    // we pass to poll() the fds to check and the event,
    // the number of the fds to check (in our case 1)
    // and the timeout, that we don't want, so -1 for us
    // if poll returns -1 there is an error, if 0 it means timeout
    // so we check for values > 0, that is the count of the fds that got some operation
    // in our caTse we just have 1, so we could simplify to == 1, but i don't know if in the future i want
    // to put more fds in this function
    while(poll(&poll_file_descriptor, 1, -1) > 0) {
        // we need to use bitwise AND with POLLIN, becasue there might be also the POLLHUP events (that means connection close) with some output
        // if we use == we will lose this edge case
        if((poll_file_descriptor.revents & POLLIN) > 0) {
            ssize_t n_bytes_read = read(master_file_descriptor, buffer, BUFFER_SIZE);
            
            // some error, as we expect output here given that the bitwise operation is true
            // TODO: add a counter for the error to have some sort of reliability before exiting the loop
            if(n_bytes_read <= 0) break;

            if(logger != NULL) {
                terminal_logger_log(logger, "INFO", "OUTPUT", buffer, (size_t)n_bytes_read);
            }

            //parse the data we get inside a state machine
            for(int i = 0; i < n_bytes_read; i++) {
                parse(master_file_descriptor, buffer[i]);
            }
        }
    }

    terminal_logger_close(master_file_descriptor, "SESSION_CLOSED");
}

/* Examples */
/* \x1b[31mRed\x1b[0m World */
/* \r\x1B[0m\x1B[27m\x1B[24m\x1B[Jmicheleverriello@Micheles-MacBook-Pro / % \x1B[K\x1B[?2004h */
void parse(int master_fd, uint8_t c) {
    switch(terminal_state) {
        case GROUND_STATE:
            if(c == ESC_ASCII) {
                terminal_state = ESCAPE_STATE;
            } else {
                TerminalActionType *terminal_action_type = NULL;
                *terminal_action_type = ACTION_PRINT;
                // what else here?
            }
            break;
        case ESCAPE_STATE:
            switch (c) {
                case CONTROL_SEQUENCE_INTRODUCER_ASCII: // csi escape sequence starting, clearing the csi_sequence pointer
                    terminal_state = CONTROL_SEQUENCE_INTRODUCER_STATE;
                    break;
                case OPERATING_SYSTEM_COMMAND_ASCII:
                    terminal_state = OPERATING_SYSTEM_COMMAND_STATE;
                    break;
                default:
                    terminal_state = GROUND_STATE;
                    break;
            }
            break;
        case CONTROL_SEQUENCE_INTRODUCER_STATE:
            parse_csi(c);
            break;
        case OPERATING_SYSTEM_COMMAND_STATE:
            parse_osc(c);
            break;
        default:
            // the idea is that in the future we manage other escape sequences other than CSI and OSC
            break;
    }
}

/* will return -1 if the buffer overflows */
/* CSI Sequences are ESC[ 1;2;3 A composed by params [0-9] divided by ; and a final byte in the range */
/*
 * params:        bytes 0x30..0x3F
 * intermediates: bytes 0x20..0x2F
 * final:         byte 0x40..0x7E
 */
int parse_csi(uint8_t c) {
    static int buffer_length = 0;
    static uint8_t buffer[CSI_BUFFER_SIZE];

    if (buffer_length == CSI_BUFFER_SIZE) {
        clear_buffer(buffer, &buffer_length);
        terminal_state = GROUND_STATE;
        return -1; // we are going to buffer overflow, it's too much as length for CSI
    }

    if (c >= 0x30 && c <= 0x3F) { // param byte
      // parameter byte
      csi_sequence_ptr->params[csi_sequence_ptr->params_len] = c;
      csi_sequence_ptr->params_len++;
    } else if (c >= 0x20 && c <= 0x2F) { // intermediate byte
      csi_sequence_ptr->intermediate_bytes[csi_sequence_ptr->intermediate_bytes_len] = c;
      csi_sequence_ptr->intermediate_bytes_len++;
    } else if (c >= 0x40 && c <= 0x7E) { // final byte
        csi_sequence_ptr->final_byte = c;
        TerminalActionType *terminal_action_type = evaluate_csi_sequence(csi_sequence_ptr);
        terminal_state = GROUND_STATE;
        clear_csi_sequence();
    } else {
      // invalid CSI byte, skipping
    }

    return 0;
}


//TODO
TerminalActionType * evaluate_csi_sequence(struct csi_sequence *csi_sequence_ptr) {
   
}


void clear_csi_sequence(void) {
    clear_buffer(csi_sequence_ptr->params, &csi_sequence_ptr->params_len);
    clear_buffer(csi_sequence_ptr->intermediate_bytes, &csi_sequence_ptr->intermediate_bytes_len);
}

/* 
 * Clearing buffer and it's length, once buffer is cleared len is zeroed
 */
void clear_buffer(uint8_t *buffer, int *length) {
    for (int i = 0; i < *length; i++) buffer[i] = 0;
    *length = 0;
}


/* will return -1 if the buffer overflows */
//TODO
void parse_osc(uint8_t c) {
}


//TODO
char to_utf8(uint8_t bytes[]) {
    return 0;
}


//TODO
void to_screen_cell(TerminalActionType *terminalActionType) {
}

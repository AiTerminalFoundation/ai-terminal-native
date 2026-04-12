#include "logger.h"
#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

struct terminal_logger {
    int master_file_descriptor;
    char *session_id;
    FILE *log_file;
    struct terminal_logger *next;
};

static struct terminal_logger *terminal_loggers = NULL;

static char *get_log_file_path_by_session_id(const char *session_id);
static int64_t get_current_timestamp_nanoseconds(void);
static int log_event(FILE *log_file, const char *severity, const char *event, const void *payload, size_t payload_size);

static char *get_log_file_path_by_session_id(const char *session_id) {
    const char *prefix_path = "/tmp/terminal_session_";
    const char *extension = ".log";

    char *file_path = malloc(strlen(prefix_path) + strlen(session_id) + strlen(extension) + 1);
    if (file_path == NULL) {
        return NULL;
    }

    sprintf(file_path, "%s%s%s", prefix_path, session_id, extension);
    return file_path;
}

static int64_t get_current_timestamp_nanoseconds(void) {
    struct timespec timestamp = {0};

    if (clock_gettime(CLOCK_REALTIME, &timestamp) != 0) {
        return -1;
    }

    return ((int64_t)timestamp.tv_sec * 1000000000LL) + timestamp.tv_nsec;
}

terminal_logger *terminal_logger_create(int master_file_descriptor, const char *session_id) {
    char *file_path = get_log_file_path_by_session_id(session_id);
    if (file_path == NULL) {
        return NULL;
    }

    FILE *log_file = fopen(file_path, "a");
    free(file_path);

    if (log_file == NULL) {
        return NULL;
    }

    char *owned_session_id = strdup(session_id);
    if (owned_session_id == NULL) {
        fclose(log_file);
        return NULL;
    }

    terminal_logger *logger = malloc(sizeof(*logger));
    if (logger == NULL) {
        free(owned_session_id);
        fclose(log_file);
        return NULL;
    }

    logger->master_file_descriptor = master_file_descriptor;
    logger->session_id = owned_session_id;
    logger->log_file = log_file;
    logger->next = terminal_loggers;
    terminal_loggers = logger;

    terminal_logger_log(logger, "INFO", "SESSION_CREATED", logger->session_id, strlen(logger->session_id));
    return logger;
}

terminal_logger *terminal_logger_find(int master_file_descriptor) {
    terminal_logger *current = terminal_loggers;

    while (current != NULL) {
        if (current->master_file_descriptor == master_file_descriptor) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

const char *terminal_logger_session_id(const terminal_logger *logger) {
    return logger != NULL ? logger->session_id : NULL;
}

int terminal_logger_log(terminal_logger *logger, const char *severity, const char *event, const void *payload, size_t payload_size) {
    if (logger == NULL) {
        return -1;
    }

    return log_event(logger->log_file, severity, event, payload, payload_size);
}

void terminal_logger_close(int master_file_descriptor, const char *event) {
    terminal_logger **current = &terminal_loggers;

    while (*current != NULL) {
        terminal_logger *logger = *current;

        if (logger->master_file_descriptor == master_file_descriptor) {
            *current = logger->next;

            if (logger->log_file != NULL) {
                terminal_logger_log(logger, "INFO", event, logger->session_id, strlen(logger->session_id));
                fclose(logger->log_file);
            }

            free(logger->session_id);
            free(logger);
            return;
        }

        current = &logger->next;
    }
}

static int log_event(FILE *log_file, const char *severity, const char *event, const void *payload, size_t payload_size) {
    if (log_file == NULL || severity == NULL || event == NULL) {
        return -1;
    }

    int64_t timestamp_nanoseconds = get_current_timestamp_nanoseconds();
    if (timestamp_nanoseconds < 0) {
        return -1;
    }

    if (fprintf(log_file, "[%lld] [%s] [%s]", (long long)timestamp_nanoseconds, severity, event) < 0) {
        return -1;
    }

    if (payload != NULL && payload_size > 0) {
        const unsigned char *bytes = payload;

        if (fputs(" ", log_file) == EOF) {
            return -1;
        }

        for (size_t index = 0; index < payload_size; index++) {
            unsigned char byte = bytes[index];

            if (byte == '\n') {
                if (fputs("\\n", log_file) == EOF) return -1;
            } else if (byte == '\r') {
                if (fputs("\\r", log_file) == EOF) return -1;
            } else if (byte == '\t') {
                if (fputs("\\t", log_file) == EOF) return -1;
            } else if (isprint(byte)) {
                if (fputc(byte, log_file) == EOF) return -1;
            } else {
                if (fprintf(log_file, "\\x%02X", byte) < 0) return -1;
            }
        }
    }

    if (fputc('\n', log_file) == EOF) {
        return -1;
    }

    return fflush(log_file);
}

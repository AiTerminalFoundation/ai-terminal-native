#ifndef logger_h
#define logger_h

#include <sys/types.h>

typedef struct terminal_logger terminal_logger;

terminal_logger *terminal_logger_create(int master_file_descriptor, const char *session_id);
terminal_logger *terminal_logger_find(int master_file_descriptor);
const char *terminal_logger_session_id(const terminal_logger *logger);
int terminal_logger_log(terminal_logger *logger, const char *severity, const char *event, const void *payload, size_t payload_size);
void terminal_logger_close(int master_file_descriptor, const char *event);

#endif

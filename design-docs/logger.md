# Logger Design Doc

The idea is to create a simple logger with basic functions and OTel compliant in order to understand what the shell is doing and be able to debug easily the various edge case scenarios, especially the cases with long output, canonical and non-canonical mode, how escape character are put by the shell.

### Implementation idea

For each i tab, a file is created called terminal\_tab\_{session\_id}.log, and the logger will write to that given file

### Fields
- `trace_id`: in this case i would use the tab terminal `session_id`.
- `span_id`: random 64 bits field generated a runtime with my custom `utils.h`.
- `severity`: `ERROR`, `INFO`, `DEBUG`, `WARN`, `FATAL`.
- `timestamp`: nanosec precision timestamp.
- `text`: the text of the log (it might be very long as it's the shell output.

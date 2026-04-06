# Logger Design Doc

The idea is to create a simple logger with basic functions and OTel compliant in order to understand what the shell is doing and be able to debug easily the various edge case scenarios, especially the cases with long output, canonical and non-canonical mode, how escape character are put by the shell.


### Fields
- `trace_id`: in this case i would use the tab terminal `session_id`
- `span_id`: UUIDv7 field generated a runtime with my custom `utils.h`
- `severity`: `ERROR`, `INFO`, `DEBUG`, `WARNING`
- `timestamp`: not sure i want to add it as i can reverse the uuidv7 that i build to have microsecond precision to get the timestamp
- `text`: the text of the log (it might be very long as it's the shell output

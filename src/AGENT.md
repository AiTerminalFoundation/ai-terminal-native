# Terminal design doc

Stack:
    Native UI: SwiftUI for macos, and GTK for linux
    Core engine: C

The app will be a native terminal emulator, his main purpose is to be smooth in the ui and super fast.

First MVP Plan:
1. C core engine:
    1.1 connection to pty
    1.2 command execution
    1.3 text renderin
2. Binding Level:
    2.1 expose api to send command?
3. MacOS UI:
    3.1 basic black window that allows to type the input and renders the text from the c core engine


# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Windows C++ application that creates a GUI for an OSC (Open Sound Control) trigger system. The application listens for OSC messages on a UDP socket and triggers keyboard events when specific OSC values are received.

## Build Commands

### Primary Build
Use the VS Code task to build the application:
```bash
# In VS Code: Ctrl+Shift+P > Tasks: Run Task > build-gui
# Or via command line (requires Visual Studio Build Tools):
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" && cl /O2 /MT /EHsc /DWIN32 /D_WINDOWS osc_trigger_gui.cpp /link ws2_32.lib user32.lib comctl32.lib /SUBSYSTEM:WINDOWS
```

The build process uses MSVC compiler with:
- `/O2` - Optimization for speed
- `/MT` - Static runtime linking
- `/EHsc` - C++ exception handling
- Required libraries: `ws2_32.lib` (Winsock), `user32.lib` (Windows API), `comctl32.lib` (Common Controls)

## Architecture

### Core Components

1. **OSCTrigger Class** (`osc_trigger_gui.cpp:33-198`)
   - Handles UDP socket creation and OSC message parsing
   - Processes OSC bundles and individual messages
   - Triggers keyboard events when target values are matched

2. **GUI System** (`osc_trigger_gui.cpp:265-379`)
   - Win32 API-based window with configuration controls
   - Real-time status logging via multiline edit control
   - Start/Stop buttons for listener management

3. **Configuration Management**
   - Window title targeting for keyboard event delivery
   - UDP port configuration (default: 55525)
   - OSC address pattern matching (default: "/flair/runstate")
   - Trigger key selection (SPACE, ENTER, F1-F5, or single characters)
   - Target value matching (integer or float)

### Key Features

- **OSC Message Processing**: Handles both individual OSC messages and bundled messages
- **Value Matching**: Supports integer and float value comparison with tolerance for floats
- **Window Targeting**: Uses `FindWindowA` to locate target application windows
- **Keyboard Simulation**: Uses `keybd_event` to send key presses to target applications
- **Threaded Architecture**: Separate thread for UDP listening to prevent GUI blocking

## Development Notes

- The application is Windows-specific and requires Winsock2 for networking
- Uses high priority class for responsive keyboard triggering
- GUI controls are created using raw Win32 API calls
- Status logging includes timestamps for debugging OSC message reception
- Socket operations include proper error handling and cleanup
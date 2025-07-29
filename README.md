# OSC Trigger GUI

A Windows C++ application that listens for OSC (Open Sound Control) messages and triggers keyboard events when specific values are received. Features both one-shot and continuous trigger modes for flexible automation workflows.

## Features

- **OSC Message Processing**: Handles both individual OSC messages and OSC bundles
- **Flexible Trigger Modes**: Choose between one-shot (trigger once then stop) or continuous (trigger repeatedly) modes
- **Configurable IP Binding**: Listen on specific IP addresses or all interfaces (0.0.0.0)
- **Window Targeting**: Target specific application windows by exact title match
- **Advanced Key Combinations**: Support for modifier keys (Ctrl, Shift, Alt) and special keys
- **Real-time Status Logging**: Timestamped debug output with hex data display
- **Responsive GUI**: Resizable window with automatic control repositioning

## Quick Start

1. **Run**: Launch `osc_trigger_gui.exe`
2. **Select Target**: Choose target window from dropdown or refresh to update list
3. **Configure IP**: Set IP address (use `0.0.0.0` for all interfaces, `127.0.0.1` for localhost)
4. **Set Parameters**: Configure port, trigger key, target value, and OSC address
5. **Choose Mode**: Check "Continuous Listening Mode" for repeated triggers, or leave unchecked for one-shot
6. **Start**: Click "START" to begin listening

## Configuration Options

### Network Settings
- **IP Address**: Interface to bind to (default: `0.0.0.0` for all interfaces)
- **UDP Port**: Port to listen on (default: `55525`)

### Trigger Settings
- **Window Title**: Exact name of target application window
- **Trigger Key**: Key combination to send (supports modifiers like `CTRL+A`, `SHIFT+F1`)
- **Target Value**: Integer value that triggers the action (default: `9`)
- **OSC Address**: OSC address pattern to match (default: `/flair/runstate`)
- **Continuous Mode**: Enable for repeated triggers, disable for one-shot behavior

### Supported Key Formats
- **Basic Keys**: `SPACE`, `ENTER`, `TAB`, `ESC`, `A`-`Z`, `0`-`9`
- **Function Keys**: `F1`-`F12`
- **Arrow Keys**: `LEFT`, `RIGHT`, `UP`, `DOWN`
- **Modifier Combinations**: `CTRL+A`, `SHIFT+F1`, `ALT+SPACE`
- **Special Keys**: `BACKSPACE`, `DELETE`, `INSERT`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`

## Building

### Prerequisites
- Windows 10/11
- Visual Studio Build Tools 2022 or Visual Studio 2022
- VS Code (optional, for integrated build task)

### Build Commands

**Using VS Code Task:**
```
Ctrl+Shift+P → Tasks: Run Task → build-gui
```

**Command Line:**
```batch
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" && cl /O2 /MT /EHsc /DWIN32 /D_WINDOWS osc_trigger_gui.cpp /link ws2_32.lib user32.lib comctl32.lib /SUBSYSTEM:WINDOWS
```

### Build Configuration
- `/O2`: Speed optimization
- `/MT`: Static runtime linking (no external dependencies)
- `/EHsc`: C++ exception handling
- **Libraries**: `ws2_32.lib` (Winsock2), `user32.lib` (Windows API), `comctl32.lib` (Common Controls)

## Technical Details

### Architecture
- **OSCTrigger Class**: Handles UDP socket operations and OSC message parsing
- **Win32 GUI**: Native Windows interface with real-time status display
- **Threaded Design**: Separate thread for network operations to prevent GUI blocking
- **One-Shot Behavior**: Automatically stops after first successful trigger

### OSC Protocol Support
- **Message Format**: Standard OSC message structure with address, type tags, and values
- **Bundle Support**: Processes OSC bundles containing multiple messages
- **Value Types**: Integer (`i`) and float (`f`) values with tolerance matching for floats
- **Endianness**: Proper big-endian to little-endian conversion for network data

### Key Features
- **Socket Reuse**: Enables address reuse for development workflows
- **Non-blocking Sockets**: Uses `select()` with timeouts for responsive operation
- **Error Handling**: Comprehensive error reporting for network and Windows API operations
- **Memory Management**: Proper cleanup of sockets and threads on shutdown

## Usage Examples

### Basic Setup
1. Set IP to `0.0.0.0` to listen on all network interfaces
2. Keep default port `55525`
3. Select target window from dropdown (e.g., "Notepad")
4. Set trigger key (e.g., `SPACE` or `CTRL+S`)
5. Configure target value to match your OSC sender

### Advanced Key Combinations
- `CTRL+A`: Select all in target application
- `SHIFT+F1`: Custom shortcut with Shift modifier
- `ALT+TAB`: Switch windows (use with caution)

### Network Configuration
- **Local testing**: Use `127.0.0.1` for localhost-only
- **Network listening**: Use `0.0.0.0` for all interfaces
- **Specific interface**: Use exact IP address of network adapter

## Troubleshooting

### Common Issues
- **"Bind failed" error**: Port already in use or invalid IP address
- **"No windows found"**: Click "Refresh" to update window list
- **Keys not working**: Ensure target window accepts input and has correct title
- **No OSC messages**: Check firewall settings and network connectivity

### Debug Information
- Status log shows timestamped debug information
- Raw packet data displayed in hexadecimal format
- Network source information (IP:port) for received messages
- Detailed OSC message parsing steps

## Trigger Modes

### One-Shot Mode (Default)
- Triggers once per session then stops automatically
- Requires manual restart for additional triggers
- Ideal for single-event automation

### Continuous Mode
- Triggers repeatedly on each OSC value match
- Continues listening until manually stopped
- Perfect for ongoing automation workflows

**Usage:**
1. Check "Continuous Listening Mode" checkbox for repeated triggers
2. Leave unchecked for traditional one-shot behavior
3. Click "START" to begin listening
4. In continuous mode, click "STOP" to end the session

## Contributing

Standard C++ with Win32 API. Key areas for contribution:
- Additional OSC data type support
- Multi-trigger modes
- Configuration file persistence
- Additional key mapping options

## License

Open source - free to use, modify, and distribute.
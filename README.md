# OSC Trigger GUI

A Windows C++ application that creates a GUI for an OSC (Open Sound Control) trigger system. The application listens for OSC messages on a UDP socket and triggers keyboard events when specific OSC values are received.

## Features

- **OSC Message Processing**: Handles both individual OSC messages and bundled messages
- **Real-time GUI**: Win32 API-based interface with live status logging
- **Configurable Targeting**: Target specific application windows by title
- **Flexible Key Mapping**: Support for SPACE, ENTER, F1-F5, or single character keys
- **Value Matching**: Integer and float value comparison with configurable tolerance
- **Threaded Architecture**: Non-blocking UDP listener with responsive GUI

## Quick Start

1. **Download**: Get the pre-built executable `osc_trigger_gui.exe`
2. **Run**: Double-click to launch the application
3. **Configure**: Set your target window, UDP port, OSC address, trigger key, and target value
4. **Start**: Click "Start Listening" to begin monitoring OSC messages

## Configuration

- **Target Window**: Name of the application window to receive keyboard events
- **UDP Port**: Port to listen for OSC messages (default: 55525)
- **OSC Address**: OSC address pattern to match (default: "/flair/runstate")
- **Trigger Key**: Key to send when target value is matched
- **Target Value**: Value to trigger on (integer or float)

## Building from Source

### Prerequisites
- Windows 10/11
- Visual Studio Build Tools 2022
- VS Code (optional, for task integration)

### Build Commands

**Using VS Code:**
```
Ctrl+Shift+P > Tasks: Run Task > build-gui
```

**Command Line:**
```batch
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" && cl /O2 /MT /EHsc /DWIN32 /D_WINDOWS osc_trigger_gui.cpp /link ws2_32.lib user32.lib comctl32.lib /SUBSYSTEM:WINDOWS
```

### Build Options
- `/O2` - Optimization for speed
- `/MT` - Static runtime linking
- `/EHsc` - C++ exception handling
- Required libraries: `ws2_32.lib`, `user32.lib`, `comctl32.lib`

## Architecture

### Core Components

1. **OSCTrigger Class** - Handles UDP socket creation and OSC message parsing
2. **GUI System** - Win32 API-based window with configuration controls
3. **Configuration Management** - Window targeting and parameter setup

### Technical Details

- **Platform**: Windows-specific (requires Winsock2)
- **Threading**: Separate thread for UDP listening
- **Priority**: High priority class for responsive triggering
- **Error Handling**: Comprehensive socket and Windows API error handling

## Usage Examples

### Basic Setup
1. Set "Target Window" to your application (e.g., "Notepad")
2. Keep default UDP port (55525)
3. Set OSC address to match your sender
4. Choose trigger key (e.g., SPACE)
5. Set target value to trigger on

### Common Use Cases
- **Live Performance**: Trigger lighting cues from audio software
- **Automation**: Remote control of Windows applications
- **Testing**: Simulate keyboard input for application testing

## Troubleshooting

- **No OSC messages received**: Check firewall settings and UDP port
- **Target window not found**: Verify exact window title spelling
- **Keys not working**: Ensure target application has focus capability

## Contributing

This project uses standard C++ with Win32 API. Contributions are welcome - please follow standard C++ coding practices and ensure Windows compatibility.

## License

Open source - feel free to modify and distribute.
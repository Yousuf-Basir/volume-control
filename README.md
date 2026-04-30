# Volume and Microphone Control

A lightweight Windows system tray application to control system volume and microphone mute status using keyboard shortcuts.

## 🚀 Download

You can download the latest portable version from the **[Releases](https://github.com/Yousuf-Basir/volume-control/releases)** page.

The app is now a single-file executable (`keypress.exe`)—no installation required!

## Features

- **Volume Control Shortcuts**
  - `Alt+F2`: Decrease volume
  - `Alt+F3`: Increase volume
- **Microphone Control**
  - `Alt+K`: Toggle Microphone Mute/Unmute
  - Instant Toast OSD (On-Screen Display) feedback
- **System Tray Integration**
  - Runs in the background with a system tray icon
  - Right-click tray icon to toggle "Start with Windows"
  - Quick access to Microphone Mute toggle

## Installation

1. Build the project using CMake:
   ```bash
   cmake -B build
   cmake --build build --config Debug
   ```

2. Copy `icon.png` to the output directory (e.g., `build/Debug/`)

3. Run `keypress.exe`

## Usage

- The app starts minimized to the system tray
- Use keyboard shortcuts to control volume:
  - Press `Alt+F2` to decrease volume
  - Press `Alt+F3` to increase volume
- Right-click the tray icon to:
  - Toggle "Start with Windows" option
  - Quit the application
- Press `ESC` in the console window to exit (if launched from console)

## Requirements

- Windows 10 or later
- Visual Studio 2019 or later (for building)
- CMake 3.10 or later

## Building

```bash
# Configure and build
cmake -S . -B build
cmake --build build --config Debug

# Run
build\Debug\keypress.exe
```

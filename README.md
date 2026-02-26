# Volume Control

A Windows system tray application that allows you to control system volume using keyboard shortcuts (Alt+F2 and Alt+F3). The app runs in the background with a system tray icon.

## Features

- **Volume Control Shortcuts**
  - `Alt+F2`: Decrease volume
  - `Alt+F3`: Increase volume
  
- **System Tray Integration**
  - Runs in the background with a system tray icon
  - Right-click tray icon to access menu
  - Toggle "Start with Windows" to launch automatically on boot
  - Click "Quit" to exit the application

- **Windows Integration**
  - Shows Windows default volume indicator when changing volume
  - Custom tray icon (icon.png)
  - Starts with Windows (optional)

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

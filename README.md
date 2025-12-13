# win32audio

A comprehensive Flutter plugin for managing Windows audio devices, controlling volume, and extracting native icons.

![image](https://user-images.githubusercontent.com/20853986/175772236-0158ca3b-7dd0-41d4-b10e-7304f3be0b71.png)

## Features

- 🎧 **Enumerate audio devices** (input/output)
- 🔊 **Get and set master volume** for devices
- 🎚️ **Control individual application volumes** (Audio Mixer)
- 🔄 **Switch between audio devices**
- 📊 **Get and set sample rates** for audio devices
- 👂 **Listen to audio device changes** in real-time
- 🎨 **Extract icons** from executables, DLLs, windows, and icon handles
- 🎯 **Support for different audio roles** (Console, Multimedia, Communications)

---

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Audio Devices](#audio-devices)
  - [Enumerate Devices](#enumerate-devices)
  - [Get Default Device](#get-default-device)
  - [Set Default Device](#set-default-device)
  - [Switch Default Device](#switch-default-device)
- [Volume Control](#volume-control)
  - [Get Volume](#get-volume)
  - [Set Volume](#set-volume)
  - [Set Device Volume](#set-device-volume)
- [Audio Mixer (Per-Application Volume)](#audio-mixer-per-application-volume)
  - [Enumerate Audio Sessions](#enumerate-audio-sessions)
  - [Set Application Volume by Process ID](#set-application-volume-by-process-id)
  - [Set Application Volume by Path](#set-application-volume-by-path)
- [Sample Rate Management](#sample-rate-management)
  - [Get Sample Rate](#get-sample-rate)
  - [Set Sample Rate](#set-sample-rate)
- [Event Listeners](#event-listeners)
  - [Setup Change Listener](#setup-change-listener)
  - [Add Change Listener](#add-change-listener)
  - [Remove Change Listener](#remove-change-listener)
- [Icon Extraction](#icon-extraction)
  - [Extract File Icon](#extract-file-icon)
  - [Extract Window Icon](#extract-window-icon)
  - [Extract Icon Handle](#extract-icon-handle)
- [Data Structures](#data-structures)
- [Complete Example](#complete-example)

---

## Installation

Add this to your package's `pubspec.yaml` file:

```yaml
dependencies:
  win32audio: ^latest_version
```

Then run:

```bash
flutter pub get
```

---

## Quick Start

```dart
import 'package:win32audio/win32audio.dart';

void main() async {
  // Get all output devices
  List<AudioDevice> devices = await Audio.enumDevices(AudioDeviceType.output) ?? [];
  
  // Get current volume
  double volume = await Audio.getVolume(AudioDeviceType.output);
  
  // Set volume to 50%
  await Audio.setVolume(0.5, AudioDeviceType.output);
  
  print('Found ${devices.length} output devices');
  print('Current volume: ${(volume * 100).toStringAsFixed(0)}%');
}
```

---

## Audio Devices

### Enumerate Devices

Get a list of all available audio devices (input or output).

```dart
// Get all output devices (speakers, headphones)
List<AudioDevice> outputDevices = await Audio.enumDevices(AudioDeviceType.output) ?? [];

// Get all input devices (microphones)
List<AudioDevice> inputDevices = await Audio.enumDevices(AudioDeviceType.input) ?? [];

// Get devices for a specific audio role
List<AudioDevice> commDevices = await Audio.enumDevices(
  AudioDeviceType.output,
  audioRole: AudioRole.communications,
) ?? [];

// Print device information
for (var device in outputDevices) {
  print('Device: ${device.name}');
  print('ID: ${device.id}');
  print('Is Active: ${device.isActive}');
  print('Icon Path: ${device.iconPath}');
  print('Icon ID: ${device.iconID}');
  print('---');
}
```

### Get Default Device

Retrieve the current default audio device.

```dart
// Get default output device
AudioDevice? defaultOutput = await Audio.getDefaultDevice(AudioDeviceType.output);

// Get default input device
AudioDevice? defaultInput = await Audio.getDefaultDevice(AudioDeviceType.input);

// Get default device for communications
AudioDevice? defaultComm = await Audio.getDefaultDevice(
  AudioDeviceType.output,
  audioRole: AudioRole.communications,
);

if (defaultOutput != null) {
  print('Default output device: ${defaultOutput.name}');
}
```

### Set Default Device

Set a specific device as the system default.

```dart
// Get all devices
List<AudioDevice> devices = await Audio.enumDevices(AudioDeviceType.output) ?? [];

if (devices.isNotEmpty) {
  // Set the first device as default for multimedia
  await Audio.setDefaultDevice(
    devices[0].id,
    multimedia: true,
  );
  
  // Set as default for all roles
  await Audio.setDefaultDevice(
    devices[0].id,
    console: true,
    multimedia: true,
    communications: true,
  );
  
  // Set as default only for communications (VoIP apps)
  await Audio.setDefaultDevice(
    devices[0].id,
    console: false,
    multimedia: false,
    communications: true,
  );
}
```

### Switch Default Device

Cycle to the next available audio device.

```dart
// Switch to next output device
await Audio.switchDefaultDevice(AudioDeviceType.output);

// Switch to next input device
await Audio.switchDefaultDevice(AudioDeviceType.input);

// Switch with specific roles
await Audio.switchDefaultDevice(
  AudioDeviceType.output,
  console: true,
  multimedia: true,
  communications: false,
);

// Example: Toggle between devices with a button
ElevatedButton(
  onPressed: () async {
    await Audio.switchDefaultDevice(AudioDeviceType.output);
    // Refresh UI
    setState(() {});
  },
  child: Text('Switch Audio Device'),
)
```

---

## Volume Control

### Get Volume

Get the master volume level (0.0 to 1.0).

```dart
// Get output volume
double outputVolume = await Audio.getVolume(AudioDeviceType.output);
print('Output volume: ${(outputVolume * 100).toStringAsFixed(0)}%');

// Get input volume (microphone)
double inputVolume = await Audio.getVolume(AudioDeviceType.input);
print('Input volume: ${(inputVolume * 100).toStringAsFixed(0)}%');

// Get volume for specific audio role
double commVolume = await Audio.getVolume(
  AudioDeviceType.output,
  audioRole: AudioRole.communications,
);
```

### Set Volume

Set the master volume level.

```dart
// Set volume to 50% (0.5)
await Audio.setVolume(0.5, AudioDeviceType.output);

// Set volume to 75%
await Audio.setVolume(0.75, AudioDeviceType.output);

// You can also use percentage values (will be converted automatically)
await Audio.setVolume(80, AudioDeviceType.output); // Sets to 80%

// Set microphone volume
await Audio.setVolume(0.6, AudioDeviceType.input);

// Example: Volume slider
Slider(
  value: currentVolume,
  min: 0.0,
  max: 1.0,
  divisions: 100,
  label: '${(currentVolume * 100).round()}%',
  onChanged: (double value) async {
    await Audio.setVolume(value, AudioDeviceType.output);
    setState(() {
      currentVolume = value;
    });
  },
)
```

### Set Device Volume

Set volume for a specific device by its ID.

```dart
// Get all devices
List<AudioDevice> devices = await Audio.enumDevices(AudioDeviceType.output) ?? [];

if (devices.isNotEmpty) {
  // Set volume for a specific device
  await Audio.setAudioDeviceVolume(devices[0].id, 0.7);
  
  // Set volume for multiple devices
  for (var device in devices) {
    await Audio.setAudioDeviceVolume(device.id, 0.5);
  }
}
```

---

## Audio Mixer (Per-Application Volume)

### Enumerate Audio Sessions

Get a list of all applications currently using audio.

```dart
// Get all audio sessions
List<ProcessVolume> audioSessions = await Audio.enumAudioMixer() ?? [];

// Get sessions for specific audio role
List<ProcessVolume> commSessions = await Audio.enumAudioMixer(
  audioRole: AudioRole.communications,
) ?? [];

// Print session information
for (var session in audioSessions) {
  print('Process ID: ${session.processId}');
  print('Process Path: ${session.processPath}');
  print('Process Name: ${session.processPath.split('\\').last}');
  print('Volume: ${(session.maxVolume * 100).toStringAsFixed(0)}%');
  print('Peak Volume: ${(session.peakVolume * 100).toStringAsFixed(0)}%');
  print('---');
}
```

### Set Application Volume by Process ID

Control volume for a specific application using its process ID.

```dart
List<ProcessVolume> sessions = await Audio.enumAudioMixer() ?? [];

if (sessions.isNotEmpty) {
  // Set volume to 30% for the first application
  await Audio.setAudioMixerVolume(sessions[0].processId, 0.3);
  
  // Mute a specific application
  await Audio.setAudioMixerVolume(sessions[0].processId, 0.0);
  
  // Set to maximum volume
  await Audio.setAudioMixerVolume(sessions[0].processId, 1.0);
}

// Example: Find and control Chrome's volume
for (var session in sessions) {
  if (session.processPath.toLowerCase().contains('chrome.exe')) {
    await Audio.setAudioMixerVolume(session.processId, 0.5);
    print('Set Chrome volume to 50%');
    break;
  }
}
```

### Set Application Volume by Path

Control volume for an application using its executable path.

```dart
// Set volume for a specific application by path
await Audio.setAudioMixerVolumeByPath(
  r'C:\Program Files\Google\Chrome\Application\chrome.exe',
  0.4,
);

// Set volume for Spotify
await Audio.setAudioMixerVolumeByPath(
  r'C:\Users\YourName\AppData\Roaming\Spotify\Spotify.exe',
  0.8,
);

// With specific audio role
await Audio.setAudioMixerVolumeByPath(
  r'C:\Program Files\Discord\Discord.exe',
  0.6,
  audioRole: AudioRole.communications,
);
```

---

## Sample Rate Management

### Get Sample Rate

Get the current sample rate of an audio device.

```dart
// Get all devices
List<AudioDevice> devices = await Audio.enumDevices(AudioDeviceType.output) ?? [];

if (devices.isNotEmpty) {
  // Get sample rate for the first device
  int sampleRate = await Audio.getSampleRate(devices[0].id);
  
  if (sampleRate > 0) {
    print('Sample Rate: $sampleRate Hz');
  } else {
    print('Failed to get sample rate');
  }
}

// Example: Display sample rates for all devices
for (var device in devices) {
  int rate = await Audio.getSampleRate(device.id);
  print('${device.name}: ${rate > 0 ? "$rate Hz" : "Unknown"}');
}
```

### Set Sample Rate

Change the sample rate of an audio device.

```dart
List<AudioDevice> devices = await Audio.enumDevices(AudioDeviceType.output) ?? [];

if (devices.isNotEmpty) {
  String deviceId = devices[0].id;
  
  // Set to CD quality (44.1 kHz)
  bool success = await Audio.setSampleRate(deviceId, 44100);
  
  // Set to DVD quality (48 kHz)
  success = await Audio.setSampleRate(deviceId, 48000);
  
  // Set to high-res audio (96 kHz)
  success = await Audio.setSampleRate(deviceId, 96000);
  
  // Set to ultra high-res (192 kHz)
  success = await Audio.setSampleRate(deviceId, 192000);
  
  if (success) {
    print('Sample rate changed successfully');
  } else {
    print('Failed to change sample rate. May require administrator privileges.');
  }
}

// Example: Sample rate selector dialog
Future<void> showSampleRateDialog(BuildContext context, AudioDevice device) async {
  int currentRate = await Audio.getSampleRate(device.id);
  
  final rates = [44100, 48000, 96000, 192000];
  
  showDialog(
    context: context,
    builder: (context) => AlertDialog(
      title: Text('Select Sample Rate'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text('Current: ${currentRate > 0 ? "$currentRate Hz" : "Unknown"}'),
          ...rates.map((rate) => ListTile(
            title: Text('$rate Hz'),
            onTap: () async {
              bool success = await Audio.setSampleRate(device.id, rate);
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(
                  content: Text(success 
                    ? 'Changed to $rate Hz' 
                    : 'Failed to change sample rate'),
                ),
              );
            },
          )),
        ],
      ),
    ),
  );
}
```

**Note:** Changing sample rates may require administrator privileges and will restart audio streams.

---

## Event Listeners

### Setup Change Listener

Initialize the audio event listener system.

```dart
void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  // Setup the change listener (required before adding listeners)
  await Audio.setupChangeListener();
  
  runApp(MyApp());
}
```

### Add Change Listener

Listen to audio device changes in real-time.

```dart
class _MyAppState extends State<MyApp> {
  List<String> eventLog = [];
  
  @override
  void initState() {
    super.initState();
    
    // Add a listener for audio device changes
    Audio.addChangeListener(_onAudioDeviceChange);
  }
  
  void _onAudioDeviceChange(String type, String id) {
    print('Event Type: $type');
    print('Device ID: $id');
    
    setState(() {
      eventLog.insert(0, '$type: $id');
      if (eventLog.length > 50) eventLog.removeLast();
    });
    
    // Handle specific event types
    switch (type) {
      case 'OnDeviceStateChanged':
        print('Device state changed');
        break;
      case 'OnDeviceAdded':
        print('New device added');
        _refreshDevices();
        break;
      case 'OnDeviceRemoved':
        print('Device removed');
        _refreshDevices();
        break;
      case 'OnDefaultDeviceChanged':
        print('Default device changed');
        _refreshDefaultDevice();
        break;
      case 'OnPropertyValueChanged':
        print('Device property changed');
        break;
    }
  }
  
  void _refreshDevices() async {
    // Refresh your device list
    var devices = await Audio.enumDevices(AudioDeviceType.output);
    setState(() {
      // Update UI
    });
  }
  
  void _refreshDefaultDevice() async {
    // Refresh default device
    var defaultDevice = await Audio.getDefaultDevice(AudioDeviceType.output);
    setState(() {
      // Update UI
    });
  }
}
```

**Event Types:**
- `OnDeviceStateChanged` - Device state changed (enabled/disabled)
- `OnDeviceAdded` - New audio device connected
- `OnDeviceRemoved` - Audio device disconnected
- `OnDefaultDeviceChanged` - Default device changed
- `OnPropertyValueChanged` - Device property changed (volume, etc.)

### Remove Change Listener

Remove a previously added listener.

```dart
@override
void dispose() {
  // Remove the listener when widget is disposed
  Audio.removeChangeListener(_onAudioDeviceChange);
  super.dispose();
}
```

**⚠️ Warning:** You may see this message in the console:

```
The 'win32audio' channel sent a message from native to Flutter on a non-platform thread.
```

This is expected behavior as the events come from a separate thread. It's safe to ignore.

---

## Icon Extraction

### Extract File Icon

Extract icons from executable files or DLLs.

```dart
final WinIcons winIcons = WinIcons();

// Extract icon from an executable
Uint8List? iconBytes = await winIcons.extractFileIcon(
  r'C:\Windows\System32\notepad.exe',
  iconID: 0,
);

// Extract icon from a DLL
Uint8List? dllIcon = await winIcons.extractFileIcon(
  r'C:\Windows\System32\shell32.dll',
  iconID: 3, // Different icons have different IDs
);

// Display the icon
if (iconBytes != null) {
  Image.memory(
    iconBytes,
    width: 32,
    height: 32,
  );
}

// Example: Extract icons for all audio devices
Map<String, Uint8List?> deviceIcons = {};

List<AudioDevice> devices = await Audio.enumDevices(AudioDeviceType.output) ?? [];

for (var device in devices) {
  if (device.iconPath.isNotEmpty) {
    deviceIcons[device.id] = await winIcons.extractFileIcon(
      device.iconPath,
      iconID: device.iconID,
    );
  }
}

// Use in a ListView
ListView.builder(
  itemCount: devices.length,
  itemBuilder: (context, index) {
    return ListTile(
      leading: deviceIcons[devices[index].id] != null
        ? Image.memory(
            deviceIcons[devices[index].id]!,
            width: 32,
            height: 32,
          )
        : Icon(Icons.speaker),
      title: Text(devices[index].name),
    );
  },
)
```

### Extract Window Icon

Extract the icon from a window using its handle (HWND).

```dart
final WinIcons winIcons = WinIcons();

// Extract icon from a window handle
int windowHandle = 123456; // HWND from win32 package or other source
Uint8List? windowIcon = await winIcons.extractWindowIcon(windowHandle);

if (windowIcon != null) {
  Image.memory(
    windowIcon,
    width: 32,
    height: 32,
  );
}

// Example: Get icon from active window
// (requires additional win32 package for getting window handles)
import 'package:win32/win32.dart';

int hwnd = GetForegroundWindow();
if (hwnd != 0) {
  Uint8List? icon = await winIcons.extractWindowIcon(hwnd);
  // Display icon
}
```

### Extract Icon Handle

Convert a raw Win32 icon handle (HICON) to PNG bytes.

```dart
final WinIcons winIcons = WinIcons();

// Extract icon from an icon handle
int iconHandle = 789012; // HICON from win32 API
Uint8List? iconPng = await winIcons.extractIconHandle(iconHandle);

if (iconPng != null) {
  Image.memory(
    iconPng,
    width: 32,
    height: 32,
  );
}
```

### Deprecated Function

The old `nativeIconToBytes` function is deprecated. Use `WinIcons` class instead:

```dart
// ❌ Old way (deprecated)
Uint8List? icon = await nativeIconToBytes(iconPath, iconID: iconID);

// ✅ New way
final WinIcons winIcons = WinIcons();
Uint8List? icon = await winIcons.extractFileIcon(iconPath, iconID: iconID);
```

---

## Data Structures

### AudioDevice

Represents a single audio device.

```dart
class AudioDevice {
  String id;           // Unique device identifier (MMDevice ID)
  String name;         // Friendly name of the device
  String iconPath;     // Path to the icon resource file
  int iconID;          // Resource ID of the icon
  bool isActive;       // Whether this is the default/active device
}
```

### ProcessVolume

Represents an application's audio session.

```dart
class ProcessVolume {
  int processId;       // Process ID (PID)
  String processPath;  // Full path to the executable
  double maxVolume;    // Current volume (0.0 to 1.0)
  double peakVolume;   // Current peak volume (0.0 to 1.0)
}
```

### AudioDeviceType

Enumeration of device types.

```dart
enum AudioDeviceType {
  output,  // Playback devices (speakers, headphones)
  input,   // Recording devices (microphones)
}
```

### AudioRole

Enumeration of audio roles for Windows.

```dart
enum AudioRole {
  console,        // Games, system sounds, voice commands
  multimedia,     // Music, movies, narration
  communications, // Voice chat, VoIP
}
```

---

## Complete Example

Here's a complete example demonstrating multiple features:

```dart
import 'package:flutter/material.dart';
import 'package:win32audio/win32audio.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Audio.setupChangeListener();
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  List<AudioDevice> devices = [];
  List<ProcessVolume> audioSessions = [];
  AudioDevice? defaultDevice;
  double currentVolume = 0.0;
  final WinIcons winIcons = WinIcons();
  Map<String, Uint8List?> deviceIcons = {};

  @override
  void initState() {
    super.initState();
    Audio.addChangeListener(_onAudioChange);
    _loadDevices();
    _loadAudioSessions();
  }

  @override
  void dispose() {
    Audio.removeChangeListener(_onAudioChange);
    super.dispose();
  }

  void _onAudioChange(String type, String id) {
    print('Audio event: $type - $id');
    if (type == 'OnDefaultDeviceChanged' || type == 'OnDeviceAdded') {
      _loadDevices();
    }
  }

  Future<void> _loadDevices() async {
    devices = await Audio.enumDevices(AudioDeviceType.output) ?? [];
    defaultDevice = await Audio.getDefaultDevice(AudioDeviceType.output);
    currentVolume = await Audio.getVolume(AudioDeviceType.output);
    
    // Load device icons
    for (var device in devices) {
      if (device.iconPath.isNotEmpty) {
        deviceIcons[device.id] = await winIcons.extractFileIcon(
          device.iconPath,
          iconID: device.iconID,
        );
      }
    }
    
    setState(() {});
  }

  Future<void> _loadAudioSessions() async {
    audioSessions = await Audio.enumAudioMixer() ?? [];
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: Text('Win32Audio Demo'),
          actions: [
            IconButton(
              icon: Icon(Icons.swap_horiz),
              onPressed: () async {
                await Audio.switchDefaultDevice(AudioDeviceType.output);
                _loadDevices();
              },
            ),
          ],
        ),
        body: Column(
          children: [
            // Volume Control
            Padding(
              padding: EdgeInsets.all(16),
              child: Column(
                children: [
                  Text(
                    'Master Volume: ${(currentVolume * 100).toStringAsFixed(0)}%',
                    style: TextStyle(fontSize: 20),
                  ),
                  Slider(
                    value: currentVolume,
                    onChanged: (value) async {
                      await Audio.setVolume(value, AudioDeviceType.output);
                      setState(() => currentVolume = value);
                    },
                  ),
                ],
              ),
            ),
            
            // Device List
            Expanded(
              child: ListView.builder(
                itemCount: devices.length,
                itemBuilder: (context, index) {
                  final device = devices[index];
                  return ListTile(
                    leading: deviceIcons[device.id] != null
                      ? Image.memory(deviceIcons[device.id]!, width: 32, height: 32)
                      : Icon(Icons.speaker),
                    title: Text(device.name),
                    trailing: device.isActive
                      ? Icon(Icons.check, color: Colors.green)
                      : null,
                    onTap: () async {
                      await Audio.setDefaultDevice(device.id);
                      _loadDevices();
                    },
                  );
                },
              ),
            ),
            
            Divider(),
            
            // Audio Sessions
            Expanded(
              child: ListView.builder(
                itemCount: audioSessions.length,
                itemBuilder: (context, index) {
                  final session = audioSessions[index];
                  final appName = session.processPath.split('\\').last;
                  
                  return ListTile(
                    title: Text(appName),
                    subtitle: Slider(
                      value: session.maxVolume,
                      onChanged: (value) async {
                        await Audio.setAudioMixerVolume(session.processId, value);
                        _loadAudioSessions();
                      },
                    ),
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }
}
```

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Issues

If you encounter any issues or have feature requests, please file them on the [GitHub issue tracker](https://github.com/Far-Se/win32audio/issues).
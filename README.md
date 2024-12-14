# win32audio

A library to get Audio Devices on Windows.

Also you can get volume and set Master volume or individual application volume

Also includes a utilitary function `nativeIconToBytes` that can be used for other purposes rather than audio device.

![image](https://user-images.githubusercontent.com/20853986/175772236-0158ca3b-7dd0-41d4-b10e-7304f3be0b71.png)



# How to use

## Enumerate Devices

#### Audio Devices

```dart
  var audioDevices = <AudioDevice>[];

    audioDevices = await Audio.enumDevices(audioDeviceType) ?? [];
```
#### Audio Mixer

```dart
  var mixerList = <ProcessVolume>[];

  mixerList = await Audio.enumAudioMixer() ?? [];
```
## Default Device

#### Get
```dart
  var defaultDevice = AudioDevice();
  
  defaultDevice = (await Audio.getDefaultDevice(audioDeviceType))!;
```

#### Set

`id` is COM string
```dart
await Audio.setDefaultDevice(audioDevices[0].id, {bool console = false, bool multimedia = true, bool communications = false});
```
#### Switch to next Audio Device
```dart
    await Audio.switchDefaultDevice(audioDeviceType, {bool console = false, bool multimedia = true, bool communications = false});
```
## Volume

Volume is double between 0 and 1.

#### Get
```dart
    final volume = await Audio.getVolume(audioDeviceType);
```

#### Set

```dart
    final volume = await Audio.setVolume(0.5,audioDeviceType);
```
#### Set Specific app volume:

```dart
    await Audio.setAudioMixerVolume(mixerList[0].processId, 0.3);
```

## Structs/Enums

```dart
class AudioDevice {
  String id = "";
  String name = "";
  String iconPath = "";
  int iconID = 0;
  bool isActive = false;
}

class ProcessVolume {
  int processId = 0;
  String processPath = "";
  double maxVolume = 1.0;
  double peakVolume = 0.0;
}

enum AudioDeviceType {
  output,
  input,
}
```
# Listen to Changes
1. Listen to audio device changes with `Audio.setupChangeListener` To setup, on `main()`
2. `Audio.addChangeListener` add it to `initState()` or whereever you want
3. `Audio.removeChangeListener` to remove a listener.
4. All callback functions: 
    `OnDeviceStateChanged`
    `OnDeviceAdded`
    `OnDeviceRemoved`
    `OnDefaultDeviceChanged`
    `OnPropertyValueChanged`

```dart
void main() {
    
  WidgetsFlutterBinding.ensureInitialized();
  await Audio.setupChangeListener();
}
[...]
initState() {
    Audio.addChangeListener((String type, String id) async {
      print(type);
      print(id);
    });
}
```

### Warning
If you use this, you will receive this message:

```The 'win32audio' channel sent a message from native to Flutter on a non-platform thread. Platform channel messages must be sent on the platform thread. Failure to do so may result in data loss or crashes, and must be fixed in the plugin or application code creating that channel.```

I'ts nothing to be scared of, the message comes from a separate thread.



# Extra Function

`nativeIconToBytes` extracts icon either from EXE file or dll file and store it as UInt8List

```dart

    Map<String, Uint8List?> _audioIcons = {};
    _audioIcons = {};
    for (var audioDevice in audioDevices) {
      if (_audioIcons[audioDevice.id] == null) {
        _audioIcons[audioDevice.id] = await nativeIconToBytes(audioDevice.iconPath, iconID: audioDevice.iconID);
      }
    }
///....
widget: (_audioIcons.containsKey(audioDevices[index].id))
                            ? Image.memory(
                                _audioIcons[audioDevices[index].id] ?? Uint8List(0),
                                width: 32,
                                height: 32,
                              )
                            : const Icon(Icons.spoke_outlined),
```

In the same way I've made a class WinIcons() with these functions: 

`extractFileIcon` that extract an asociated icon from any file.

`extractWindowIcon` which extracts an icon from HWND/ Window Handle

`extractIconHandle` which extracts an icon from HICON/ Icon Handle.
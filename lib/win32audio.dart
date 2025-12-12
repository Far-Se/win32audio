// ignore_for_file: public_member_api_docs, sort_constructors_first

import 'dart:async';

import 'package:flutter/services.dart';

/// Represents a single audio device (Input or Output) on Windows.
class AudioDevice {
  /// The unique identifier of the audio device (MMDevice ID).
  String id = "";

  /// The friendly name of the audio device.
  String name = "";

  /// The file path to the resource containing the device's icon.
  /// This is usually a DLL or EXE file.
  String iconPath = "";

  /// The resource ID of the icon within [iconPath].
  int iconID = 0;

  /// Whether this device is currently the default or active device.
  bool isActive = false;

  @override
  String toString() {
    return "AudioDevice{id: $id, name: $name, iconPath: $iconPath, iconID: $iconID, isActive: $isActive}";
  }

  /// Converts the [AudioDevice] instance to a map.
  Map<String, dynamic> toMap() {
    return <String, dynamic>{
      "id": id,
      "name": name,
      "iconPath": iconPath,
      "iconID": iconID,
      "isActive": isActive,
    };
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;

    return other is AudioDevice && other.id == id && other.name == name && other.iconPath == iconPath && other.iconID == iconID && other.isActive == isActive;
  }

  @override
  int get hashCode {
    return Object.hash(id, name, iconPath, iconID, isActive);
  }
}

/// Represents the volume control status of a specific process.
class ProcessVolume {
  /// The Process ID (PID).
  int processId = 0;

  /// The full path to the process executable.
  String processPath = "";

  /// The current master volume of the process (0.0 to 1.0).
  double maxVolume = 1.0;

  /// The peak volume currently being emitted by the process (0.0 to 1.0).
  double peakVolume = 0.0;

  @override
  String toString() {
    return "ProcessVolume{processId: $processId, processPath: $processPath, maxVolume: $maxVolume, peakVolume: $peakVolume}";
  }

  /// Converts the [ProcessVolume] instance to a map.
  Map<String, dynamic> toMap() {
    return <String, dynamic>{
      "processId": processId,
      "processPath": processPath,
      "maxVolume": maxVolume,
      "peakVolume": peakVolume,
    };
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;

    return other is ProcessVolume && other.processId == processId && other.processPath == processPath && other.maxVolume == maxVolume && other.peakVolume == peakVolume;
  }

  @override
  int get hashCode {
    return Object.hash(processId, processPath, maxVolume, peakVolume);
  }
}

/// Enumeration of audio device types.
enum AudioDeviceType {
  /// Playback devices (Speakers, Headphones).
  output,

  /// Recording devices (Microphones).
  input,
}

/// Enumeration of audio roles for Windows Multimedia devices.
enum AudioRole {
  /// Games, system notification sounds, and voice commands.
  console,

  /// Music, movies, narration, and live music recording.
  multimedia,

  /// Voice chat and VoIP applications.
  communications,
}

/// The MethodChannel used for communicating with the native Windows implementation.
const MethodChannel audioMethodChannel = MethodChannel("win32audio");

/// Main class for interacting with Windows Audio APIs.
class Audio {
  /// Internal state to track if the listener for native events is active.
  static bool listenerActive = false;

  static final List<void Function(String type, String id)> _changeListeners = <void Function(String type, String id)>[];

  /// Initializes the audio event listener.
  ///
  /// This must be called before adding any listeners via [addChangeListener].
  /// Usually handled automatically when you start interacting with the plugin.
  static Future<void> setupChangeListener() async {
    if (!listenerActive) {
      listenerActive = true;
      await audioMethodChannel.invokeMethod('initAudioListener');
      audioMethodChannel.setMethodCallHandler((MethodCall call) async {
        if (call.method == 'onAudioDeviceChange') {
          // 'name' represents the event type (e.g., OnDeviceStateChanged)
          // 'id' is the device ID
          for (void Function(String type, String id) listener in _changeListeners) {
            listener(
              call.arguments["name"] as String? ?? "",
              call.arguments["id"] as String? ?? "",
            );
          }
        }
      });
    }
  }

  /// Adds a callback that listens to audio device changes (Added, Removed, Default Changed, etc.).
  static void addChangeListener(void Function(String type, String id) callback) {
    _changeListeners.add(callback);
    // Ensure the native listener is running
    setupChangeListener();
  }

  /// Removes a previously added audio device change listener.
  static void removeChangeListener(void Function(String type, String id) callback) {
    _changeListeners.remove(callback);
  }

  /// Returns a Future list of audio devices of a specified type.
  ///
  /// [audioDeviceType] specifies whether to search for output or input devices.
  /// [audioRole] filters the default status (defaults to [AudioRole.multimedia]).
  static Future<List<AudioDevice>?> enumDevices(AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "role": audioRole.index};
    final Map<dynamic, dynamic>? map = await audioMethodChannel.invokeMethod("enumAudioDevices", arguments);

    if (map == null) return null;

    final List<AudioDevice> audioDevices = <AudioDevice>[];
    for (dynamic key in map.keys) {
      final AudioDevice audioDevice = AudioDevice();
      final Map<dynamic, dynamic> data = map[key];
      audioDevice.id = data["id"] ?? "";
      audioDevice.name = data["name"] ?? "";

      final String iconInfo = data["iconInfo"] ?? "";
      final List<String> iconData = iconInfo.split(",");
      if (iconData.length == 2) {
        audioDevice.iconPath = iconData[0];
        audioDevice.iconID = int.tryParse(iconData[1]) ?? -1;
      } else {
        audioDevice.iconPath = iconInfo;
        audioDevice.iconID = -1;
      }
      audioDevice.isActive = data["isActive"] ?? false;
      audioDevices.add(audioDevice);
    }
    return audioDevices;
  }

  /// Returns the default audio device for the given [AudioDeviceType].
  static Future<AudioDevice?> getDefaultDevice(AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "role": audioRole.index};
    final Map<dynamic, dynamic>? map = await audioMethodChannel.invokeMethod("getDefaultDevice", arguments);

    if (map == null) return null;

    final AudioDevice audioDevice = AudioDevice();
    audioDevice.id = map["id"] ?? "";
    audioDevice.name = map["name"] ?? "";

    final String iconInfo = map["iconInfo"] ?? "";
    final List<String> iconData = iconInfo.split(",");

    if (iconData.length == 2) {
      audioDevice.iconPath = iconData[0];
      audioDevice.iconID = int.tryParse(iconData[1]) ?? -1;
    } else {
      audioDevice.iconPath = iconInfo;
      audioDevice.iconID = -1;
    }
    audioDevice.isActive = map["isActive"] ?? false;
    return audioDevice;
  }

  /// Sets the system default audio device using its [deviceID].
  ///
  /// You can specify which role(s) to set this device as default for:
  /// * [console]
  /// * [multimedia]
  /// * [communications]
  static Future<int> setDefaultDevice(String deviceID, {bool console = false, bool multimedia = true, bool communications = false}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{
      "deviceID": deviceID,
      "console": console,
      "multimedia": multimedia,
      "communications": communications,
    };

    final int? result = await audioMethodChannel.invokeMethod<int>("setDefaultAudioDevice", arguments);
    return result ?? 0;
  }

  /// Gets the master volume level (0.0 to 1.0) for the current default device of the given type.
  static Future<double> getVolume(AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "role": audioRole.index};
    final double? result = await audioMethodChannel.invokeMethod<double>("getAudioVolume", arguments);
    return result ?? 0.0;
  }

  /// Sets the master volume for the default device of the given [audioDeviceType].
  ///
  /// [volume] should be between 0.0 and 1.0. If > 1, it is treated as a percentage (divided by 100).
  static Future<int> setVolume(double volume, AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    if (volume > 1) volume = (volume / 100).toDouble();
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "volumeLevel": volume, "role": audioRole.index};
    final int? result = await audioMethodChannel.invokeMethod<int>("setAudioVolume", arguments);
    return result ?? 0;
  }

  /// Sets the volume of a specific audio device by its [deviceID].
  ///
  /// [volume] should be between 0.0 and 1.0.
  static Future<int> setAudioDeviceVolume(String deviceID, double volume) async {
    if (volume > 1) volume = (volume / 100).toDouble();
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceID": deviceID, "volumeLevel": volume};
    final int? result = await audioMethodChannel.invokeMethod<int>("setAudioDeviceVolume", arguments);
    return result ?? 0;
  }

  /// Switches the default device to the next available one, or toggles between available devices.
  /// (Logic depends on implementation of `switchDefaultDevice` in native code).
  static Future<bool> switchDefaultDevice(AudioDeviceType audioDeviceType,
      {AudioRole audioRole = AudioRole.multimedia, bool console = false, bool multimedia = true, bool communications = false}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{
      "deviceType": audioDeviceType.index,
      "role": audioRole.index,
      "console": console,
      "multimedia": multimedia,
      "communications": communications,
    };
    final bool? result = await audioMethodChannel.invokeMethod<bool>("switchDefaultDevice", arguments);
    return result ?? false;
  }

  /// Gets the current sample rate (in Hz) of the specified audio device.
  ///
  /// Returns the sample rate as an integer (e.g., 44100, 48000, 96000).
  /// Returns 0 if the operation fails.
  static Future<int> getSampleRate(String deviceID) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceID": deviceID};
    final int? result = await audioMethodChannel.invokeMethod<int>("getSampleRate", arguments);
    return result ?? 0;
  }

  /// Sets the sample rate (in Hz) for the specified audio device.
  ///
  /// Common sample rates include: 44100, 48000, 96000, 192000.
  /// Returns true if successful, false otherwise.
  ///
  /// Note: This modifies the device's default format and may require
  /// administrative privileges or cause audio streams to restart.
  static Future<bool> setSampleRate(String deviceID, int sampleRate) async {
    final Map<String, dynamic> arguments = <String, dynamic>{
      "deviceID": deviceID,
      "sampleRate": sampleRate,
    };
    final bool? result = await audioMethodChannel.invokeMethod<bool>("setSampleRate", arguments);
    return result ?? false;
  }

  /// Lists all active application audio sessions (mixers).
  ///
  /// Returns a list of [ProcessVolume] containing volume info for each app.
  static Future<List<ProcessVolume>?> enumAudioMixer({AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"role": audioRole.index};
    final Map<dynamic, dynamic>? map = await audioMethodChannel.invokeMethod("enumAudioMixer", arguments);

    if (map == null) return null;

    final List<ProcessVolume> processVolumes = <ProcessVolume>[];
    for (dynamic key in map.keys) {
      final ProcessVolume processVolume = ProcessVolume();
      final Map<dynamic, dynamic> data = map[key];
      processVolume.processId = key as int;
      processVolume.processPath = data["processPath"] ?? "";
      processVolume.maxVolume = (data["maxVolume"] as num?)?.toDouble() ?? 0.0;
      processVolume.peakVolume = (data["peakVolume"] as num?)?.toDouble() ?? 0.0;
      processVolumes.add(processVolume);
    }
    return processVolumes;
  }

  /// Sets the volume for a specific process identified by [processID].
  ///
  /// [volume] should be 0.0 to 1.0.
  static Future<bool> setAudioMixerVolume(int processID, double volume, {AudioRole audioRole = AudioRole.multimedia}) async {
    if (volume > 1) volume = (volume / 100).toDouble();
    final Map<String, dynamic> arguments = <String, dynamic>{"processID": processID, "volumeLevel": volume, "role": audioRole.index};
    final bool? result = await audioMethodChannel.invokeMethod<bool>("setAudioMixerVolume", arguments);
    return result ?? false;
  }

  /// Sets the volume for a specific process identified by its executable path [processPath].
  ///
  /// [volume] should be 0.0 to 1.0.
  static Future<bool> setAudioMixerVolumeByPath(String processPath, double volume, {AudioRole audioRole = AudioRole.multimedia}) async {
    if (volume > 1) volume = (volume / 100).toDouble();
    final Map<String, dynamic> arguments = <String, dynamic>{"processPath": processPath, "volumeLevel": volume, "role": audioRole.index};
    final bool? result = await audioMethodChannel.invokeMethod<bool>("setAudioMixerVolumeByPath", arguments);
    return result ?? false;
  }
}

/// Helper function to convert a native icon location to bytes.
@Deprecated("Use WinIcons class instead")
Future<Uint8List?> nativeIconToBytes(String iconlocation, {int iconID = 0}) async {
  final Map<String, dynamic> arguments = <String, dynamic>{"iconLocation": iconlocation, "iconID": iconID};
  final Uint8List? result = await audioMethodChannel.invokeMethod<Uint8List>("iconToBytes", arguments);
  return result;
}

/// Utility class for extracting icons from files and windows on Windows.
class WinIcons {
  /// Extracts the icon from a file path (e.g., .exe, .dll).
  ///
  /// [iconlocation] is the absolute path to the file.
  /// [iconID] is the resource ID index (default 0).
  Future<Uint8List?> extractFileIcon(String iconlocation, {int iconID = 0}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"iconLocation": iconlocation, "iconID": iconID};
    final Uint8List? result = await audioMethodChannel.invokeMethod<Uint8List>("iconToBytes", arguments);
    return result;
  }

  @Deprecated("Use extractFileIcon instead")
  Future<Uint8List?> extractExecutableIcon(String iconlocation, {int iconID = 0}) async {
    return await extractFileIcon(iconlocation, iconID: iconID);
  }

  /// Extracts the small icon associated with a specific Window Handle ([hWnd]).
  Future<Uint8List?> extractWindowIcon(int hWnd) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"hWnd": hWnd};
    final Uint8List? result = await audioMethodChannel.invokeMethod<Uint8List>("getWindowIcon", arguments);
    return result;
  }

  /// Converts a raw Win32 icon handle ([hIcon]) to PNG bytes.
  Future<Uint8List?> extractIconHandle(int hIcon) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"hIcon": hIcon};
    final Uint8List? result = await audioMethodChannel.invokeMethod<Uint8List>("getIconPng", arguments);
    return result;
  }
}

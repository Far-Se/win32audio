// ignore_for_file: public_member_api_docs, sort_constructors_first

import "package:flutter/services.dart";

class AudioDevice {
  String id = "";
  String name = "";
  String iconPath = "";
  int iconID = 0;
  bool isActive = false;
  @override
  String toString() {
    return "AudioDevice{id: $id, name: $name, iconPath: $iconPath, iconID: $iconID, isActive: $isActive}";
  }

  //tomap
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
    return id.hashCode ^ name.hashCode ^ iconPath.hashCode ^ iconID.hashCode ^ isActive.hashCode;
  }
}

class ProcessVolume {
  int processId = 0;
  String processPath = "";
  double maxVolume = 1.0;
  double peakVolume = 0.0;
  @override
  String toString() {
    return "ProcessVolume{processId: $processId, processPath: $processPath, maxVolume: $maxVolume, peakVolume: $peakVolume}";
  }

  // tomap
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
    return processId.hashCode ^ processPath.hashCode ^ maxVolume.hashCode ^ peakVolume.hashCode;
  }
}

enum AudioDeviceType {
  output,
  input,
}

enum AudioRole {
  console,
  multimedia,
  communications,
}

const MethodChannel audioMethodChannel = MethodChannel("win32audio");

class Audio {
  static void Function()? _onDevicesChanged;

  static Future<void> initialize({required void Function() onDevicesChanged}) async {
    _onDevicesChanged = onDevicesChanged;

    audioMethodChannel.setMethodCallHandler((MethodCall call) async {
      if (call.method == 'onDevicesChanged' && _onDevicesChanged != null) {
        _onDevicesChanged!();
      }
    });

    // await audioMethodChannel.invokeMethod('initialize');
  }

  /// Returns a Future list of audio devices of a specified type.
  /// The type is specified by the [AudioDeviceType] enum.
  ///
  static Future<List<AudioDevice>?> enumDevices(AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "role": audioRole.index};
    final Map<dynamic, dynamic> map = await audioMethodChannel.invokeMethod("enumAudioDevices", arguments);
    List<AudioDevice>? audioDevices = <AudioDevice>[];
    for (int key in map.keys) {
      final AudioDevice audioDevice = AudioDevice();
      audioDevice.id = map[key]["id"];
      audioDevice.name = map[key]["name"];
      final List<String> iconData = map[key]["iconInfo"].split(",");
      if (iconData.length == 2) {
        audioDevice.iconPath = iconData[0];
        audioDevice.iconID = int.tryParse(iconData[1]) ?? -1;
      } else {
        audioDevice.iconPath = map[key]["iconInfo"];
        audioDevice.iconID = -1;
      }
      audioDevice.isActive = map[key]["isActive"];
      audioDevices.add(audioDevice);
    }
    return audioDevices;
  }

  /// Returns a Future containing the default audio device of the given type.
  /// The type is specified by the [AudioDeviceType] enum.
  ///
  static Future<AudioDevice?> getDefaultDevice(AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "role": audioRole.index};
    final Map<dynamic, dynamic> map = await audioMethodChannel.invokeMethod("getDefaultDevice", arguments);
    final AudioDevice audioDevice = AudioDevice();
    audioDevice.id = map["id"];
    audioDevice.name = map["name"];
    final List<String> iconData = map["iconInfo"].split(",");
    if (iconData.length == 2) {
      audioDevice.iconPath = iconData[0];
      audioDevice.iconID = int.tryParse(iconData[1]) ?? -1;
    } else {
      audioDevice.iconPath = map["iconInfo"];
      audioDevice.iconID = -1;
    }
    audioDevice.isActive = map["isActive"];
    return audioDevice;
  }

  /// Sets the default audio device.
  /// The type is specified by the [AudioDeviceType] enum.
  static Future<int> setDefaultDevice(String deviceID, {bool console = false, bool multimedia = true, bool communications = false}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{
      "deviceID": deviceID,
      "console": console,
      "multimedia": multimedia,
      "communications": communications,
    };

    final int? result = await audioMethodChannel.invokeMethod<int>("setDefaultAudioDevice", arguments);
    return result as int;
  }

  /// Returns the current volume for the given audio device type.
  /// The type is specified by the [AudioDeviceType] enum.
  static Future<double> getVolume(AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "role": audioRole.index};
    final double? result = await audioMethodChannel.invokeMethod<double>("getAudioVolume", arguments);
    return result as double;
  }

  /// This function sets the volume of the specified audio device type. The volume level is a number between 0 and 1, with 1 being the maximum volume.
  /// The type is specified by the [AudioDeviceType] enum.
  ///
  static Future<int> setVolume(double volume, AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    if (volume > 1) volume = (volume / 100).toDouble();
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "volumeLevel": volume, "role": audioRole.index};
    final int? result = await audioMethodChannel.invokeMethod<int>("setAudioVolume", arguments);
    return result as int;
  }

  /// This function switches the audio device to the specified type. The type is specified by the [AudioDeviceType] enum.
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
    return result as bool;
  }

  /// Returns a Future with a list of ProcessVolume objects containing information about all audio mixers.
  static Future<List<ProcessVolume>?> enumAudioMixer({AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"role": audioRole.index};
    final Map<dynamic, dynamic> map = await audioMethodChannel.invokeMethod("enumAudioMixer", arguments);
    List<ProcessVolume>? processVolumes = <ProcessVolume>[];
    for (int key in map.keys) {
      final ProcessVolume processVolume = ProcessVolume();
      processVolume.processId = key;
      processVolume.processPath = map[key]["processPath"];
      processVolume.maxVolume = map[key]["maxVolume"];
      processVolume.peakVolume = map[key]["peakVolume"];
      processVolumes.add(processVolume);
    }
    return processVolumes;
  }

  /// This function sets the audio mixer volume for the specified process ID. The volume level is a number between 0 and 1, with 1 being the maximum volume.
  /// The process ID is the process ID of the process that is using the audio mixer.
  /// The process ID can be obtained by calling the enumAudioMixer function.
  static Future<bool> setAudioMixerVolume(int processID, double volume, {AudioRole audioRole = AudioRole.multimedia}) async {
    if (volume > 1) volume = (volume / 100).toDouble();
    final Map<String, dynamic> arguments = <String, dynamic>{"processID": processID, "volumeLevel": volume, "role": audioRole.index};
    final bool? result = await audioMethodChannel.invokeMethod<bool>("setAudioMixerVolume", arguments);
    return result as bool;
  }
}

/// This function converts a native icon location to bytes.
/// The icon location is the path to the icon file.
/// The icon ID is the icon ID of the icon file.
/// The icon ID can be obtained by calling the enumAudioDevices function.
@Deprecated("Use WinIcons class instead")
Future<Uint8List?> nativeIconToBytes(String iconlocation, {int iconID = 0}) async {
  final Map<String, dynamic> arguments = <String, dynamic>{"iconLocation": iconlocation, "iconID": iconID};
  final Uint8List? result = await audioMethodChannel.invokeMethod<Uint8List>("iconToBytes", arguments);
  return result;
}

/// A utilitary class that handles Win Icons.
class WinIcons {
  /// This function extracts an icon from any file.

  Future<Uint8List?> extractFileIcon(String iconlocation, {int iconID = 0}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"iconLocation": iconlocation, "iconID": iconID};
    final Uint8List? result = await audioMethodChannel.invokeMethod<Uint8List>("iconToBytes", arguments);
    return result;
  }

  @Deprecated("Use extractFileIcon instead")
  Future<Uint8List?> extractExecutableIcon(String iconlocation, {int iconID = 0}) async {
    return await extractFileIcon(iconlocation, iconID: iconID);
  }

  /// This function extracts an icon from a HWND / Window Handle
  Future<Uint8List?> extractWindowIcon(int hWnd) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"hWnd": hWnd};
    final Uint8List? result = await audioMethodChannel.invokeMethod<Uint8List>("getWindowIcon", arguments);
    return result;
  }

  /// This function extracts an icon from a HICON / IconHandle
  Future<Uint8List?> extractIconHandle(int hIcon) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"hIcon": hIcon};
    final Uint8List? result = await audioMethodChannel.invokeMethod<Uint8List>("getIconPng", arguments);
    return result;
  }
}

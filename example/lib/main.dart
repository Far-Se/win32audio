import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:typed_data';

import 'package:win32audio/win32audio.dart';
import 'widgets/animated_progress_bar.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  State<MyApp> createState() => _MyAppState();
}

Map<String, Uint8List?> _audioIcons = {};

class _MyAppState extends State<MyApp> {
  var defaultDevice = AudioDevice();
  var audioDevices = <AudioDevice>[];
  var audioDeviceType = AudioDeviceType.output;

  var mixerList = <ProcessVolume>[];
  bool _stateFetchAudioMixerPeak = false;

  double __volume = 0.0;
  String fetchStatus = "";
  @override
  void initState() {
    super.initState();
    fetchAudioDevices();
    Timer.periodic(const Duration(milliseconds: 150), (timer) async {
      if (_stateFetchAudioMixerPeak) {
        mixerList = await Audio.enumAudioMixer() ?? [];

        for (var mixer in mixerList) {
          if (_audioIcons[mixer.processPath] == null) {
            _audioIcons[mixer.processPath] = await nativeIconToBytes(mixer.processPath);
          }
        }
        setState(() {});
      }
    });
  }

  Future<void> fetchAudioDevices() async {
    if (!mounted) return;
    audioDevices = await Audio.enumDevices(audioDeviceType) ?? [];
    __volume = await Audio.getVolume(audioDeviceType);
    defaultDevice = (await Audio.getDefaultDevice(audioDeviceType))!;

    _audioIcons = {};
    for (var audioDevice in audioDevices) {
      if (_audioIcons[audioDevice.id] == null) {
        _audioIcons[audioDevice.id] = await nativeIconToBytes(audioDevice.iconPath, iconID: audioDevice.iconID);
      }
    }

    mixerList = await Audio.enumAudioMixer() ?? [];
    for (var mixer in mixerList) {
      if (_audioIcons[mixer.processPath] == null) {
        _audioIcons[mixer.processPath] = await nativeIconToBytes(mixer.processPath);
      }
    }

    setState(() {
      fetchStatus = "Get";
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: Scaffold(
        appBar: AppBar(
          title: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: double.infinity),
            child: //write a widget that expands to the whole screen and has three elements, one on the left and two on the right.
                Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Container(
                  alignment: Alignment.centerLeft,
                  width: 100,
                  child: TextButton(
                    child: Text(
                      fetchStatus,
                      style: const TextStyle(color: Colors.white),
                      strutStyle: const StrutStyle(forceStrutHeight: true),
                    ),
                    onPressed: () async {
                      setState(() {
                        fetchStatus = 'Getting...';
                      });
                      fetchAudioDevices();
                    },
                  ),
                ),
                Row(
                  crossAxisAlignment: CrossAxisAlignment.end,
                  children: <Widget>[
                    Container(
                      alignment: Alignment.centerRight,
                      child: TextButton(
                          child: Row(
                            mainAxisAlignment: MainAxisAlignment.end,
                            crossAxisAlignment: CrossAxisAlignment.center,
                            children: <Widget>[
                              Padding(
                                padding: const EdgeInsets.only(right: 5),
                                child: Align(
                                  alignment: Alignment.topCenter,
                                  heightFactor: 1.15,
                                  child: Text(
                                    defaultDevice.name,
                                    textAlign: TextAlign.justify,
                                    style: const TextStyle(color: Colors.white),
                                    strutStyle: const StrutStyle(forceStrutHeight: true),
                                  ),
                                ),
                              ),
                              const Icon(Icons.swap_vertical_circle_outlined, color: Colors.white),
                            ],
                          ),
                          onPressed: () async {
                            await Audio.switchDefaultDevice(audioDeviceType);
                            fetchAudioDevices();
                            setState(() {});
                          }),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
        body: Column(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            //?  SELECT AUDIO DEVICE
            //? DROPDOWN
            Column(children: [
              DropdownButton(
                  value: audioDeviceType,
                  items: AudioDeviceType.values.map((e) {
                    return DropdownMenuItem(
                      value: e,
                      child: Text(e.toString()),
                    );
                  }).toList(),
                  onChanged: (e) async {
                    audioDeviceType = e as AudioDeviceType;
                    fetchStatus = e.toString();
                    fetchAudioDevices();
                    setState(() {});
                  }),
            ]),
            //? VOLUME INFO
            Flexible(
              //VOLUME
              flex: 1,
              fit: FlexFit.tight,
              child: Center(
                child: Text(
                  "Volume:${(__volume * 100).toStringAsFixed(0)}%",
                  style: const TextStyle(fontSize: 32),
                ),
              ),
            ),
            //? VOLUMESCROLLER
            Flexible(
              //SLIDER
              flex: 1,
              fit: FlexFit.tight,
              child: Slider(
                value: __volume,
                min: 0,
                max: 1,
                divisions: 25,
                onChanged: (e) async {
                  await Audio.setVolume(e.toDouble(), audioDeviceType);
                  __volume = e;
                  setState(() {});
                },
              ),
            ),
            //? LIST AUDIO DEVICES
            Flexible(
                flex: 3,
                fit: FlexFit.loose,
                child: ListView.builder(
                    itemCount: audioDevices.length,
                    itemBuilder: (context, index) {
                      return ListTile(
                        leading: (_audioIcons.containsKey(audioDevices[index].id))
                            ? Image.memory(
                                _audioIcons[audioDevices[index].id] ?? Uint8List(0),
                                width: 32,
                                height: 32,
                              )
                            : const Icon(Icons.spoke_outlined),
                        title: Text(audioDevices[index].name),
                        trailing: IconButton(
                          icon: Icon((audioDevices[index].isActive == true ? Icons.check_box_outlined : Icons.check_box_outline_blank)),
                          onPressed: () async {
                            await Audio.setDefaultDevice(audioDevices[index].id);
                            fetchAudioDevices();
                            setState(() {});
                          },
                        ),
                      );
                    })),
            const Divider(
              thickness: 5,
              height: 10,
              color: Color.fromARGB(12, 0, 0, 0),
            ),
            //? AUDIO MIXER
            SizedBox(
              child: CheckboxListTile(
                title: const Text("Countinously Fetch Audio Mixer"),
                value: _stateFetchAudioMixerPeak,
                controlAffinity: ListTileControlAffinity.leading,
                onChanged: (e) {
                  setState(() {
                    _stateFetchAudioMixerPeak = e!;
                  });
                },
              ),
            ),
            //? LIST AUDIO MIXER
            Flexible(
              flex: 3,
              child: ListView.builder(
                  itemCount: mixerList.length,
                  itemBuilder: (context, index) {
                    return Column(
                      children: [
                        const Divider(
                          height: 6,
                          thickness: 3,
                          color: Color.fromARGB(10, 0, 0, 0),
                          indent: 100,
                          endIndent: 100,
                        ),
                        Row(
                          children: [
                            Flexible(
                                child: ListTile(
                                    leading: (_audioIcons.containsKey(mixerList[index].processPath))
                                        ? Image.memory(
                                            _audioIcons[mixerList[index].processPath] ?? Uint8List(0),
                                            width: 32,
                                            height: 32,
                                          )
                                        : const Icon(Icons.spoke_outlined),
                                    title: Text(mixerList[index].processPath))),
                            Flexible(
                              child: Padding(
                                padding: const EdgeInsets.only(right: 20),
                                child: Column(
                                  children: [
                                    Slider(
                                      value: mixerList[index].maxVolume,
                                      min: 0,
                                      max: 1,
                                      divisions: 25,
                                      onChanged: (e) async {
                                        await Audio.setAudioMixerVolume(mixerList[index].processId, e.toDouble());
                                        mixerList[index].maxVolume = e;
                                        setState(() {});
                                      },
                                    ),
                                    Padding(
                                      padding: const EdgeInsets.symmetric(horizontal: 25),
                                      child: FAProgressBar(
                                        currentValue: mixerList[index].peakVolume * mixerList[index].maxVolume * 100,
                                        size: 12,
                                        maxValue: 100,
                                        changeColorValue: 100,
                                        changeProgressColor: Colors.pink,
                                        progressColor: Colors.lightBlue,
                                        animatedDuration: const Duration(milliseconds: 300),
                                        direction: Axis.horizontal,
                                        verticalDirection: VerticalDirection.up,
                                        formatValueFixed: 2,
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                            ),
                          ],
                        ),
                      ],
                    );
                  }),
            )
          ],
        ),
      ),
    );
  }
}

#pragma once

#include "common.h"

namespace win32audio {

class AudioManager {
public:
    static std::vector<DeviceProps> EnumAudioDevices(EDataFlow deviceType, ERole eRole);
    static DeviceProps GetDefaultDevice(EDataFlow deviceType, ERole eRole);
    static HRESULT SetDefaultDevice(LPWSTR devID, bool console, bool multimedia, bool communications);
    static float GetVolume(EDataFlow deviceType, ERole eRole);
    static bool SetVolume(float volumeLevel, EDataFlow deviceType, ERole eRole);
    static bool SetAudioDeviceVolume(float volumeLevel, LPWSTR devID);
    static int SwitchDefaultDevice(EDataFlow deviceType, ERole role, bool console, bool multimedia, bool communications);
    
    // Session API
    static std::vector<ProcessVolume> GetProcessVolumes(ERole eRole, int pID = 0, float volume = 0.0);
    static bool SetProcessVolumeByPath(ERole eRole, std::string path, float volume);

    // Sample Rate API
    static int GetSampleRate(LPWSTR devID);
    static bool SetSampleRate(LPWSTR devID, int sampleRate);

private:
    static ComPtr<IAudioSessionEnumerator> GetAudioSessionEnumerator(ERole eRole);
    static float GetSetProcessMasterVolume(IAudioSessionControl *session, float volume = 0.0);
    static float GetPeakVolume(IAudioSessionControl *session);
    static std::string GetProcessNameFromPid(DWORD pid);
    static HRESULT GetDeviceProperty(IMMDevice *pDevice, DeviceProps *pOutput);
};

} // namespace win32audio

#include "audio_manager.h"
#include "include/Policyconfig.h"
#include "include/encoding.h"

#include <psapi.h>
#include <propvarutil.h>
#include <functiondiscoverykeys_devpkey.h>
#include <map>

// For CLSID_MMDeviceEnumerator
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioEndpointVolume = __uuidof(IAudioEndpointVolume);

#pragma comment(lib, "propsys")

// Define PKEY_AudioEngine_DeviceFormat manually if not found by linker
#ifndef PKEY_AudioEngine_DeviceFormat
EXTERN_C const PROPERTYKEY DECLSPEC_SELECTANY PKEY_AudioEngine_DeviceFormat = 
    { { 0xf19f064d, 0x82c, 0x4e27, { 0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c } }, 0 };
#endif

namespace win32audio {

// Removed conflicting enum definition. Using system defined AudioSessionState.

HRESULT AudioManager::GetDeviceProperty(IMMDevice *pDevice, DeviceProps *pOutput) {
    if (pDevice == nullptr || pOutput == nullptr) {
        return E_POINTER;
    }

    ComPtr<IPropertyStore> pPropertyStore;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
    if (FAILED(hr)) {
        return hr;
    }

    PROPVARIANT varName = {0};
    PropVariantInit(&varName);
    hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &varName);
    if (SUCCEEDED(hr)) {
        if (IsPropVariantString(varName)) {
            STRRET strret;
            if (SUCCEEDED(PropVariantToStrRet(varName, &strret)) && strret.pOleStr) {
                 pOutput->name = strret.pOleStr;
                 CoTaskMemFree(strret.pOleStr);
            } else {
                hr = E_UNEXPECTED;
            }
        } else {
            hr = E_UNEXPECTED;
        }
        PropVariantClear(&varName);
    }

    PROPVARIANT varIconPath = {0};
    PropVariantInit(&varIconPath);
    hr = pPropertyStore->GetValue(PKEY_DeviceClass_IconPath, &varIconPath);
    if (SUCCEEDED(hr)) {
        if (IsPropVariantString(varIconPath)) {
            STRRET strret;
            if (SUCCEEDED(PropVariantToStrRet(varIconPath, &strret)) && strret.pOleStr) {
                pOutput->iconInfo = strret.pOleStr;
                CoTaskMemFree(strret.pOleStr);
            } else {
                pOutput->iconInfo = L"missing,0";
                hr = E_UNEXPECTED;
            }
        } else {
            pOutput->iconInfo = L"missing,0";
            hr = E_UNEXPECTED;
        }
        PropVariantClear(&varIconPath);
    }

    return hr;
}

std::vector<DeviceProps> AudioManager::EnumAudioDevices(EDataFlow deviceType, ERole eRole) {
    std::vector<DeviceProps> output;
    ScopedCoInitialize com;
    if (!com.succeeded()) {
        OutputDebugString(L"Failed to initialize COM\n");
        return output;
    }

    ComPtr<IMMDeviceEnumerator> pEnumerator;
    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr) || !pEnumerator) {
        return output;
    }

    ComPtr<IMMDevice> pActive;
    std::wstring activeDevID;

    hr = pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
    if (SUCCEEDED(hr) && pActive) {
        LPWSTR activeID = nullptr;
        hr = pActive->GetId(&activeID);
        if (SUCCEEDED(hr) && activeID) {
            activeDevID = activeID;
            CoTaskMemFree(activeID);
        }
    }

    ComPtr<IMMDeviceCollection> pCollection;
    hr = pEnumerator->EnumAudioEndpoints(deviceType, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr) || !pCollection) {
        return output;
    }

    UINT cEndpoints = 0;
    hr = pCollection->GetCount(&cEndpoints);
    if (FAILED(hr)) {
        return output;
    }

    for (UINT n = 0; n < cEndpoints; ++n) {
        ComPtr<IMMDevice> pDevice;
        hr = pCollection->Item(n, &pDevice);
        if (FAILED(hr) || !pDevice) {
            continue;
        }

        DeviceProps device;
        if (SUCCEEDED(GetDeviceProperty(pDevice.Get(), &device))) {
            LPWSTR id = nullptr;
            hr = pDevice->GetId(&id);
            if (SUCCEEDED(hr) && id) {
                std::wstring currentID(id);
                device.id = currentID;
                device.isActive = (currentID == activeDevID);
                CoTaskMemFree(id);
            }
            output.push_back(device);
        }
    }

    return output;
}

DeviceProps AudioManager::GetDefaultDevice(EDataFlow deviceType, ERole eRole) {
    DeviceProps activeDevice;
    ScopedCoInitialize com;
    if (com.succeeded()) {
        ComPtr<IMMDeviceEnumerator> pEnumerator;
        HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
        if (SUCCEEDED(hr) && pEnumerator) {
            ComPtr<IMMDevice> pActive;
            hr = pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
            if (SUCCEEDED(hr) && pActive) {
                GetDeviceProperty(pActive.Get(), &activeDevice);
                LPWSTR aid;
                hr = pActive->GetId(&aid);
                if (SUCCEEDED(hr) && aid != nullptr) {
                    activeDevice.id = aid;
                    CoTaskMemFree(aid);
                }
            }
        }
    }
    return activeDevice;
}

HRESULT AudioManager::SetDefaultDevice(LPWSTR devID, bool console, bool multimedia, bool communications) {
    ComPtr<IPolicyConfigVista> pPolicyConfig;
    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient),
                                  NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID *)&pPolicyConfig);
    if (SUCCEEDED(hr) && pPolicyConfig) {
        if (console)
            hr = pPolicyConfig->SetDefaultEndpoint(devID, eConsole);
        if (multimedia)
            hr = pPolicyConfig->SetDefaultEndpoint(devID, eMultimedia);
        if (communications)
            hr = pPolicyConfig->SetDefaultEndpoint(devID, eCommunications);
    }
    return hr;
}

float AudioManager::GetVolume(EDataFlow deviceType, ERole eRole) {
    float volumeLevel = 0.0f;
    ScopedCoInitialize com;
    if (com.succeeded()) {
        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator);
        if (SUCCEEDED(hr) && enumerator) {
            ComPtr<IMMDevice> device;
            hr = enumerator->GetDefaultAudioEndpoint(deviceType, eRole, &device);
            if (SUCCEEDED(hr) && device) {
                ComPtr<IAudioEndpointVolume> volumeControl;
                hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, (void**)&volumeControl);
                if (SUCCEEDED(hr) && volumeControl) {
                    hr = volumeControl->GetMasterVolumeLevelScalar(&volumeLevel);
                }
            }
        }
    }
    return volumeLevel;
}

bool AudioManager::SetVolume(float volumeLevel, EDataFlow deviceType, ERole eRole) {
    ScopedCoInitialize com;
    if (com.succeeded()) {
        ComPtr<IMMDeviceEnumerator> pEnumerator;
        HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
        if (SUCCEEDED(hr) && pEnumerator) {
            ComPtr<IMMDevice> pActive;
            pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
            if (pActive) {
                ComPtr<IAudioEndpointVolume> m_spVolumeControl;
                hr = pActive->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (void **)&m_spVolumeControl);
                if (SUCCEEDED(hr) && m_spVolumeControl) {
                    if (volumeLevel > 1.0f)
                        volumeLevel = volumeLevel / 100.0f;
                    m_spVolumeControl->SetMasterVolumeLevelScalar(volumeLevel, NULL);
                }
            }
        }
    }
    return true;
}

int AudioManager::SwitchDefaultDevice(EDataFlow deviceType, ERole role, bool console, bool multimedia, bool communications) {
    std::vector<DeviceProps> devices = EnumAudioDevices(deviceType, role);
    if (devices.empty()) {
        return 0; // No devices found
    }

    std::wstring activeDeviceId;
    for (const auto &device : devices) {
        if (!activeDeviceId.empty()) {
            activeDeviceId = device.id;
            break;
        }
        if (device.isActive) {
            activeDeviceId = L"x";
        }
    }

    if (activeDeviceId.empty() || activeDeviceId == L"x") {
        activeDeviceId = devices[0].id;
    }

    try {
        SetDefaultDevice((LPWSTR)activeDeviceId.c_str(), console, multimedia, communications);
        return 1; // Success
    } catch (const std::exception &) {
        return 0; // Failed
    }
}

bool AudioManager::SetAudioDeviceVolume(float volumeLevel, LPWSTR devID) {
    ScopedCoInitialize com;
    if (!com.succeeded()) return false;

    ComPtr<IMMDeviceEnumerator> pEnumerator;
    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) return false;

    ComPtr<IMMDevice> pDevice;
    hr = pEnumerator->GetDevice(devID, &pDevice);
    if (FAILED(hr)) return false;

    ComPtr<IAudioEndpointVolume> pAudioVolume;
    hr = pDevice->Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&pAudioVolume);
    if (FAILED(hr)) return false;

    hr = pAudioVolume->SetMasterVolumeLevelScalar(volumeLevel, NULL);
    return SUCCEEDED(hr);
}

// Session API implementations

ComPtr<IAudioSessionEnumerator> AudioManager::GetAudioSessionEnumerator(ERole eRole) {
    ComPtr<IMMDeviceEnumerator> deviceEnumerator;
    HRESULT x = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
    if (FAILED(x) || !deviceEnumerator) return nullptr;

    ComPtr<IMMDevice> device;
    deviceEnumerator->GetDefaultAudioEndpoint(eRender, eRole, &device);
    if (!device) return nullptr;

    ComPtr<IAudioSessionManager2> sessionManager;
    device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void **)&sessionManager);
    if (!sessionManager) return nullptr;

    ComPtr<IAudioSessionEnumerator> enumerator;
    sessionManager->GetSessionEnumerator(&enumerator);

    return enumerator;
}

std::string AudioManager::GetProcessNameFromPid(DWORD pid) {
    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (handle) {
        TCHAR buffer[MAX_PATH];
        if (GetModuleFileNameEx(handle, NULL, buffer, sizeof(buffer))) {
            CloseHandle(handle);
            std::wstring test(&buffer[0]);
            return Encoding::WideToUtf8(test);
        }
        CloseHandle(handle);
    }
    return "";
}

float AudioManager::GetSetProcessMasterVolume(IAudioSessionControl *session, float volume) {
    ComPtr<ISimpleAudioVolume> info;
    session->QueryInterface(__uuidof(ISimpleAudioVolume), (void **)&info);
    if (!info) return 0.0f;

    if (volume != 0.00) {
        info->SetMasterVolume(volume, NULL);
    }
    float maxVolume = 0.0f;
    info->GetMasterVolume(&maxVolume);
    return maxVolume;
}

float AudioManager::GetPeakVolume(IAudioSessionControl *session) {
    if (!session) return 0.0f;
    ComPtr<IAudioMeterInformation> info;
    session->QueryInterface(__uuidof(IAudioMeterInformation), (void **)&info);
    if (!info) return 0.0f;

    float peakVolume = 0.0f;
    info->GetPeakValue(&peakVolume);
    return peakVolume;
}

std::vector<ProcessVolume> AudioManager::GetProcessVolumes(ERole eRole, int pID, float volume) {
    std::vector<ProcessVolume> volumes;
    ScopedCoInitialize com;
    if (com.succeeded()) {
        ComPtr<IAudioSessionEnumerator> enumerator = GetAudioSessionEnumerator(eRole);
        if (!enumerator) return volumes;

        int sessionCount = 0;
        enumerator->GetCount(&sessionCount);

        std::map<DWORD, std::pair<ProcessVolume, int>> sessionMap;

        for (int index = 0; index < sessionCount; index++) {
            ComPtr<IAudioSessionControl> session;
            ComPtr<IAudioSessionControl2> session2;
            enumerator->GetSession(index, &session);
            if (!session) continue;
            
            session->QueryInterface(__uuidof(IAudioSessionControl2), (void **)&session2);
            if (!session2) continue;

            DWORD id = 0;
            if (FAILED(session2->GetProcessId(&id))) continue;
            
            if (id == 0) continue;

            // Use system defined AudioSessionState
            AudioSessionState state = AudioSessionStateInactive; 
            if (FAILED(session->GetState(&state))) state = AudioSessionStateInactive;
            if (state == AudioSessionStateExpired) continue;

            if (pID == (int)id && volume != 0.00) {
                GetSetProcessMasterVolume(session.Get(), volume);
                continue; 
            }

            if (pID != 0 && volume != 0.00) continue;

            std::string processPath = GetProcessNameFromPid(id);
            float maxVolume = GetSetProcessMasterVolume(session.Get());
            float peakVolume = GetPeakVolume(session.Get());

            ProcessVolume data;
            data.processPath = processPath;
            data.processId = (int)id;
            data.maxVolume = maxVolume;
            data.peakVolume = peakVolume;

            if (sessionMap.find(id) == sessionMap.end()) {
                sessionMap[id] = {data, (int)state};
            } else {
                auto& existingPair = sessionMap[id];
                bool currentActive = (state == AudioSessionStateActive);
                bool existingActive = (existingPair.second == (int)AudioSessionStateActive);

                if (currentActive && !existingActive) {
                    existingPair = {data, (int)state};
                } else if (currentActive == existingActive) {
                    if (data.peakVolume > existingPair.first.peakVolume) {
                         existingPair = {data, (int)state};
                    }
                }
            }
        }

        if (pID != 0 && volume != 0.00) {
           return std::vector<ProcessVolume>{};
        }

        for (auto const& [pid, val] : sessionMap) {
            volumes.push_back(val.first);
        }
        
        return volumes;
    }
    return std::vector<ProcessVolume>{};
}

bool AudioManager::SetProcessVolumeByPath(ERole eRole, std::string path, float volume) {
    ScopedCoInitialize com;
    if (com.succeeded()) {
        ComPtr<IAudioSessionEnumerator> enumerator = GetAudioSessionEnumerator(eRole);
        if (!enumerator) return false;

        int sessionCount = 0;
        enumerator->GetCount(&sessionCount);
        for (int index = 0; index < sessionCount; index++) {
            ComPtr<IAudioSessionControl> session;
            ComPtr<IAudioSessionControl2> session2;
            enumerator->GetSession(index, &session);
            if (!session) continue;
            
            session->QueryInterface(__uuidof(IAudioSessionControl2), (void **)&session2);
            if (!session2) continue;

            DWORD id = 0;
            session2->GetProcessId(&id);
            std::string processPath = "";
            if ((int)id != 0)
                processPath = GetProcessNameFromPid(id);
            else {
                continue;
            }
            if (path == processPath) {
                GetSetProcessMasterVolume(session.Get(), volume);
                break;
            }
        }
    }
    return true;
}

int AudioManager::GetSampleRate(LPWSTR devID) {
    ScopedCoInitialize com;
    if (!com.succeeded()) return 0;

    ComPtr<IMMDeviceEnumerator> pEnumerator;
    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) return 0;

    ComPtr<IMMDevice> pDevice;
    hr = pEnumerator->GetDevice(devID, &pDevice);
    if (FAILED(hr)) return 0;

    ComPtr<IAudioClient> pAudioClient;
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) return 0;

    WAVEFORMATEX* pwfx = nullptr;
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr) || !pwfx) return 0;

    int sampleRate = pwfx->nSamplesPerSec;
    CoTaskMemFree(pwfx);
    
    return sampleRate;
}

bool AudioManager::SetSampleRate(LPWSTR devID, int sampleRate) {
    ScopedCoInitialize com;
    if (!com.succeeded()) return false;

    ComPtr<IMMDeviceEnumerator> pEnumerator;
    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) return false;

    ComPtr<IMMDevice> pDevice;
    hr = pEnumerator->GetDevice(devID, &pDevice);
    if (FAILED(hr)) return false;

    ComPtr<IPropertyStore> pPropertyStore;
    hr = pDevice->OpenPropertyStore(STGM_READWRITE, &pPropertyStore);
    if (FAILED(hr)) return false;

    // Get current format
    PROPVARIANT varFormat;
    PropVariantInit(&varFormat);
    hr = pPropertyStore->GetValue(PKEY_AudioEngine_DeviceFormat, &varFormat);
    if (FAILED(hr)) {
        PropVariantClear(&varFormat);
        return false;
    }

    if (varFormat.vt != VT_BLOB || !varFormat.blob.pBlobData) {
        PropVariantClear(&varFormat);
        return false;
    }

    // Copy the format and modify sample rate
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)varFormat.blob.pBlobData;
    
    // Create a new format with the desired sample rate
    std::vector<BYTE> formatBuffer(sizeof(WAVEFORMATEXTENSIBLE));
    WAVEFORMATEXTENSIBLE* pWfxEx = (WAVEFORMATEXTENSIBLE*)formatBuffer.data();
    
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        memcpy(pWfxEx, pwfx, sizeof(WAVEFORMATEXTENSIBLE));
    } else {
        // Convert to extensible format
        pWfxEx->Format = *pwfx;
        pWfxEx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        pWfxEx->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        pWfxEx->Samples.wValidBitsPerSample = pwfx->wBitsPerSample;
        pWfxEx->dwChannelMask = pwfx->nChannels == 2 ? 3 : 0; // Stereo or mono
        pWfxEx->SubFormat = pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ? 
            KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
    }

    // Update sample rate and dependent fields
    pWfxEx->Format.nSamplesPerSec = sampleRate;
    pWfxEx->Format.nAvgBytesPerSec = sampleRate * pWfxEx->Format.nBlockAlign;

    PropVariantClear(&varFormat);

    // Set the new format
    PROPVARIANT varNewFormat;
    PropVariantInit(&varNewFormat);
    varNewFormat.vt = VT_BLOB;
    varNewFormat.blob.cbSize = sizeof(WAVEFORMATEXTENSIBLE);
    varNewFormat.blob.pBlobData = (BYTE*)pWfxEx;

    hr = pPropertyStore->SetValue(PKEY_AudioEngine_DeviceFormat, varNewFormat);
    if (FAILED(hr)) {
        return false;
    }

    hr = pPropertyStore->Commit();
    return SUCCEEDED(hr);
}

} // namespace win32audio

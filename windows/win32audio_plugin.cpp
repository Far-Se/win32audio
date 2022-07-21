#include "win32audio_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

#include <ole2.h>
#include <ShellAPI.h>
#include <olectl.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <propvarutil.h>
#include <stdio.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <string>
#include <vector>
#include "include/Policyconfig.h"
#include "include/encoding.h"
//#include "hicon_to_bytes.cpp"

#pragma warning(push)
#pragma warning(disable : 4201)
#include "hicon_to_bytes.cpp"
// #pragma warning(disable: 4201)
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#pragma warning(pop)

#pragma comment(lib, "ole32")
#pragma comment(lib, "propsys")
using namespace std;

// #pragma warning(disable: 4244)

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <sstream>

#include <endpointvolume.h>
#include <audiopolicy.h>
#include <psapi.h>

struct ProcessVolume
{
    int processId = 0;
    std::string processPath = "";
    float maxVolume = 1.0;
    float peakVolume = 0.0;
};

struct DeviceProps
{
    wstring id;
    wstring name;
    wstring iconInfo;
    bool isActive;
};

static HRESULT getDeviceProperty(IMMDevice *pDevice, DeviceProps *output)
{
    IPropertyStore *pStore = NULL;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pStore);
    if (SUCCEEDED(hr))
    {
        PROPVARIANT prop;
        PropVariantInit(&prop);
        hr = pStore->GetValue(PKEY_Device_FriendlyName, &prop);
        if (SUCCEEDED(hr))
        {
            if (IsPropVariantString(prop))
            {
                // 3h of debugging wchar to char conversion just to find out
                // this dumb function does not work propertly :)
                // output->name = PropVariantToStringWithDefault(prop, L"missing"); <- 3h of debugging

                STRRET strret;
                PropVariantToStrRet(prop, &strret);
                output->name = strret.pOleStr;
            }
            else
                hr = E_UNEXPECTED;
        }
        PROPVARIANT prop2;
        PropVariantInit(&prop2);
        hr = pStore->GetValue(PKEY_DeviceClass_IconPath, &prop2);
        if (SUCCEEDED(hr))
        {
            if (IsPropVariantString(prop2))
            {
                STRRET strret;
                PropVariantToStrRet(prop2, &strret);
                output->iconInfo = strret.pOleStr;
            }

            else
            {
                output->iconInfo = L"missing,0";
                hr = E_UNEXPECTED;
            }
        }
        PropVariantClear(&prop);
        PropVariantClear(&prop2);
        pStore->Release();
    }
    // delete pStore;
    return hr;
}

std::vector<DeviceProps> EnumAudioDevices(EDataFlow deviceType = eRender)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
        if (SUCCEEDED(hr))
        {
            IMMDevice *pActive = NULL;

            pEnumerator->GetDefaultAudioEndpoint(deviceType, eMultimedia, &pActive);
            LPWSTR activeID;
            pActive->GetId(&activeID);
            wstring activeDevID(activeID);

            pActive->Release();

            IMMDeviceCollection *pCollection = NULL;
            hr = pEnumerator->EnumAudioEndpoints(deviceType, DEVICE_STATE_ACTIVE, &pCollection);
            if (SUCCEEDED(hr))
            {
                UINT cEndpoints = 0;
                hr = pCollection->GetCount(&cEndpoints);
                if (SUCCEEDED(hr))
                {
                    for (UINT n = 0; SUCCEEDED(hr) && n < cEndpoints; ++n)
                    {
                        IMMDevice *pDevice = NULL;
                        hr = pCollection->Item(n, &pDevice);
                        if (SUCCEEDED(hr))
                        {
                            DeviceProps device;
                            getDeviceProperty(pDevice, &device);

                            LPWSTR id;
                            pDevice->GetId(&id);
                            wstring currentID(id);
                            device.id = currentID;

                            if (currentID.compare(activeDevID) == 0)
                                device.isActive = true;
                            else
                                device.isActive = false;
                            output.push_back(device);
                            pDevice->Release();
                        }
                    }
                }
                pCollection->Release();
            }
            pEnumerator->Release();
        }
    }
    return output;
}

DeviceProps getDefaultDevice(EDataFlow deviceType = eRender)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
        if (SUCCEEDED(hr))
        {
            IMMDevice *pActive = NULL;

            pEnumerator->GetDefaultAudioEndpoint(deviceType, eMultimedia, &pActive);
            DeviceProps activeDevice;
            getDeviceProperty(pActive, &activeDevice);
            LPWSTR aid;
            pActive->GetId(&aid);
            activeDevice.id = aid;

            return activeDevice;
        }
    }
    return DeviceProps();
}

static HRESULT setDefaultDevice(LPWSTR devID)
{
    IPolicyConfigVista *pPolicyConfig = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient),
                                  NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID *)&pPolicyConfig);
    if (SUCCEEDED(hr))
    {
        hr = pPolicyConfig->SetDefaultEndpoint(devID, eConsole);
        hr = pPolicyConfig->SetDefaultEndpoint(devID, eMultimedia);
        hr = pPolicyConfig->SetDefaultEndpoint(devID, eCommunications);
        pPolicyConfig->Release();
    }
    // delete pPolicyConfig;
    return hr;
}

float getVolume(EDataFlow deviceType = eRender)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
        if (SUCCEEDED(hr))
        {
            IMMDevice *pActive = NULL;

            pEnumerator->GetDefaultAudioEndpoint(deviceType, eMultimedia, &pActive);
            DeviceProps activeDevice;
            getDeviceProperty(pActive, &activeDevice);
            LPWSTR aid;
            pActive->GetId(&aid);
            activeDevice.id = aid;

            IAudioEndpointVolume *m_spVolumeControl = NULL;
            hr = pActive->Activate(__uuidof(m_spVolumeControl), CLSCTX_INPROC_SERVER, NULL, (void **)&m_spVolumeControl);
            if (SUCCEEDED(hr))
            {
                float volumeLevel = 0.0;
                m_spVolumeControl->GetMasterVolumeLevelScalar(&volumeLevel);

                m_spVolumeControl->Release();
                pActive->Release();
                return volumeLevel;
            }
        }
    }
    return 0.0;
}
bool registerNotificationCallback(EDataFlow deviceType = eRender)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
        if (SUCCEEDED(hr))
        {
            IMMDevice *pActive = NULL;

            pEnumerator->GetDefaultAudioEndpoint(deviceType, eMultimedia, &pActive);
            IMMNotificationClient *pNotify = NULL;
            pEnumerator->RegisterEndpointNotificationCallback(pNotify);
        }
    }
    return 0.0;
}
bool setVolume(float volumeLevel, EDataFlow deviceType = eRender)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
        if (SUCCEEDED(hr))
        {
            IMMDevice *pActive = NULL;

            pEnumerator->GetDefaultAudioEndpoint(deviceType, eMultimedia, &pActive);
            DeviceProps activeDevice;
            getDeviceProperty(pActive, &activeDevice);
            LPWSTR aid;
            pActive->GetId(&aid);
            activeDevice.id = aid;

            IAudioEndpointVolume *m_spVolumeControl = NULL;
            hr = pActive->Activate(__uuidof(m_spVolumeControl), CLSCTX_INPROC_SERVER, NULL, (void **)&m_spVolumeControl);
            if (SUCCEEDED(hr))
            {
                if (volumeLevel > 1)
                    volumeLevel = volumeLevel / 100;
                m_spVolumeControl->SetMasterVolumeLevelScalar((float)volumeLevel, NULL);
                m_spVolumeControl->Release();
                pActive->Release();
            }
        }
    }
    return true;
}
static int switchDefaultDevice(EDataFlow deviceType = eRender)
{
    std::vector<DeviceProps> result = EnumAudioDevices(deviceType);
    if (!result.empty())
    {
        std::wstring activateID(L"");
        for (const auto &device : result)
        {
            if (activateID == L"x")
            {
                activateID = device.id;
                break;
            }
            if (device.isActive)
                activateID = L"x";
        }
        if (activateID == L"x" || activateID == L"")
            activateID = result[0].id;
        setDefaultDevice((LPWSTR)activateID.c_str());
        return 1;
    }
    return 0;
}

/// Audio Session

IAudioSessionEnumerator *GetAudioSessionEnumerator()
{
    IMMDeviceEnumerator *deviceEnumerator = nullptr;
    HRESULT x = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);
    if (x)
    {
    }
    IMMDevice *device = nullptr;
    deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);

    IAudioSessionManager2 *sessionManager = nullptr;
    device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void **)&sessionManager);

    IAudioSessionEnumerator *enumerator = nullptr;
    sessionManager->GetSessionEnumerator(&enumerator);

    deviceEnumerator->Release();
    device->Release();
    sessionManager->Release();

    return enumerator;
}

std::string GetProcessNameFromPid(DWORD pid)
{
    std::string name;

    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (handle)
    {
        TCHAR buffer[MAX_PATH];
        if (GetModuleFileNameEx(handle, NULL, buffer, sizeof(buffer)))
        {
            CloseHandle(handle);
            wstring test(&buffer[0]); // convert to wstring
            std::string buff(Encoding::WideToUtf8(test));
            return buff;
        }
    }
    CloseHandle(handle);

    return std::move(name);
}

float getSetProcessMasterVolume(IAudioSessionControl *session, float volume = 0.0)
{
    ISimpleAudioVolume *info = nullptr;
    session->QueryInterface(__uuidof(ISimpleAudioVolume), (void **)&info);
    if (volume != 0.00)
    {
        info->SetMasterVolume(volume, NULL);
    }
    float maxVolume;
    info->GetMasterVolume(&maxVolume);
    info->Release();
    info = nullptr;

    return maxVolume;
}

float GetPeakVolumeFromAudioSessionControl(IAudioSessionControl *session)
{
    IAudioMeterInformation *info = nullptr;
    session->QueryInterface(__uuidof(IAudioMeterInformation), (void **)&info);

    float peakVolume;
    info->GetPeakValue(&peakVolume);

    info->Release();
    info = nullptr;

    return peakVolume;
}
std::vector<ProcessVolume> GetProcessVolumes(int pID = 0, float volume = 0.0)
{
    std::vector<ProcessVolume> volumes;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (hr)
    {
        IAudioSessionEnumerator *enumerator = GetAudioSessionEnumerator();
        int sessionCount;
        enumerator->GetCount(&sessionCount);
        for (int index = 0; index < sessionCount; index++)
        {
            IAudioSessionControl *session = nullptr;
            IAudioSessionControl2 *session2 = nullptr;
            enumerator->GetSession(index, &session);
            session->QueryInterface(__uuidof(IAudioSessionControl2), (void **)&session2);

            DWORD id = 0;
            session2->GetProcessId(&id);
            std::string processPath = "";
            if ((int)id != 0)
                processPath = GetProcessNameFromPid(id);
            else
            {
                session2->Release();
                session->Release();
                continue;
            }
            if (pID == (int)id && volume != 0.00)
            {
                getSetProcessMasterVolume(session, volume);
                session2->Release();
                session->Release();
                break;
            }
            float maxVolume = getSetProcessMasterVolume(session);
            float peakVolume = GetPeakVolumeFromAudioSessionControl(session);

            ProcessVolume data;
            data.processPath = processPath;
            data.processId = (int)id;
            data.maxVolume = maxVolume;
            data.peakVolume = peakVolume;

            volumes.push_back(data);
            session2->Release();
            session->Release();
        }
        enumerator->Release();
        if (volume != 0.00)
        {
            return std::vector<ProcessVolume>{};
        }
        return volumes;
    }
    return std::vector<ProcessVolume>{};
}

namespace win32audio
{

    // static
    void Win32audioPlugin::RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar)
    {
        auto channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(registrar->messenger(), "win32audio", &flutter::StandardMethodCodec::GetInstance());
        auto plugin = std::make_unique<Win32audioPlugin>();
        channel->SetMethodCallHandler([plugin_pointer = plugin.get()](const auto &call, auto result)
                                      { plugin_pointer->HandleMethodCall(call, std::move(result)); });

        registrar->AddPlugin(std::move(plugin));
    }

    Win32audioPlugin::Win32audioPlugin() {}

    Win32audioPlugin::~Win32audioPlugin() {}

    void Win32audioPlugin::HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue> &method_call, std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
    {
        if (method_call.method_name().compare("enumAudioDevices") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            std::vector<DeviceProps> devices = EnumAudioDevices((EDataFlow)deviceType);
            // loop through devices and add them to a map
            flutter::EncodableMap map;
            for (const auto &device : devices)
            {
                flutter::EncodableMap deviceMap;
                deviceMap[flutter::EncodableValue("id")] = flutter::EncodableValue(Encoding::WideToUtf8(device.id));
                deviceMap[flutter::EncodableValue("name")] = flutter::EncodableValue(Encoding::WideToUtf8(device.name));
                deviceMap[flutter::EncodableValue("iconInfo")] = flutter::EncodableValue(Encoding::WideToUtf8(device.iconInfo));
                deviceMap[flutter::EncodableValue("isActive")] = flutter::EncodableValue(device.isActive);
                map[flutter::EncodableValue(Encoding::WideToUtf8(device.id))] = flutter::EncodableValue(deviceMap);
            }
            result->Success(flutter::EncodableValue(map));
        }
        else if (method_call.method_name().compare("getDefaultDevice") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            DeviceProps device = getDefaultDevice((EDataFlow)deviceType);

            flutter::EncodableMap deviceMap;
            deviceMap[flutter::EncodableValue("id")] = flutter::EncodableValue(Encoding::WideToUtf8(device.id));
            deviceMap[flutter::EncodableValue("name")] = flutter::EncodableValue(Encoding::WideToUtf8(device.name));
            deviceMap[flutter::EncodableValue("iconInfo")] = flutter::EncodableValue(Encoding::WideToUtf8(device.iconInfo));
            deviceMap[flutter::EncodableValue("isActive")] = flutter::EncodableValue(device.isActive);
            result->Success(flutter::EncodableValue(deviceMap));
        }
        else if (method_call.method_name().compare("setDefaultAudioDevice") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            std::string deviceID = std::get<std::string>(args.at(flutter::EncodableValue("deviceID")));
            std::wstring deviceIDW = Encoding::Utf8ToWide(deviceID);
            HRESULT nativeFuncResult = setDefaultDevice((LPWSTR)deviceIDW.c_str());
            result->Success(flutter::EncodableValue((int)nativeFuncResult));
        }
        else if (method_call.method_name().compare("getAudioVolume") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            float nativeFuncResult = getVolume((EDataFlow)deviceType);
            result->Success(flutter::EncodableValue((double)nativeFuncResult));
        }
        else if (method_call.method_name().compare("setAudioVolume") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            setVolume((float)volumeLevel, (EDataFlow)deviceType);
            result->Success(flutter::EncodableValue((int)1));
        }
        else if (method_call.method_name().compare("switchDefaultDevice") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int nativeFuncResult = switchDefaultDevice((EDataFlow)deviceType);
            result->Success(flutter::EncodableValue((double)nativeFuncResult));
        }
        else if (method_call.method_name().compare("iconToBytes") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            std::string iconLocation = std::get<std::string>(args.at(flutter::EncodableValue("iconLocation")));
            int iconID = std::get<int>(args.at(flutter::EncodableValue("iconID")));
            std::wstring iconLocationW = Encoding::Utf8ToWide(iconLocation);
            HICON icon = getIconFromFile((LPWSTR)iconLocationW.c_str(), iconID);

            std::vector<CHAR> buff;
            bool resultIcon = GetIconData(icon, 32, buff);
            if (!resultIcon)
            {
                buff.clear();
                resultIcon = GetIconData(icon, 24, buff);
            }
            std::vector<uint8_t> buff_uint8;
            for (auto i : buff)
            {
                buff_uint8.push_back(i);
            }
            result->Success(flutter::EncodableValue(buff_uint8));
        }
        else if (method_call.method_name().compare("getWindowIcon") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int hWND = std::get<int>(args.at(flutter::EncodableValue("hWnd")));

            LRESULT iconResult = SendMessage((HWND)((LONG_PTR)hWND), WM_GETICON, 2, 0); // ICON_SMALL2 - User Made Apps
            if (iconResult == 0)
                iconResult = GetClassLongPtr((HWND)((LONG_PTR)hWND), -14); // GCLP_HICON - Microsoft Win Apps
            if (iconResult != 0)
            {

                HICON icon = (HICON)iconResult;
                std::vector<CHAR> buff;
                bool resultIcon = GetIconData(icon, 32, buff);
                if (!resultIcon)
                {
                    buff.clear();
                    resultIcon = GetIconData(icon, 24, buff);
                }
                if (resultIcon)
                {
                    std::vector<uint8_t> buff_uint8;
                    for (auto i : buff)
                    {
                        buff_uint8.push_back(i);
                    }
                    result->Success(flutter::EncodableValue(buff_uint8));
                }
                else
                {
                    std::vector<uint8_t> iconBytes;
                    iconBytes.push_back(204);
                    iconBytes.push_back(204);
                    iconBytes.push_back(204);
                    result->Success(flutter::EncodableValue(iconBytes));
                }
            }
            else
            {

                std::vector<uint8_t> iconBytes;
                iconBytes.push_back(204);
                iconBytes.push_back(204);
                iconBytes.push_back(204);
                result->Success(flutter::EncodableValue(iconBytes));
            }
        }
        else if (method_call.method_name().compare("getIconPng") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int hIcon = std::get<int>(args.at(flutter::EncodableValue("hIcon")));
            std::vector<CHAR> buff;
            bool resultIcon = GetIconData((HICON)((LONG_PTR)hIcon), 32, buff);
            if (!resultIcon)
            {
                buff.clear();
                resultIcon = GetIconData((HICON)((LONG_PTR)hIcon), 24, buff);
            }
            if (resultIcon)
            {
                std::vector<uint8_t> buff_uint8;
                for (auto i : buff)
                {
                    buff_uint8.push_back(i);
                }
                result->Success(flutter::EncodableValue(buff_uint8));
            }
            else
            {
                std::vector<uint8_t> iconBytes;
                iconBytes.push_back(204);
                iconBytes.push_back(204);
                iconBytes.push_back(204);
                result->Success(flutter::EncodableValue(iconBytes));
            }
        }
        else if (method_call.method_name().compare("enumAudioMixer") == 0)
        {
            std::vector<ProcessVolume> devices = GetProcessVolumes();
            // loop through devices and add them to a map
            flutter::EncodableMap map;
            for (const auto &device : devices)
            {
                flutter::EncodableMap deviceMap;
                deviceMap[flutter::EncodableValue("processId")] = flutter::EncodableValue(device.processId);
                deviceMap[flutter::EncodableValue("processPath")] = flutter::EncodableValue(device.processPath);
                deviceMap[flutter::EncodableValue("maxVolume")] = flutter::EncodableValue(device.maxVolume);
                deviceMap[flutter::EncodableValue("peakVolume")] = flutter::EncodableValue(device.peakVolume);
                map[flutter::EncodableValue(device.processId)] = flutter::EncodableValue(deviceMap);
            }
            result->Success(flutter::EncodableValue(map));
        }
        else if (method_call.method_name().compare("setAudioMixerVolume") == 0)
        {

            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int processID = std::get<int>(args.at(flutter::EncodableValue("processID")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            std::vector<ProcessVolume> devices = GetProcessVolumes(processID, (float)volumeLevel);
            result->Success(flutter::EncodableValue(true));
        }
        // write method ListAudioDevices

        else
        {
            result->NotImplemented();
        }
    }

} // namespace audio

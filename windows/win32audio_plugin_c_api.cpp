#include "include/win32audio/win32audio_plugin_c_api.h"

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
#include <endpointvolume.h>
// #include "hicon_to_bytes.cpp"

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
const IID IID_IAudioEndpointVolume = __uuidof(IAudioEndpointVolume);

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/encodable_value.h>

#include <memory>
#include <sstream>

#include <endpointvolume.h>
#include <audiopolicy.h>
#include <psapi.h>
#include <propvarutil.h>

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

static HRESULT getDeviceProperty(IMMDevice *pDevice, DeviceProps *pOutput)
{
    if (pDevice == nullptr || pOutput == nullptr)
    {
        return E_POINTER;
    }

    IPropertyStore *pPropertyStore = nullptr;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
    if (FAILED(hr))
    {
        return hr;
    }

    PROPVARIANT varName = {0};
    PropVariantInit(&varName);
    hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &varName);
    if (SUCCEEDED(hr))
    {
        if (IsPropVariantString(varName))
        {
            STRRET strret;
            if (SUCCEEDED(PropVariantToStrRet(varName, &strret)))
            {
                pOutput->name = strret.pOleStr;
            }
            else
            {
                hr = E_UNEXPECTED;
            }
        }
        else
        {
            hr = E_UNEXPECTED;
        }

        PropVariantClear(&varName);
    }

    PROPVARIANT varIconPath = {0};
    PropVariantInit(&varIconPath);
    hr = pPropertyStore->GetValue(PKEY_DeviceClass_IconPath, &varIconPath);
    if (SUCCEEDED(hr))
    {
        if (IsPropVariantString(varIconPath))
        {
            STRRET strret;
            if (SUCCEEDED(PropVariantToStrRet(varIconPath, &strret)))
            {
                pOutput->iconInfo = strret.pOleStr;
            }
            else
            {
                pOutput->iconInfo = L"missing,0";
                hr = E_UNEXPECTED;
            }
        }
        else
        {
            pOutput->iconInfo = L"missing,0";
            hr = E_UNEXPECTED;
        }

        PropVariantClear(&varIconPath);
    }

    pPropertyStore->Release();

    return hr;
}

std::vector<DeviceProps> EnumAudioDevices(EDataFlow deviceType, ERole eRole)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to initialize COM\n");
        return output;
    }

    IMMDeviceEnumerator *pEnumerator = nullptr;
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
    if (FAILED(hr) || !pEnumerator)
    {
        OutputDebugString(L"Failed to create device enumerator\n");
        CoUninitialize();
        return output;
    }

    IMMDevice *pActive = nullptr;
    wstring activeDevID;

    hr = pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
    if (SUCCEEDED(hr) && pActive)
    {
        LPWSTR activeID = nullptr;
        hr = pActive->GetId(&activeID);
        if (SUCCEEDED(hr) && activeID)
        {
            activeDevID = activeID;
            CoTaskMemFree(activeID);
        }
        pActive->Release();
    }

    IMMDeviceCollection *pCollection = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(deviceType, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr) || !pCollection)
    {
        OutputDebugString(L"Failed to enumerate audio endpoints\n");
        if (pEnumerator)
            pEnumerator->Release();
        CoUninitialize();
        return output;
    }

    UINT cEndpoints = 0;
    hr = pCollection->GetCount(&cEndpoints);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to get count of audio endpoints\n");
        pCollection->Release();
        pEnumerator->Release();
        CoUninitialize();
        return output;
    }

    for (UINT n = 0; n < cEndpoints; ++n)
    {
        IMMDevice *pDevice = nullptr;
        hr = pCollection->Item(n, &pDevice);
        if (FAILED(hr) || !pDevice)
        {
            OutputDebugString(L"Failed to get audio endpoint\n");
            continue;
        }

        DeviceProps device;
        if (SUCCEEDED(getDeviceProperty(pDevice, &device)))
        {
            LPWSTR id = nullptr;
            hr = pDevice->GetId(&id);
            if (SUCCEEDED(hr) && id)
            {
                wstring currentID(id);
                device.id = currentID;
                device.isActive = (currentID == activeDevID);
                CoTaskMemFree(id);
            }
            output.push_back(device);
        }

        pDevice->Release();
    }

    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    return output;
}

DeviceProps getDefaultDevice(EDataFlow deviceType, ERole eRole)
{
    DeviceProps activeDevice;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
        if (SUCCEEDED(hr) && pEnumerator != nullptr)
        {
            IMMDevice *pActive = NULL;

            hr = pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
            if (SUCCEEDED(hr) && pActive != nullptr)
            {
                getDeviceProperty(pActive, &activeDevice);
                LPWSTR aid;
                hr = pActive->GetId(&aid);
                if (SUCCEEDED(hr) && aid != nullptr)
                {
                    activeDevice.id = aid;
                    CoTaskMemFree(aid);
                }
            }

            if (pActive != nullptr)
                pActive->Release();
        }

        if (pEnumerator != nullptr)
            pEnumerator->Release();
    }

    CoUninitialize();

    return activeDevice;
}

static HRESULT setDefaultDevice(LPWSTR devID, bool console, bool multimedia, bool communications)
{
    IPolicyConfigVista *pPolicyConfig = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient),
                                  NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID *)&pPolicyConfig);
    if (SUCCEEDED(hr))
    {
        if (console)
            hr = pPolicyConfig->SetDefaultEndpoint(devID, eConsole);
        if (multimedia)
            hr = pPolicyConfig->SetDefaultEndpoint(devID, eMultimedia);
        if (communications)
            hr = pPolicyConfig->SetDefaultEndpoint(devID, eCommunications);
        pPolicyConfig->Release();
    }
    // delete pPolicyConfig;
    return hr;
}

float getVolume(EDataFlow deviceType, ERole eRole)
{
    float volumeLevel = 0.0f;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *enumerator = nullptr;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&enumerator));
        if (SUCCEEDED(hr))
        {
            IMMDevice *device = nullptr;
            hr = enumerator->GetDefaultAudioEndpoint(deviceType, eRole, &device);
            if (SUCCEEDED(hr))
            {
                IAudioEndpointVolume *volumeControl = nullptr;
                hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void **>(&volumeControl));
                if (SUCCEEDED(hr))
                {
                    hr = volumeControl->GetMasterVolumeLevelScalar(&volumeLevel);
                    volumeControl->Release();
                }
                device->Release();
            }
            enumerator->Release();
        }
    }
    CoUninitialize();

    return volumeLevel;
}
bool registerNotificationCallback(EDataFlow deviceType, ERole eRole)
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

            pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
            IMMNotificationClient *pNotify = NULL;
            pEnumerator->RegisterEndpointNotificationCallback(pNotify);
        }
    }
    return 0.0;
}
bool setVolume(float volumeLevel, EDataFlow deviceType, ERole eRole)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
        if (SUCCEEDED(hr) && pEnumerator != nullptr)
        {
            IMMDevice *pActive = NULL;

            pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
            if (pActive != nullptr)
            {
                DeviceProps activeDevice;
                getDeviceProperty(pActive, &activeDevice);
                LPWSTR aid;
                if (SUCCEEDED(pActive->GetId(&aid)))
                {
                    activeDevice.id = aid;
                    CoTaskMemFree(aid);
                }

                IAudioEndpointVolume *m_spVolumeControl = NULL;
                hr = pActive->Activate(__uuidof(m_spVolumeControl), CLSCTX_INPROC_SERVER, NULL, (void **)&m_spVolumeControl);
                if (SUCCEEDED(hr) && m_spVolumeControl != nullptr)
                {
                    if (volumeLevel > 1)
                        volumeLevel = volumeLevel / 100;
                    m_spVolumeControl->SetMasterVolumeLevelScalar((float)volumeLevel, NULL);
                    m_spVolumeControl->Release();
                }
                pActive->Release();
            }
            pEnumerator->Release();
        }
    }
    CoUninitialize();

    return true;
}
static int switchDefaultDevice(EDataFlow deviceType, ERole role, bool console, bool multimedia, bool communications)
{
    std::vector<DeviceProps> devices = EnumAudioDevices(deviceType, role);
    if (devices.empty())
    {
        return 0; // No devices found
    }

    std::wstring activeDeviceId;
    for (const auto &device : devices)
    {
        if (!activeDeviceId.empty())
        {
            activeDeviceId = device.id;
            break;
        }
        if (device.isActive)
        {
            activeDeviceId = L"x";
        }
    }

    if (activeDeviceId.empty() || activeDeviceId == L"x")
    {
        activeDeviceId = devices[0].id;
    }

    try
    {
        setDefaultDevice((LPWSTR)activeDeviceId.c_str(), console, multimedia, communications);
        return 1; // Success
    }
    catch (const std::exception &)
    {
        return 0; // Failed to set default device
    }
}
bool setAudioDeviceVolume(float volumeLevel, LPWSTR devID)
{
    HRESULT hr = CoInitialize(NULL);  // Initialize COM library
    if (FAILED(hr)) {
        return false;  // COM initialization failed
    }

    // Create a device enumerator to get access to audio devices
    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) {
        CoUninitialize();
        return false;  // Failed to create device enumerator
    }

    IMMDevice* pDevice = nullptr;
    hr = pEnumerator->GetDevice(devID, &pDevice);
    if (FAILED(hr)) {
        pEnumerator->Release();
        CoUninitialize();
        return false;  // Failed to get the device by devID
    }

    IAudioEndpointVolume* pAudioVolume = nullptr;
    hr = pDevice->Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&pAudioVolume);
    if (FAILED(hr)) {
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return false;  // Failed to activate the audio endpoint volume
    }

    // Set the volume
    hr = pAudioVolume->SetMasterVolumeLevelScalar(volumeLevel, NULL);
    if (FAILED(hr)) {
        pAudioVolume->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return false;  // Failed to set the volume
    }

    // Cleanup
    pAudioVolume->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return true;
}

/// Audio Session

IAudioSessionEnumerator *GetAudioSessionEnumerator(ERole eRole)
{
    IMMDeviceEnumerator *deviceEnumerator = nullptr;
    HRESULT x = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);
    if (x)
    {
    }
    IMMDevice *device = nullptr;
    deviceEnumerator->GetDefaultAudioEndpoint(eRender, eRole, &device);

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
std::vector<ProcessVolume> GetProcessVolumes(ERole eRole, int pID = 0, float volume = 0.0)
{
    std::vector<ProcessVolume> volumes;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (hr)
    {
        IAudioSessionEnumerator *enumerator = GetAudioSessionEnumerator(eRole);
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

// static
class Win32audioPlugin : public flutter::Plugin, public IMMNotificationClient
{

public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar)
    {
        auto channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "win32audio",
            &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<Win32audioPlugin>(std::move(channel));
        plugin->channel_->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto &call, auto result)
            {
                plugin_pointer->HandleMethodCall(call, std::move(result));
            });

        registrar->AddPlugin(std::move(plugin));
    }

    Win32audioPlugin(std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel)
        : channel_(std::move(channel)) {}

    ~Win32audioPlugin() override
    {
        if (device_enumerator_)
        {
            device_enumerator_->UnregisterEndpointNotificationCallback(this);
            device_enumerator_->Release();
        }
    }

private:
    void HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue> &method_call,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
    {
        if (method_call.method_name().compare("initAudioListener") == 0)
        {
            Initialize();
            result->Success(flutter::EncodableValue(true));
        }
        else if (method_call.method_name().compare("enumAudioDevices") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            std::vector<DeviceProps> devices = EnumAudioDevices((EDataFlow)deviceType, (ERole)role);
            // loop through devices and add them to a map
            flutter::EncodableMap map;
            int i = 0;
            for (const auto &device : devices)
            {
                flutter::EncodableMap deviceMap;
                deviceMap[flutter::EncodableValue("id")] = flutter::EncodableValue(Encoding::WideToUtf8(device.id));
                deviceMap[flutter::EncodableValue("name")] = flutter::EncodableValue(Encoding::WideToUtf8(device.name));
                deviceMap[flutter::EncodableValue("iconInfo")] = flutter::EncodableValue(Encoding::WideToUtf8(device.iconInfo));
                deviceMap[flutter::EncodableValue("isActive")] = flutter::EncodableValue(device.isActive);
                map[i] = flutter::EncodableValue(deviceMap);
                i++;
            }
            result->Success(flutter::EncodableValue(map));
        }
        else if (method_call.method_name().compare("getDefaultDevice") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            DeviceProps device = getDefaultDevice((EDataFlow)deviceType, (ERole)role);

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
            bool console = std::get<bool>(args.at(flutter::EncodableValue("console")));
            bool multimedia = std::get<bool>(args.at(flutter::EncodableValue("multimedia")));
            bool communications = std::get<bool>(args.at(flutter::EncodableValue("communications")));
            std::wstring deviceIDW = Encoding::Utf8ToWide(deviceID);
            HRESULT nativeFuncResult = setDefaultDevice((LPWSTR)deviceIDW.c_str(), console, multimedia, communications);
            result->Success(flutter::EncodableValue((int)nativeFuncResult));
        }
        else if (method_call.method_name().compare("getAudioVolume") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            float nativeFuncResult = getVolume((EDataFlow)deviceType, (ERole)role);
            result->Success(flutter::EncodableValue((double)nativeFuncResult));
        }
        else if (method_call.method_name().compare("setAudioVolume") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            setVolume((float)volumeLevel, (EDataFlow)deviceType, (ERole)role);
            result->Success(flutter::EncodableValue((int)1));
        }
        else if (method_call.method_name().compare("switchDefaultDevice") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            bool console = std::get<bool>(args.at(flutter::EncodableValue("console")));
            bool multimedia = std::get<bool>(args.at(flutter::EncodableValue("multimedia")));
            bool communications = std::get<bool>(args.at(flutter::EncodableValue("communications")));
            bool nativeFuncResult = switchDefaultDevice((EDataFlow)deviceType, (ERole)role, console, multimedia, communications);
            result->Success(flutter::EncodableValue(nativeFuncResult));
        }
        else if (method_call.method_name().compare("setAudioDeviceVolume") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            std::string deviceID = std::get<std::string>(args.at(flutter::EncodableValue("deviceID")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            std::wstring deviceIDW = Encoding::Utf8ToWide(deviceID);
            setAudioDeviceVolume((float)volumeLevel, (LPWSTR)deviceIDW.c_str());
            result->Success(flutter::EncodableValue((int)1));;
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
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            std::vector<ProcessVolume> devices = GetProcessVolumes((ERole)role);
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
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            int processID = std::get<int>(args.at(flutter::EncodableValue("processID")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            std::vector<ProcessVolume> devices = GetProcessVolumes((ERole)role, processID, (float)volumeLevel);
            result->Success(flutter::EncodableValue(true));
        }
        // write method ListAudioDevices

        else
        {
            // MessageBoxA(NULL, "Method not implemented", "Win32AudioPlugin", MB_ICONWARNING | MB_OK);
            result->NotImplemented();
        }
    }

    void Initialize()
    {
        CoInitialize(nullptr);
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&device_enumerator_));
        if (device_enumerator_)
        {
            device_enumerator_->RegisterEndpointNotificationCallback(this);
        }
    }

    void Dispose()
    {
        if (device_enumerator_)
        {
            device_enumerator_->UnregisterEndpointNotificationCallback(this);
            device_enumerator_->Release();
            device_enumerator_ = nullptr;
        }
    }

    // IMMNotificationClient methods
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state) override
    {

        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnDeviceStateChanged")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
        return S_OK;
    }

    STDMETHODIMP OnDeviceAdded(LPCWSTR device_id) override
    {

        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnDeviceAdded")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
        return S_OK;
    }

    STDMETHODIMP OnDeviceRemoved(LPCWSTR device_id) override
    {

        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnDeviceRemoved")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
        return S_OK;
    }

    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR device_id) override
    {
        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnDefaultDeviceChanged")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));

        return S_OK;
    }

    STDMETHODIMP OnPropertyValueChanged(LPCWSTR device_id, const PROPERTYKEY key) override
    {

        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnPropertyValueChanged")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))}};
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
        return S_OK;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient))
        {
            *ppv = static_cast<IMMNotificationClient *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG)
    AddRef() override
    {
        return InterlockedIncrement(&ref_count_);
    }

    STDMETHODIMP_(ULONG)
    Release() override
    {
        ULONG count = InterlockedDecrement(&ref_count_);
        if (count == 0)
        {
            delete this;
        }
        return count;
    }

    void NotifyFlutter(const std::string &event)
    {
        channel_->InvokeMethod("onAudioDeviceChange", nullptr);
    }

    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
    IMMDeviceEnumerator *device_enumerator_;
    ULONG ref_count_;
};
void Win32audioPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar)
{
    Win32audioPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarManager::GetInstance()
            ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

#include "include/win32audio/win32audio_plugin_c_api.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/encodable_value.h>

#include "audio_manager.h"
#include "icon_manager.h"
#include "include/encoding.h"

using namespace win32audio;

namespace {

constexpr wchar_t kWindowClassName[] = L"Win32AudioPluginMessageWindow";
constexpr UINT WM_AUDIO_EVENT = WM_USER + 101;

struct AudioEvent {
    std::string name;
    std::string id;
};

} // namespace

class Win32audioPlugin : public flutter::Plugin, public IMMNotificationClient {
  public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar) {
        auto channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "win32audio",
            &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<Win32audioPlugin>(std::move(channel));
        plugin->channel_->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto &call, auto result) {
                plugin_pointer->HandleMethodCall(call, std::move(result));
            });

        registrar->AddPlugin(std::move(plugin));
    }

    Win32audioPlugin(std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel)
        : channel_(std::move(channel)) {}

    ~Win32audioPlugin() override {
        Dispose();
    }

  private:
    HWND window_handle_ = nullptr;
    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
    ComPtr<IMMDeviceEnumerator> device_enumerator_;
    ULONG ref_count_ = 1;

    void InitWindow() {
        WNDCLASS wc = {};
        wc.lpfnWndProc = Win32audioPlugin::WndProc;
        wc.lpszClassName = kWindowClassName;
        wc.hInstance = GetModuleHandle(nullptr);
        RegisterClass(&wc);

        window_handle_ = CreateWindowEx(0, kWindowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
        if (window_handle_) {
            SetWindowLongPtr(window_handle_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
    }

    void DestroyHiddenWindow() {
        if (window_handle_) {
            DestroyWindow(window_handle_);
            window_handle_ = nullptr;
        }
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        if (message == WM_AUDIO_EVENT) {
            auto* plugin = reinterpret_cast<Win32audioPlugin*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            auto* event = reinterpret_cast<AudioEvent*>(lparam);
            if (plugin && event) {
                plugin->HandleAudioEvent(event->name, event->id);
            }
            delete event;
            return 0;
        }
        return DefWindowProc(hwnd, message, wparam, lparam);
    }

    void HandleAudioEvent(const std::string& name, const std::string& id) {
        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue(name)},
            {flutter::EncodableValue("id"), flutter::EncodableValue(id)},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
    }

    void HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue> &method_call,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
        if (method_call.method_name().compare("initAudioListener") == 0) {
            Initialize();
            result->Success(flutter::EncodableValue(true));
        } else if (method_call.method_name().compare("enumAudioDevices") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            
            std::vector<DeviceProps> devices = AudioManager::EnumAudioDevices((EDataFlow)deviceType, (ERole)role);

            flutter::EncodableMap map;
            int i = 0;
            for (const auto &device : devices) {
                flutter::EncodableMap deviceMap;
                deviceMap[flutter::EncodableValue("id")] = flutter::EncodableValue(Encoding::WideToUtf8(device.id));
                deviceMap[flutter::EncodableValue("name")] = flutter::EncodableValue(Encoding::WideToUtf8(device.name));
                deviceMap[flutter::EncodableValue("iconInfo")] = flutter::EncodableValue(Encoding::WideToUtf8(device.iconInfo));
                deviceMap[flutter::EncodableValue("isActive")] = flutter::EncodableValue(device.isActive);
                map[i] = flutter::EncodableValue(deviceMap);
                i++;
            }
            result->Success(flutter::EncodableValue(map));
        } else if (method_call.method_name().compare("getDefaultDevice") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            
            DeviceProps device = AudioManager::GetDefaultDevice((EDataFlow)deviceType, (ERole)role);

            flutter::EncodableMap deviceMap;
            deviceMap[flutter::EncodableValue("id")] = flutter::EncodableValue(Encoding::WideToUtf8(device.id));
            deviceMap[flutter::EncodableValue("name")] = flutter::EncodableValue(Encoding::WideToUtf8(device.name));
            deviceMap[flutter::EncodableValue("iconInfo")] = flutter::EncodableValue(Encoding::WideToUtf8(device.iconInfo));
            deviceMap[flutter::EncodableValue("isActive")] = flutter::EncodableValue(device.isActive);
            result->Success(flutter::EncodableValue(deviceMap));
        } else if (method_call.method_name().compare("setDefaultAudioDevice") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            std::string deviceID = std::get<std::string>(args.at(flutter::EncodableValue("deviceID")));
            bool console = std::get<bool>(args.at(flutter::EncodableValue("console")));
            bool multimedia = std::get<bool>(args.at(flutter::EncodableValue("multimedia")));
            bool communications = std::get<bool>(args.at(flutter::EncodableValue("communications")));
            std::wstring deviceIDW = Encoding::Utf8ToWide(deviceID);
            
            HRESULT nativeFuncResult = AudioManager::SetDefaultDevice((LPWSTR)deviceIDW.c_str(), console, multimedia, communications);
            result->Success(flutter::EncodableValue((int)nativeFuncResult));
        } else if (method_call.method_name().compare("getAudioVolume") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            
            float nativeFuncResult = AudioManager::GetVolume((EDataFlow)deviceType, (ERole)role);
            result->Success(flutter::EncodableValue((double)nativeFuncResult));
        } else if (method_call.method_name().compare("setAudioVolume") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            
            AudioManager::SetVolume((float)volumeLevel, (EDataFlow)deviceType, (ERole)role);
            result->Success(flutter::EncodableValue((int)1));
        } else if (method_call.method_name().compare("switchDefaultDevice") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            bool console = std::get<bool>(args.at(flutter::EncodableValue("console")));
            bool multimedia = std::get<bool>(args.at(flutter::EncodableValue("multimedia")));
            bool communications = std::get<bool>(args.at(flutter::EncodableValue("communications")));
            
            bool nativeFuncResult = AudioManager::SwitchDefaultDevice((EDataFlow)deviceType, (ERole)role, console, multimedia, communications);
            result->Success(flutter::EncodableValue(nativeFuncResult));
        } else if (method_call.method_name().compare("setAudioDeviceVolume") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            std::string deviceID = std::get<std::string>(args.at(flutter::EncodableValue("deviceID")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            std::wstring deviceIDW = Encoding::Utf8ToWide(deviceID);
            
            AudioManager::SetAudioDeviceVolume((float)volumeLevel, (LPWSTR)deviceIDW.c_str());
            result->Success(flutter::EncodableValue((int)1));
        } else if (method_call.method_name().compare("iconToBytes") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            std::string iconLocation = std::get<std::string>(args.at(flutter::EncodableValue("iconLocation")));
            int iconID = std::get<int>(args.at(flutter::EncodableValue("iconID")));
            std::wstring iconLocationW = Encoding::Utf8ToWide(iconLocation);
            
            HICON icon = IconManager::GetIconFromFile(iconLocationW, iconID);
            
            if (icon) {
                 std::vector<char> buff;
                bool resultIcon = IconManager::GetIconData(icon, 32, buff);
                if (!resultIcon) {
                    buff.clear();
                    resultIcon = IconManager::GetIconData(icon, 24, buff);
                }
                
                DestroyIcon(icon); 

                if (resultIcon) {
                    std::vector<uint8_t> buff_uint8(buff.begin(), buff.end());
                    result->Success(flutter::EncodableValue(buff_uint8));
                } else {
                    std::vector<uint8_t> iconBytes = {204, 204, 204};
                    result->Success(flutter::EncodableValue(iconBytes));
                }
            } else {
                 std::vector<uint8_t> iconBytes = {204, 204, 204};
                 result->Success(flutter::EncodableValue(iconBytes));
            }

        } else if (method_call.method_name().compare("getWindowIcon") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int hWND = std::get<int>(args.at(flutter::EncodableValue("hWnd")));

            LRESULT iconResult = SendMessage((HWND)((LONG_PTR)hWND), WM_GETICON, 2, 0); 
            if (iconResult == 0)
                iconResult = GetClassLongPtr((HWND)((LONG_PTR)hWND), -14); 

            if (iconResult != 0) {
                HICON icon = (HICON)iconResult;
                std::vector<char> buff;
                bool resultIcon = IconManager::GetIconData(icon, 32, buff);
                if (!resultIcon) {
                    buff.clear();
                    resultIcon = IconManager::GetIconData(icon, 24, buff);
                }
                
                if (resultIcon) {
                    std::vector<uint8_t> buff_uint8(buff.begin(), buff.end());
                    result->Success(flutter::EncodableValue(buff_uint8));
                } else {
                    std::vector<uint8_t> iconBytes = {204, 204, 204};
                    result->Success(flutter::EncodableValue(iconBytes));
                }
            } else {
                std::vector<uint8_t> iconBytes = {204, 204, 204};
                result->Success(flutter::EncodableValue(iconBytes));
            }
        } else if (method_call.method_name().compare("getIconPng") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int hIcon = std::get<int>(args.at(flutter::EncodableValue("hIcon")));
            std::vector<char> buff;
            bool resultIcon = IconManager::GetIconData((HICON)((LONG_PTR)hIcon), 32, buff);
            if (!resultIcon) {
                buff.clear();
                resultIcon = IconManager::GetIconData((HICON)((LONG_PTR)hIcon), 24, buff);
            }
            if (resultIcon) {
                std::vector<uint8_t> buff_uint8(buff.begin(), buff.end());
                result->Success(flutter::EncodableValue(buff_uint8));
            } else {
                std::vector<uint8_t> iconBytes = {204, 204, 204};
                result->Success(flutter::EncodableValue(iconBytes));
            }
        } else if (method_call.method_name().compare("enumAudioMixer") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            
            std::vector<ProcessVolume> devices = AudioManager::GetProcessVolumes((ERole)role);
            
            flutter::EncodableMap map;
            for (const auto &device : devices) {
                flutter::EncodableMap deviceMap;
                deviceMap[flutter::EncodableValue("processId")] = flutter::EncodableValue(device.processId);
                deviceMap[flutter::EncodableValue("processPath")] = flutter::EncodableValue(device.processPath);
                deviceMap[flutter::EncodableValue("maxVolume")] = flutter::EncodableValue(device.maxVolume);
                deviceMap[flutter::EncodableValue("peakVolume")] = flutter::EncodableValue(device.peakVolume);
                map[flutter::EncodableValue(device.processId)] = flutter::EncodableValue(deviceMap);
            }
            result->Success(flutter::EncodableValue(map));
        } else if (method_call.method_name().compare("setAudioMixerVolume") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            int processID = std::get<int>(args.at(flutter::EncodableValue("processID")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            
            AudioManager::GetProcessVolumes((ERole)role, processID, (float)volumeLevel);
            result->Success(flutter::EncodableValue(true));
        } else if (method_call.method_name().compare("setAudioMixerVolumeByPath") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            std::string processPath = std::get<std::string>(args.at(flutter::EncodableValue("processPath")));
            double volumeLevel = std::get<double>(args.at(flutter::EncodableValue("volumeLevel")));
            
            bool nativeFuncResult = AudioManager::SetProcessVolumeByPath((ERole)role, processPath, (float)volumeLevel);
            result->Success(flutter::EncodableValue(nativeFuncResult));
        } else if (method_call.method_name().compare("getSampleRate") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            std::string deviceID = std::get<std::string>(args.at(flutter::EncodableValue("deviceID")));
            std::wstring deviceIDW = Encoding::Utf8ToWide(deviceID);
            
            int sampleRate = AudioManager::GetSampleRate((LPWSTR)deviceIDW.c_str());
            result->Success(flutter::EncodableValue(sampleRate));
        } else if (method_call.method_name().compare("setSampleRate") == 0) {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            std::string deviceID = std::get<std::string>(args.at(flutter::EncodableValue("deviceID")));
            int sampleRate = std::get<int>(args.at(flutter::EncodableValue("sampleRate")));
            std::wstring deviceIDW = Encoding::Utf8ToWide(deviceID);
            
            bool success = AudioManager::SetSampleRate((LPWSTR)deviceIDW.c_str(), sampleRate);
            result->Success(flutter::EncodableValue(success));
        } else {
            result->NotImplemented();
        }
    }

    void Initialize() {
        if (device_enumerator_) return;

        CoInitialize(nullptr);
        InitWindow(); 
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&device_enumerator_));
        
        if (SUCCEEDED(hr) && device_enumerator_) {
            device_enumerator_->RegisterEndpointNotificationCallback(this);
        }
    }

    void Dispose() {
        if (device_enumerator_) {
            device_enumerator_->UnregisterEndpointNotificationCallback(this);
            device_enumerator_ = nullptr; 
        }
        DestroyHiddenWindow();
    }

    // IMMNotificationClient methods
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state) override {
        if (window_handle_) {
             AudioEvent* event = new AudioEvent{"OnDeviceStateChanged", Encoding::WideToUtf8(device_id)};
             PostMessage(window_handle_, WM_AUDIO_EVENT, 0, (LPARAM)event);
        }
        return S_OK;
    }

    STDMETHODIMP OnDeviceAdded(LPCWSTR device_id) override {
        if (window_handle_) {
             AudioEvent* event = new AudioEvent{"OnDeviceAdded", Encoding::WideToUtf8(device_id)};
             PostMessage(window_handle_, WM_AUDIO_EVENT, 0, (LPARAM)event);
        }
        return S_OK;
    }

    STDMETHODIMP OnDeviceRemoved(LPCWSTR device_id) override {
        if (window_handle_) {
             AudioEvent* event = new AudioEvent{"OnDeviceRemoved", Encoding::WideToUtf8(device_id)};
             PostMessage(window_handle_, WM_AUDIO_EVENT, 0, (LPARAM)event);
        }
        return S_OK;
    }

    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR device_id) override {
         if (window_handle_) {
             std::string id = device_id ? Encoding::WideToUtf8(device_id) : "";
             AudioEvent* event = new AudioEvent{"OnDefaultDeviceChanged", id};
             PostMessage(window_handle_, WM_AUDIO_EVENT, 0, (LPARAM)event);
        }
        return S_OK;
    }

    STDMETHODIMP OnPropertyValueChanged(LPCWSTR device_id, const PROPERTYKEY key) override {
        if (window_handle_) {
             AudioEvent* event = new AudioEvent{"OnPropertyValueChanged", Encoding::WideToUtf8(device_id)};
             PostMessage(window_handle_, WM_AUDIO_EVENT, 0, (LPARAM)event);
        }
        return S_OK;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppv = static_cast<IMMNotificationClient *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&ref_count_);
    }

    STDMETHODIMP_(ULONG) Release() override {
        return InterlockedDecrement(&ref_count_);
    }
};

void Win32audioPluginCApiRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar) {
    Win32audioPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarManager::GetInstance()
            ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

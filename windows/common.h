#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <utility>
#include <iostream>

#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <commctrl.h>

// For ComPtr
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace win32audio {

// Clean up allocated strings automatically
struct CoTaskMemFreeDeleter {
    void operator()(void *p) const { CoTaskMemFree(p); }
};
using UniqueCoTaskMemString = std::unique_ptr<WCHAR, CoTaskMemFreeDeleter>;

class ScopedCoInitialize {
  public:
    ScopedCoInitialize() {
        hr_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    }
    ~ScopedCoInitialize() {
        if (SUCCEEDED(hr_)) {
            CoUninitialize();
        }
    }
    HRESULT result() const { return hr_; }
    bool succeeded() const { return SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE; }

  private:
    HRESULT hr_;
};

struct ProcessVolume {
    int processId = 0;
    std::string processPath = "";
    float maxVolume = 1.0f;
    float peakVolume = 0.0f;
};

struct DeviceProps {
    std::wstring id;
    std::wstring name;
    std::wstring iconInfo;
    bool isActive = false;
};

} // namespace win32audio

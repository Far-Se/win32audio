#ifndef FLUTTER_PLUGIN_WIN32AUDIO_PLUGIN_H_
#define FLUTTER_PLUGIN_WIN32AUDIO_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

class Win32audioPlugin : public flutter::Plugin
{
public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

    Win32audioPlugin();

    virtual ~Win32audioPlugin();

    // Disallow copy and assign.
    Win32audioPlugin(const Win32audioPlugin &) = delete;
    Win32audioPlugin &operator=(const Win32audioPlugin &) = delete;

private:
    // Called when a method is called on this plugin's channel from Dart.
    void HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue> &method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

#endif // FLUTTER_PLUGIN_WIN32AUDIO_PLUGIN_H_

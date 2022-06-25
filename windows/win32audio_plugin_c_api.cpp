#include "include/win32audio/win32audio_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "win32audio_plugin.h"

void Win32audioPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  win32audio::Win32audioPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

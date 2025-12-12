#pragma once

#include "common.h"

namespace win32audio {

class IconManager {
public:
    static HICON GetIconFromFile(std::wstring file, int index = 0);
    static bool GetIconData(HICON hIcon, int nColorBits, std::vector<char>& buff);
};

} // namespace win32audio

#include "icon_manager.h"

#include <tchar.h>
#include <shellapi.h>
#include <commoncontrols.h>
#include <memory> 

namespace win32audio {

// RAII helper for generic GDI objects
struct GdiObjectDeleter {
    void operator()(HGDIOBJ obj) const {
        if (obj) DeleteObject(obj);
    }
};
using ScopedGdiObject = std::unique_ptr<std::remove_pointer<HGDIOBJ>::type, GdiObjectDeleter>;

// RAII helper for HDC
struct HdcDeleter {
    void operator()(HDC hdc) const {
        if (hdc) DeleteDC(hdc);
    }
};
using ScopedHDC = std::unique_ptr<std::remove_pointer<HDC>::type, HdcDeleter>;

struct ICONDIRENTRY {
    UCHAR nWidth;
    UCHAR nHeight;
    UCHAR nNumColorsInPalette;
    UCHAR nReserved;
    WORD nNumColorPlanes;
    WORD nBitsPerPixel;
    ULONG nDataLength;
    ULONG nOffset;
};

HICON IconManager::GetIconFromFile(std::wstring file, int index) {
    HICON hIcon = NULL;
    LPCWSTR filePath = file.c_str(); 
    
    if (file.find(L".dll") != std::string::npos || file.find(L".exe") != std::string::npos) {
        ExtractIconExW(filePath, index, &hIcon, NULL, 1);
    } else {
        HINSTANCE instance = GetModuleHandle(NULL);
        WORD iconID = (WORD)index;
        // ExtractAssociatedIconW takes LPWSTR (mutable) so we copy
        std::vector<wchar_t> mutablePath(file.begin(), file.end());
        mutablePath.push_back(0);
        hIcon = ExtractAssociatedIconW(instance, mutablePath.data(), &iconID);
    }
    return hIcon;
}

bool IconManager::GetIconData(HICON hIcon, int nColorBits, std::vector<char> &buff) {
    if (!hIcon) return false;

    ScopedHDC dc(CreateCompatibleDC(NULL));
    if (!dc) return false;

    ICONINFO iconInfoRaw;
    if (!GetIconInfo(hIcon, &iconInfoRaw)) {
        return false;
    }
    
    ScopedGdiObject hbmColor(iconInfoRaw.hbmColor);
    ScopedGdiObject hbmMask(iconInfoRaw.hbmMask);

    char icoHeader[6] = {0, 0, 1, 0, 1, 0};
    buff.insert(buff.end(), std::begin(icoHeader), std::end(icoHeader));

    BITMAPINFO bmInfo = {0};
    bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmInfo.bmiHeader.biBitCount = 0; 
    if (!GetDIBits(dc.get(), (HBITMAP)hbmColor.get(), 0, 0, NULL, &bmInfo, DIB_RGB_COLORS)) {
        return false;
    }

    int nBmInfoSize = sizeof(BITMAPINFOHEADER);
    if (nColorBits < 24) {
        nBmInfoSize += sizeof(RGBQUAD) * (int)(1ULL << nColorBits);
    }

    std::vector<UCHAR> bitmapInfo(nBmInfoSize);
    BITMAPINFO *pBmInfo = (BITMAPINFO *)bitmapInfo.data();
    memcpy(pBmInfo, &bmInfo, sizeof(BITMAPINFOHEADER));

    if (bmInfo.bmiHeader.biSizeImage == 0) {
         int stride = ((bmInfo.bmiHeader.biWidth * bmInfo.bmiHeader.biBitCount + 31) / 32) * 4;
         bmInfo.bmiHeader.biSizeImage = stride * abs(bmInfo.bmiHeader.biHeight);
    }

    if (bmInfo.bmiHeader.biSizeImage == 0) return false;

    std::vector<UCHAR> bits(bmInfo.bmiHeader.biSizeImage);
    pBmInfo->bmiHeader.biBitCount = (WORD)nColorBits;
    pBmInfo->bmiHeader.biCompression = BI_RGB;
    
    if (!GetDIBits(dc.get(), (HBITMAP)hbmColor.get(), 0, bmInfo.bmiHeader.biHeight, bits.data(), pBmInfo, DIB_RGB_COLORS)) {
        return false;
    }

    BITMAPINFO maskInfo = {0};
    maskInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    if (!GetDIBits(dc.get(), (HBITMAP)hbmMask.get(), 0, 0, NULL, &maskInfo, DIB_RGB_COLORS) ||
        maskInfo.bmiHeader.biBitCount != 1) {
        return false;
    }

    if (maskInfo.bmiHeader.biSizeImage == 0) {
         int stride = ((maskInfo.bmiHeader.biWidth * maskInfo.bmiHeader.biBitCount + 31) / 32) * 4;
         maskInfo.bmiHeader.biSizeImage = stride * abs(maskInfo.bmiHeader.biHeight);
    }
    
    std::vector<UCHAR> maskBits(maskInfo.bmiHeader.biSizeImage);
    std::vector<UCHAR> maskInfoBytes(sizeof(BITMAPINFO) + 2 * sizeof(RGBQUAD));
    BITMAPINFO *pMaskInfo = (BITMAPINFO *)maskInfoBytes.data();
    memcpy(pMaskInfo, &maskInfo, sizeof(maskInfo));

    if (!GetDIBits(dc.get(), (HBITMAP)hbmMask.get(), 0, maskInfo.bmiHeader.biHeight, maskBits.data(), pMaskInfo, DIB_RGB_COLORS)) {
        return false;
    }

    ICONDIRENTRY dir;
    dir.nWidth = (UCHAR)(pBmInfo->bmiHeader.biWidth > 255 ? 0 : pBmInfo->bmiHeader.biWidth);
    dir.nHeight = (UCHAR)(pBmInfo->bmiHeader.biHeight > 255 ? 0 : pBmInfo->bmiHeader.biHeight);
    dir.nNumColorsInPalette = (nColorBits <= 8 ? (1 << nColorBits) : 0);
    if (nColorBits == 4) dir.nNumColorsInPalette = 16;
    dir.nReserved = 0;
    dir.nNumColorPlanes = 0;
    dir.nBitsPerPixel = pBmInfo->bmiHeader.biBitCount;
    dir.nDataLength = pBmInfo->bmiHeader.biSizeImage + pMaskInfo->bmiHeader.biSizeImage + nBmInfoSize;
    dir.nOffset = sizeof(dir) + sizeof(icoHeader);

    const char* dirPtr = reinterpret_cast<const char*>(&dir);
    buff.insert(buff.end(), dirPtr, dirPtr + sizeof(dir));

    int nBitsSize = pBmInfo->bmiHeader.biSizeImage;
    pBmInfo->bmiHeader.biHeight *= 2; 
    pBmInfo->bmiHeader.biCompression = 0;
    pBmInfo->bmiHeader.biSizeImage += pMaskInfo->bmiHeader.biSizeImage;

    const char* bmiPtr = reinterpret_cast<const char*>(&pBmInfo->bmiHeader);
    buff.insert(buff.end(), bmiPtr, bmiPtr + nBmInfoSize);

    const char* bitsPtr = reinterpret_cast<const char*>(bits.data());
    buff.insert(buff.end(), bitsPtr, bitsPtr + nBitsSize);

    const char* maskPtr = reinterpret_cast<const char*>(maskBits.data());
    buff.insert(buff.end(), maskPtr, maskPtr + pMaskInfo->bmiHeader.biSizeImage);

    return true;
}

} // namespace win32audio

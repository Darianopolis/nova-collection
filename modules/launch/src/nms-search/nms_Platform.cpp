#undef UNICODE
#define UNICODE

#undef _UNICODE
#define _UNICODE

#undef NOMINMAX
#define NOMINMAX

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN

// #include <windowsx.h>
// #include <shellapi.h>

#include <Windows.h>
#include <shellapi.h>
#include <combaseapi.h>
#include <wincodec.h>
#include <CommCtrl.h>

// -----------------------------------------------------------------------------

#include "nms_Platform.hpp"

#include <nova/core/nova_Core.hpp>

namespace nms
{
// -----------------------------------------------------------------------------

    struct ComState
    {
        bool created = false;
        IWICImagingFactory* wic;

        ankerl::unordered_dense::map<u64, nova::Image> imageCache;

        ComState()
        {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                __uuidof(IWICImagingFactory),
                (void**)&wic);
        }

        ~ComState()
        {
            wic->Release();
            CoUninitialize();
        }
    };

    static ComState NmsComState;

    void ClearIconCache()
    {
        for (auto&[hash, texture] : NmsComState.imageCache) {
            texture.Destroy();
        }
        NmsComState.imageCache.clear();
    }

    nova::Image LoadIconFromPath(
        nova::Context context,
        std::string_view path)
    {
        // Query shell for path icon

        HICON icon = {};
        NOVA_DEFER(&) { DestroyIcon(icon); };

        SHFILEINFO info = {};

        auto wPath = nova::ToUtf16(path);

        if (auto list = (HIMAGELIST)SHGetFileInfoW(wPath.c_str(),
            FILE_ATTRIBUTE_NORMAL,
            &info, sizeof(info),
            SHGFI_SYSICONINDEX))
        {
            icon = ImageList_GetIcon(list, info.iIcon, ILD_NORMAL);
            ImageList_Destroy(list);
        }

        if (!icon)
        {
            SHGetFileInfoW(wPath.c_str(),
                FILE_ATTRIBUTE_NORMAL,
                &info, sizeof(info),
                SHGFI_ICON | SHGFI_LARGEICON);
            icon = info.hIcon;

            if (!icon)
                return {};
        }

        // Extract image data from icon

        IWICBitmap* bitmap = nullptr;
        NmsComState.wic->CreateBitmapFromHICON(icon, &bitmap);
        NOVA_DEFER(&) { bitmap->Release(); };

        u32 width, height;
        bitmap->GetSize(&width, &height);

        IWICFormatConverter* converter = nullptr;
        NmsComState.wic->CreateFormatConverter(&converter);
        NOVA_DEFER(&) { converter->Release(); };
        converter->Initialize(
            bitmap,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr, 0,
            WICBitmapPaletteTypeMedianCut);

        usz dataSize = width * height * 4;
        auto pixelData = NOVA_STACK_ALLOC(BYTE, dataSize);
        converter->CopyPixels(nullptr, width * 4, UINT(dataSize), pixelData);

        // Hash and check to find matching existing image

        u64 hash = ankerl::unordered_dense::detail::wyhash::hash(pixelData, dataSize);
        auto& texture = NmsComState.imageCache[hash];
        if (texture)
            return texture;

        nova::Log("Loading icon {}, size = ({}, {})", path, width, height);
        nova::Log("  Num images = {}", NmsComState.imageCache.size());

        texture = nova::Image::Create(context, Vec3U(width, height, 0),
            nova::ImageUsage::Sampled,
            nova::Format::RGBA8_UNorm);

        texture.Set({}, texture.Extent(), pixelData);
        texture.Transition(nova::ImageLayout::Sampled);

        return texture;
    }
}
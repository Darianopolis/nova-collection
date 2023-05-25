#include "Platform.hpp"

namespace nms
{
    void ConvertToWString(std::string_view input, std::wstring& output)
    {
        int length = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, input.data(), int(input.size()), nullptr, 0);
        output.resize(length);
        MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, input.data(), int(input.size()), output.data(), length);
    }

    void ConvertToString(std::wstring_view input, std::string& output)
    {
        BOOL usedDefault = FALSE;
        int length = WideCharToMultiByte(CP_UTF8, MB_PRECOMPOSED, input.data(), int(input.size()), nullptr, 0, "?", &usedDefault);
        output.resize(length);
        WideCharToMultiByte(CP_UTF8, MB_PRECOMPOSED, input.data(), int(input.size()), output.data(), length, "?", &usedDefault);
    }

// -----------------------------------------------------------------------------

    struct IconLoaderThreadState
    {
        IWICImagingFactory* wic;

        IconLoaderThreadState()
        {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                __uuidof(IWICImagingFactory),
                (void**)&wic);
        }

        ~IconLoaderThreadState()
        {
            wic->Release();
            CoUninitialize();
        }
    };

    static thread_local IconLoaderThreadState NmsIconLoaderThreadState = {};

    nova::Image* LoadIconFromPath(
        nova::Context* context,
        nova::CommandPool* cmdPool,
        nova::ResourceTracker* tracker,
        nova::Queue* queue,
        nova::Fence* fence,
        std::string_view path)
    {
        HICON icon = {};
        NOVA_ON_SCOPE_EXIT(&) { DestroyIcon(icon); };

        SHFILEINFO info = {};

        auto wPath = ConvertToWString(path);

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
                return nullptr;
        }

        IWICBitmap* bitmap = nullptr;
        NmsIconLoaderThreadState.wic->CreateBitmapFromHICON(icon, &bitmap);
        NOVA_ON_SCOPE_EXIT(&) { bitmap->Release(); };

        u32 width, height;
        bitmap->GetSize(&width, &height);

        IWICFormatConverter* converter = nullptr;
        NmsIconLoaderThreadState.wic->CreateFormatConverter(&converter);
        NOVA_ON_SCOPE_EXIT(&) { converter->Release(); };
        converter->Initialize(
            bitmap,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr, 0,
            WICBitmapPaletteTypeMedianCut);

        NOVA_LOG("Loading icon {}, size = ({}, {})", path, width, height);

        usz dataSize = width * height * 4;
        auto image = context->CreateImage(Vec3U(width, height, 0),
            nova::ImageUsage::Sampled,
            nova::Format::RGBA8U);
        NOVA_ON_SCOPE_FAILURE(&) { context->DestroyImage(image); };

        auto staging = context->CreateBuffer(dataSize,
            nova::BufferUsage::TransferSrc,
            nova::BufferFlags::CreateMapped);
        NOVA_ON_SCOPE_EXIT(&) { context->DestroyBuffer(staging); };

        converter->CopyPixels(nullptr, width * 4, UINT(dataSize), (BYTE*)staging->mapped);

        auto cmd = cmdPool->BeginPrimary(tracker);
        cmd->CopyToImage(image, staging);
        queue->Submit({cmd}, {}, {fence});
        fence->Wait();

        return image;
    }
}
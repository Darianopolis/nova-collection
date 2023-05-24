#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>

#include <nova/imdraw/nova_ImDraw2D.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <array>
#include <iostream>

#include <windowsx.h>
#include <shellapi.h>
#include <wincodec.h>
#include <CommCtrl.h>

using namespace nova::types;

void TryMain()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    // glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    auto window = glfwCreateWindow(1920, 1200, "next", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    auto context = nova::Context::Create(true);

    auto surface = context->CreateSurface(glfwGetWin32Window(window));
    auto swapchain = context->CreateSwapchain(surface,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Fifo);

    auto queue = context->graphics;
    auto commandPool = context->CreateCommandPool();
    auto fence = context->CreateFence();
    auto tracker = context->CreateResourceTracker();

// -----------------------------------------------------------------------------

    auto imDraw = nova::ImDraw2D::Create(context);

// -----------------------------------------------------------------------------

    int count;
    auto mode = glfwGetVideoMode(glfwGetMonitors(&count)[0]);
    int mWidth = mode->width;
    int mHeight = mode->height;

    std::cout << "Monitor size = " << mWidth << ", " << mHeight << '\n';

    auto font = imDraw->LoadFont("SEGUISB.TTF", 35.f, commandPool, tracker, fence, queue);
    auto fontSmall = imDraw->LoadFont("SEGOEUI.TTF", 18.f, commandPool, tracker, fence, queue);

    auto image = context->CreateImage({ 1, 1, 0 }, nova::ImageUsage::Sampled, nova::Format::RGBA8U);
    NOVA_ON_EXIT(&) { context->DestroyImage(image); };
    {
        auto staging = context->CreateBuffer(4, nova::BufferUsage::Storage, nova::BufferFlags::CreateMapped);
        staging->Get<std::array<u8, 4>>(0) = { 255, 255, 255, 255 };

        auto cmd = commandPool->BeginPrimary(tracker);
        cmd->CopyToImage(image, staging);
        queue->Submit({cmd}, {}, {fence});
        fence->Wait();

        context->DestroyBuffer(staging);
    }
    auto imageID = imDraw->RegisterTexture(image, imDraw->defaultSampler);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IWICImagingFactory* wic;
    CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        __uuidof(IWICImagingFactory),
        (void**)&wic);
    NOVA_ON_SCOPE_EXIT(&) { wic->Release(); };

    auto loadImage = [&](std::wstring path) -> nova::Image* {
        HICON icon = {};
        SHFILEINFO info = {};

        auto list = (HIMAGELIST)SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info), SHGFI_SYSICONINDEX);
        if (list)
        {
            icon = ImageList_GetIcon(list, info.iIcon, ILD_NORMAL);
            ImageList_Destroy(list);
        }

        if (!icon)
        {
            SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON);
            icon = info.hIcon;
        }

        if (icon)
        {
            IWICBitmap* bitmap;
            wic->CreateBitmapFromHICON(icon, &bitmap);
            NOVA_ON_SCOPE_EXIT(&) { bitmap->Release(); };

            u32 width, height;
            bitmap->GetSize(&width, &height);

            IWICFormatConverter* converter;
            wic->CreateFormatConverter(&converter);
            NOVA_ON_SCOPE_EXIT(&) { converter->Release(); };
            converter->Initialize(
                bitmap,
                GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone,
                nullptr, 0,
                WICBitmapPaletteTypeMedianCut);

            NOVA_LOG("Loading icon, size = ({}, {})", width, height);

            usz dataSize = width * height * 4;
            auto image = context->CreateImage(Vec3U(width, height, 0), nova::ImageUsage::Sampled, nova::Format::RGBA8U);
            NOVA_ON_SCOPE_FAILURE(&) { context->DestroyImage(image); };

            auto staging = context->CreateBuffer(dataSize, nova::BufferUsage::TransferSrc, nova::BufferFlags::CreateMapped);
            converter->CopyPixels(nullptr, width * 4, UINT(dataSize), (BYTE*)staging->mapped);

            // for (u32 x = 0; x < width; ++x)
            // {
            //     for (u32 y = 0; y < height; ++y)
            //     {
            //         u32 pixelOffset = x + (y * width);
            //         usz byteOffset = pixelOffset * 4;
            //         NOVA_LOG("Pixel[{}, {}] = ({}, {}, {}, {})",
            //             x, y,
            //             (u8)staging->mapped[byteOffset + 0],
            //             (u8)staging->mapped[byteOffset + 1],
            //             (u8)staging->mapped[byteOffset + 2],
            //             (u8)staging->mapped[byteOffset + 3]);
            //     }
            // }

            auto cmd = commandPool->BeginPrimary(tracker);
            cmd->CopyToImage(image, staging);
            queue->Submit({cmd}, {}, {fence});
            fence->Wait();

            return image;
        }
NOVA_DEBUG();
        return nullptr;
    };

    // auto icon = loadImage(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\BeamNG.drive\\BeamNG.drive.exe");

    std::array<nova::Image*, 5> icons;
    icons[0] = loadImage(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\BeamNG.drive\\BeamNG.drive.exe");
    icons[1] = loadImage(L"C:\\Users\\Darian\\AppData\\Local\\osu!\\osu!.exe");
    icons[2] = loadImage(L"C:\\Program Files\\Cakewalk\\Cakewalk Core\\Cakewalk.exe");
    icons[3] = loadImage(L"D:\\Dev\\Projects\\pyrite\\nova");
    icons[4] = loadImage(L"D:\\Dev\\Projects\\nomoreshortcuts\\nomoreshortcuts-v2");
    NOVA_ON_SCOPE_EXIT(&) { for (auto icon : icons) context->DestroyImage(icon); };
    std::array<nova::ImTextureID, 5> iconIDs;
    for (u32 i = 0; i < 5; ++i)
        iconIDs[i] = imDraw->RegisterTexture(icons[i], imDraw->defaultSampler);

    u32 selectedItem = 0;
    auto drawWindow = [&] {

        Vec4 backgroundColor = { 0.1f, 0.1f, 0.1f, 1.f };
        Vec4 borderColor =  { 0.6f, 0.6f, 0.6f, 0.5f };
        Vec4 highlightColor = { 0.4f, 0.4f, 0.4f, 0.2f, };

        Vec2 pos = { mWidth * 0.5f, mHeight * 0.5f };

        f32 hInputWidth = 960.f;
        f32 hInputHeight = 29.f;

        f32 outputItemHeight = 76.f;
        u32 outputCount = 5;

        f32 hOutputWidth = 600.f;
        f32 hOutputHeight = 0.5f * outputItemHeight * outputCount;

        f32 margin = 18.f;

        f32 cornerRadius = 18.f;
        f32 borderWidth = 2.f;

        Vec2 textInset = { 74.5f, 37.f };
        Vec2 textSmallInset = { 76.f, 60.f };

        f32 iconSize = 50;
        f32 iconPadding = (outputItemHeight - iconSize) / 2.f;

        imDraw->DrawRect({
            .centerColor = backgroundColor,
            .borderColor = borderColor,
            .centerPos = pos - Vec2(0.f, hInputHeight),
            .halfExtent = Vec2(hInputWidth, hInputHeight) + Vec2(borderWidth),
            .cornerRadius = cornerRadius,
            .borderWidth = borderWidth,

            .texTint = { 0.f, 0.f, 0.f, 0.f },
            .texIndex = imageID,
        });

        imDraw->DrawRect({
            .centerColor = backgroundColor,
            .borderColor = borderColor,
            .centerPos = pos + Vec2(0.f, hOutputHeight + margin + borderWidth),
            .halfExtent = Vec2(hOutputWidth, hOutputHeight) + Vec2(borderWidth),
            .cornerRadius = cornerRadius,
            .borderWidth = borderWidth,

            .texTint = { 0.f, 0.f, 0.f, 0.f },
            .texIndex = imageID,
        });

        imDraw->DrawRect({
            .centerColor = highlightColor,
            .centerPos = pos
                + Vec2(0.f, margin + borderWidth + outputItemHeight * (0.5f + selectedItem)),
            .halfExtent = Vec2(hOutputWidth, outputItemHeight * 0.5f)
                - Vec2(2.f),
            .cornerRadius = cornerRadius - borderWidth - 2.f,

            .texTint = { 0.f, 0.f, 0.f, 0.f },
            .texIndex = imageID,
        });

        auto files = std::array {
            "BeamNG.drive.exe",
            "osu!.exe",
            "Cakewalk.exe",
            "nova",
            "nomoreshortcuts-v2",
        };

        auto paths = std::array {
            "C:/Program Files (x86)/Steam/steamapps/common/BeamNG.drive",
            "C:/Users/Darian/AppData/Local/osu!",
            "C:/Program Files/Cakewalk/Cakewalk Core",
            "D:/Dev/Projects/pyrite",
            "D:/Dev/Projects/nomoreshortcuts",
        };

        for (u32 i = 0; i < outputCount; ++i)
        {
            imDraw->DrawRect({
                .centerPos = pos
                    + Vec2(
                        -hOutputWidth + (iconSize / 2.f) + iconPadding,
                        margin + borderWidth + outputItemHeight * (0.5f + i)),
                .halfExtent = Vec2(iconSize) / 2.f,

                .texTint = { 1.f, 1.f, 1.f, 1.f },
                .texIndex = iconIDs[i],
                .texCenterPos = { 0.5f, 0.5f },
                .texHalfExtent = { 0.5f, 0.5f },
            });

            imDraw->DrawString(
                files[i],
                pos + Vec2(-hOutputWidth, margin + borderWidth)
                    + Vec2(0.f, outputItemHeight * i)
                    + textInset,
                font);

            imDraw->DrawString(
                paths[i],
                pos + Vec2(-hOutputWidth, margin + borderWidth)
                    + Vec2(0.f, outputItemHeight * i)
                    + textSmallInset,
                fontSmall);
        }
    };

    bool redraw = true;
    bool skipUpdate = false;

    auto lastFrame = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

// -----------------------------------------------------------------------------

        if (!skipUpdate)
        {
            static bool upPressed = false;
            if (glfwGetKey(window, GLFW_KEY_UP))
            {
                if (!upPressed)
                {
                    upPressed = true;
                    selectedItem = std::max(selectedItem, 1u) - 1;
                    redraw = true;
                }
            }
            else
                upPressed = false;

            static bool downPressed = false;
            if (glfwGetKey(window, GLFW_KEY_DOWN))
            {
                if (!downPressed)
                {
                    downPressed = true;
                    selectedItem = std::min(selectedItem, 3u) + 1;
                    redraw = true;
                }
            }
            else
                downPressed = false;
        }
        else
        {
            redraw = true;
        }

        skipUpdate = false;

        if (!redraw)
        {
            glfwWaitEvents();
            skipUpdate = true;
        }
        redraw = false;

// -----------------------------------------------------------------------------

        imDraw->Reset();

        drawWindow();

// -----------------------------------------------------------------------------

        // Record frame

        fence->Wait();
        commandPool->Reset();

        auto cmd = commandPool->BeginPrimary(tracker);
        cmd->SetViewport({ imDraw->maxBounds.x - imDraw->minBounds.x, imDraw->maxBounds.y - imDraw->minBounds.y }, false);
        cmd->SetBlendState(1, true);
        cmd->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        // Update window size, record primary buffer and present

        glfwSetWindowSize(window, i32(imDraw->maxBounds.x - imDraw->minBounds.x), i32(imDraw->maxBounds.y - imDraw->minBounds.y));
        glfwSetWindowPos(window, i32(imDraw->minBounds.x), i32(imDraw->minBounds.y));

        queue->Acquire({swapchain}, {fence});

        cmd->BeginRendering({swapchain->image}, {Vec4(0.f)}, true);
        imDraw->Record(cmd);
        cmd->EndRendering();

        cmd->Transition(swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0);

        queue->Submit({cmd}, {fence}, {fence});
        queue->Present({swapchain}, {fence});
    }
}

int main()
{
    try
    {
        TryMain();
    }
    catch(...) {}
}
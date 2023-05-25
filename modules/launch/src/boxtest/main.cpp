#include <Platform.hpp>

using namespace nova::types;

void TryMain()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    // glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    auto window = glfwCreateWindow(1920, 1200, "No More Shortcuts", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    {
        GLFWimage image;

        int channels;
        image.pixels = stbi_load("favicon.png", &image.width, &image.height, &channels, STBI_rgb_alpha);
        NOVA_ON_SCOPE_EXIT(&) { stbi_image_free(image.pixels); };

        glfwSetWindowIcon(window, 1, &image);
    }

    auto context = nova::Context::Create(true);

    auto surface = context->CreateSurface(glfwGetWin32Window(window));
    auto swapchain = context->CreateSwapchain(surface,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Mailbox);

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

// -----------------------------------------------------------------------------

    std::array<std::filesystem::path, 5> paths {
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\BeamNG.drive\\BeamNG.drive.exe",
        "C:\\Users\\Darian\\AppData\\Local\\osu!\\osu!.exe",
        "C:\\Program Files\\Cakewalk\\Cakewalk Core\\Cakewalk.exe",
        "D:\\Dev\\Projects\\pyrite\\nova",
        "D:\\Dev\\Projects\\nomoreshortcuts\\nomoreshortcuts-v2",
    };

    std::array<nova::Image*, 5> icons;
    for (u32 i = 0; i < 5; ++i)
        icons[i] = nms::LoadIconFromPath(context, commandPool, tracker, queue, fence, paths[i].string());
    NOVA_ON_SCOPE_EXIT(&) {
        for (auto icon : icons)
            context->DestroyImage(icon);
    };

    std::array<nova::ImTextureID, 5> iconIDs;
    for (u32 i = 0; i < 5; ++i)
        iconIDs[i] = imDraw->RegisterTexture(icons[i], imDraw->defaultSampler);

// -----------------------------------------------------------------------------

    std::string query = "BeamNG .exe";

    u32 selectedItem = 0;
    auto drawWindow = [&] {

        Vec4 backgroundColor = { 0.1f, 0.1f, 0.1f, 1.f };
        Vec4 borderColor =  { 0.6f, 0.6f, 0.6f, 0.5f };
        Vec4 highlightColor = { 0.4f, 0.4f, 0.4f, 0.2f, };

        Vec2 pos = { mWidth * 0.5f, mHeight * 0.5f };

        Vec2 hInputSize = { 960.f, 29.f };

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

        // Input box

        imDraw->DrawRect({
            .centerColor = backgroundColor,
            .borderColor = borderColor,
            .centerPos = pos - Vec2(0.f, hInputSize.y),
            .halfExtent = hInputSize + Vec2(borderWidth),
            .cornerRadius = cornerRadius,
            .borderWidth = borderWidth,

            .texTint = { 0.f, 0.f, 0.f, 0.f },
            .texIndex = imageID,
        });

        // Input text

        {
            auto size = imDraw->MeasureString(query, font);

            imDraw->DrawString(query,
                pos - Vec2(size.x / 2.f, 17.f),
                font);
        }

        // Output box

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

        // Highlight

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

        for (u32 i = 0; i < outputCount; ++i)
        {
            // Icon

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

            // Filename

            imDraw->DrawString(
                paths[i].filename().string(),
                pos + Vec2(-hOutputWidth, margin + borderWidth)
                    + Vec2(0.f, outputItemHeight * i)
                    + textInset,
                font);

            // Path

            imDraw->DrawString(
                paths[i].parent_path().string(),
                pos + Vec2(-hOutputWidth, margin + borderWidth)
                    + Vec2(0.f, outputItemHeight * i)
                    + textSmallInset,
                fontSmall);
        }
    };

// -----------------------------------------------------------------------------

    bool redraw = true;
    bool skipUpdate = false;

// -----------------------------------------------------------------------------

    static auto onChar = [&](u32 codepoint) {
        query.push_back(c8(codepoint));
    };
    glfwSetCharCallback(window, [](auto, u32 codepoint) { onChar(codepoint); });

    static auto onKey = [&](u32 key, i32 action, u32 mods) {
        if (skipUpdate)
            return;

        if (action == GLFW_RELEASE)
            return;

        if (key == GLFW_KEY_BACKSPACE)
        {
            if (mods & GLFW_MOD_SHIFT)
                query.clear();
            else
                query.resize(std::max(1ull, query.size()) - 1);

            redraw = true;
        }
        else
        if (key == GLFW_KEY_UP)
        {
            selectedItem = std::max(selectedItem, 1u) - 1;
            redraw = true;
        }
        else
        if (key == GLFW_KEY_DOWN)
        {
            selectedItem = std::min(selectedItem, 3u) + 1;
            redraw = true;
        }
        else
        if (key == GLFW_KEY_LEFT)
        {
            selectedItem = 0;
            redraw = true;
        }
        else
        if (key == GLFW_KEY_RIGHT)
        {
            selectedItem = 4;
            redraw = true;
        }
    };
    glfwSetKeyCallback(window, [](auto, i32 key, [[maybe_unused]] i32 scancode, i32 action, i32 mods) { onKey(key, action, mods); });

// -----------------------------------------------------------------------------


    RegisterHotKey(glfwGetWin32Window(window), 1, MOD_CONTROL | MOD_SHIFT, VK_SPACE);

    bool running = true;
    while (running)
    {
        MSG msg = {};
        while (GetMessage(&msg, glfwGetWin32Window(window), 0, 0) && msg.message != WM_HOTKEY);

        glfwShowWindow(window);
        glfwSetWindowShouldClose(window, GLFW_FALSE);

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

// -----------------------------------------------------------------------------

            if (skipUpdate)
                redraw = true;

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

            if (glfwGetKey(window, GLFW_KEY_ESCAPE))
                glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        glfwHideWindow(window);
    }
}

int main()
{
    try
    {
        TryMain();
    }
    catch(...)
    {
        NOVA_LOG("Closing after exception");
    }
}
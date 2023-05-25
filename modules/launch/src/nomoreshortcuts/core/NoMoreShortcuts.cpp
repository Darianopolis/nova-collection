#include "NoMoreShortcuts.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <combaseapi.h>

#include <thread>

using namespace std::literals;

App::App()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    window = glfwCreateWindow(1920, 1200, "No More Shortcuts", nullptr, nullptr);

    HWND hwnd = glfwGetWin32Window(window);
    SetWindowLongW(hwnd, GWL_EXSTYLE, GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

    // TODO: Chroma key is an ugly hack, use nchittest to do analytical transparency
    //   Or, do full screeen pass to filter out unintentional chroma key matches and
    //   apply chroma key based on alpha.
    SetLayeredWindowAttributes(hwnd, RGB(1, 0, 0), 0, LWA_COLORKEY);

    {
        GLFWimage iconImage;

        int channels;
        iconImage.pixels = stbi_load("favicon.png", &iconImage.width, &iconImage.height, &channels, STBI_rgb_alpha);
        NOVA_ON_SCOPE_EXIT(&) { stbi_image_free(iconImage.pixels); };

        glfwSetWindowIcon(window, 1, &iconImage);
    }

    context = nova::Context::Create(true);

    surface = context->CreateSurface(hwnd);
    swapchain = context->CreateSwapchain(surface,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Mailbox);

    queue = context->graphics;
    commandPool = context->CreateCommandPool();
    fence = context->CreateFence();
    tracker = context->CreateResourceTracker();

// -----------------------------------------------------------------------------

    imDraw = nova::ImDraw2D::Create(context);

// -----------------------------------------------------------------------------

    int count;
    auto mode = glfwGetVideoMode(glfwGetMonitors(&count)[0]);
    mWidth = mode->width;
    mHeight = mode->height;

// -----------------------------------------------------------------------------

    font = imDraw->LoadFont("SEGUISB.TTF", 35.f, commandPool, tracker, fence, queue);
    fontSmall = imDraw->LoadFont("SEGOEUI.TTF", 18.f, commandPool, tracker, fence, queue);

    emptyImage = context->CreateImage({ 1, 1, 0 }, nova::ImageUsage::Sampled, nova::Format::RGBA8U);
    {
        auto staging = context->CreateBuffer(4, nova::BufferUsage::Storage, nova::BufferFlags::CreateMapped);
        staging->Get<std::array<u8, 4>>(0) = { 0, 0, 0, 0 };

        auto cmd = commandPool->BeginPrimary(tracker);
        cmd->CopyToImage(emptyImage, staging);
        queue->Submit({cmd}, {}, {fence});
        fence->Wait();

        context->DestroyBuffer(staging);
    }
    emptyTexID = imDraw->RegisterTexture(emptyImage, imDraw->defaultSampler);

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

    keywords.push_back("");

    using namespace std::chrono;

    resultList = std::make_unique<ResultListPriorityCollector>();
    favResultList = std::make_unique<FavResultList>(&keywords);
    NOVA_LOGEXPR(favResultList);
    fileResultList = std::make_unique<FileResultList>(favResultList.get());
    resultList->AddList(favResultList.get());
    resultList->AddList(fileResultList.get());

    show = false;

    ResetItems();
    UpdateQuery();
}

void App::ResetItems(bool end)
{
    items.clear();
    if (end)
    {
        auto item = resultList->Prev(nullptr);
        if (item)
        {
            auto itemP = item.get();
            items.push_back(std::move(item));
            while ((items.size() < 5) && (item = resultList->Prev(itemP)))
            {
                itemP = item.get();
                items.push_back(std::move(item));
            }
            std::reverse(items.begin(), items.end());
        }
        selection = (uint32_t)items.size() - 1;
    }
    else
    {
        auto item = resultList->Next(nullptr);
        if (item)
        {
            auto itemP = item.get();
            items.push_back(std::move(item));
            while ((items.size() < 5) && (item = resultList->Next(itemP)))
            {
                itemP = item.get();
                items.push_back(std::move(item));
            }
        }
        selection = 0;
    }
}

void App::ResetQuery()
{
    keywords.clear();
    keywords.emplace_back();
    // tree.setMatchBits(1, 1, 0, 0);
    // tree.matchBits = 1;
    resultList->Query(QueryAction::SET, "");
    UpdateQuery();
}

void App::Update()
{
    // overlay::Update(*mainLayer, *this, menu.visible);
}

std::string App::JoinQuery()
{
    auto str = std::string{};
    for (auto& k : keywords) {
        if (k.empty())
            continue;

        if (!str.empty())
            str += ' ';

        str.append(k);
    }

    return str;
}

void App::UpdateQuery()
{
    // queryText.text = JoinQuery();
    // queryText.Layout(*stage, true);
    // queryBox.size.y = queryText.size.y + 20;

    ResetItems();
    Update();
}

void App::Move(int delta)
{
    auto i = delta;
    if (i < 0)
    {
        while (MoveSelectedUp() && ++i < 0);
    }
    else if (i > 0)
    {
        while (MoveSelectedDown() && --i > 0);
    }

    if (delta != i)
    {
        Update();
    }
}

bool App::MoveSelectedUp()
{
    if (items.empty())
        return false;

    if (items.size() < 5)
    {
        if (selection == 0)
            return false;

        selection--;
    }
    else if (selection > 2)
    {
        selection--;
    }
    else
    {
        auto prev = resultList->Prev(items[0].get());
        if (prev)
        {
            std::rotate(items.rbegin(), items.rbegin() + 1, items.rend());
            items[0] = std::move(prev);
        }
        else if (selection > 0)
        {
            selection--;
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool App::MoveSelectedDown()
{
    if (items.empty())
         return false;

    if (items.size() < 5)
    {
        if (selection == items.size() - 1)
            return false;

        selection++;
    }
    else if (selection < 2)
    {
        selection++;
    }
    else
    {
        auto next = resultList->Next(items[items.size() - 1].get());
        if (next)
        {
            std::rotate(items.begin(), items.begin() + 1, items.end());
            items[items.size() - 1] = std::move(next);
        }
        else if (selection < 4)
        {
            selection++;
        }
        else
        {
            return false;
        }
    }

    return true;
}

void App::Draw()
{
    Vec4 backgroundColor = { 0.1f, 0.1f, 0.1f, 1.f };
    Vec4 borderColor =  { 0.6f, 0.6f, 0.6f, 0.5f };
    Vec4 highlightColor = { 0.4f, 0.4f, 0.4f, 0.2f, };

    Vec2 pos = { mWidth * 0.5f, mHeight * 0.5f };

    Vec2 hInputSize = { 960.f, 29.f };

    f32 outputItemHeight = 76.f;
    u32 outputCount = u32(items.size());

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
        .texIndex = emptyTexID,
    });

    // Input text

    {
        auto query = JoinQuery();

        auto size = imDraw->MeasureString(query, font);

        imDraw->DrawString(query,
            pos - Vec2(size.x / 2.f, 17.f),
            font);
    }

    if (items.empty())
        return;

    // Output box

    imDraw->DrawRect({
        .centerColor = backgroundColor,
        .borderColor = borderColor,
        .centerPos = pos + Vec2(0.f, hOutputHeight + margin + borderWidth),
        .halfExtent = Vec2(hOutputWidth, hOutputHeight) + Vec2(borderWidth),
        .cornerRadius = cornerRadius,
        .borderWidth = borderWidth,

        .texTint = { 0.f, 0.f, 0.f, 0.f },
        .texIndex = emptyTexID,
    });

    // Highlight

    imDraw->DrawRect({
        .centerColor = highlightColor,
        .centerPos = pos
            + Vec2(0.f, margin + borderWidth + outputItemHeight * (0.5f + selection)),
        .halfExtent = Vec2(hOutputWidth, outputItemHeight * 0.5f)
            - Vec2(2.f),
        .cornerRadius = cornerRadius - borderWidth - 2.f,

        .texTint = { 0.f, 0.f, 0.f, 0.f },
        .texIndex = emptyTexID,
    });

    for (u32 i = 0; i < outputCount; ++i)
    {
        auto path = items[i]->GetPath();

        // Icon

        IconResult* icon;
        auto iter = iconCache.find(path);
        if (iter == iconCache.end())
        {
            icon = &iconCache[path];
            icon->image = nms::LoadIconFromPath(
                context, commandPool, tracker, queue, fence,
                path.string());

            icon->texID = icon->image
                ? imDraw->RegisterTexture(icon->image, imDraw->defaultSampler)
                : emptyTexID;
        }
        else
        {
            icon = &iter->second;
        }

        imDraw->DrawRect({
            .centerPos = pos
                + Vec2(
                    -hOutputWidth + (iconSize / 2.f) + iconPadding,
                    margin + borderWidth + outputItemHeight * (0.5f + i)),
            .halfExtent = Vec2(iconSize) / 2.f,

            .texTint = { 1.f, 1.f, 1.f, 1.f },
            .texIndex = icon->texID,
            .texCenterPos = { 0.5f, 0.5f },
            .texHalfExtent = { 0.5f, 0.5f },
        });

        // Filename

        imDraw->DrawString(
            path.filename().empty()
                ? path.string()
                : path.filename().string(),
            pos + Vec2(-hOutputWidth, margin + borderWidth)
                + Vec2(0.f, outputItemHeight * i)
                + textInset,
            font);

        // Path

        imDraw->DrawString(
            path.has_parent_path()
                ? path.parent_path().string()
                : path.string(),
            pos + Vec2(-hOutputWidth, margin + borderWidth)
                + Vec2(0.f, outputItemHeight * i)
                + textSmallInset,
            fontSmall);
    }
}

void App::Run()
{
    glfwSetWindowUserPointer(window, this);
    glfwSetCharCallback(window, [](auto w, u32 codepoint) {
        auto app = (App*)glfwGetWindowUserPointer(w);
        app->OnChar(codepoint);
    });
    glfwSetKeyCallback(window, [](auto w, i32 key, i32 scancode, i32 action, i32 mods) {
        (void)scancode;

        auto app = (App*)glfwGetWindowUserPointer(w);
        app->OnKey(key, action, mods);
    });

    auto hwnd = glfwGetWin32Window(window);
    RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_SHIFT, VK_SPACE);

    MSG msg = {};

    while (running)
    {
        show = true;
        glfwShowWindow(window);
        glfwSetWindowShouldClose(window, GLFW_FALSE);

        while (!glfwWindowShouldClose(window))
        {
            while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);

                if (msg.message == WM_HOTKEY)
                    glfwShowWindow(window);

                DispatchMessage(&msg);
            }
            glfwPollEvents();

            imDraw->Reset();
            Draw();

// -----------------------------------------------------------------------------

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

            cmd->BeginRendering({swapchain->image}, {Vec4(1.f / 255.f, 0.f, 0.f, 0.f)}, true);
            imDraw->Record(cmd);
            cmd->EndRendering();

            cmd->Transition(swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0);

            queue->Submit({cmd}, {fence}, {fence});
            queue->Present({swapchain}, {fence});

            if (!show)
                glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        if (!running)
            break;

        glfwHideWindow(window);
        while (GetMessage(&msg, hwnd, 0, 0) && msg.message != WM_HOTKEY);

    }
}

void App::OnChar(u32 codepoint)
{
    auto c = static_cast<char>(codepoint);
    auto& keyword = keywords[keywords.size() - 1];
    if (c == ' ')
    {
        if (keyword.size() == 0 || keywords.size() == 8) return;
        [[maybe_unused]] auto set = static_cast<uint8_t>(1 << keywords.size());
        // tree.setMatchBits(set, set, set, 0);
        // tree.matchBits |= set;
        keywords.push_back("");
    }
    else
    {
        if (c < ' ' || c > '~')
            return;
        // auto matchBit = static_cast<uint8_t>(1 << (keywords.size() - 1));
        keyword += c;
        // filter(matchBit, keyword, true);
        resultList->Query(QueryAction::SET, JoinQuery());
    }
    UpdateQuery();
}

void App::OnKey(u32 key, i32 action, i32 mods)
{
    if (action == GLFW_RELEASE)
        return;

    switch (key)
    {
    break;case GLFW_KEY_ESCAPE:
        if (mods & GLFW_MOD_SHIFT)
            running = false;
        show = false;
    // break;case MouseLButton:
    //     if (overlay::Mouseover(e, queryBox) || overlay::Mouseover(e, resultsBox))
    //     {
    //         std::cout << "  Over query!\n";
    //         Update();
    //         overlay::Focus(*mainLayer);
    //     }

    //     if (menu.visible && overlay::Mouseover(e, menu))
    //     {
    //         std::cout << "Hiding menu!\n";
    //         overlay::Quit(*stage, 0);
    //     }
    //     else
    //     {
    //         menu.visible = false;
    //         overlay::Hide(*menuLayer);
    //     }
    break;case GLFW_KEY_DOWN:
        Move(1);
    break;case GLFW_KEY_UP:
        Move(-1);
    break;case GLFW_KEY_LEFT:
        ResetItems();
        Update();
    break;case GLFW_KEY_RIGHT:
        ResetItems(true);
        Update();
    break;case GLFW_KEY_ENTER: {
        if (!items.empty())
        {
            auto view = items[selection].get();
            auto str = view->GetPath().string();
            std::cout << std::format("Running {}!\n", str);

            favResultList->IncrementUses(view->GetPath());
            ResetQuery();
            show = false;
            // overlay::Hide(*mainLayer);

            // system(("explorer \""+ str +"\"").c_str());

            if ((GetKeyState(VK_LSHIFT) & 0x8000)
                && (GetKeyState(VK_LCONTROL) & 0x8000)) {
                ShellExecuteA(nullptr, "runas", str.c_str(), nullptr, nullptr, SW_SHOW);
            } else {
                ShellExecuteA(nullptr, "open", str.c_str(), nullptr, nullptr, SW_SHOW);
            }
        }
    }
    break;case GLFW_KEY_DELETE:
        if ((mods & GLFW_MOD_SHIFT) && !items.empty())
        {
            auto view = items[selection].get();
            favResultList->ResetUses(view->GetPath());
            ResetQuery();
        }
    break;case GLFW_KEY_BACKSPACE:
        if (mods & GLFW_MOD_SHIFT) {
            ResetQuery();
        }
        else
        {
            auto& keyword = keywords[keywords.size() - 1];
            [[maybe_unused]] auto matchBit = static_cast<uint8_t>(1 << (keywords.size() - 1));
            if (keyword.length() > 0)
            {
                keyword.pop_back();
                // filter(matchBit, keyword, false);
                resultList->Query(QueryAction::SET, JoinQuery());
                UpdateQuery();
            }
            else if (keywords.size() > 1)
            {
                keywords.pop_back();
                // tree.setMatchBits(matchBit, 0, matchBit, 0);
                // tree.matchBits &= ~matchBit;
                resultList->Query(QueryAction::SET, JoinQuery());
                UpdateQuery();
            }
        }
    break;case GLFW_KEY_C:
        if ((mods & GLFW_MOD_CONTROL) && !items.empty())
        {
            auto view = items[selection].get();
            auto str = view->GetPath().string();
            std::cout << std::format("Copying {}!\n", str);

            OpenClipboard(glfwGetWin32Window(window));
            EmptyClipboard();
            auto contentHandle = GlobalAlloc(GMEM_MOVEABLE, str.size() + 1);
            auto contents = GlobalLock(contentHandle);
            memcpy(contents, str.data(), str.size() + 1);
            for (auto c = (char*)contents; *c; ++c)
                *c = *c == '\\' ? '/' : *c;
            GlobalUnlock(contentHandle);
            SetClipboardData(CF_TEXT, contentHandle);
            CloseClipboard();
        }
    break;case GLFW_KEY_F5:
        if (mods & GLFW_MOD_CONTROL)
        {
            static std::atomic_bool indexing = false;

            bool expected = false;
            if (indexing.compare_exchange_strong(expected, true))
            {
                std::thread t([=, this] {
                    AllocConsole();
                    freopen("CONOUT$", "w", stdout);

                    std::vector<char> drives;
                    wchar_t driveNames[1024];
                    GetLogicalDriveStringsW(1023, driveNames);
                    for (wchar_t* drive = driveNames; *drive; drive += wcslen(drive) + 1)
                        drives.push_back((char)drive[0]);

                    for (auto& d : drives)
                    {
                        auto* node = IndexDrive(d);
                        auto saveLoc = std::format("{}\\.nms\\{}.index", getenv("USERPROFILE"), d);
                        std::cout << std::format("Saving to {}\n", saveLoc);
                        (void)node;
                        node->Save(saveLoc);
                    }

                    std::cout << "Indexing complete. Close this window and refresh the index with F5 in app.\n";
                    std::cout.flush();
                    FreeConsole();

                    indexing = false;
                });

                t.detach();
            }
        }
        else
        {
            resultList = std::make_unique<ResultListPriorityCollector>();
            favResultList = std::make_unique<FavResultList>(&keywords);
            fileResultList = std::make_unique<FileResultList>(favResultList.get());
            resultList->AddList(favResultList.get());
            resultList->AddList(fileResultList.get());

            ResetItems();
            UpdateQuery();
        }
    }
}

// void App::OnEvent(const overlay::Event &e)
// {
//     using namespace overlay::Events;

//     // if (e.event() != MouseMoved) {
//     //   std::cout << std::format("Event - {}\n",
//     //       magic_enum::enum_name<EventID>(e.event()));
//     // }

//     switch (e.event)
//     {
//     break;case IconsLoaded:
//         Update();
//     break;case Hotkey:
//         show = true;
//         Update();
//         overlay::Focus(*mainLayer);
//     break;case NotifyContext:
//         {
//             menu.visible = true;
//             menu.anchor = {
//                 overlay::GetScreen(*stage),
//                 overlay::Alignments::TopLeft,
//                 *overlay::GetMousePos(e) + overlay::Vec{50, -10},
//                 overlay::Alignments::BottomRight
//             };

//             overlay::Update(*menuLayer, *this, menu.visible);
//             overlay::Focus(*menuLayer);
//         }
//     break;case FocusLost:
//         if (menu.visible)
//         {
//             std::cout << "Lost menu focus!\n";
//             menu.visible = false;

//             // // Need to hide main window to prevent issue with window layers not being properly adjusted
//             // // This won't be required once the menu is rendered in a different layer!
//             // show = false;
//             // update();

//             overlay::Hide(*menuLayer);
//         }
//     break;case KeyPressed:
//         {
//             using enum overlay::KeyCode;


//         }
//     break;case MouseScroll:
//         Move((int)-e.GetDelta().y);
//     break;case MouseMoved:
//         if (menu.visible)
//         {
//             auto next = overlay::Mouseover(e, menu.highlight);
//             if (next != menu.highlight.visible)
//             {
//                 menu.highlight.visible = next;
//                 overlay::Update(*menuLayer, menu, true);
//             }
//         }
//     }
// }

// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
// int main()
{
    try
    {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        // auto stage = overlay::CreateStage();
        // auto app = App{&stage};

        // return overlay::Run(stage, [&](const overlay::Event& e) {
        //     app.OnEvent(e);
        // });

        App app;
        app.Run();
    }
    catch (std::exception& e)
    {
        std::cout << "Error: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cout << "Something went wrong!";
    }
    return 1;
}
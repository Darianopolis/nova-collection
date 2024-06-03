#undef UNICODE
#define UNICODE

#undef _UNICODE
#define _UNICODE

#undef NOMINMAX
#define NOMINMAX

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <shellapi.h>
#include <combaseapi.h>

// -----------------------------------------------------------------------------

#include "nms_Search.hpp"

#include <nova/core/nova_Core.hpp>
#include <nova/vfs/nova_VirtualFilesystem.hpp>

#include <stb_image.h>

#include <imgui.h>

using namespace std::literals;

App::App()
{
    app = nova::Application::Create();

    window = nova::Window::Create(app)
        .SetTitle("No More Shortcuts")
        .SetDecorate(false)
        .SetTransparency(nova::TransparencyMode::PerPixel)
        .Show(true);

    context = nova::Context::Create({ .debug = false });
    queue = context.Queue(nova::QueueFlags::Graphics, 0);
    draw = std::make_unique<nova::draw::Draw2D>(context);

    {
        char module_filename[4096];
        GetModuleFileNameA(nullptr, module_filename, sizeof(module_filename));
        nova::Log("Module filename: {}", module_filename);
        exe_dir = std::filesystem::path(module_filename).parent_path();
        nova::Log(" exe dir: {}", exe_dir.string());
    }

    create_directories(std::filesystem::path(index_file).parent_path());

    searcher.init(context, queue);

//     // {
//     //     GLFWimage icon_image;

//     //     i32 channels;
//     //     icon_image.pixels = stbi_load("favicon.png", &icon_image.width, &icon_image.height, &channels, STBI_rgb_alpha);
//     //     NOVA_DEFER(&) { stbi_image_free(icon_image.pixels); };

//     //     glfwSetWindowIcon(window, 1, &icon_image);
//     // }

    swapchain = nova::Swapchain::Create(context, window,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Layered,
        nova::SwapchainFlags::PreMultipliedAlpha);

// -----------------------------------------------------------------------------

    auto display = app.PrimaryDisplay();
    window_width = display.Size().x;
    window_height = display.Size().y;

// -----------------------------------------------------------------------------

    font = draw->LoadFont(nova::vfs::Load("SEGUISB.TTF"), 35.f * ui_scale);
    font_small = draw->LoadFont(nova::vfs::Load("SEGOEUI.TTF"), 18.f * ui_scale);

// -----------------------------------------------------------------------------

    keywords.push_back("");

    using namespace std::chrono;

    result_list = std::make_unique<ResultListPriorityCollector>();
    fav_result_list = std::make_unique<FavResultList>();
    file_result_list = std::make_unique<FileResultList>(&searcher, fav_result_list.get());
    result_list->AddList(fav_result_list.get());
    result_list->AddList(file_result_list.get());

    show = false;

    UpdateIndex();
    ResetItems();
    UpdateQuery();
}

static bool mutable_true = true;

App::~App()
{
    queue.WaitIdle();
    nms::ClearIconCache();
    items.clear();
    result_list.reset();
    fav_result_list.reset();
    file_result_list.reset();
    font_small.reset();
    font.reset();
    swapchain.Destroy();
    searcher.destroy();
    draw.reset();
    context.Destroy();
    app.Destroy();
}

void App::ResetItems(bool end)
{
    items.clear();
    if (end)
    {
        auto item = result_list->Prev(nullptr);
        if (item)
        {
            auto itemP = item.get();
            items.push_back(std::move(item));
            while ((items.size() < 5) && (item = result_list->Prev(itemP)))
            {
                itemP = item.get();
                items.push_back(std::move(item));
            }
            std::ranges::reverse(items);
        }
        selection = u32(items.size() - 1);
    }
    else
    {
        auto item = result_list->Next(nullptr);
        if (item)
        {
            auto itemP = item.get();
            items.push_back(std::move(item));
            while ((items.size() < 5) && (item = result_list->Next(itemP)))
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
    result_list->FilterStrings(keywords);
    UpdateQuery();
}

std::string App::JoinQuery() const
{
    auto str = std::string {};
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
    ResetItems();
}

void App::Move(i32 delta)
{
    auto i = delta;
    if (i < 0) {
        while (MoveSelectedUp() && ++i < 0);
    } else if (i > 0) {
        while (MoveSelectedDown() && --i > 0);
    }
}

bool App::MoveSelectedUp()
{
    if (items.empty()) {
        return false;
    }

    if (items.size() < 5) {
        if (selection == 0) {
            return false;
        }

        selection--;

    } else if (selection > 2) {
        selection--;
    } else {
        auto prev = result_list->Prev(items[0].get());
        if (prev) {
            std::rotate(items.rbegin(), items.rbegin() + 1, items.rend());
            items[0] = std::move(prev);

        } else if (selection > 0) {
            selection--;
        } else {
            return false;
        }
    }

    return true;
}

bool App::MoveSelectedDown()
{
    if (items.empty()) {
        return false;
    }

    if (items.size() < 5) {
        if (selection == items.size() - 1) {
            return false;
        }

        selection++;

    } else if (selection < 2) {
        selection++;
    } else {
        auto next = result_list->Next(items[items.size() - 1].get());
        if (next) {
            std::rotate(items.begin(), items.begin() + 1, items.end());
            items[items.size() - 1] = std::move(next);

        } else if (selection < 4) {
            selection++;
        } else {
            return false;
        }
    }

    return true;
}

void App::Draw()
{
    Vec4 background_color = { 0.1f, 0.1f, 0.1f, 1.f };
    Vec4 border_color =  { 0.6f, 0.6f, 0.6f, 0.5f };
    Vec4 highlight_color = { 0.4f, 0.4f, 0.4f, 0.2f, };

    Vec2 pos = { window_width * 0.5f, window_height * 0.5f };

    Vec2 half_input_size = Vec2 { 960.f, 29.f } * ui_scale;

    f32 output_item_height = 76.f * ui_scale;
    u32 output_count = u32(items.size());

    f32 half_output_width = 600.f * ui_scale;
    f32 half_output_height = 0.5f * output_item_height * output_count;

    f32 margin = 18.f * ui_scale;

    f32 corner_radius = 18.f * ui_scale;
    f32 border_width = 2.f * ui_scale;

    Vec2 text_inset = Vec2 { 74.5f, 37.f } * ui_scale;
    Vec2 text_small_inset = Vec2 { 76.f, 60.f } * ui_scale;

    f32 icon_size = 50 * ui_scale;
    f32 icon_padding = (output_item_height - icon_size) / 2.f;

    f32 input_text_vertical_offset = 17.f * ui_scale;

    f32 highlight_inset = 2.f * ui_scale;

    // Input box

    draw->DrawRect({
        .center_color = background_color,
        .border_color = border_color,
        .center_pos = pos - Vec2(0.f, half_input_size.y),
        .half_extent = half_input_size + Vec2(border_width),
        .corner_radius = corner_radius,
        .border_width = border_width,
    });

    // Input text

    {
        auto query = JoinQuery();
        auto bounds = draw->MeasureString(query, *font);

        if (!bounds.Empty())
        {
            draw->DrawString(query,
                pos - Vec2(bounds.Width() * 0.5f, input_text_vertical_offset),
                *font);
        }
    }

    if (items.empty())
        return;

    // Output box

    draw->DrawRect({
        .center_color = background_color,
        .border_color = border_color,
        .center_pos = pos + Vec2(0.f, half_output_height + margin + border_width),
        .half_extent = Vec2(half_output_width, half_output_height) + Vec2(border_width),
        .corner_radius = corner_radius,
        .border_width = border_width,
    });

    // Highlight

    draw->DrawRect({
        .center_color = highlight_color,
        .center_pos = pos
            + Vec2(0.f, margin + border_width + output_item_height * (0.5f + f32(selection))),
        .half_extent = Vec2(half_output_width, output_item_height * 0.5f)
            - Vec2(highlight_inset),
        .corner_radius = corner_radius - border_width - highlight_inset,
    });

    for (u32 i = 0; i < output_count; ++i)
    {
        auto path = items[i]->GetPath();

        // Icon

        IconResult* icon;
        auto iter = icon_cache.find(path);
        if (iter == icon_cache.end())
        {
            icon = &icon_cache[path];
            icon->texture = nms::LoadIconFromPath(context, path.string());
        }
        else
        {
            icon = &iter->second;
        }

        if (icon->texture)
        {
            draw->DrawRect({
                .center_pos = pos
                    + Vec2(-half_output_width + (icon_size / 2.f) + icon_padding,
                        margin + border_width + output_item_height * (0.5f + f32(i))),
                .half_extent = Vec2(icon_size) / 2.f,

                .tex_tint = Vec4(1.f),
                .tex_handle = {icon->texture.Descriptor(), draw->default_sampler.Descriptor()},
                .tex_center_pos = { 0.5f, 0.5f },
                .tex_half_extent = { 0.5f, 0.5f },
            });
        }

        // Filename

        draw->DrawString(
            path.filename().empty()
                ? path.string()
                : path.filename().string(),
            pos + Vec2(-half_output_width, margin + border_width)
                + Vec2(0.f, output_item_height * f32(i))
                + text_inset,
            *font);

        // Path

        draw->DrawString(
            path.has_parent_path()
                ? path.parent_path().string()
                : path.string(),
            pos + Vec2(-half_output_width, margin + border_width)
                + Vec2(0.f, output_item_height * f32(i))
                + text_small_inset,
            *font_small);
    }
}

void App::Run()
{
    app.AddCallback([&](const nova::AppEvent& e) {
        switch (e.type) {
            break;case nova::EventType::Text:
                for (const char* c = e.text.text; *c; c++) {
                    OnChar(*c);
                }
            break;case nova::EventType::Input:
                {
                    auto vk = app.ToVirtualKey(e.input.channel);
                    OnKey(vk, e.input.pressed);
                }
            break;case nova::EventType::Hotkey:
                nova::Log("Received hotkey event!");
                window.Show(true);
        }
    });

    // TODO: This should all be handled by nova
    RegisterHotKey((HWND)window.NativeHandle(), 1, MOD_CONTROL | MOD_SHIFT, VK_SPACE);

    show = true;

    for (;app.ProcessEvents(); show = true) {

        if (!running) {
            nova::Log("Destroying window!");
            window.Destroy();
            continue;
        }

        if (!show) {
            nova::Log("Hiding window!");
            window.Show(false);
            continue;
        }

        // TODO: This should both be in the windowing API
        if (!IsWindowVisible((HWND)window.NativeHandle()) || window.Minimized()) {
            app.WaitForEvents();
            continue;
        }

        draw->Reset();
        Draw();

        // Wait for frame

        queue.WaitIdle();

        // Record commands

        auto cmd = queue.Begin();

        // Update window size, record primary buffer and present

        // Window borders need to lie on pixel boundaries
        draw->bounds.Expand({
            .min = glm::floor(draw->bounds.min),
            .max = glm::ceil(draw->bounds.max),
        });

        window.SetSize({u32(draw->Bounds().Width()), u32(draw->Bounds().Height())}, nova::WindowPart::Client);
        window.SetPosition({i32(draw->Bounds().min.x), i32(draw->Bounds().min.y)}, nova::WindowPart::Client);

        queue.Acquire({swapchain}, {});

        cmd.ClearColor(swapchain.Target(), Vec4(0.f, 1/255.f, 0.f, 0.f));
        draw->Record(cmd, swapchain.Target());

        cmd.Present(swapchain);

        queue.Submit({cmd}, {});
        queue.Present({swapchain}, {});
    }

    nova::Log("Complete, exiting");
}

void App::OnChar(u32 codepoint)
{
    auto c = static_cast<c8>(codepoint);
    auto& keyword = keywords[keywords.size() - 1];
    if (c == ' ')
    {
        if (keyword.size() == 0 || keywords.size() == 8)
            return;
        keywords.push_back("");
    }
    else
    {
        if (c < ' ' || c > '~')
            return;
        keyword += c;
        result_list->FilterStrings(keywords);
    }
    UpdateQuery();
}

void App::UpdateIndex()
{
    if (std::filesystem::exists(index_file)) {
        load_index(index, index_file.c_str());
    } else {
        index_filesystem(index);
        sort_index(index);
        save_index(index, index_file.c_str());
    }
    searcher.set_index(index);
    file_result_list->FilterStrings(keywords);
}

void App::OnKey(nova::VirtualKey key, bool pressed)
{
    auto shift = app.IsVirtualKeyDown(nova::VirtualKey::LeftShift) || app.IsVirtualKeyDown(nova::VirtualKey::RightShift);
    auto ctrl = app.IsVirtualKeyDown(nova::VirtualKey::LeftControl) || app.IsVirtualKeyDown(nova::VirtualKey::RightControl);

    if (!pressed)
        return;

    switch (key)
    {
    break;case nova::VirtualKey::Escape:
        if (shift)
            running = false;
        show = false;
    break;case nova::VirtualKey::Down:
        Move(1);
    break;case nova::VirtualKey::Up:
        Move(-1);
    break;case nova::VirtualKey::Left:
        ResetItems();
    break;case nova::VirtualKey::Right:
        ResetItems(true);
    break;case nova::VirtualKey::Enter: {
        if (!items.empty())
        {
            auto view = items[selection].get();
            auto str = view->GetPath().string();
            nova::Log("Running {}!", str);

            NOVA_STACK_POINT();

            fav_result_list->IncrementUses(view->GetPath());
            ResetQuery();
            show = false;

            if ((GetKeyState(VK_LSHIFT) & 0x8000)
                    && (GetKeyState(VK_LCONTROL) & 0x8000)) {
                ShellExecuteA(
                    nullptr,
                    "open",
                    (exe_dir / "nms-launch.exe").string().c_str(),
                    nova::Fmt("runas \"{}\"", str).c_str(),
                    nullptr,
                    SW_SHOW);
            } else if (GetKeyState(VK_LCONTROL) & 0x8000) {
                // open selected in explorer
                std::system(
                    nova::Fmt("explorer /select, \"{}\"", str.c_str()).c_str());
            } else {
                // open
                auto params = nova::Fmt("open \"{}\"", str);
                ShellExecuteA(
                    nullptr,
                    "open",
                    (exe_dir / "nms-launch.exe").string().c_str(),
                    params.data(),
                    nullptr,
                    SW_SHOW);
            }
        }
    }
    break;case nova::VirtualKey::Delete:
        if (shift && !items.empty())
        {
            auto* view = items[selection].get();
            fav_result_list->ResetUses(view->GetPath());
            ResetQuery();
        }
    break;case nova::VirtualKey::Backspace:
        if (shift) {
            ResetQuery();
        }
        else
        {
            auto& keyword = keywords[keywords.size() - 1];
            [[maybe_unused]] auto matchBit = static_cast<u8>(1 << (keywords.size() - 1));
            if (keyword.length() > 0)
            {
                keyword.pop_back();
                // filter(matchBit, keyword, false);
                result_list->FilterStrings(keywords);
                UpdateQuery();
            }
            else if (keywords.size() > 1)
            {
                keywords.pop_back();
                // tree.setMatchBits(matchBit, 0, matchBit, 0);
                // tree.matchBits &= ~matchBit;
                result_list->FilterStrings(keywords);
                UpdateQuery();
            }
        }
    break;case nova::VirtualKey::C:
        if (ctrl && !items.empty())
        {
            auto view = items[selection].get();
            auto str = view->GetPath().string();
            nova::Log("Copying {}!", str);

            fav_result_list->IncrementUses(view->GetPath());
            OpenClipboard(HWND(window.NativeHandle()));
            EmptyClipboard();
            auto contentHandle = GlobalAlloc(GMEM_MOVEABLE, str.size() + 1);
            auto contents = GlobalLock(contentHandle);
            memcpy(contents, str.data(), str.size() + 1);
            for (auto c = (c8*)contents; *c; ++c)
                *c = *c == '\\' ? '/' : *c;
            GlobalUnlock(contentHandle);
            SetClipboardData(CF_TEXT, contentHandle);
            CloseClipboard();
        }
    break;case nova::VirtualKey::F5:
        nova::Log("F5 pressed!");
        if (ctrl)
        {
            nova::Log("  re-indexing");
            ::ShellExecuteA(nullptr, "open", (exe_dir / "nms-index.exe").string().c_str(), nullptr, nullptr, SW_SHOW);
        }
        else
        {
            result_list = std::make_unique<ResultListPriorityCollector>();
            fav_result_list = std::make_unique<FavResultList>();
            file_result_list = std::make_unique<FileResultList>(&searcher, fav_result_list.get());
            result_list->AddList(fav_result_list.get());
            result_list->AddList(file_result_list.get());

            UpdateIndex();
            ResetItems();
            UpdateQuery();
        }
    }
}

// -----------------------------------------------------------------------------
//                               Entry point
// -----------------------------------------------------------------------------

static bool has_console;
void LogError(std::string_view message)
{
    if (has_console) {
        nova::Log(message);
    } else {
        MessageBoxW(nullptr, nova::ToUtf16(message).c_str(), L"NoMoreShortcuts - Error", MB_OK);
    }
}

void Main()
{
    try
    {
        App app;
        app.Run();
    }
    catch(const std::exception& exception)
    {
        LogError(nova::Fmt("Error\n{}", exception.what()));
    }
    catch(...)
    {
        LogError("Unknown error");
    }

    nova::Log("at end of main!");
}

i32 WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, i32)
{
    has_console = false;
    Main();
    return 0;
}

i32 main()
{
    has_console = true;
    Main();
    return 0;
}
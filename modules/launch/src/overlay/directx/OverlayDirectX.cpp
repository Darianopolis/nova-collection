#include <optional>

// #define WIN32_LEAN_AND_MEAN
// #define NOMINMAX
// #include <Windows.h>
// #include <windowsx.h>
// #include <d2d1.h>
// #include <dwrite.h>
// #include <shellapi.h>
// #include <wincodec.h>
// #include <CommCtrl.h>

#include "Win32Include.hpp"

#include <chrono>
#include <thread>
#include <condition_variable>
#include <future>
#include <ranges>

#include "Overlay.hpp"
#include "OverlayDirectX.hpp"
#include "ScopeGuards.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

constexpr UINT WMAPP_REDRAW_ICONS   = WM_APP + 1;
constexpr UINT WMAPP_NOTIFYCALLBACK = WM_APP + 2;

namespace overlay
{
    void ConvertToWString(std::string_view input, std::wstring& output)
    {
        int length = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, input.data(), int(input.size()), nullptr, 0);
        output.resize(length);
        MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, input.data(), int(input.size()), output.data(), length);
    }

    std::wstring ConvertToWString(std::string_view input)
    {
        std::wstring str;
        ConvertToWString(input, str);
        return str;
    }

    void ConvertToString(std::wstring_view input, std::string& output)
    {
        BOOL usedDefault = FALSE;
        int length = WideCharToMultiByte(CP_UTF8, MB_PRECOMPOSED, input.data(), int(input.size()), nullptr, 0, "?", &usedDefault);
        output.resize(length);
        WideCharToMultiByte(CP_UTF8, MB_PRECOMPOSED, input.data(), int(input.size()), output.data(), length, "?", &usedDefault);
    }

    std::string ConvertToString(std::wstring_view input)
    {
        std::string str;
        ConvertToString(input, str);
        return str;
    }

    // ---------------------------------------- //
    // ------------- Icon Loading ------------- //
    // ---------------------------------------- //

    // CachedIcon::~CachedIcon() {}

    IconLoader::IconLoader()
    {
        for (auto i{0}; i < 2; ++i)
            threads.push_back(Worker());
    }

    std::thread IconLoader::Worker()
    {
        return std::thread([&]() {
            CoInitialize(nullptr);

            while (true)
            {
                auto lock = std::unique_lock{mutex};

                while (queue.empty())
                    cv.wait(lock);

                auto request = queue.front();
                queue.pop();

                if (request->path.empty())
                    break; // Empty path treated as quit message

                // FIXME -
                if (icons.contains(request->path))
                    continue; // Don't double load icons
                icons[request->path] = request->future;

                lock.unlock();

                auto path = ConvertToWString(request->path);

                // using namespace std::chrono_literals;
                // std::this_thread::sleep_for(50ms);

                using IconPtr = std::unique_ptr<HICON__,
                    decltype([](HICON i) { if (i) DestroyIcon(i); })>;

                auto icon = IconPtr{};
                auto info = SHFILEINFO{};
                auto list = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(
                    path.c_str(),
                    FILE_ATTRIBUTE_NORMAL,
                    &info,
                    sizeof(info),
                    SHGFI_SYSICONINDEX));

                if (list)
                {
                    icon = IconPtr{ImageList_GetIcon(list, info.iIcon, ILD_NORMAL)};
                    ImageList_Destroy(list);
                }

                if (!icon)
                {
                    SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),
                        SHGFI_ICON | SHGFI_LARGEICON);
                    icon = IconPtr{info.hIcon};
                }

                auto bitmap = ComPtr<ID2D1Bitmap>{};

                if (icon)
                {
                    auto wic = stage->wic.Get();

                    auto wicBitmap = ComPtr<IWICBitmap>{};
                    wic->CreateBitmapFromHICON(icon.get(), wicBitmap.GetAddressOf());

                    auto converter = ComPtr<IWICFormatConverter>{};
                    wic->CreateFormatConverter(converter.GetAddressOf());

                    converter->Initialize(
                        wicBitmap.Get(),
                        GUID_WICPixelFormat32bppPBGRA,
                        WICBitmapDitherTypeNone,
                        nullptr, 0,
                        WICBitmapPaletteTypeMedianCut);

                    auto props = D2D1_BITMAP_PROPERTIES{
                        .pixelFormat = {
                            .format = DXGI_FORMAT_B8G8R8A8_UNORM,
                            .alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED},
                        .dpiX = 96,
                        .dpiY = 96};

                    renderTarget->CreateBitmapFromWicBitmap(
                        converter.Get(),
                        &props,
                        bitmap.GetAddressOf());
                }

                lock.lock();
                // request->icon = bitmap.get();
                // icons[request->path] = { { std::move(bitmap) }, 0 };
                {
                    std::lock_guard future_lock{request->future->mutex};
                    request->future->icon = std::move(bitmap);
                }
                request->cv.notify_one();

                if (!request->silent)
                    PostMessageW(hWnd, WMAPP_REDRAW_ICONS, 0, 0);

                request->silent = false;
            }

            CoUninitialize();
        });
    }

    IconLoader::~IconLoader()
    {
        for (auto& t : threads)
        {
            std::lock_guard lock { mutex };
            queue.push(std::make_shared<IconRequest>("", false, nullptr));
        }
        cv.notify_all();

        for (auto& t : threads)
            t.join();
    }

    std::shared_ptr<IconFuture> IconLoader::Load(std::string path, bool silent)
    {
        auto lock = std::unique_lock{mutex};

        if (auto result = icons.find(path); result != icons.end())
        {
            result->second->age = 0;
            return result->second;
        }

        auto future = std::make_shared<IconFuture>();
        auto request = std::make_shared<IconRequest>(std::move(path), silent, future);

        queue.push(request);
        cv.notify_one();

        using namespace std::chrono_literals;

        // Limited timeout search
        if (blockWait && !silent)
        {
            request->silent = true;
            if (request->cv.wait_for(lock, 1ms) == std::cv_status::timeout || request->silent)
            {
                // request->silent implies spurious wakeup
                blockWait = false;
                request->silent = false;
            }
        }

        return future;
    }

    void IconLoader::Clear()
    {
        auto lock = std::lock_guard{mutex};
        icons.clear();
    }

    void IconLoader::NewFrame(Frame& frame)
    {
        blockWait = true;
        stage = frame.stage;
        hWnd = frame.layer->hWnd;
        if (version != frame.version)
        {
            version = frame.version;
            renderTarget = frame.renderTarget;
            icons.clear();
        }

        auto lock = std::unique_lock{mutex};
        while (queue.size() > 5)
            queue.pop();

        for (auto i = std::begin(icons); i != std::end(icons);)
        {
            i = (i->second.use_count() == 0 && i->second->age++ > 10)
                ? icons.erase(i)
                : ++i;
        }
    }

    // ---------------------------------- //
    // ------------- Events ------------- //
    // ---------------------------------- //

    bool KeyDown(const Event& event, KeyCode code)
    {
        auto state = GetAsyncKeyState(static_cast<int>(code));
        return (1 << 15) & state;
    }

    // --------------------------------- //
    // ------------- Frame ------------- //
    // --------------------------------- //

    Frame::Frame(Layer* _layer, bool _sticky, Rect _bounds)
        : layer(_layer)
        , bounds(_bounds)
        , stage(_layer->stage)
        , version(_layer->stage->renderTargetVersion)
        , sticky(_sticky)
        , screenPos{_bounds.left, _bounds.top}
    {

        auto width = static_cast<int>(bounds.right - bounds.left);
        auto height = static_cast<int>(bounds.bottom - bounds.top);

        rect = { 0, 0, width, height };
        hdcScreen = GetDC(nullptr);
        hdc = CreateCompatibleDC(hdcScreen);
        bitmap = CreateCompatibleBitmap(hdcScreen, width, height);
        bitmapOld = static_cast<HBITMAP>(SelectObject(hdc, bitmap));

        if (!stage->BindDC(hdc, rect) && !stage->BindDC(hdc, rect))
        {
            std::cout << "Failed to bind!\n";
            return;
        }

        renderTarget = stage->renderTarget.Get();
        version = stage->renderTargetVersion;

        renderTarget->BeginDraw();
        // std::cout << "Bound, pRenderTarget = " << pRenderTarget << '\n';
        drawing = true;

        stage->iconLoader.NewFrame(*this);
    }

    Frame CreateFrame(Layer& layer, bool sticky, Rect bounds)
    {
        return Frame(&layer, sticky, bounds);
    }

    bool Drawable(Frame& frame)
    {
        return frame.CanDraw();
    }

    void Push(Frame& frame)
    {
        frame.Push();
    }

    bool Frame::CanDraw()
    {
        return drawing;
    }

    void Frame::Push()
    {
        if (!drawing)
            return;

        auto width = static_cast<int>(bounds.right - bounds.left);
        auto height = static_cast<int>(bounds.bottom - bounds.top);

        // Draw windows bounds in debug mode
        if (stage->debug)
        {
            auto brush = ComPtr<ID2D1SolidColorBrush>{};
            renderTarget->CreateSolidColorBrush(D2D1::ColorF(0,0,0,1), brush.GetAddressOf());
            renderTarget->DrawRectangle(D2D1::RectF(0, 0, width, height), brush.Get(), 5);
        }

        // Draw bitmap to window and update Layer bounds
        renderTarget->EndDraw();
        drawing = false;

        // std::cout << "Bounds = " << bounds << '\n';

        // https://duckmaestro.com/2010/06/06/per-pixel-alpha-blending-in-win32-desktop-applications/
        auto blend = BLENDFUNCTION{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        auto ptPos = POINT{static_cast<LONG>(bounds.left), static_cast<LONG>(bounds.top)};
        auto sizeWnd = SIZE{width, height};
        auto ptSrc = POINT{0, 0};
        UpdateLayeredWindow(layer->hWnd, hdcScreen, &ptPos, &sizeWnd,
            hdc, &ptSrc, 0, &blend, ULW_ALPHA);
        layer->bounds = bounds;

        // Show window. This must happen after updating the window layer to prevent flickering!
        if (!IsWindowVisible(layer->hWnd))
        {
            std::cout << "showing window!\n";
            ShowWindow(layer->hWnd, 1);
        }
        auto toFront = SetWindowPos(
            layer->hWnd,
            sticky ? HWND_TOPMOST : HWND_NOTOPMOST,
            0, 0, 0, 0,
            SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
    }

    Frame::~Frame()
    {
        if (drawing)
            stage->renderTarget->EndDraw();

        SelectObject(hdc, bitmapOld);
        DeleteObject(bitmap);
        DeleteObject(hdc);
        ReleaseDC(nullptr, hdcScreen);
    }

    // --------------------------------- //
    // ------------- Color ------------- //
    // --------------------------------- //

    inline ID2D1SolidColorBrush* get_brush(const Frame& frame, Color c)
    {
        frame.stage->brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a));
        return frame.stage->brush.Get();
    }

    void Frame::Box(
            const Node& node,
            Color bg,
            Color border,
            float borderWidth,
            float cornerRadius)
    {

        auto p = node.PointAt(Alignments::TopLeft) - screenPos;

        auto hBSize = 0.5f * borderWidth - 0.1f;

        renderTarget->DrawRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(
                    p.x - hBSize,
                    p.y - hBSize,
                    p.x + node.size.x + hBSize,
                    p.y + node.size.y + hBSize),
                cornerRadius + hBSize, cornerRadius + hBSize),
            get_brush(*this, border),
            borderWidth + 0.2);

        renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(
                    p.x,
                    p.y,
                    p.x + node.size.x,
                    p.y + node.size.y),
                cornerRadius, cornerRadius),
            get_brush(*this, bg));
    }

    // --------------------------------- //
    // ------------- Stage ------------- //
    // --------------------------------- //

    bool Stage::BindDC(HDC hdc, RECT &rc)
    {
        if (!renderTarget)
        {
            const auto properties = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                0, 0, D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE, D2D1_FEATURE_LEVEL_DEFAULT);

            renderTargetVersion = GetUID();

            if (FAILED(d2d1->CreateDCRenderTarget(&properties, renderTarget.GetAddressOf())))
                return false;

            renderTarget->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0), brush.ReleaseAndGetAddressOf());
        }

        if (FAILED(renderTarget->BindDC(hdc, &rc)))
        {
            renderTarget.Reset();
            return false;
        }

        return true;
    }

    void Stage::InitDirectX()
    {
        D2D1CreateFactory(
            D2D1_FACTORY_TYPE_MULTI_THREADED,
            d2d1.GetAddressOf());

        DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(dwrite.GetAddressOf()));

        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
            __uuidof(IWICImagingFactory),
            reinterpret_cast<void**>(wic.GetAddressOf()));
    }

    void Stage::UpdateScreens()
    {
        screen.size = Vec{
            static_cast<float>(GetSystemMetrics(SM_CXSCREEN)),
            static_cast<float>(GetSystemMetrics(SM_CYSCREEN))
        };
        screen.anchor = Anchor{&screen, Alignments::TopLeft, Vec{0, 0}, Alignments::TopLeft};
        screen.position = Vec{0, 0};
    }

    int Stage::InitWin32()
    {
        instance = GetModuleHandleW(nullptr);
        CoCreateGuid(&guid);

        // Setup mouse tracking
        mouse_pos = std::optional<Vec>();

        // Icon
        auto info = SHFILEINFO{};
        auto list = reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(
            L"favicon.ico",
            FILE_ATTRIBUTE_NORMAL, &info, sizeof(info), SHGFI_SYSICONINDEX));
        if (list)
            icon = ImageList_GetIcon(list, info.iIcon, ILD_NORMAL);

        auto wc = WNDCLASSEXW{
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = WndProc,
            .hInstance = instance,
            .hCursor = LoadCursorW(nullptr, IDC_ARROW),
            .lpszClassName = L"Layer",
        };

        if (!RegisterClassExW(&wc))
        {
            auto error = GetLastError();
            std::cout << "Failed to register class - " << error << '\n';
            return error;
        }

        return 0;
    }

    int Stage::MakeWindow()
    {
        for (auto i = 0; i < 2; ++i)
        {
            auto name = ConvertToWString(std::format("Layer{}", i));
            std::wcout << L"Creating window - " << name << L'\n';

            auto hWnd = CreateWindowExW(
                WS_EX_LAYERED
                    | WS_EX_NOACTIVATE,
                L"Layer",
                name.c_str(),
                WS_POPUP,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                nullptr, nullptr, instance, this);

            if (!hWnd)
            {
                auto error = GetLastError();
                std::cout << "Failed to register class - " << error << '\n';
                return error;
            }

            layers.emplace_back(new Layer{this, hWnd, Rect{}});
        }

        mouseTrack.hwndTrack = layers.front()->hWnd;
        TrackMouseEvent(&mouseTrack);

        return 0;
    }

    void Stage::CreateTrayIcon(std::string_view tooltip)
    {
        // Create System tray icon
        // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataw
        auto nid = NOTIFYICONDATAW{
            .cbSize = sizeof(NOTIFYICONDATAW),
            .hWnd = layers.front()->hWnd,
            .uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID,
            .uCallbackMessage = WMAPP_NOTIFYCALLBACK,
            .hIcon = icon,
            .guidItem = guid
        };

        auto wtip = ConvertToWString(tooltip);

        memcpy(&nid.szTip, wtip.c_str(), (wtip.length() + 1) * 2);

        Shell_NotifyIconW(NIM_ADD, &nid);
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }

    // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples
    //       /Win7Samples/winui/shell/appshellintegration
    //       /NotificationIcon/NotificationIcon.cpp
    // Use a guid to uniquely identify our icon
    // class __declspec(uuid("9D0B8B92-4E1C-488e-A1E1-2331AFCE2CB5")) PrinterIcon;

    Stage::Stage()
        : mouseTrack{ .cbSize = sizeof(mouseTrack), .dwFlags = TME_LEAVE }
    {
        CoInitialize(nullptr);
        SetProcessDPIAware();

        InitDirectX();
        InitWin32();
        UpdateScreens();
        MakeWindow();
        CreateTrayIcon("OverlayTooltip");
    }

    Stage::~Stage()
    {
        for (auto& l : layers)
        {
            if (l && l->hWnd)
                DestroyWindow(l->hWnd);
        }

        CoUninitialize();
    }

    void Stage::Quit(int code)
    {
        PostQuitMessage(code);
    }

    Layer* CreateLayer(Stage& stage, uint32_t id)
    {
        return stage.GetLayer(id);
    }

    Node* GetScreen(Stage& stage)
    {
        return &stage.screen;
    }

    void AddHotkeyImpl(Stage& stage, uint32_t id, KeyCode key, uint32_t modifier)
    {
        RegisterHotKey(stage.layers.front()->hWnd, id, modifier, static_cast<UINT>(key));
    }

    void RemoveHotkey(Stage& stage, uint32_t id)
    {
        UnregisterHotKey(stage.layers.front()->hWnd, id);
    }

    LRESULT CALLBACK Stage::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Stage* pThis = nullptr;

        if (msg == WM_NCCREATE)
        {
            auto pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = static_cast<Stage*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        }
        else
        {
            pThis = reinterpret_cast<Stage*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        return pThis
            ? pThis->HandleMessage(hwnd, msg, wParam, lParam)
            : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT Stage::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // std::cout << std::format("msg = {:#x}\n", msg);

        auto layerIter = std::ranges::find_if(layers, [&](auto& l) { return l->hWnd == hWnd; });
        if (layerIter == std::end(layers))
        {
            std::cout << "Not a managed window!\n";
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
        auto layer = layerIter->get();

        // std::cout << "Message, HWND = " << layer->hWnd << " bounds = " << layer->bounds << '\n';

        switch (msg)
        {
        break;case WM_HOTKEY:
            SendEvent(Event{layer, this, EventCategory::Stage, EventID::Hotkey, static_cast<uint32_t>(wParam)});
        break;case WMAPP_REDRAW_ICONS:
            // std::cout << "Drawing icons!\n";
            SendEvent(Event{layer, this, EventCategory::Stage, EventID::IconsLoaded, 0u});
        break;case WMAPP_NOTIFYCALLBACK:
            {
                // https://stackoverflow.com/questions/41649303/difference-between-notifyicon-version-and-notifyicon-version-4-used-in-notifyico
                // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataw
                auto notifyEvent = LOWORD(lParam);
                auto x = static_cast<float>(GET_X_LPARAM(wParam));
                auto y = static_cast<float>(GET_Y_LPARAM(wParam));
                mouse_pos = Vec { x, y };
                switch (notifyEvent)
                {
                break;case NIN_SELECT:
                    SendEvent(Event{layer, this, EventCategory::Stage, EventID::NotifySelect});
                break;case WM_CONTEXTMENU:
                    SendEvent(Event{layer, this, EventCategory::Stage, EventID::NotifyContext});
                }
            }
        break;case WM_DESTROY:
            PostQuitMessage(0);
        break;case WM_KEYDOWN:
            SendEvent(Event{layer, this, EventCategory::Button, EventID::KeyPressed, static_cast<KeyCode>(wParam)});
        break;case WM_KEYUP:
            SendEvent(Event{layer, this, EventCategory::Button, EventID::KeyReleased, static_cast<KeyCode>(wParam)});
        break;case WM_CHAR:
            SendEvent(Event{layer, this, EventCategory::Button, EventID::CharTyped, static_cast<uint32_t>(wParam)});
        // break;case WM_MOUSEACTIVATE: {
        //   auto hitTest = LOWORD(lParam);
        //   std::cout << "WM_MOUSEACTIVATE - layer = " << layer << '\n';
        //   if (hitTest == HTCLIENT) {
        //     // TODO - Allow the use to specify whether to activate a window
        //     // TODO - Differentiate between different forms of the MouseActivate Event
        //     std::cout << "  Activate!\n";
        //     sendEvent(Event{layer, this, EventCategory::Stage, EventID::MouseActivate});
        //     return MA_ACTIVATE;
        //   } else {
        //     return MA_NOACTIVATE;
        //   }
        // }
        // break;case WM_ACTIVATE:
        //   if (wParam != WA_INACTIVE) {
        //     std::cout << "WM_ACTIVATE - layer = " << layer << '\n';
        //     sendEvent(Event{layer, this, EventCategory::Stage, EventID::MouseActivate});
        //   }
        // break;case WM_NCACTIVATE:
        //   sendEvent(Event{*this, EventCategory::Stage, EventID::WindowActivate});
        break;case WM_LBUTTONDOWN:
            SendEvent(Event{layer, this, EventCategory::Button, EventID::KeyPressed, KeyCode::MouseLButton});
        break;case WM_LBUTTONUP:
            SendEvent(Event{layer, this, EventCategory::Button, EventID::KeyReleased, KeyCode::MouseLButton});
        break;case WM_MOUSELEAVE:
            SendEvent(Event{layer, this, EventCategory::Mouse, EventID::MouseLeave});
            mouse_pos = std::optional<Vec>();
        break;case WM_MOUSEMOVE:
            {
                auto newMouse = Vec {
                    static_cast<float>(GET_X_LPARAM(lParam)) + layer->bounds.left,
                    static_cast<float>(GET_Y_LPARAM(lParam)) + layer->bounds.top
                };

                if (!mouse_pos.has_value())
                {
                    mouseTrack.hwndTrack = layer->hWnd;
                    TrackMouseEvent(&mouseTrack);
                    SetCapture(hWnd);
                    SendEvent(Event{layer, this, EventCategory::Mouse, EventID::MouseEnter, Vec { 0, 0 }});
                }

                SendEvent(Event{layer, this, EventCategory::Mouse, EventID::MouseMoved,
                    newMouse - mouse_pos.value_or(newMouse)});
                mouse_pos = newMouse;
            }
        break;case WM_MOUSEWHEEL:
              case WM_MOUSEHWHEEL:
            {
                auto newMouse = Vec {
                    static_cast<float>(GET_X_LPARAM(lParam)) + layer->bounds.left,
                    static_cast<float>(GET_Y_LPARAM(lParam)) + layer->bounds.top
                };

                auto delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
                if (msg == WM_MOUSEWHEEL)
                {
                    SendEvent(Event{layer, this, EventCategory::Mouse, EventID::MouseScroll, Vec { 0, delta }});
                }
                else
                {
                    SendEvent(Event{layer, this, EventCategory::Mouse, EventID::MouseScroll, Vec { delta, 0 }});
                }
            }
        break;case WM_SETFOCUS:
            SendEvent(Event{layer, this, EventCategory::Stage, EventID::FocusGained});
        break;case WM_KILLFOCUS:
            SendEvent(Event{layer, this, EventCategory::Stage, EventID::FocusLost});
        break;default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }

        return 0;
    }

    std::optional<Vec> GetMousePos(const Event& event)
    {
        return event.stage->mouse_pos;
    }

    size_t Stage::GetUID()
    {
        return nextUID += 2;
    }

    Stage CreateStage()
    {
        return Stage();
    }

    int Run(Stage& stage, std::function<void(const Event&)> eventHandler)
    {
        return stage.Run(eventHandler);
    }

    void DeleteImpl(Stage* stage)
    {
        delete stage;
    }

    void Quit(Stage& stage, int code)
    {
        stage.Quit(code);
    }

    Rect& GetLayerBounds(Layer& layer)
    {
        return layer.bounds;
    }

    void Layer::Hide()
    {
        ShowWindow(hWnd, 0);
    }

    void Hide(Layer& layer)
    {
        ShowWindow(layer.hWnd, 0);
    }

    void Layer::Focus()
    {
        SetForegroundWindow(hWnd);
    }

    void Focus(Layer& layer)
    {
        SetForegroundWindow(layer.hWnd);
    }

    int Stage::Run(std::function<void(const Event&)> callback)
    {
        this->eventHandler = callback;
        SendEvent(Event{nullptr, this, EventCategory::Stage, EventID::Initialize});

        auto msg = MSG{};
        while (GetMessageW(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return static_cast<int>(msg.wParam);
    }

    // ------------------------------- //
    // ------------- Box ------------- //
    // ------------------------------- //

    Box::~Box()
    {}

    void Box::Draw(const Frame& context)
    {
        if (!visible)
            return;

        auto pRenderTarget = context.renderTarget;
        auto p = PointAt(Alignments::TopLeft) - context.screenPos;

        if (borderWidth > 0)
        {
            auto hBSize = 0.5f * borderWidth - 0.1f;
            if (cornerRadius == 0)
            {
                pRenderTarget->DrawRectangle(
                    D2D1::RectF(
                        p.x - hBSize,
                        p.y - hBSize,
                        p.x + size.x + hBSize,
                        p.y + size.y + hBSize),
                    get_brush(context, border),
                    borderWidth);
            }
            else
            {
                pRenderTarget->DrawRoundedRectangle(
                    D2D1::RoundedRect(
                        D2D1::RectF(
                            p.x - hBSize,
                            p.y - hBSize,
                            p.x + size.x + hBSize,
                            p.y + size.y + hBSize),
                        cornerRadius + hBSize, cornerRadius + hBSize),
                    get_brush(context, border),
                    borderWidth + 0.2);
            }
        }

        if (cornerRadius == 0)
        {
            pRenderTarget->FillRectangle(
                D2D1::RectF(
                    p.x,
                    p.y,
                    p.x + size.x,
                    p.y + size.y),
                get_brush(context, background));
        }
        else
        {
            pRenderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(
                    p.x,
                    p.y,
                    p.x + size.x,
                    p.y + size.y),
                cornerRadius, cornerRadius),
            get_brush(context, background));
        }
    }

    // ------------ //
    // --- Icon --- //
    // ------------ //

    IconFuture::~IconFuture() {}

    void Icon::Reposition(Rect& bounds)
    {
        size.x = GetSystemMetrics(SM_CXICON);
        size.y = GetSystemMetrics(SM_CYICON);
        static_cast<Node*>(this)->Reposition(bounds);
    }

    void IconCache::operator=(nullptr_t)
    {
        if (iconFuture)
        {
            std::lock_guard lock{iconFuture->mutex};
            iconFuture->cancelled = true;
        }
        iconFuture = nullptr;
    }

    IconCache::IconCache() {}

    IconCache::~IconCache()
    {
        if (iconFuture)
        {
            std::lock_guard lock{iconFuture->mutex};
            iconFuture->cancelled = true;
        }
    }

    void Icon::Draw(const Frame& context)
    {
        if (!visible)
            return;

        // auto bitmap = context.stage->icon_loader.load(path);

        // if (bitmap) {
        //   auto p = point_at(Alignments::TopLeft) - context.screen_pos;
        //   context.render_target->DrawBitmap(
        //     bitmap,
        //     D2D1::RectF(p.x, p.y, p.x + size.x, p.y + size.y));
        // }

        if (!cache.iconFuture)
        {
            // std::cout << "future not found!\n";
            cache.iconFuture = context.stage->iconLoader.Load(path);
            // std::cout << "Got future! - " << cache.icon_future << '\n';
        }

        // std::cout << "future = " << cache.icon_future << '\n';
        std::lock_guard future_lock{cache.iconFuture->mutex};
        if (cache.iconFuture->icon)
        {
            // std::cout << "   icon = " << cache.icon_future->icon.get() << '\n';
            auto p = PointAt(Alignments::TopLeft) - context.screenPos;
            context.renderTarget->DrawBitmap(
                cache.iconFuture->icon.Get(),
                D2D1::RectF(p.x, p.y, p.x + size.x, p.y + size.y));
        }
    }

    // ------------------------------------ //
    // ---------- Text And Fonts ---------- //
    // ------------------------------------ //

    void FontCache::operator=(nullptr_t)
    {
        format = nullptr;
    }

    FontCache::FontCache() {}

    FontCache::~FontCache() = default;

    void delete_impl(FontCache* cache)
    {
        delete cache;
    }

    IDWriteTextFormat* GetTextFormat(Font& font, const Stage& stage)
    {
        if (!font.cache.format)
        {
            std::cout << "GetTextFormat!\n";
            std::cout << " stage = " << &stage << '\n';
            std::cout << " dwrite = " << stage.dwrite.Get() << '\n';
            auto factory = stage.dwrite.Get();

            factory->CreateTextFormat(
                ConvertToWString(font.name).c_str(),
                nullptr,
                static_cast<DWRITE_FONT_WEIGHT>(font.weight),
                static_cast<DWRITE_FONT_STYLE>(font.style),
                static_cast<DWRITE_FONT_STRETCH>(font.stretch),
                font.size,
                ConvertToWString(font.locale).c_str(),
                font.cache.format.ReleaseAndGetAddressOf());

            font.cache.format->SetTextAlignment(static_cast<DWRITE_TEXT_ALIGNMENT>(font.align));
            font.cache.format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            if (font.ellipsize)
            {
                auto trimming = DWRITE_TRIMMING{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
                auto trimmingSign = ComPtr<IDWriteInlineObject>{};
                factory->CreateEllipsisTrimmingSign(font.cache.format.Get(), trimmingSign.GetAddressOf());
                font.cache.format->SetTrimming(&trimming, trimmingSign.Get());
            }

            return nullptr;
        }

        return font.cache.format.Get();
    }

    Font::~Font() {}

    // -------------------------------- //
    // ------------- Text ------------- //
    // -------------------------------- //

    void TextCache::operator=(nullptr_t)
    {
        layout = nullptr;
    }

    TextCache::TextCache() {}
    TextCache::~TextCache() = default;

    Text::~Text() {}

    void Text::Layout(const Stage& stage, bool reset)
    {
        if (reset)
            cache = nullptr;

        auto format = GetTextFormat(*font, stage);
        if (cache.layout)
        {
            if (format)
                return;
            format = GetTextFormat(*font, stage);
        }
        else if (!format)
        {
            format = GetTextFormat(*font, stage);
        }

        auto pDWriteFactory = stage.dwrite.Get();
        auto wString = ConvertToWString(text);

        pDWriteFactory->CreateTextLayout(
            wString.c_str(),
            wString.length(),
            format,
            bounds.x, bounds.y,
            cache.layout.ReleaseAndGetAddressOf());

        if (lineHeight != 0)
        {
            cache.layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineHeight, baseline);
            cache.layout->SetWordWrapping(DWRITE_WORD_WRAPPING_EMERGENCY_BREAK);
        }

        if (nowrap)
        {
            cache.layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }

        // Pull font metrics and convert them into bounding rectangles relevant to the
        // guide bound rectangle.

        auto metrics = DWRITE_TEXT_METRICS{};
        cache.layout->GetMetrics(&metrics);
        auto textBounds = Rect{
            metrics.left,
            metrics.top,
            metrics.left + metrics.width,
            metrics.top + metrics.height
        };

        auto ometrics = DWRITE_OVERHANG_METRICS{};
        cache.layout->GetOverhangMetrics(&ometrics);
        auto overhangBounds = Rect{
            -ometrics.left,
            -ometrics.top,
            bounds.x + ometrics.right,
            bounds.y + ometrics.bottom
        };

        auto bottomBaseline = textBounds.top;
        auto topBaseline = textBounds.top;
        auto lineMetrics = std::vector<DWRITE_LINE_METRICS>(metrics.lineCount);
        auto lines = 0u;
        cache.layout->GetLineMetrics(lineMetrics.data(), metrics.lineCount, &lines);
        if (lines == metrics.lineCount)
        {
            topBaseline += lineMetrics[0].baseline;
            for (auto i = 0; i < lines - 1; ++i)
            {
                auto& lm = lineMetrics[i];
                bottomBaseline += lm.height;
            }
            bottomBaseline += lineMetrics[lines - 1].baseline;
        }

        topleftOffset = Vec {
            leftAdvance ? -textBounds.left : -overhangBounds.left,
            alignTopToLine ? -textBounds.top : -overhangBounds.top
        };

        size = Vec {
            (rightAdvance ? textBounds.right : overhangBounds.right) + topleftOffset.x,
            (alignToDescender ? overhangBounds.bottom : bottomBaseline) + topleftOffset.y
        };

        padding.bottom = alignToDescender ? 0 : overhangBounds.bottom - bottomBaseline;
    }

    int Text::LineCount()
    {
        if (!cache.layout)
            return 0;
        auto metrics = DWRITE_TEXT_METRICS{};
        cache.layout->GetMetrics(&metrics);
        return metrics.lineCount;
    }

    void Text::Draw(const Frame& context)
    {
        if (!visible)
            return;

        auto pRenderTarget = context.renderTarget;

        Layout(*context.stage);

        auto p = PointAt(Alignments::TopLeft) - context.screenPos;
        auto tp = p + topleftOffset;

        if (context.stage->debug)
        {
            // Debug - Create a box to show the actual bounds
            auto brush = ComPtr<ID2D1SolidColorBrush>{};
            pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2,0.2,0.2,0.5), brush.GetAddressOf());
            pRenderTarget->FillRectangle(
                    D2D1::RectF(
                        p.x,
                        p.y,
                        p.x + size.x,
                        p.y + size.y),
                brush.Get());

            // Debug - Create a box to show the baseline padding
            pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2,0.7,0.2,0.5), brush.ReleaseAndGetAddressOf());
            pRenderTarget->FillRectangle(
                    D2D1::RectF(
                        p.x,
                        p.y + size.y,
                        p.x + size.x,
                        p.y + size.y + padding.bottom),
                brush.Get());
        }

        // Render with correct antialising setting
        pRenderTarget->SetTextAntialiasMode(transparent_target
            ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE
            : D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
        pRenderTarget->DrawTextLayout(
            D2D1::Point2F(tp.x, tp.y),
            cache.layout.Get(),
            get_brush(context, color));
    }

    int Start(std::function<void(const Event&)> callback)
    {
        return Stage{}.Run(callback);
    }
}

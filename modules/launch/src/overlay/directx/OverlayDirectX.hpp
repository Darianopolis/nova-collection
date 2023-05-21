#pragma once

#ifndef OVERLAY_H
#include "Overlay.hpp"
#endif
#include "OverlayDirectXLayout.hpp"

#include "Win32Include.hpp"

#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace overlay
{
    void ConvertToWString(std::string_view input, std::wstring& output);
    std::wstring ConvertToWString(std::string_view input);

    void ConvertToString(std::wstring_view input, std::string& output);
    std::string ConvertToString(std::wstring_view input);

    // --------------------------------------- //
    // ------------- Icon Loader ------------- //
    // --------------------------------------- //

    struct IconRequest
    {
        std::string path;
        bool silent;
        std::shared_ptr<IconFuture> future;
        std::condition_variable cv;
    };

    // struct CachedIcon {
    //   win::ComPtr<win::d2d1::Bitmap> icon;
    //   size_t age;

    //   ~CachedIcon();
    // };


    // Worker thread style IconLoading
    // TODO: Implement icon unloading!
    class IconLoader
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::queue<std::shared_ptr<IconRequest>> queue;
        std::vector<std::thread> threads;
        // std::unordered_map<std::string, CachedIcon> icons;
        std::unordered_map<std::string, std::shared_ptr<IconFuture>> icons;
        size_t version;
        ID2D1DCRenderTarget *render_target;
        Stage *stage{nullptr};
        bool block_wait{true};

        std::thread worker();
    public:
        HWND hWnd;

        IconLoader();
        ~IconLoader();
        std::shared_ptr<IconFuture> load(std::string path, bool silent = false);
        void newFrame(Frame& frame);
        void clear();
    };

    // ------------- //
    // --- Layer --- //
    // ------------- //

    struct Layer
    {
        Stage *stage;
        HWND hWnd;
        Rect bounds;

        void hide();
        void focus();
    };

    // ------------- //
    // --- Frame --- //
    // ------------- //

    struct Frame
    {
        size_t version;
        ID2D1DCRenderTarget* render_target;
        HDC hdc;
        Vec screen_pos;
        Layer *layer;
        Stage *stage;

        bool sticky;
        Rect bounds;
        RECT rect;
        HDC hdc_screen;
        HBITMAP bitmap;
        HBITMAP bitmap_old;

        bool drawing{false};

        Frame(Layer *layer, bool sticky, Rect bounds);
        ~Frame();

        bool canDraw();
        void push();

        void box(
            const Node& node,
            Color bg);

        void box(
            const Node& node,
            Color bg,
            Color border,
            float border_width);

        void box(
            const Node& node,
            Color bg,
            float corner_radius);

        void box(
            const Node& node,
            Color bg,
            Color border,
            float border_width,
            float corner_radius);
    };

    // ------------- //
    // --- Stage --- //
    // ------------- //

    class Stage
    {
    public:
        ComPtr<ID2D1Factory> d2d1;
        ComPtr<IDWriteFactory> dwrite;
        ComPtr<IWICImagingFactory> wic;

        TRACKMOUSEEVENT mouse_track;
        HINSTANCE instance{nullptr};
        std::vector<std::unique_ptr<Layer>> layers;
        std::optional<Vec> mouse_pos;

        size_t render_target_version{0};
        ComPtr<ID2D1DCRenderTarget> render_target;
        ComPtr<ID2D1SolidColorBrush> brush;

        Node screen;

        GUID guid;
        HICON icon;
        IconLoader icon_loader;

        std::function<void(const Event&)> event_handler;

        size_t next_uid{1};

        bool debug{false};

        static LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        Stage();
        ~Stage();
        void initDirectX();
        int initWin32();
        int createWindow();
        void updateScreens();
        void createTrayIcon(std::string_view tooltip);

        size_t getUID();

        bool bindDC(HDC hdc, RECT &rc);

        Layer* layer(int id)
        {
            return layers[id].get();
        }

        int run(std::function<void(const Event&)> event_handler);
        void quit(int);

        void sendEvent(Event&& event)
        {
            if (event_handler) event_handler(event);
        }
    };

    int Start(std::function<void(Event&)> callback);
}

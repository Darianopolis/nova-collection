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
        ID2D1DCRenderTarget* renderTarget = nullptr;
        Stage* stage = nullptr;
        bool blockWait = true;

        std::thread Worker();
    public:
        HWND hWnd;

        IconLoader();
        ~IconLoader();
        std::shared_ptr<IconFuture> Load(std::string path, bool silent = false);
        void NewFrame(Frame& frame);
        void Clear();
    };

    // ------------- //
    // --- Layer --- //
    // ------------- //

    struct Layer
    {
        Stage* stage;
        HWND hWnd;
        Rect bounds;

        void Hide();
        void Focus();
    };

    // ------------- //
    // --- Frame --- //
    // ------------- //

    struct Frame
    {
        size_t version;
        ID2D1DCRenderTarget* renderTarget;
        HDC hdc;
        Vec screenPos;
        Layer* layer;
        Stage* stage;

        bool sticky;
        Rect bounds;
        RECT rect;
        HDC hdcScreen;
        HBITMAP bitmap;
        HBITMAP bitmapOld;

        bool drawing = false;

        Frame(Layer* layer, bool sticky, Rect bounds);
        ~Frame();

        bool CanDraw();
        void Push();

        void Box(
            const Node& node,
            Color bg);

        void Box(
            const Node& node,
            Color bg,
            Color border,
            float borderWidth);

        void Box(
            const Node& node,
            Color bg,
            float cornerRadius);

        void Box(
            const Node& node,
            Color bg,
            Color border,
            float borderWidth,
            float cornerRadius);
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

        TRACKMOUSEEVENT mouseTrack;
        HINSTANCE instance{nullptr};
        std::vector<std::unique_ptr<Layer>> layers;
        std::optional<Vec> mouse_pos;

        size_t renderTargetVersion = 0;
        ComPtr<ID2D1DCRenderTarget> renderTarget;
        ComPtr<ID2D1SolidColorBrush> brush;

        Node screen;

        GUID guid;
        HICON icon;
        IconLoader iconLoader;

        std::function<void(const Event&)> eventHandler;

        size_t nextUID = 1;

        bool debug = false;

        static LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        Stage();
        ~Stage();
        void InitDirectX();
        int InitWin32();
        int MakeWindow();
        void UpdateScreens();
        void CreateTrayIcon(std::string_view tooltip);

        size_t GetUID();

        bool BindDC(HDC hdc, RECT &rc);

        Layer* GetLayer(int id)
        {
            return layers[id].get();
        }

        int Run(std::function<void(const Event&)> eventHandler);
        void Quit(int);

        void SendEvent(Event&& event)
        {
            if (eventHandler)
                eventHandler(event);
        }
    };

    int Start(std::function<void(Event&)> callback);
}

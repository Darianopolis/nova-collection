#pragma once
#ifndef OVERLAY_WIN_H
#define OVERLAY_WIN_H

#ifndef OVERLAY_H
#include "Overlay.hpp"
#endif
#include "OverlayDirectXLayout.hpp"

#include <WinAPIWrapper.h>

#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace overlay_ui {
  
  // --------------------------------------- //
  // ------------- Icon Loader ------------- //
  // --------------------------------------- //

  struct IconRequest {
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
  class IconLoader {
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<std::shared_ptr<IconRequest>> queue;
    std::vector<std::thread> threads;
    // std::unordered_map<std::string, CachedIcon> icons;
    std::unordered_map<std::string, std::shared_ptr<IconFuture>> icons;
    size_t version;
    win::d2d1::DCRenderTarget *render_target;
    Stage *stage{nullptr};
    bool block_wait{true};

    std::thread worker();
  public:
    win::HWnd hWnd;

    IconLoader();
    ~IconLoader();
    std::shared_ptr<IconFuture> load(std::string path, bool silent = false);
    void newFrame(Frame& frame);
    void clear();
  };

  // ------------- //
  // --- Layer --- //
  // ------------- //

  struct Layer {
    Stage *stage;
    win::HWnd hWnd;
    Rect bounds;

    void hide();
    void focus();
  };

  // ------------- //
  // --- Frame --- //
  // ------------- //

  struct Frame {
    size_t version;
    win::d2d1::DCRenderTarget* render_target;
    win::gdi::HDeviceContext hdc;
    Vec screen_pos;
    Layer *layer;
    Stage *stage;

    bool sticky;
    Rect bounds;
    win::Rect rect;
    win::gdi::HDeviceContext hdc_screen;
    win::gdi::HBitmap bitmap;
    win::gdi::HBitmap bitmap_old;
    
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

  class Stage {
  public:
    win::ComPtr<win::d2d1::Factory> d2d1;
    win::ComPtr<win::dwrite::Factory> dwrite;
    win::ComPtr<win::wic::Factory> wic;

    win::TrackMouseEvent mouse_track;
    win::HInstance instance{nullptr};
    std::vector<std::unique_ptr<Layer>> layers;
    std::optional<Vec> mouse_pos;

    size_t render_target_version{0};
    win::ComPtr<win::d2d1::DCRenderTarget> render_target;
    win::ComPtr<win::d2d1::SolidColorBrush> brush;

    Node screen;

    win::Guid guid;
    win::HIcon icon;
    IconLoader icon_loader;

    std::function<void(const Event&)> event_handler;

    size_t next_uid{1};

    bool debug{false};

    static win::LResult __stdcall WndProc(win::HWnd hWnd, win::UInt msg, win::WParam wParam, win::LParam lParam);
    win::LResult HandleMessage(win::HWnd hWnd, win::UInt msg, win::WParam wParam, win::LParam lParam);

    Stage();
    ~Stage();
    void initDirectX();
    int initWin32();
    int createWindow();
    void updateScreens();
    void createTrayIcon(std::string_view tooltip);

    size_t getUID();

    bool bindDC(win::gdi::HDeviceContext hdc, win::Rect &rc);

    Layer* layer(int id) {
      return layers[id].get();
    }
    
    int run(std::function<void(const Event&)> event_handler);
    void quit(int);

    void sendEvent(Event&& event) {
      if (event_handler) event_handler(event);
    }
  };

  int Start(std::function<void(Event&)> callback);
}

#endif // !OVERLAY_WIN_H

#pragma once

#include "nms_Query.hpp"

#include <nova/window/nova_Window.hpp>
#include <nova/ui/nova_Draw2D.hpp>

#include "nms_Platform.hpp"

using namespace nova::types;

class App
{
public:
    nova::Application app;
    nova::Window   window;

    nova::Context context;
    nova::Queue     queue;

    std::unique_ptr<nova::draw::Draw2D> draw;

    nova::Swapchain swapchain = {};

    nova::draw::Font*       font = {};
    f32                font_size = 35.f;
    nova::draw::Font* font_small = {};
    f32          font_small_size = 18.f;

    i32 window_width, window_height;

    std::vector<std::string> keywords;

    std::filesystem::path exe_dir;

    std::filesystem::path index_file = nova::env::GetUserDirectory() / ".nms/index.bin";
    index_t                   index;
    file_searcher_t        searcher;

    std::unique_ptr<FileResultList>         file_result_list;
    std::unique_ptr<FavResultList>           fav_result_list;
    std::unique_ptr<ResultListPriorityCollector> result_list;

    std::vector<std::unique_ptr<ResultItem>> items;
    u32                                  selection;

    struct IconResult
    {
        nova::Image texture = {};
    };

    ankerl::unordered_dense::map<std::filesystem::path, IconResult> icon_cache;

    bool show;
    bool running = true;

    float ui_scale = 1.f;

    i32 updates = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_update;

    App();
    ~App();

    void ResetItems(bool end = false);

    void Draw();

    void ResetQuery();
    std::string JoinQuery() const;
    void UpdateQuery();

    void Move(i32 delta);
    bool MoveSelectedUp();
    bool MoveSelectedDown();

    void OnChar(u32 codepoint);
    void OnKey(nova::VirtualKey key, bool pressed);

    void UpdateIndex();

    void Run();
};

i32 AppMain();
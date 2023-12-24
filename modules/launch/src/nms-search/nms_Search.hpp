#pragma once

#include "nms_Query.hpp"

#include "nms_Platform.hpp"

using namespace nova::types;

class App
{
public:
    nova::Context context;
    nova::Queue queue;
    std::unique_ptr<nova::draw::Draw2D> imDraw;

    nova::Swapchain swapchain = {};
    nova::CommandPool commandPool = {};
    nova::Fence fence = {};

    std::unique_ptr<nova::draw::Font> font = {};
    std::unique_ptr<nova::draw::Font> fontSmall = {};

    GLFWwindow* window = {};
    i32 mWidth, mHeight;

    std::vector<std::string> keywords;

    std::vector<std::unique_ptr<ResultItem>> items;
    u32 selection;

    std::filesystem::path exe_dir;

    std::string indexFile = std::format("{}\\.nms\\index.bin", getenv("USERPROFILE"));
    index_t index;
    file_searcher_t searcher;

    std::unique_ptr<FileResultList> fileResultList;
    std::unique_ptr<FavResultList> favResultList;
    std::unique_ptr<ResultListPriorityCollector> resultList;

    struct IconResult
    {
        nova::Image texture = {};
    };

    ankerl::unordered_dense::map<std::filesystem::path, IconResult> iconCache;

    bool show;
    bool running = true;

    i32 updates = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_update;

    App();
    ~App();

    void ResetItems(bool end = false);

    void Draw();

    void ResetQuery();
    std::string JoinQuery();
    void UpdateQuery();

    void Move(i32 delta);
    bool MoveSelectedUp();
    bool MoveSelectedDown();

    void OnChar(u32 codepoint);
    void OnKey(u32 key, i32 action, i32 mods);

    void UpdateIndex();

    void Run();
};

i32 AppMain();
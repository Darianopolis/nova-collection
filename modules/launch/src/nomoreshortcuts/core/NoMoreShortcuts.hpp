#pragma once

#include <Platform.hpp>

#include "Query.hpp"
#include "Index.hpp"

#include <UnicodeCollator.hpp>

#include <filesystem>
#include <vector>
#include <iostream>
#include <chrono>
#include <format>

using namespace nova::types;

class App
{
public:
    GLFWwindow* window;

    nova::Context* context = {};
    VkSurfaceKHR surface = {};
    nova::Swapchain* swapchain = {};
    nova::Queue* queue = {};
    nova::CommandPool* commandPool = {};
    nova::Fence* fence = {};
    nova::ResourceTracker* tracker = {};

    nova::ImDraw2D* imDraw = {};
    nova::ImFont* font = {};
    nova::ImFont* fontSmall = {};

    nova::Image* emptyImage = {};
    nova::ImTextureID emptyTexID;

    int mWidth, mHeight;

    std::vector<std::string> keywords;

    std::vector<std::unique_ptr<ResultItem>> items;
    uint32_t selection;

    std::unique_ptr<FileResultList> fileResultList;
    std::unique_ptr<FavResultList> favResultList;
    std::unique_ptr<ResultListPriorityCollector> resultList;

    struct IconResult
    {
        nova::Image* image = {};
        nova::ImTextureID texID;
    };

    ankerl::unordered_dense::map<std::filesystem::path, IconResult> iconCache;

    bool show;
    bool running = true;

    int updates = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_update;

    App();

    void ResetItems(bool end = false);

    void Update();
    void Draw();

    void ResetQuery();
    std::string JoinQuery();
    void UpdateQuery();

    void Move(int delta);
    bool MoveSelectedUp();
    bool MoveSelectedDown();

    void OnChar(u32 codepoint);
    void OnKey(u32 key, i32 action, i32 mods);

    void Run();
};

int AppMain();
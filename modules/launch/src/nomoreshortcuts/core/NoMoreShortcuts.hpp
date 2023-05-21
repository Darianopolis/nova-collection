#pragma once

#include "Query.hpp"
#include "Index.hpp"

#include <Overlay.hpp>
#include <UnicodeCollator.hpp>

#include <filesystem>
#include <vector>
#include <iostream>
#include <chrono>
#include <format>

class App;

class LaunchItem : public overlay::NodeImpl<LaunchItem>
{
public:
    overlay::Text nameText;
    overlay::Text pathText;
    overlay::Box box;
    overlay::Icon icon;
    overlay::Stage* stage;

    std::unique_ptr<ResultItem> view;

    LaunchItem(App& app, std::unique_ptr<ResultItem> view);

    void SetPath(std::unique_ptr<ResultItem> newView);

    template<class NodeVisitor>
    void Traverse(NodeVisitor& visit)
    {
        visit(box);
        visit(nameText);
        visit(pathText);
        visit(icon);
    }
};

class ContextMenu : public overlay::NodeImpl<ContextMenu>
{
public:
    overlay::Box box;
    overlay::Box highlight;
    overlay::Text closeText;
    overlay::Stage* stage;

    ContextMenu(App& app);

    template<class NodeVisitor>
    inline void Traverse(NodeVisitor& visit)
    {
        if (visible)
        {
            visit(box);
            visit(highlight);
            visit(closeText);
        }
    }
};

class App : public overlay::NodeImpl<App>
{
public:

    overlay::Font nameFont{"Sans Serif", 35};
    overlay::Font pathFont{"Sans Serif", 18};
    overlay::Font menuFont{"Sans Serif", 24};

    float queryWidth = 1920;
    float itemWidth = 1200;
    float corner = 15;

    overlay::Color HighlightColour{0.4, 0.2};
    overlay::Color TextColour{1};
    overlay::Color Transparent{0, 0};
    overlay::Color BorderColour{0.4, 0.5};
    overlay::Color BgColour{0.1};

    overlay::Stage* stage;

    overlay::Box queryBox;

    std::vector<std::string> keywords;
    overlay::Text queryText;
    overlay::Box resultsBox;

    std::vector<std::unique_ptr<LaunchItem>> items;
    uint32_t selection;

    std::unique_ptr<FileResultList> fileResultList;
    std::unique_ptr<FavResultList> favResultList;
    std::unique_ptr<ResultListPriorityCollector> resultList;

    ContextMenu menu;

    bool show;

    overlay::Layer* mainLayer;
    overlay::Layer* menuLayer;

    int updates = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_update;

    App(overlay::Stage* stage);

    void ResetItems(bool end = false);

    void FixItemAnchors();

    template<class NodeVisitor>
    inline void Traverse(NodeVisitor& visit)
    {
        if (show)
        {
            visit(queryBox);
            visit(resultsBox);
            visit(queryText);
            for (auto& i : items)
                visit(*i);
        }
        visit(menu);
    }

    void Update();

    void ResetQuery();
    std::string JoinQuery();
    void UpdateQuery();

    void Move(int delta);
    bool MoveSelectedUp();
    bool MoveSelectedDown();

    void OnEvent(const overlay::Event &e);
};

int AppMain();
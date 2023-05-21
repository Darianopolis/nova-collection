#pragma once
#ifndef NO_MORE_SHORTCUTS_HPP
#define NO_MORE_SHORTCUTS_HPP

#include "Overlay.hpp"
#include "UnicodeCollator.hpp"
#include "Query.hpp"
#include "Index.hpp"

// #define MAGIC_ENUM_RANGE_MIN 0
// #define MAGIC_ENUM_RANGE_MAX 512
// #include "magic_enum.hpp"

#include <filesystem>
#include <vector>
#include <iostream>
#include <chrono>
#include <format>

namespace ui = overlay_ui;

class App;

class LaunchItem : public ui::NodeImpl<LaunchItem> {
public:
  ui::Text nameText;
  ui::Text pathText;
  ui::Box box;
  ui::Icon icon;
  ui::Stage* stage;

  std::unique_ptr<ResultItem> view;

  LaunchItem(App& app, std::unique_ptr<ResultItem> view);

  void setPath(std::unique_ptr<ResultItem> newView);

  template<class NodeVisitor>
  void traverse(NodeVisitor& visit) {
    visit(box);
    visit(nameText);
    visit(pathText);
    visit(icon);
  }
};

class ContextMenu : public ui::NodeImpl<ContextMenu> {
public:
  ui::Box box;
  ui::Box highlight;
  ui::Text closeText;
  ui::Stage *stage;

  ContextMenu(App& app);

  template<class NodeVisitor>
  inline void traverse(NodeVisitor& visit) {
    if (visible) {
      visit(box);
      visit(highlight);
      visit(closeText);
    }
  }
};

class App : public ui::NodeImpl<App> {
public:

  ui::Font nameFont{"Sans Serif", 35};
  ui::Font pathFont{"Sans Serif", 18};
  ui::Font menuFont{"Sans Serif", 24};

  float queryWidth = 1920;
  float itemWidth = 1200;
  float corner = 15;

  ui::Color HighlightColour{0.4, 0.2};
  ui::Color TextColour{1};
  ui::Color Transparent{0, 0};
  ui::Color BorderColour{0.4, 0.5};
  ui::Color BgColour{0.1};

  ui::Stage *stage;

  ui::Box queryBox;
  
  std::vector<std::string> keywords;
  ui::Text queryText;
  ui::Box resultsBox;

  std::vector<std::unique_ptr<LaunchItem>> items;
  uint32_t selection;

  std::unique_ptr<FileResultList> fileResultList;
  std::unique_ptr<FavResultList> favResultList;
  std::unique_ptr<ResultListPriorityCollector> resultList;

  ContextMenu menu;

  bool show;

  ui::Layer *mainLayer;
  ui::Layer *menuLayer;

  int updates = 0;
  std::chrono::time_point<std::chrono::steady_clock> last_update;

  App(ui::Stage *stage);

  void resetItems(bool end = false);

  void fixItemAnchors();

  template<class NodeVisitor>
  inline void traverse(NodeVisitor& visit) {
    if (show) {
      visit(queryBox);
      visit(resultsBox);
      visit(queryText);
      for (auto& i : items) visit(*i);
    }
    visit(menu);
  }

  void resetQuery();

  void update();

  std::string join_query();

  void updateQuery();

  void move(int delta);

  bool moveSelectedUp();

  bool moveSelectedDown();

  void onEvent(const ui::Event &e);
};

int AppMain();

#endif // !NO_MORE_SHORTCUTS_HPP

#include "NoMoreShortcuts.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <combaseapi.h>

#include <thread>

using namespace std::literals;

LaunchItem::LaunchItem(App& app, std::unique_ptr<ResultItem> view)
  : nameText(&app.nameFont, "", app.TextColour, ui::Vec{app.itemWidth, 0})
  , pathText(&app.pathFont, "", app.TextColour, ui::Vec{app.itemWidth, 0})
  , box(app.HighlightColour, app.Transparent, 0, app.corner)
  , icon()
  , stage(app.stage) {

  using namespace ui::Alignments;

  size = ui::Vec{app.itemWidth, 0};
  box.size = size;
  box.anchor.parent = this;
  box.visible = false;

  icon.size = ui::Vec{40, 40};
  icon.anchor = ui::Anchor{this, Left, ui::Vec{12, 0}, Left};
  auto textOffset = icon.size.x + 36;

  nameText.anchor = ui::Anchor{this,
    TopLeft, ui::Vec{textOffset, 12}, TopLeft};
  pathText.anchor = ui::Anchor{this,
    BottomLeft, ui::Vec{textOffset, -12}, BottomLeft};
  pathText.line_height = 20;

  nameText.line_height = 25;

  nameText.bounds = ui::Vec{size.x - textOffset - 12, 0};
  pathText.bounds = ui::Vec{size.x - textOffset - 12, 0};

  setPath(std::move(view));
}

void LaunchItem::setPath(std::unique_ptr<ResultItem> newView) {
  this->view = std::move(newView);

  auto folder = view->path().parent_path().string();
  auto name = view->path().filename().string();

  nameText.text = name.empty() ? folder : name;
  pathText.text = folder;
  icon.path = view->path().string();
  icon.cache = nullptr;

  nameText.layout(*stage, true);
  pathText.layout(*stage, true);

  auto height = 55.f + 20.f * pathText.lineCount();

  size.y = height;
  box.size.y = height;
}

// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //

ContextMenu::ContextMenu(App& app)
  : box(app.BgColour, app.BorderColour, 2, 5)
  , stage(app.stage)
  , closeText(&app.menuFont, "Quit No More Shortcuts",
      app.TextColour, ui::Vec{400, 0})
  , highlight(app.HighlightColour, app.Transparent, 0, 3) {

  using namespace ui::Alignments;

  visible = false;

  closeText.anchor.parent = this;
  std::cout << "Making closing text!\n";
  closeText.layout(*stage);

  size = ui::Vec{
    std::max(closeText.size.x + 20.f, 300.f),
    closeText.size.y + 30.f};

  box.anchor.parent = this;
  box.size = size;

  highlight.anchor.parent = this;
  highlight.size = size - ui::Vec{10, 10};
  highlight.visible = false;
}

// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //

App::App(ui::Stage *stage)
  : stage(stage)
  , menu(*this)
  , queryBox(BgColour, BorderColour, 2, corner)
  , queryText(&nameFont, "", TextColour, ui::Vec{queryWidth - 20, 0})
  , resultsBox(BgColour, BorderColour, 2, corner) {

  nameFont.align = ui::FontAlign::Leading;
  nameFont.ellipsize = true;
  pathFont.align = ui::FontAlign::Leading;
  pathFont.weight = ui::FontWeight::Thin;

  mainLayer = ui::layer(*stage, 0);
  menuLayer = ui::layer(*stage, 1);

  ui::update(*mainLayer, *this, false);
  ui::focus(*mainLayer);

  visible = false;

  using namespace ui::Alignments;

  keywords.push_back("");

  queryBox.anchor = ui::Anchor{ui::screen(*stage),
    Center, ui::Vec{0, 0}, Bottom};
  queryBox.size = ui::Vec{queryWidth, 0};

  // queryText.alignTopToLine(true);
  // queryText.leftAdvance(true);
  // queryText.rightAdvance(true);

  queryText.anchor = ui::Anchor{&queryBox, Bottom, ui::Vec{0, -17}, Bottom};
  queryText.align_top_to_line = true;
  queryText.left_advance = true;
  queryText.right_advance = true;

  resultsBox.size = ui::Vec{800 * 1.5, 0};
  resultsBox.anchor = ui::Anchor{&queryBox, Bottom, ui::Vec{0, 20}, Top};

  using namespace std::chrono;

  auto begin = steady_clock::now();
  // tree = PathTree::load(getenv("USERPROFILE") + std::string("\\.nms\\tree.bin"));
  // index = std::make_unique<Index>(getenv("USERPROFILE") + std::string("\\.nms\\tree.bin"));
  auto dur = duration_cast<milliseconds>(steady_clock::now() - begin).count();

  // tree.setMatchBits(1, 1, 0, 0);
  // tree.matchBits = 1;
  // tree.useInheritMatch = true;

  resultList = std::make_unique<ResultListPriorityCollector>();
  favResultList = std::make_unique<FavResultList>(&keywords);
  fileResultList = std::make_unique<FileResultList>(favResultList.get());
  resultList->addList(favResultList.get());
  resultList->addList(fileResultList.get());

  // std::cout << std::format("loaded {} nodes in {} ms\n", fileResultList.tree.nodes.size(), dur);

  // collator = UnicodeCollator::new_ascii_collator();

  // index = std::make_unique<Index>(&index->tree, collator.get());

  // auto first = tree.next(nullptr);
  // auto count = 0;
  // while (count++ < 10 && first) {
  //   std::cout << std::format("{} - {}\n", count, first->path().string());
  //   first = tree.next(&*first);
  // }

  show = false;
  menu.visible = false;

  using enum ui::KeyMod;
  using enum ui::KeyCode;

  ui::add_hotkey(*stage, 0, KeySpace, ModControl, ModShift, ModNoRepeat);

  resetItems();
  updateQuery();
}

void App::resetItems(bool end) {
  items.clear();
  if (end) {
    auto item = resultList->prev(nullptr);
    if (item) {
      auto itemP = item.get();
      items.push_back(std::make_unique<LaunchItem>(*this, std::move(item)));
      while ((items.size() < 5) && (item = resultList->prev(itemP))) {
        itemP = item.get();
        items.push_back(std::make_unique<LaunchItem>(*this, std::move(item)));
      }
      std::reverse(items.begin(), items.end());
    }
    selection = items.size() - 1;
  } else {
    auto item = resultList->next(nullptr);
    if (item) {
      auto itemP = item.get();
      items.push_back(std::make_unique<LaunchItem>(*this, std::move(item)));
      while ((items.size() < 5) && (item = resultList->next(itemP))) {
        itemP = item.get();
        items.push_back(std::make_unique<LaunchItem>(*this, std::move(item)));
      }
    }
    selection = 0;
  }
  fixItemAnchors();
}

void App::fixItemAnchors() {
  using namespace ui::Alignments;

  if (items.empty()) {
    resultsBox.visible = false;
  } else {
    resultsBox.visible = true;
    auto height = items[0]->size.y;
    items[0]->anchor = ui::Anchor{&resultsBox, Top, ui::Vec{0, 0}, Top};
    for (auto i = 1; i < items.size(); ++i) {
      height += items[i]->size.y;
      items[i]->anchor = ui::Anchor{items[i - 1].get(),
        Bottom, ui::Vec{0, 0}, Top};
    }
    resultsBox.size.y = height;
  }

  for (auto& i : items) {
    i->box.visible = false;
  }
  if (selection < items.size())
    items[selection]->box.visible = true;
}

void App::resetQuery() {
  keywords.clear();
  keywords.emplace_back();
  // tree.setMatchBits(1, 1, 0, 0);
  // tree.matchBits = 1;
  resultList->query(QueryAction::SET, "");
  updateQuery();
}

void App::update() {
  ui::update(*mainLayer, *this, menu.visible);
}

std::string App::join_query() {
  auto str = std::string{};
  for (auto& k : keywords) {
    if (k.empty()) continue;
    if (!str.empty()) str += ' ';
    str.append(k);
  }

  return str;
}

void App::updateQuery() {
  queryText.text = join_query();
  queryText.layout(*stage, true);
  queryBox.size.y = queryText.size.y + 20;

  resetItems();
  update();
}

// void filter(uint8_t matchBit, std::string_view keyword, bool lazy) {
//   auto needle = std::string{keyword};
//   std::transform(needle.begin(), needle.end(), needle.begin(), [](char c) { return std::tolower(c); });
//   std::cout << std::format("filter {} -> {}\n", keyword, needle);

//   using namespace std::chrono;
//   std::cout << std::format("filter bits [{}] on \"{}\"\n", static_cast<uint32_t>(matchBit), keyword);
//   auto start = steady_clock::now();
//   if (lazy) {
//     tree.matchLazy(tree.matchBits, matchBit, [&](std::string_view str) {
//       return collator->fuzzy_find(str, needle);
//     });
//   } else {
//     tree.match(matchBit, [&](std::string_view str) {
//       return collator->fuzzy_find(str, needle);
//     });
//   }

//   tree.propogateMatches(true);
//   auto dur = steady_clock::now() - start;
//   std::cout << std::format("completed in {} ms", duration_cast<milliseconds>(dur).count());
// }

void App::move(int delta) {
  auto i = delta;
  if (i < 0) {
    while (moveSelectedUp() && ++i < 0);
  } else if (i > 0) {
    while (moveSelectedDown() && --i > 0);
  }

  if (delta != i) {
    fixItemAnchors();
    update();
  }
}

bool App::moveSelectedUp() {
  if (items.empty()) return false;

  if (items.size() < 5) {
    if (selection == 0) return false;
    selection--;
  } else if (selection > 2) {
    selection--;
  } else {
    auto prev = resultList->prev(items[0].get()->view.get());
    if (prev) {
      std::rotate(items.rbegin(), items.rbegin() + 1, items.rend());
      items[0]->setPath(std::move(prev));
    } else if (selection > 0) {
      selection--;
    } else {
      return false;
    }
  }
  return true;
}

bool App::moveSelectedDown() {
  if (items.empty()) return false;

  if (items.size() < 5) {
    if (selection == items.size() - 1) return false;
    selection++;
  } else if (selection < 2) {
    selection++;
  } else {
    auto next = resultList->next(items[items.size() - 1].get()->view.get());
    if (next) {
      std::rotate(items.begin(), items.begin() + 1, items.end());
      items[items.size() - 1]->setPath(std::move(next));
    } else if (selection < 4) {
      selection++;
    } else {
      return false;
    }
  }

  return true;
}

void App::onEvent(const ui::Event &e) {
  using namespace ui::Events;

  // if (e.event() != MouseMoved) {
  //   std::cout << std::format("Event - {}\n",
  //       magic_enum::enum_name<EventID>(e.event()));
  // }

  switch (e.event) {
    // break;case MouseActivate:case WindowActivate: {
    //   std::cout << "Mouse activated, layer = " << e.layer() << ", main = " << mainLayer << ", menu = " << menuLayer << '\n';
    //   // if (e.layer() == mainLayer) {
    //     update();
    //   // }
    // }
    break;case IconsLoaded:
      update();
    break;case Hotkey:
      show = true;
      update();
      ui::focus(*mainLayer);
    break;case NotifyContext: {
      menu.visible = true;
      menu.anchor = {
        ui::screen(*stage),
        ui::Alignments::TopLeft,
        *ui::mouse_pos(e) + ui::Vec{50, -10},
        ui::Alignments::BottomRight
      };

      ui::update(*menuLayer, *this, menu.visible);
      ui::focus(*menuLayer);
    }
    break;case FocusLost:
      if (menu.visible) {
        std::cout << "Lost menu focus!\n";
        menu.visible = false;

        // // Need to hide main window to prevent issue with window layers not being properly adjusted
        // // This won't be required once the menu is rendered in a different layer!
        // show = false;
        // update();

        ui::hide(*menuLayer);
      }
    break;case KeyPressed: {
      using enum ui::KeyCode;

      switch (e.key()) {
        break;case KeyEscape:
          show = false;
          ui::hide(*mainLayer);
          if (menu.visible) {
            menu.visible = false;
            ui::hide(*menuLayer);
          }
        break;case MouseLButton:
          std::cout << "MouseLButton!\n";
          if (ui::mouseover(e, queryBox) ||ui::mouseover(e, resultsBox)) {
            std::cout << "  Over query!\n";
            update();
            ui::focus(*mainLayer);
          }
          if (menu.visible && ui::mouseover(e, menu)) {
            std::cout << "Hiding menu!\n";
            ui::quit(*stage, 0);
          } else {
            menu.visible = false;
            ui::hide(*menuLayer);
          }
        break;case KeyArrowDown:
          move(1);
        break;case KeyArrowUp:
          move(-1);
        break;case KeyArrowLeft:
          resetItems();
          update();
        break;case KeyArrowRight:
          resetItems(true);
          update();
        break;case KeyReturn: {
          if (!items.empty()) {
            auto view = items[selection]->view.get();
            auto str = view->path().string();
            std::cout << std::format("Running {}!\n", str);

            favResultList->incrementUses(view->path());
            resetQuery();
            show = false;
            ui::hide(*mainLayer);

            // system(("explorer \""+ str +"\"").c_str());

            if ((GetKeyState(VK_LSHIFT) & 0x8000)
              && (GetKeyState(VK_LCONTROL) & 0x8000)) {
              ShellExecuteA(nullptr, "runas", str.c_str(), nullptr, nullptr, SW_SHOW);
            } else {
              ShellExecuteA(nullptr, "open", str.c_str(), nullptr, nullptr, SW_SHOW);
            }
          }
        }
        break;case KeyDelete: {
          if (ui::key_down(e, KeyLShift) && !items.empty()) {
            auto view = items[selection]->view.get();
            favResultList->resetUses(view->path());
            resetQuery();
          }
        }
        break;case KeyBackspace: {
          if (ui::key_down(e, KeyLShift)) {
            resetQuery();
          } else {
            auto& keyword = keywords[keywords.size() - 1];
            auto matchBit = static_cast<uint8_t>(1 << (keywords.size() - 1));
            if (keyword.length() > 0) {
              keyword.pop_back();
              // filter(matchBit, keyword, false);
              resultList->query(QueryAction::SET, join_query());
              updateQuery();
            } else if (keywords.size() > 1) {
              keywords.pop_back();
              // tree.setMatchBits(matchBit, 0, matchBit, 0);
              // tree.matchBits &= ~matchBit;
              resultList->query(QueryAction::SET, join_query());
              updateQuery();
            }
          }
        }
        break;case KeyC: {
          if (GetKeyState(VK_LCONTROL) & 0x8000) {
            if (!items.empty()) {
              auto view = items[selection]->view.get();
              auto str = view->path().string();
              std::cout << std::format("Copying {}!\n", str);

              OpenClipboard((HWND)mainLayer->hWnd);
              EmptyClipboard();
              auto contentHandle = GlobalAlloc(GMEM_MOVEABLE, str.size() + 1);
              auto contents = GlobalLock(contentHandle);
              memcpy(contents, str.data(), str.size() + 1);
              for (auto c = (char*)contents; *c; ++c)
                *c = *c == '\\' ? '/' : *c;
              GlobalUnlock(contentHandle);
              SetClipboardData(CF_TEXT, contentHandle);
              CloseClipboard();
            }
          }
        }
        break;case KeyF5: {
          if (GetKeyState(VK_CONTROL) & 0x8000) {

            static std::atomic_bool indexing = false;

            bool expected = false;
            if (indexing.compare_exchange_strong(expected, true)) {
              std::thread t([=, this] {
                AllocConsole();
                auto handle = freopen("CONOUT$", "w", stdout);

                std::vector<char> drives;
                wchar_t driveNames[1024];
                GetLogicalDriveStringsW(1023, driveNames);
                for (wchar_t* drive = driveNames; *drive; drive += wcslen(drive) + 1) {
                  drives.push_back((char)drive[0]);
                }

                for (auto& d : drives) {
                  auto *node = index_drive(d);
                  auto saveLoc = std::format("{}\\.nms\\{}.index", getenv("USERPROFILE"), d);
                  std::cout << std::format("Saving to {}\n", saveLoc);
                  node->save(saveLoc);
                }

                std::cout << "Indexing complete. Close this window and refresh the index with F5 in app.\n";
                FreeConsole();

                indexing = false;
              });

              t.detach();
            }
          } else {
            resultList = std::make_unique<ResultListPriorityCollector>();
            favResultList = std::make_unique<FavResultList>(&keywords);
            fileResultList = std::make_unique<FileResultList>(favResultList.get());
            resultList->addList(favResultList.get());
            resultList->addList(fileResultList.get());

            resetItems();
            updateQuery();
          }
        }
      }
      break;case CharTyped: {
        auto c = static_cast<char>(e.codepoint());
        auto& keyword = keywords[keywords.size() - 1];
        if (c == ' ') {
          if (keyword.size() == 0 || keywords.size() == 8) return;
          auto set = static_cast<uint8_t>(1 << keywords.size());
          // tree.setMatchBits(set, set, set, 0);
          // tree.matchBits |= set;
          keywords.push_back("");
        } else {
          if (c < ' ' || c > '~') return;
          // auto matchBit = static_cast<uint8_t>(1 << (keywords.size() - 1));
          keyword += c;
          // filter(matchBit, keyword, true);
          resultList->query(QueryAction::SET, join_query());
        }
        updateQuery();
      }
    }
    break;case MouseScroll:
      move((int)-e.delta().y);
    break;case MouseMoved: {
      if (menu.visible) {
        auto next = ui::mouseover(e, menu.highlight);
        if (next != menu.highlight.visible) {
          menu.highlight.visible = next;
          ui::update(*menuLayer, menu, true);
        }
      }
    }
  }
}

// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //

int AppMain()
{
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  auto stage = ui::stage();
  auto app = App{&stage};

  return ui::run(stage, [&](const ui::Event& e) {
    app.onEvent(e);
  });
}
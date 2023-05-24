#include "NoMoreShortcuts.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <combaseapi.h>

#include <thread>

using namespace std::literals;

LaunchItem::LaunchItem(App& app, std::unique_ptr<ResultItem> view)
    : nameText(&app.nameFont, "", app.TextColour, overlay::Vec{app.itemWidth, 0})
    , pathText(&app.pathFont, "", app.TextColour, overlay::Vec{app.itemWidth, 0})
    , box(app.HighlightColour, app.Transparent, 0, app.corner)
    , icon()
    , stage(app.stage)
{

    using namespace overlay::Alignments;

    size = overlay::Vec{app.itemWidth, 0};
    box.size = size;
    box.anchor.parent = this;
    box.visible = false;

    icon.size = overlay::Vec{40, 40};
    icon.anchor = overlay::Anchor{this, Left, overlay::Vec{12, 0}, Left};
    auto textOffset = icon.size.x + 36;

    nameText.anchor = overlay::Anchor{this,
        TopLeft, overlay::Vec{textOffset, 12}, TopLeft};
    pathText.anchor = overlay::Anchor{this,
        BottomLeft, overlay::Vec{textOffset, -12}, BottomLeft};
    pathText.lineHeight = 20;

    nameText.lineHeight = 25;

    nameText.bounds = overlay::Vec{size.x - textOffset - 12, 0};
    pathText.bounds = overlay::Vec{size.x - textOffset - 12, 0};

    SetPath(std::move(view));
}

void LaunchItem::SetPath(std::unique_ptr<ResultItem> newView)
{
    this->view = std::move(newView);

    auto folder = view->GetPath().parent_path().string();
    auto name = view->GetPath().filename().string();

    nameText.text = name.empty() ? folder : name;
    pathText.text = folder;
    icon.path = view->GetPath().string();
    icon.cache = nullptr;

    nameText.Layout(*stage, true);
    pathText.Layout(*stage, true);

    auto height = 55.f + 20.f * pathText.LineCount();

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
            app.TextColour, overlay::Vec{400, 0})
    , highlight(app.HighlightColour, app.Transparent, 0, 3)
{

    using namespace overlay::Alignments;

    visible = false;

    closeText.anchor.parent = this;
    std::cout << "Making closing text!\n";
    closeText.Layout(*stage);

    size = overlay::Vec{
        std::max(closeText.size.x + 20.f, 300.f),
        closeText.size.y + 30.f};

    box.anchor.parent = this;
    box.size = size;

    highlight.anchor.parent = this;
    highlight.size = size - overlay::Vec{10, 10};
    highlight.visible = false;
}

// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //

App::App(overlay::Stage* stage)
    : stage(stage)
    , menu(*this)
    , queryBox(BgColour, BorderColour, 2, corner)
    , queryText(&nameFont, "", TextColour, overlay::Vec{queryWidth - 20, 0})
    , resultsBox(BgColour, BorderColour, 2, corner)
{

    nameFont.align = overlay::FontAlign::Leading;
    nameFont.ellipsize = true;
    pathFont.align = overlay::FontAlign::Leading;
    pathFont.weight = overlay::FontWeight::Thin;

    mainLayer = overlay::CreateLayer(*stage, 0);
    menuLayer = overlay::CreateLayer(*stage, 1);

    overlay::Update(*mainLayer, *this, false);
    overlay::Focus(*mainLayer);

    visible = false;

    using namespace overlay::Alignments;

    keywords.push_back("");

    queryBox.anchor = overlay::Anchor{overlay::GetScreen(*stage),
        Center, overlay::Vec{0, 0}, Bottom};
    queryBox.size = overlay::Vec{queryWidth, 0};

    // queryText.alignTopToLine(true);
    // queryText.leftAdvance(true);
    // queryText.rightAdvance(true);

    queryText.anchor = overlay::Anchor{&queryBox, Bottom, overlay::Vec{0, -17}, Bottom};
    queryText.alignTopToLine = true;
    queryText.leftAdvance = true;
    queryText.rightAdvance = true;

    resultsBox.size = overlay::Vec{800 * 1.5, 0};
    resultsBox.anchor = overlay::Anchor{&queryBox, Bottom, overlay::Vec{0, 20}, Top};

    using namespace std::chrono;

    resultList = std::make_unique<ResultListPriorityCollector>();
    favResultList = std::make_unique<FavResultList>(&keywords);
    fileResultList = std::make_unique<FileResultList>(favResultList.get());
    resultList->AddList(favResultList.get());
    resultList->AddList(fileResultList.get());

    show = false;
    menu.visible = false;

    using enum overlay::KeyMod;
    using enum overlay::KeyCode;

    overlay::AddHotkey(*stage, 0, KeySpace, ModControl, ModShift, ModNoRepeat);

    ResetItems();
    UpdateQuery();
}

void App::ResetItems(bool end)
{
    items.clear();
    if (end)
    {
        auto item = resultList->Prev(nullptr);
        if (item)
        {
            auto itemP = item.get();
            items.push_back(std::make_unique<LaunchItem>(*this, std::move(item)));
            while ((items.size() < 5) && (item = resultList->Prev(itemP)))
            {
                itemP = item.get();
                items.push_back(std::make_unique<LaunchItem>(*this, std::move(item)));
            }
            std::reverse(items.begin(), items.end());
        }
        selection = (uint32_t)items.size() - 1;
    }
    else
    {
        auto item = resultList->Next(nullptr);
        if (item)
        {
            auto itemP = item.get();
            items.push_back(std::make_unique<LaunchItem>(*this, std::move(item)));
            while ((items.size() < 5) && (item = resultList->Next(itemP)))
            {
                itemP = item.get();
                items.push_back(std::make_unique<LaunchItem>(*this, std::move(item)));
            }
        }
        selection = 0;
    }
    FixItemAnchors();
}

void App::FixItemAnchors() {
    using namespace overlay::Alignments;

    if (items.empty())
    {
        resultsBox.visible = false;
    }
    else
    {
        resultsBox.visible = true;
        auto height = items[0]->size.y;
        items[0]->anchor = overlay::Anchor{&resultsBox, Top, overlay::Vec{0, 0}, Top};
        for (auto i = 1; i < items.size(); ++i)
        {
            height += items[i]->size.y;
            items[i]->anchor = overlay::Anchor{items[i - 1].get(),
                Bottom, overlay::Vec{0, 0}, Top};
        }
        resultsBox.size.y = height;
    }

    for (auto& i : items)
        i->box.visible = false;

    if (selection < items.size())
        items[selection]->box.visible = true;
}

void App::ResetQuery()
{
    keywords.clear();
    keywords.emplace_back();
    // tree.setMatchBits(1, 1, 0, 0);
    // tree.matchBits = 1;
    resultList->Query(QueryAction::SET, "");
    UpdateQuery();
}

void App::Update()
{
    overlay::Update(*mainLayer, *this, menu.visible);
}

std::string App::JoinQuery()
{
    auto str = std::string{};
    for (auto& k : keywords) {
        if (k.empty())
            continue;

        if (!str.empty())
            str += ' ';

        str.append(k);
    }

    return str;
}

void App::UpdateQuery()
{
    queryText.text = JoinQuery();
    queryText.Layout(*stage, true);
    queryBox.size.y = queryText.size.y + 20;

    ResetItems();
    Update();
}

void App::Move(int delta)
{
    auto i = delta;
    if (i < 0)
    {
        while (MoveSelectedUp() && ++i < 0);
    }
    else if (i > 0)
    {
        while (MoveSelectedDown() && --i > 0);
    }

    if (delta != i)
    {
        FixItemAnchors();
        Update();
    }
}

bool App::MoveSelectedUp()
{
    if (items.empty())
        return false;

    if (items.size() < 5)
    {
        if (selection == 0)
            return false;

        selection--;
    }
    else if (selection > 2)
    {
        selection--;
    }
    else
    {
        auto prev = resultList->Prev(items[0].get()->view.get());
        if (prev)
        {
            std::rotate(items.rbegin(), items.rbegin() + 1, items.rend());
            items[0]->SetPath(std::move(prev));
        }
        else if (selection > 0)
        {
            selection--;
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool App::MoveSelectedDown() {
    if (items.empty())
         return false;

    if (items.size() < 5)
    {
        if (selection == items.size() - 1)
            return false;

        selection++;
    }
    else if (selection < 2)
    {
        selection++;
    }
    else
    {
        auto next = resultList->Next(items[items.size() - 1].get()->view.get());
        if (next)
        {
            std::rotate(items.begin(), items.begin() + 1, items.end());
            items[items.size() - 1]->SetPath(std::move(next));
        }
        else if (selection < 4)
        {
            selection++;
        }
        else
        {
            return false;
        }
    }

    return true;
}

void App::OnEvent(const overlay::Event &e)
{
    using namespace overlay::Events;

    // if (e.event() != MouseMoved) {
    //   std::cout << std::format("Event - {}\n",
    //       magic_enum::enum_name<EventID>(e.event()));
    // }

    switch (e.event)
    {
    break;case IconsLoaded:
        Update();
    break;case Hotkey:
        show = true;
        Update();
        overlay::Focus(*mainLayer);
    break;case NotifyContext:
        {
            menu.visible = true;
            menu.anchor = {
                overlay::GetScreen(*stage),
                overlay::Alignments::TopLeft,
                *overlay::GetMousePos(e) + overlay::Vec{50, -10},
                overlay::Alignments::BottomRight
            };

            overlay::Update(*menuLayer, *this, menu.visible);
            overlay::Focus(*menuLayer);
        }
    break;case FocusLost:
        if (menu.visible)
        {
            std::cout << "Lost menu focus!\n";
            menu.visible = false;

            // // Need to hide main window to prevent issue with window layers not being properly adjusted
            // // This won't be required once the menu is rendered in a different layer!
            // show = false;
            // update();

            overlay::Hide(*menuLayer);
        }
    break;case KeyPressed:
        {
            using enum overlay::KeyCode;

            switch (e.GetKey())
            {
            break;case KeyEscape:
                show = false;
                overlay::Hide(*mainLayer);
                if (menu.visible)
                {
                    menu.visible = false;
                    overlay::Hide(*menuLayer);
                }
            break;case MouseLButton:
                if (overlay::Mouseover(e, queryBox) || overlay::Mouseover(e, resultsBox))
                {
                    std::cout << "  Over query!\n";
                    Update();
                    overlay::Focus(*mainLayer);
                }

                if (menu.visible && overlay::Mouseover(e, menu))
                {
                    std::cout << "Hiding menu!\n";
                    overlay::Quit(*stage, 0);
                }
                else
                {
                    menu.visible = false;
                    overlay::Hide(*menuLayer);
                }
            break;case KeyArrowDown:
                Move(1);
            break;case KeyArrowUp:
                Move(-1);
            break;case KeyArrowLeft:
                ResetItems();
                Update();
            break;case KeyArrowRight:
                ResetItems(true);
                Update();
            break;case KeyReturn: {
                if (!items.empty())
                {
                    auto view = items[selection]->view.get();
                    auto str = view->GetPath().string();
                    std::cout << std::format("Running {}!\n", str);

                    favResultList->IncrementUses(view->GetPath());
                    ResetQuery();
                    show = false;
                    overlay::Hide(*mainLayer);

                    // system(("explorer \""+ str +"\"").c_str());

                    if ((GetKeyState(VK_LSHIFT) & 0x8000)
                        && (GetKeyState(VK_LCONTROL) & 0x8000)) {
                        ShellExecuteA(nullptr, "runas", str.c_str(), nullptr, nullptr, SW_SHOW);
                    } else {
                        ShellExecuteA(nullptr, "open", str.c_str(), nullptr, nullptr, SW_SHOW);
                    }
                }
            }
            break;case KeyDelete:
                if (overlay::KeyDown(e, KeyLShift) && !items.empty())
                {
                    auto view = items[selection]->view.get();
                    favResultList->ResetUses(view->GetPath());
                    ResetQuery();
                }
            break;case KeyBackspace:
                if (overlay::KeyDown(e, KeyLShift)) {
                    ResetQuery();
                }
                else
                {
                    auto& keyword = keywords[keywords.size() - 1];
                    [[maybe_unused]] auto matchBit = static_cast<uint8_t>(1 << (keywords.size() - 1));
                    if (keyword.length() > 0)
                    {
                        keyword.pop_back();
                        // filter(matchBit, keyword, false);
                        resultList->Query(QueryAction::SET, JoinQuery());
                        UpdateQuery();
                    }
                    else if (keywords.size() > 1)
                    {
                        keywords.pop_back();
                        // tree.setMatchBits(matchBit, 0, matchBit, 0);
                        // tree.matchBits &= ~matchBit;
                        resultList->Query(QueryAction::SET, JoinQuery());
                        UpdateQuery();
                    }
                }
            break;case KeyC:
                if ((GetKeyState(VK_LCONTROL) & 0x8000) && !items.empty())
                {
                    auto view = items[selection]->view.get();
                    auto str = view->GetPath().string();
                    std::cout << std::format("Copying {}!\n", str);

                    OpenClipboard(mainLayer->hWnd);
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
            break;case KeyF5:
                if (GetKeyState(VK_CONTROL) & 0x8000)
                {

                    static std::atomic_bool indexing = false;

                    bool expected = false;
                    if (indexing.compare_exchange_strong(expected, true))
                    {
                        std::thread t([=, this] {
                            AllocConsole();
                            freopen("CONOUT$", "w", stdout);

                            std::vector<char> drives;
                            wchar_t driveNames[1024];
                            GetLogicalDriveStringsW(1023, driveNames);
                            for (wchar_t* drive = driveNames; *drive; drive += wcslen(drive) + 1)
                                drives.push_back((char)drive[0]);

                            for (auto& d : drives)
                            {
                                auto* node = IndexDrive(d);
                                auto saveLoc = std::format("{}\\.nms\\{}.index", getenv("USERPROFILE"), d);
                                std::cout << std::format("Saving to {}\n", saveLoc);
                                node->Save(saveLoc);
                            }

                            std::cout << "Indexing complete. Close this window and refresh the index with F5 in app.\n";
                            std::cout.flush();
                            FreeConsole();

                            indexing = false;
                        });

                        t.detach();
                    }
                }
                else
                {
                    resultList = std::make_unique<ResultListPriorityCollector>();
                    favResultList = std::make_unique<FavResultList>(&keywords);
                    fileResultList = std::make_unique<FileResultList>(favResultList.get());
                    resultList->AddList(favResultList.get());
                    resultList->AddList(fileResultList.get());

                    ResetItems();
                    UpdateQuery();
                }
            }
        break;case CharTyped:
            {
                auto c = static_cast<char>(e.GetCodepoint());
                auto& keyword = keywords[keywords.size() - 1];
                if (c == ' ')
                {
                    if (keyword.size() == 0 || keywords.size() == 8) return;
                    [[maybe_unused]] auto set = static_cast<uint8_t>(1 << keywords.size());
                    // tree.setMatchBits(set, set, set, 0);
                    // tree.matchBits |= set;
                    keywords.push_back("");
                }
                else
                {
                    if (c < ' ' || c > '~')
                        return;
                    // auto matchBit = static_cast<uint8_t>(1 << (keywords.size() - 1));
                    keyword += c;
                    // filter(matchBit, keyword, true);
                    resultList->Query(QueryAction::SET, JoinQuery());
                }
                UpdateQuery();
            }
        }
    break;case MouseScroll:
        Move((int)-e.GetDelta().y);
    break;case MouseMoved:
        if (menu.visible)
        {
            auto next = overlay::Mouseover(e, menu.highlight);
            if (next != menu.highlight.visible)
            {
                menu.highlight.visible = next;
                overlay::Update(*menuLayer, menu, true);
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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
// int main() {
    try
    {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        auto stage = overlay::CreateStage();
        auto app = App{&stage};

        return overlay::Run(stage, [&](const overlay::Event& e) {
            app.OnEvent(e);
        });
    }
    catch (std::exception& e)
    {
        std::cout << "Error: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cout << "Something went wrong!";
    }
    return 1;
}
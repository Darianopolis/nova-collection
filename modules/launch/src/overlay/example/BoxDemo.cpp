#include <iostream>

#include "Overlay.hpp"

namespace ui = overlay_ui;

const std::string loremIpsumText = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
    "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "
    "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat "
    "nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui "
    "officia deserunt mollit anim id est laborumy.";

class BoxDemo : public ui::NodeImpl<BoxDemo>
{
public:
    ui::Box box1;
    ui::Box box2;
    ui::Box box3;

    ui::Font font;
    ui::Text str;

    ui::Icon icon;

    ui::Font loremFont;
    ui::Text loremIpsum;

    ui::Node* beingDragged = nullptr;

    ui::Stage *stage;
    ui::Layer *layer;

    BoxDemo(ui::Stage *stage)
        : stage(stage)
        , box1(ui::Color{ 0.1f, 0.1f, 0.1f }, ui::Color{ 0, 1, 0 }, 5, 0)
        , box2(ui::Color{ 0.1f, 0.1f, 0.1f }, ui::Color{ 1, 0, 0 }, 5, 50)
        , box3(ui::Color{ 0.1f, 0.1f, 0.1f }, ui::Color{ 0, 0, 1 }, 5, 20)
        , font("Sans Serif", 35)
        , str(&font, "Hello ylq", ui::Color{ 1, 1, 1 }, ui::Vec{ 600, 0 })
        , loremFont("Consolas", 23)
        , loremIpsum(&loremFont, loremIpsumText, ui::Color{ 1, 1, 1 }, ui::Vec{ 700, 250 })
        , icon("C:\\Users\\Darian\\Desktop\\ShareX.lnk")
    {

        using namespace ui::Alignments;

        box1.anchor = { ui::screen(*stage), Center, ui::Vec{ -5, 0 }, Right };
        box1.size = ui::Vec{ 150, 150 };

        box2.anchor = { ui::screen(*stage), Center, ui::Vec{ 5, 0 }, Left };
        box2.size = ui::Vec{ 150, 150 };

        box3.anchor = { ui::screen(*stage), Center, ui::Vec{ 0, -60 }, Bottom };
        box3.size = ui::Vec{ 150, 150 };

        str.transparent_target = true;
        str.align_top_to_line = true;
        str.anchor.parent = ui::screen(*stage);

        loremFont.align = ui::FontAlign::Justified;
        loremFont.stretch = ui::FontStretch::Expanded;
        loremIpsum.transparent_target = true;
        loremIpsum.anchor.parent = ui::screen(*stage);

        icon.anchor.parent = ui::screen(*stage);

        layer = ui::layer(*stage, 0);
    }

    template<class Visit>
    void traverse(Visit& visit)
    {
        visit(box1);
        visit(box2);
        visit(box3);
        visit(str);
        visit(loremIpsum);
        visit(icon);
    }

    struct DragVisit
    {
        BoxDemo& app;
        const ui::Event& event;

        template<class Node>
        void operator ()(Node& n) {
            if (n.check_hit(*ui::mouse_pos(event)))
            {
                app.beingDragged = &n;
                ui::focus(*app.layer);
            }
        };
    };

    void event(const ui::Event& e)
    {
        using namespace ui::Events;
        using enum ui::KeyCode;

        switch (e.event)
        {
        break;case KeyPressed:
            if (e.key() == KeyR)
            {
                std::cout << "---------------------- Reset! ------------------";
                ui::update(*layer, *this, true);
            }
            else if (e.key() == MouseLButton)
            {
                auto visit = DragVisit(*this, e);
                traverse(visit);
            }
        break;case KeyReleased:
            beingDragged = nullptr;
            ui::update(*layer, *this, true);
        break;case MouseMoved:
            if (beingDragged)
            {
                beingDragged->anchor.offset += e.delta();
                ui::update(*layer, *this, true, true);
            }
        break;case MouseLeave:
            beingDragged = nullptr;
            ui::update(*layer, *this, true, false);
        }
    }
};

int main()
{
    auto stage = ui::Stage();
    auto app = BoxDemo(&stage);
    return ui::run(stage, [&](auto& e) { app.event(e); });
}

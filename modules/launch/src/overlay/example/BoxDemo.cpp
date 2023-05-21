#include <Overlay.hpp>

#include <iostream>

const std::string loremIpsumText = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
    "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "
    "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat "
    "nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui "
    "officia deserunt mollit anim id est laborumy.";

class BoxDemo : public overlay::NodeImpl<BoxDemo>
{
public:
    overlay::Box box1;
    overlay::Box box2;
    overlay::Box box3;

    overlay::Font font;
    overlay::Text str;

    overlay::Icon icon;

    overlay::Font loremFont;
    overlay::Text loremIpsum;

    overlay::Node* beingDragged = nullptr;

    overlay::Stage* stage;
    overlay::Layer* layer;

    BoxDemo(overlay::Stage* stage)
        : stage(stage)
        , box1(overlay::Color{ 0.1f, 0.1f, 0.1f }, overlay::Color{ 0, 1, 0 }, 5, 0)
        , box2(overlay::Color{ 0.1f, 0.1f, 0.1f }, overlay::Color{ 1, 0, 0 }, 5, 50)
        , box3(overlay::Color{ 0.1f, 0.1f, 0.1f }, overlay::Color{ 0, 0, 1 }, 5, 20)
        , font("Sans Serif", 35)
        , str(&font, "Hello ylq", overlay::Color{ 1, 1, 1 }, overlay::Vec{ 600, 0 })
        , loremFont("Consolas", 23)
        , loremIpsum(&loremFont, loremIpsumText, overlay::Color{ 1, 1, 1 }, overlay::Vec{ 700, 250 })
        , icon("C:\\Users\\Darian\\Desktop\\ShareX.lnk")
    {

        using namespace overlay::Alignments;

        box1.anchor = { overlay::GetScreen(*stage), Center, overlay::Vec{ -5, 0 }, Right };
        box1.size = overlay::Vec{ 150, 150 };

        box2.anchor = { overlay::GetScreen(*stage), Center, overlay::Vec{ 5, 0 }, Left };
        box2.size = overlay::Vec{ 150, 150 };

        box3.anchor = { overlay::GetScreen(*stage), Center, overlay::Vec{ 0, -60 }, Bottom };
        box3.size = overlay::Vec{ 150, 150 };

        str.transparent_target = true;
        str.alignTopToLine = true;
        str.anchor.parent = overlay::GetScreen(*stage);

        loremFont.align = overlay::FontAlign::Justified;
        loremFont.stretch = overlay::FontStretch::Expanded;
        loremIpsum.transparent_target = true;
        loremIpsum.anchor.parent = overlay::GetScreen(*stage);

        icon.anchor.parent = overlay::GetScreen(*stage);

        layer = overlay::CreateLayer(*stage, 0);
    }

    template<class Visit>
    void Traverse(Visit& visit)
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
        const overlay::Event& event;

        template<class Node>
        void operator()(Node& n) {
            if (n.CheckHit(*overlay::GetMousePos(event)))
            {
                app.beingDragged = &n;
                overlay::Focus(*app.layer);
            }
        };
    };

    void Event(const overlay::Event& e)
    {
        using namespace overlay::Events;
        using enum overlay::KeyCode;

        switch (e.event)
        {
        break;case KeyPressed:
            if (e.GetKey() == KeyR)
            {
                std::cout << "---------------------- Reset! ------------------";
                overlay::Update(*layer, *this, true);
            }
            else if (e.GetKey() == MouseLButton)
            {
                auto visit = DragVisit(*this, e);
                Traverse(visit);
            }
        break;case KeyReleased:
            beingDragged = nullptr;
            overlay::Update(*layer, *this, true);
        break;case MouseMoved:
            if (beingDragged)
            {
                beingDragged->anchor.offset += e.GetDelta();
                overlay::Update(*layer, *this, true, true);
            }
        break;case MouseLeave:
            beingDragged = nullptr;
            overlay::Update(*layer, *this, true, false);
        }
    }
};

int main()
{
    auto stage = overlay::Stage();
    auto app = BoxDemo(&stage);
    return overlay::Run(stage, [&](auto& e) { app.Event(e); });
}

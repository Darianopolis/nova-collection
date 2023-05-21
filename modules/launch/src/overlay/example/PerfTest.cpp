#include "Overlay.hpp"

#include <random>
#include <chrono>

namespace ui = overlay_ui;

static auto dist = std::uniform_real_distribution<float>(0, 1);
static auto engine = std::default_random_engine();

struct App : ui::NodeImpl<App>
{
    ui::Stage *stage;
    ui::Layer *layer;
    std::vector<std::unique_ptr<ui::Box>> boxes;

    // ui::Color bg = rc();
    // ui::Color fg = rc();

    static ui::Color rc()
    {
        return ui::Color{dist(engine), dist(engine), dist(engine), dist(engine)};
        // return ui::Color{dist(engine), dist(engine), dist(engine), 1};
    }

    App(ui::Stage &stage)
        : stage(&stage)
    {
        layer = ui::layer(stage, 0);

        float num = 50;
        float width = 3840;
        float height = 2160;
        float dx = width / num;
        float dy = height / num;

        for (float x = 0; x < width; x += dx)
        {
            for (float y = 0; y < height; y += dy)
            {
                auto &box = boxes.emplace_back(new ui::Box{rc(), rc(), 0, 0});
                // box->anchor.parent = ui::screen(stage);
                // box->anchor.from = ui::Alignments::TopLeft;
                // box->anchor.offset = ui::Vec{(float)x, (float)y};
                // box->anchor.to = ui::Alignments::TopLeft;
                box->anchor = ui::Anchor{
                    ui::screen(stage),
                    ui::Alignments::TopLeft,
                    ui::Vec{(float)x, (float)y},
                    ui::Alignments::TopLeft};
                box->size = ui::Vec{dx, dy};
            }
        }
    }

    template<class Visit>
    void traverse(Visit& visit)
    {
        for (auto& b : boxes) visit(*b);
        // for (auto& b : boxes) visit(*static_cast<Node*>(b.get()));
    }

    void change()
    {
        for (auto& b : boxes)
            b->background = rc();
    }

    std::chrono::time_point<std::chrono::steady_clock> last_time;
    size_t updates = 0;

    void onEvent(const ui::Event& e)
    {
        // std::cout << "Event!\n";
        using namespace std::chrono;

        change();
        // if (e.event == ui::EventID::Initialize)
            ui::update(*layer, *this, false);

        ++updates;
        if (duration_cast<milliseconds>(steady_clock::now() - last_time).count() > 1000)
        {
            std::cout << "FPS = " << updates << '\n';
            updates = 0;
            last_time = steady_clock::now();
        }
    }
};

int main()
{
    auto stage = ui::stage();
    auto app = App(stage);
    return ui::run(stage, [&](auto& e) { app.onEvent(e); });
    // using namespace std::chrono;
    // auto last_time = steady_clock::now();
    // auto updates = 0;
    // while (true) {
    //   ++updates;
    //   if (duration_cast<milliseconds>(steady_clock::now() - last_time).count() > 1000) {
    //     std::cout << "FPS = " << updates << '\n';
    //     updates = 0;
    //     last_time = steady_clock::now();
    //   }

    //   app.change();
    //   ui::update(*app.layer, app, false);
    // }
}
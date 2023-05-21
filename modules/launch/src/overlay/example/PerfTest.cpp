#include <Overlay.hpp>

#include <random>
#include <chrono>

static auto dist = std::uniform_real_distribution<float>(0, 1);
static auto engine = std::default_random_engine();

struct App : overlay::NodeImpl<App>
{
    overlay::Stage* stage;
    overlay::Layer* layer;
    std::vector<std::unique_ptr<overlay::Box>> boxes;

    // overlay::Color bg = rc();
    // overlay::Color fg = rc();

    static overlay::Color rc()
    {
        return overlay::Color{dist(engine), dist(engine), dist(engine), dist(engine)};
        // return overlay::Color{dist(engine), dist(engine), dist(engine), 1};
    }

    App(overlay::Stage &stage)
        : stage(&stage)
    {
        layer = overlay::CreateLayer(stage, 0);

        float num = 50;
        float width = 3840;
        float height = 2160;
        float dx = width / num;
        float dy = height / num;

        for (float x = 0; x < width; x += dx)
        {
            for (float y = 0; y < height; y += dy)
            {
                auto &box = boxes.emplace_back(new overlay::Box{rc(), rc(), 0, 0});
                // box->anchor.parent = overlay::screen(stage);
                // box->anchor.from = overlay::Alignments::TopLeft;
                // box->anchor.offset = overlay::Vec{(float)x, (float)y};
                // box->anchor.to = overlay::Alignments::TopLeft;
                box->anchor = overlay::Anchor{
                    overlay::GetScreen(stage),
                    overlay::Alignments::TopLeft,
                    overlay::Vec{(float)x, (float)y},
                    overlay::Alignments::TopLeft};
                box->size = overlay::Vec{dx, dy};
            }
        }
    }

    template<class Visit>
    void Traverse(Visit& visit)
    {
        for (auto& b : boxes) visit(*b);
        // for (auto& b : boxes) visit(*static_cast<Node*>(b.get()));
    }

    void Change()
    {
        for (auto& b : boxes)
            b->background = rc();
    }

    std::chrono::time_point<std::chrono::steady_clock> last_time;
    size_t updates = 0;

    void OnEvent(const overlay::Event& e)
    {
        // std::cout << "Event!\n";
        using namespace std::chrono;

        Change();
        // if (e.event == overlay::EventID::Initialize)
            overlay::Update(*layer, *this, false);

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
    auto stage = overlay::CreateStage();
    auto app = App(stage);
    return overlay::Run(stage, [&](auto& e) { app.OnEvent(e); });
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
    //   overlay::update(*app.layer, app, false);
    // }
}
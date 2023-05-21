#pragma once
#ifndef OVERLAY_H
#define OVERLAY_H

#include <iostream>
#include <optional>
#include <variant>
#include <functional>

#ifdef _WIN32
#include "directx/OverlayDirectXLayout.hpp"
#endif

namespace overlay
{
    struct Vec {
        float x, y;

        explicit constexpr Vec(float x, float y)
            : x(x)
            , y(y)
        {}

        explicit constexpr Vec()
            : x(0)
            , y(0)
        {}

        friend constexpr Vec operator+(const Vec& a, const Vec& b)
        {
            return Vec{a.x + b.x, a.y + b.y};
        }

        friend constexpr Vec operator-(const Vec& a, const Vec& b) {
            return Vec{ a.x - b.x, a.y - b.y};
        }

        constexpr Vec& operator+=(const Vec& other)
        {
            x += other.x;
            y += other.y;
            return *this;
        }

        constexpr Vec& operator-=(const Vec& other)
        {
            x -= other.x;
            y -= other.y;
            return *this;
        }

        friend std::ostream& operator<<(std::ostream& os, const Vec& dt)
        {
            return os << "(" << dt.x << ", " << dt.y << ")";
        }

        auto operator<=>(const Vec&) const = default;
    };

    struct Rect
    {
        float left, top, right, bottom;

        explicit constexpr Rect(float l, float t, float r, float b)
            : left(l)
            , top(t)
            , right(r)
            , bottom(b)
        {}

        explicit constexpr Rect()
            : left(0)
            , top(0)
            , right(0)
            , bottom(0)
        {}

        friend std::ostream& operator<<(std::ostream& os, const Rect& o)
        {
            return os << "[l=" << o.left << ",t=" << o.top << ",r=" << o.right << ",b=" << o.bottom << "]";
        }

        auto operator<=>(const Rect&) const = default;
    };

    struct Align
    {
        float x, y;

        explicit constexpr Align(float x, float y): x(x), y(y) {}

        // Default construct as Center
        explicit constexpr Align(): x(0), y(0) {}
    };

    namespace Alignments
    {
        constexpr Align TopLeft { -0.5, -0.5 };
        constexpr Align Top { 0, -0.5 };
        constexpr Align TopRight { 0.5, -0.5 };

        constexpr Align Left { -0.5, 0 };
        constexpr Align Center { 0, 0 };
        constexpr Align Right { 0.5, 0 };

        constexpr Align BottomLeft { -0.5, 0.5 };
        constexpr Align Bottom { 0, 0.5 };
        constexpr Align BottomRight { 0.5, 0.5 };
    }

    struct Color
    {
    public:
        float r, g, b, a;
        // ColorCache cache;

        explicit Color(float r, float g, float b, float a = 1.0F)
            : r(r)
            , g(g)
            , b(b)
            , a(a)
        {}

        explicit Color(float greyscale, float alpha = 1.0F)
            : r(greyscale)
            , g(greyscale)
            , b(greyscale)
            , a(alpha)
        {}

        explicit Color(): r(0), g(0), b(0), a(1) {}
    };

    // ---------------------------------- //
    // ------------- Events ------------- //
    // ---------------------------------- //

    enum class EventCategory
    {
        Button, Mouse, Stage
    };

    enum class EventID
    {
        KeyPressed, KeyReleased,
        CharTyped,
        MouseMoved, MouseLeave, MouseEnter,
        MouseScroll,
        NotifySelect, NotifyContext,
        FocusGained, FocusLost,
        MouseActivate,
        WindowActivate,
        Hotkey,
        IconsLoaded,
        Initialize,
    };

    namespace Events
    {
        using enum EventCategory;
        using enum EventID;
    };

    struct Layer;
    class Node;
    class Stage;
    enum class KeyCode;

    struct Event
    {

        Layer* layer;
        Stage* stage;
        EventCategory category;
        EventID event;

        std::variant<KeyCode, uint32_t, Vec>(data);

        // Event should only be constructed by a Stage's event handler.
        explicit Event(Layer* layer, Stage* stage, EventCategory category, EventID event) noexcept;
        explicit Event(Layer* layer, Stage* stage, EventCategory category, EventID event, KeyCode key) noexcept;
        explicit Event(Layer* layer, Stage* stage, EventCategory category, EventID event, uint32_t codepoint) noexcept;
        explicit Event(Layer* layer, Stage* stage, EventCategory category, EventID event, Vec delta) noexcept;

        bool operator==(EventID id) const noexcept
        {
            return event == id;
        }

        // Valid for [KeyPressed, KeyReleased]
        KeyCode GetKey() const noexcept;

        // Valid for [CharTyped]
        uint32_t GetCodepoint() const noexcept;

        // Valid for [MouseMoved, MouseScrolled]
        Vec GetDelta() const noexcept;
    };

    // --------------------------------- //
    // ------------- Stage ------------- //
    // --------------------------------- //

    struct Frame;
    void DeleteImpl(Frame*);
    void DeleteImpl(Stage*);

    std::optional<Vec> GetMousePos(const Event& event);
    bool KeyDown(const Event& event, KeyCode code);
    bool Mouseover(const Event& event, Node& node);

    Frame CreateFrame(Layer& layer, bool sticky, Rect bounds);
    void Push(Frame&);
    bool Drawable(Frame&);
    void Focus(Layer&);
    void Hide(Layer&);
    Stage CreateStage();
    int Run(Stage&, std::function<void(const Event&)> event_handler);

    Layer* CreateLayer(Stage&, uint32_t id);
    Node* GetScreen(Stage&);
    void Quit(Stage&, int code);

    void AddHotkeyImpl(Stage&, uint32_t id, KeyCode key, uint32_t modifier);

    template<AllKeyMods ... KeyMods>
    inline void AddHotkey(Stage& stage, uint32_t id, KeyCode key, KeyMods && ... mods)
    {
        AddHotkeyImpl(stage, id, key, (static_cast<uint32_t>(mods) | ... | 0));
    }

    void RemoveHotkey(Stage& stage, uint32_t id);

    // -------------------------------- //
    // ------------- Node ------------- //
    // -------------------------------- //

    class Node;

    struct Anchor
    {
        Node* parent;
        Align from;
        Vec offset;
        Align to;
    };

    struct NodeDrawVisitor;
    struct NodeLayoutVisitor;

    class Node
    {
        friend class Stage;

    public:
        Anchor anchor { this, Alignments::Center, Vec{0, 0}, Alignments::Center };
        Vec size { 0, 0 };
        Rect padding { 0, 0, 0, 0 };
        bool visible = true;
        Vec position { 0, 0 };

        Node(): visible(true) {}
        virtual ~Node() {}

        virtual void Visit(NodeDrawVisitor&) {}
        virtual void Visit(NodeLayoutVisitor&) {}

        template<class NodeVisitor>
        inline void Traverse(NodeVisitor&) { }
        inline void Draw(Frame&) { }

        Vec PointAt(const Align alignTo) const
        {
            return Vec {
                position.x + size.x * (alignTo.x - anchor.to.x),
                position.y + size.y * (alignTo.y - anchor.to.y)
            };
        }

        void Reposition(Rect& b)
        {
            position = anchor.parent->PointAt(anchor.from) + anchor.offset;

            if (!visible)
                return;

            const auto topLeft = PointAt(Alignments::TopLeft);
            const auto botRight = PointAt(Alignments::BottomRight);

            b.left = std::min(b.left, topLeft.x - padding.left);
            b.right = std::max(b.right, botRight.x + padding.right);
            b.top = std::min(b.top, topLeft.y - padding.top);
            b.bottom = std::max(b.bottom, botRight.y + padding.bottom);
        }

        bool CheckHit(const Vec& p) const
        {
            const auto topLeft = PointAt(Alignments::TopLeft);
            const auto botRight = PointAt(Alignments::BottomRight);

            return (p.x >= topLeft.x && p.y >= topLeft.y
                && p.x < botRight.x && p.y < botRight.y);
        }
    };

    template<class NodeImpl>
    inline void Draw(NodeImpl& node, Frame& frame)
    {
        node.Draw(frame);
    }

    template<class NodeImpl, class Visitor>
    inline void Traverse(NodeImpl& node, Visitor& visit)
    {
        node.Traverse(visit);
    }

    template<class NodeImpl>
    inline void Reposition(NodeImpl& node, Rect& rect)
    {
        node.Reposition(rect);
    }

    template<class Impl>
    struct NodeImpl : Node
    {
        virtual void Visit(NodeDrawVisitor& visit)
        {
            visit(*static_cast<Impl*>(this));
        }

        virtual void Visit(NodeLayoutVisitor& visit)
        {
            visit(*static_cast<Impl*>(this));
        }
    };

    // ------------------------------- //
    // ------------- Box ------------- //
    // ------------------------------- //

    struct Box : public NodeImpl<Box>
    {
        Color background;
        Color border;
        int borderWidth = 0;
        int cornerRadius = 0;

        Box(Color bgColor, Color borderColor, float borderWidth, float cornerRadius);
        ~Box();

        void Draw(const Frame& context);
    };

    // --------------------------------- //
    // ------------- Icons ------------- //
    // --------------------------------- //

    struct Icon : public NodeImpl<Node>
    {
        std::string path;

        IconCache cache;

        Icon();
        Icon(std::string_view path);

        void Reposition(Rect& bounds);

        void Draw(const Frame& context);
    };

    // -------------------------------- //
    // ------------- Font ------------- //
    // -------------------------------- //

    struct Font
    {
        std::string name;
        float size;

        std::string locale = "en-us";
        FontWeight weight = FontWeight::Regular;
        FontStyle style = FontStyle::Normal;
        FontStretch stretch = FontStretch::Normal;
        FontAlign align = FontAlign::Center;
        bool ellipsize = false;
        FontCache cache;

        Font(std::string_view name, float size);
        ~Font();
    };

    struct Text : public NodeImpl<Text>
    {
        Font* font = nullptr;
        std::string text;
        Color color;
        Vec bounds;

        Vec topleftOffset;
        bool alignToDescender = false;
        bool transparent_target = false;
        bool leftAdvance = false;
        bool rightAdvance = false;
        bool alignTopToLine = false;
        float lineHeight = 0;
        float baseline = 0;
        bool nowrap = false;

        TextCache cache;

        Text(Font* font, std::string_view str, Color color, Vec bounds);
        ~Text();

        void Layout(const Stage& context, bool reset = false);

        int LineCount();

        void Draw(const Frame& context);
    };

    struct NodeDrawVisitor
    {
        Frame& frame;

        inline void operator()(Node& node)
        {
            node.Visit(*this);
        }

        template<class NodeImpl>
        void operator()(NodeImpl& node)
        {
            overlay::Draw(node, frame);
            overlay::Traverse(node, *this);
        }
    };

    struct NodeLayoutVisitor
    {
        Rect& rect;

        inline void operator()(Node& node)
        {
            node.Visit(*this);
        }

        template<class NodeImpl>
        void operator()(NodeImpl& node)
        {
            overlay::Reposition(node, rect);
            overlay::Traverse(node, *this);
        }
    };

    Rect& GetLayerBounds(Layer&);

    inline void LazySnapFloor(float& cur, float next)
    {
        constinit static auto snap = 100.f;
        constinit static auto buffer = 100.f;

        if (next < cur)
        {
            cur = snap * std::floor(next / snap);
        }
        else if (next > cur + snap + buffer)
        {
            cur = snap * std::floor((next - snap) / snap);
        }
    }

    inline void LazySnapCeil(float& cur, float next)
    {
        constinit static auto snap = 100.f;
        constinit static auto buffer = 100.f;

        if (next > cur)
        {
            cur = snap * std::ceil(next / snap);
        }
        else if (next < cur - snap - buffer)
        {
            cur = snap * std::ceil((next + snap) / snap);
        }
    }

    template<class NodeImpl>
    inline void Update(Layer& layer, NodeImpl &node, bool sticky, bool lazySnap = false)
    {

        constinit static auto snap = 100.f;
        constinit static auto buffer = 100.f;
        constexpr auto initial = Rect{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
        };

        auto rect = initial;
        NodeLayoutVisitor{rect}(node);

        auto& bounds = GetLayerBounds(layer);

        if (rect == initial)
        {
            bounds = Rect();
            return;
        }
        else if (lazySnap)
        {
            LazySnapFloor(bounds.left, rect.left);
            LazySnapFloor(bounds.top, rect.top);
            LazySnapCeil(bounds.right, rect.right);
            LazySnapCeil(bounds.bottom, rect.bottom);
        }
        else
        {
            bounds.left = snap * std::floor(rect.left / snap);
            bounds.top = snap * std::floor(rect.top / snap);
            bounds.right = snap * std::ceil(rect.right / snap);
            bounds.bottom = snap * std::ceil(rect.bottom / snap);
        }

        auto frame = overlay::CreateFrame(layer, sticky, bounds);
        if (!overlay::Drawable(frame))
            return;

        NodeDrawVisitor{frame}(node);

        overlay::Push(frame);
    }
}

#ifdef _WIN32
#include "directx/OverlayDirectX.hpp"
#endif

#endif // !OVERLAYUI_H
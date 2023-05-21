#include "Overlay.hpp"

namespace overlay
{
    // ---------------------------------- //
    // ------------- Events ------------- //
    // ---------------------------------- //

    Event::Event(Layer* _layer, Stage* _stage, EventCategory _category, EventID _event) noexcept
        : layer(_layer)
        , stage(_stage)
        , category(_category)
        , event(_event)
    {}

    Event::Event(Layer* _layer, Stage* _stage, EventCategory _category, EventID _event, KeyCode key) noexcept
        : layer(_layer)
        , stage(_stage)
        , category(_category)
        , event(_event)
        , data(key)
    {}

    Event::Event(Layer* _layer, Stage*_stage, EventCategory _category, EventID _event, uint32_t codepoint) noexcept
        : layer(_layer)
        , stage(_stage)
        , category(_category)
        , event(_event)
        , data(codepoint)
    {}

    Event::Event(Layer* _layer, Stage* _stage, EventCategory _category, EventID _event, Vec delta) noexcept
        : layer(_layer)
        , stage(_stage)
        , category(_category)
        , event(_event)
        , data(delta)
    {}

    KeyCode Event::GetKey() const noexcept
    {
        return std::get<KeyCode>(data);
    }

    uint32_t Event::GetCodepoint() const noexcept
    {
        return std::get<uint32_t>(data);
    }

    Vec Event::GetDelta() const noexcept
    {
        return std::get<Vec>(data);
    }

    bool Mouseover(const Event& event, Node& node)
    {
        auto pos = overlay::GetMousePos(event);
        return pos && node.CheckHit(*pos);
    }

    // -------------------------------- //
    // ------------- Text ------------- //
    // -------------------------------- //

    Text::Text(Font* _font, std::string_view str, Color _color, Vec _bounds)
        : font(_font)
        , text(str)
        , color(_color)
        , bounds(_bounds)
    {}

    // -------------------------------- //
    // ------------- Font ------------- //
    // -------------------------------- //

    Font::Font(std::string_view _name, float _size)
        : name(_name)
        , size(_size)
    {}

    // ------------------------------- //
    // ------------- Box ------------- //
    // ------------------------------- //

    Box::Box(Color bgColor, Color borderColor, float _borderWidth, float cornerRadius)
        : borderWidth((int)_borderWidth)
        , background(bgColor)
        , border(borderColor)
        , cornerRadius((int)cornerRadius)
    {
        padding = Rect{_borderWidth, _borderWidth, _borderWidth, _borderWidth};
    }

    // -------------------------------- //
    // ------------- Icon ------------- //
    // -------------------------------- //

    Icon::Icon() {}

    Icon::Icon(std::string_view path)
        : path(path)
    {}
}

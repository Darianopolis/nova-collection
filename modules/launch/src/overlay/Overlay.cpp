#include "Overlay.hpp"

namespace overlay_ui {
  // ---------------------------------- //
  // ------------- Events ------------- //
  // ---------------------------------- //

  Event::Event(Layer *layer, Stage *stage, EventCategory category, EventID event) noexcept
    : layer(layer)
    , stage(stage)
    , category(category)
    , event(event) {
  }

  Event::Event(Layer *layer, Stage *stage, EventCategory category, EventID event, KeyCode key) noexcept
    : layer(layer)
    , stage(stage)
    , category(category)
    , event(event)
    , data(key) {
  }

  Event::Event(Layer *layer, Stage *stage, EventCategory category, EventID event, uint32_t codepoint) noexcept
    : layer(layer)
    , stage(stage)
    , category(category)
    , event(event)
    , data(codepoint) {
  }

  Event::Event(Layer *layer, Stage *stage, EventCategory category, EventID event, Vec delta) noexcept
    : layer(layer)
    , stage(stage)
    , category(category)
    , event(event)
    , data(delta) {
  }

  KeyCode Event::key() const noexcept {
    return std::get<KeyCode>(data);
  }

  uint32_t Event::codepoint() const noexcept {
    return std::get<uint32_t>(data);
  }
  
  Vec Event::delta() const noexcept {
    return std::get<Vec>(data);
  }

  bool mouseover(const Event& event, Node& node) {
    auto pos = overlay_ui::mouse_pos(event);
    return pos && node.check_hit(*pos);
  }

  // -------------------------------- //
  // ------------- Text ------------- //
  // -------------------------------- //

  Text::Text(Font* font, std::string_view str, Color color, Vec bounds)
    : font(font)
    , text(str)
    , color(color)
    , bounds(bounds) {
  }

  // -------------------------------- //
  // ------------- Font ------------- //
  // -------------------------------- //

  Font::Font(std::string_view name, float size)
    : name(name)
    , size(size) {
  }

  // ------------------------------- //
  // ------------- Box ------------- //
  // ------------------------------- //

  Box::Box(Color bg_color, Color border_color, float border_width, float corner_radius)
    : border_width(border_width)
    , background(bg_color)
    , border(border_color)
    , corner_radius(corner_radius) { 
    padding = Rect{border_width, border_width, border_width, border_width};
  }

  // -------------------------------- //
  // ------------- Icon ------------- //
  // -------------------------------- //

  Icon::Icon() {
  }

  Icon::Icon(std::string_view path)
    : path(path) {
  }
}

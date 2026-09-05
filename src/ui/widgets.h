#pragma once
#include <Arduino.h>

namespace ui {

struct Rect {
  int x, y, w, h;
};

bool inRect(const Rect &r, int x, int y);

void drawButton(const Rect &r, const char *label, uint16_t col);

// A menu tile: same face as drawButton, but the label is clamped to the tile
// width and an optional value line sits under it (pass nullptr for none).
void drawTile(const Rect &r, const char *label, const char *value,
              uint16_t col);

} // namespace ui

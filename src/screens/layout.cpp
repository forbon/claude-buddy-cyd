#include "layout.h"
#include "render/character.h"

namespace screens {

ui::Rect denyBtn, approveBtn;
ui::Rect ackBtn;
ui::Rect setBtns[7];
ui::Rect menuTiles[6];

void computeButtons(TFT_eSPI &t) {
  int W = t.width(), H = t.height();
  int bh = 56, m = 8;
  denyBtn = {m, H - bh - 6, (W - 3 * m) / 2, bh};
  approveBtn = {denyBtn.x + denyBtn.w + m, H - bh - 6, (W - 3 * m) / 2, bh};
  int sy = 48, sbh = 30, gap = 5; // 7 rows fit 240x320 (48 + 7*35 = 293)
  for (int i = 0; i < 7; i++)
    setBtns[i] = {20, sy + i * (sbh + gap), W - 40, sbh};
  // Menu tiles: 2 columns x 3 rows under the title, ending at y=254 so the
  // token line still fits below (2*102 + 3*12 = 240, 54 + 3*70 - 10 = 254).
  int tm = 12, tw = (W - 3 * tm) / 2, th = 60, ty0 = 54;
  for (int i = 0; i < 6; i++)
    menuTiles[i] = {tm + (i % 2) * (tw + tm), ty0 + (i / 2) * (th + 10), tw, th};
  // "Got it" button: the cardless needs-you screen frees the whole lower third,
  // so centre a roomy pill there as the dismiss call-to-action.
  int aw = 150, ah = 46, cardTop = render::REG_Y + render::REG_H;
  ackBtn = {(W - aw) / 2, cardTop + (H - cardTop - ah) / 2, aw, ah};
}

} // namespace screens

#include "settings.h"
#include "layout.h"
#include "app/ctx.h"
#include "net/ble.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace screens {

// Page 0: the long-press menu as a 2-column tile grid. Tiles that carry a value
// (Quiet, Brightness) show it under the label, so a glance tells you the state
// without opening anything.
static void renderMenu() {
  TFT_eSPI &t = ui::tft();
  int W = t.width();
  t.fillScreen(TFT_BLACK);
  ui::gtext("Menu", W / 2, 14, &FreeSansBold18pt7b, C_CORAL, TFT_BLACK,
            TC_DATUM);
  char bri[12];
  if (app::ctx.autoDim)
    snprintf(bri, sizeof(bri), "auto");
  else
    snprintf(bri, sizeof(bri), "%d%%", app::ctx.brightPct);
  // NOTE: no Battery tile on purpose -- the gauge is fully automatic (top-bar
  // glyph + Stats panel show it). Power off keeps the first slot in red: it's
  // the tile reached for most, and waking back up is a single tap.
  const char *labels[6] = {"Power off", "Stats",    "Quiet",
                           "Brightness", "Settings", "Close"};
  const char *values[6] = {nullptr, nullptr, app::ctx.dnd ? "on" : "off",
                           bri,     nullptr, nullptr};
  for (int i = 0; i < 6; i++)
    ui::drawTile(menuTiles[i], labels[i], values[i],
                 i == 0 ? C_NO : (i == 2 && app::ctx.dnd) ? 0x7B40 : C_FACE);
  // The setup secret for ~/.claude/buddy.json. It sits here, not on the Stats
  // panel: the grid ends at y=254 and leaves the bottom free, whereas Stats'
  // 13 rows + hint already fill 320px and 16 hex chars are wider than its
  // 114px value column.
  ui::gtext("buddy.json token", W / 2, 266, &FreeSans9pt7b, C_MUTED, TFT_BLACK,
            TC_DATUM);
  ui::gtext(net::ble.state().token.c_str(), W / 2, 286, &FreeSansBold9pt7b,
            C_TEXT, TFT_BLACK, TC_DATUM);
}

// Page 1: the preferences that change how the device behaves over time. Rows
// are full width (the values are sentences, not chips) and each tap cycles.
static void renderPrefs() {
  TFT_eSPI &t = ui::tft();
  int W = t.width();
  t.fillScreen(TFT_BLACK);
  ui::gtext("Settings", W / 2, 14, &FreeSansBold18pt7b, C_CORAL, TFT_BLACK,
            TC_DATUM);
  char off[28], slp[28], wow[28], ndg[28], ask[28];
  uint32_t sec = app::ctx.screenOffMs / 1000;
  if (!sec)
    snprintf(off, sizeof(off), "Screen off: never");
  else if (sec < 60)
    snprintf(off, sizeof(off), "Screen off: %us", (unsigned)sec);
  else
    snprintf(off, sizeof(off), "Screen off: %umin", (unsigned)(sec / 60));
  snprintf(slp, sizeof(slp), "Deep sleep: %s",
           app::ctx.autoSleep ? "after 1h" : "off");
  snprintf(wow, sizeof(wow), "Wake on work: %s",
           app::ctx.wakeOnWork ? "on" : "off");
  uint32_t nsec = app::ctx.nudgeMs / 1000;
  if (!nsec)
    snprintf(ndg, sizeof(ndg), "Nudge screen: off");
  else if (nsec < 60)
    snprintf(ndg, sizeof(ndg), "Nudge screen: %us", (unsigned)nsec);
  else
    snprintf(ndg, sizeof(ndg), "Nudge screen: %umin", (unsigned)(nsec / 60));
  snprintf(ask, sizeof(ask), "Ask on device: %s",
           app::ctx.askOnDevice ? "on" : "off");
  const char *labels[7] = {off, slp, wow, ndg,
                           ask, "Recalibrate touch", "Back"};
  for (int i = 0; i < 7; i++)
    ui::drawButton(setBtns[i], labels[i], C_FACE);
  // Deep sleep is only ever reached from a dark screen, so the two settings can
  // contradict each other -- say so instead of letting it look broken.
  if (!app::ctx.screenOffMs && app::ctx.autoSleep)
    ui::gtext("deep sleep needs screen off", W / 2, 296, &FreeSans9pt7b,
              C_MUTED, TFT_BLACK, TC_DATUM);
}

void renderSettings(int page) {
  if (page)
    renderPrefs();
  else
    renderMenu();
}

} // namespace screens

#include "display.h"
#include <Arduino.h>

namespace hal {

// Backlight on a LEDC PWM channel so it can dim (pre-sleep fade + user
// brightness) instead of a hard on/off. TFT_BACKLIGHT_ON=1 -> active HIGH, so a
// higher duty is brighter.
#define BL_CH 7
#define BL_FREQ 5000
#define BL_RES 8 // 0..255 duty

// The ILI9341_2 init (TFT_eSPI issue #1172) gets this panel's geometry and
// inversion right but ships an incomplete colour setup: it never writes GMCTRN1
// (negative gamma), truncates GMCTRP1, and uses VCOM values that differ from
// stock. VCOM sets the black level, so it anchors the bottom of the transfer
// curve -- exactly where this panel misbehaves (near-black C_CARD came up navy,
// coral washed to cream). Replaying the stock ILI9341 colour block after init.
//
// Writing gamma alone was tried first and changed nothing, which fits: with
// VCOM off, the curve is shifted no matter what the gamma registers say.
//
// Must sit inside startWrite()/endWrite(): TFT_eSPI::writedata() ends with CS_L
// and leaves the bus asserted, so calling it standalone desyncs the next
// command (an earlier attempt flipped the panel into INVON and noise).
//
// ponytail: delete this function and its call to go back to the panel's own
// curve, then re-tune C_CARD and the character tint in the palette instead.
static void writeStockColour(TFT_eSPI &t) {
  struct Reg { uint8_t cmd, len; uint8_t data[15]; };
  static const Reg regs[] = {
      {0xC5, 2, {0x3E, 0x28}},                    // VMCTR1 (alt: 30 30)
      {0xC7, 1, {0x86}},                          // VMCTR2 (alt: B7)
      {0xE0, 15, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07,
                  0x10, 0x03, 0x0E, 0x09, 0x00}}, // GMCTRP1
      {0xE1, 15, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08,
                  0x0F, 0x0C, 0x31, 0x36, 0x0F}}, // GMCTRN1
  };
  t.startWrite();
  for (const Reg &r : regs) {
    t.writecommand(r.cmd);
    for (uint8_t i = 0; i < r.len; i++)
      t.writedata(r.data[i]);
  }
  t.endWrite();
}

void Display::begin() {
  tft_.init();
  writeStockColour(tft_);
  tft_.setRotation(0); // portrait 240x320, USB at bottom
  // Attach the PWM channel AFTER init(): TFT_eSPI::init() can pinMode the
  // backlight pin itself, which would steal it back from LEDC and kill dimming.
  ledcSetup(BL_CH, BL_FREQ, BL_RES);
  ledcAttachPin(TFT_BL, BL_CH);
  backlight(true);
  tft_.fillScreen(TFT_BLACK);
}

void Display::backlightLevel(uint8_t pct) {
  if (pct > 100)
    pct = 100;
  cur_ = tgt_ = pct; // an instant set cancels any glide in flight
  ledcWrite(BL_CH, (uint32_t)pct * 255 / 100);
}

void Display::glideTo(uint8_t pct) { tgt_ = pct > 100 ? 100 : pct; }

// Ease-out step toward the glide target: a quarter of the remaining distance
// every 16 ms (~250 ms for a full swing). Smooths the pre-sleep dim and the
// ambient-light brightness changes instead of visibly snapping.
void Display::tick(uint32_t now) {
  if (cur_ == tgt_ || now - lastStep_ < 16)
    return;
  lastStep_ = now;
  int d = (int)tgt_ - (int)cur_;
  int step = d / 4;
  if (step == 0)
    step = d > 0 ? 1 : -1;
  cur_ = (uint8_t)((int)cur_ + step);
  ledcWrite(BL_CH, (uint32_t)cur_ * 255 / 100);
}

void Display::setBrightness(uint8_t pct) {
  if (pct < 10)
    pct = 10; // keep "on" visibly lit even at the lowest setting
  if (pct > 100)
    pct = 100;
  brightness_ = pct;
}

void Display::backlight(bool on) { backlightLevel(on ? brightness_ : 0); }

} // namespace hal

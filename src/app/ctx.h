#pragma once
#include <Arduino.h>

namespace app {

// Small cross-cutting runtime state shared between the loop (which writes it)
// and the screens (which render it). Everything else stays private to the
// module that owns it.
struct Ctx {
  uint32_t sessionStart = 0; // millis when the current Claude session began
  int intensity = 0;         // session tier: 0 calm / 1 busy / 2 intense
  bool dnd = false;          // "Quiet": silence the LED, no auto-wake nudges
  int brightPct = 100;       // the user's "on" backlight level (PWM %)
  bool autoDim = false;      // follow the onboard light sensor: night-dim when
                             // the room goes dark ("Brightness: auto")
  uint32_t screenOffMs = 30000; // idle time before the backlight cuts; 0 = never
  bool autoSleep = true;        // dark + no events for an hour -> deep sleep
  bool wakeOnWork = true;       // light up when Claude starts a turn
  uint32_t nudgeMs = 45000;     // unanswered turn -> wake the screen after
                                // this long; 0 = never (LED still nudges)
  bool askOnDevice = true;      // answer PermissionRequest prompts by tapping
                                // the device; off = pass back to the terminal
};

extern Ctx ctx;

} // namespace app

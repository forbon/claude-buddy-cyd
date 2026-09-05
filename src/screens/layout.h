#pragma once
#include <TFT_eSPI.h>
#include "ui/widgets.h"

namespace screens {

// Shared tap-target layout, computed once from the panel size at boot: the
// bottom Deny/Approve action bar (ask screen), the Settings rows, and the
// "Got it" pill on the needs-you screen.
extern ui::Rect denyBtn, approveBtn;
extern ui::Rect ackBtn; // "Got it" pill on the needs-you screen (dismisses -> idle)
// Settings page rows: screen-off / deep sleep / wake-on-work / nudge /
// ask-on-device / recalibrate / back
extern ui::Rect setBtns[7];
// Menu tiles (2 x 3): Power off / Stats / Quiet / Brightness / Settings / Close
extern ui::Rect menuTiles[6];

void computeButtons(TFT_eSPI &t);

} // namespace screens

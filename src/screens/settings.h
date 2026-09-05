#pragma once

namespace screens {

// The long-press menu. page 0 = the tile grid (Power off / Stats / Quiet /
// Brightness / Settings / Close), page 1 = the preferences list (screen-off
// timeout, deep sleep, wake-on-work, recalibrate). Labels reflect app::ctx;
// tap dispatch stays with the caller (main's loop).
void renderSettings(int page);

} // namespace screens

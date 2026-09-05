"""Unit tests for the hook's pure logic (no device, no bridge, no Claude Code).
Run: cd tools && python -m unittest test_buddy_hook -v"""
import json
import os
import tempfile
import time
import unittest

import buddy_hook as bh


class TestRateLimits(unittest.TestCase):
    """_rate_limits() turns the statusline's file into the device payload."""

    def setUp(self):
        self.tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".json")
        self.tmp.close()
        self._saved = bh.RL_STATE
        bh.RL_STATE = self.tmp.name

    def tearDown(self):
        bh.RL_STATE = self._saved
        os.unlink(self.tmp.name)

    def write(self, pct5, pct7, in5=7800, in7=270000, age=0):
        now = time.time()
        with open(self.tmp.name, "w", encoding="utf-8") as f:
            json.dump({"five_hour": {"pct": pct5, "resets_at": now + in5},
                       "seven_day": {"pct": pct7, "resets_at": now + in7}}, f)
        if age:
            os.utime(self.tmp.name, (now - age, now - age))

    def test_percent_and_reset_time(self):
        now = time.time()
        self.write(72.4, 41)
        r = bh._rate_limits()
        self.assertEqual(r["r5"], 72)      # rounded to a whole percent
        self.assertEqual(r["r7"], 41)
        # local wall clock, formatted here because the device has none
        self.assertEqual(r["r5t"], time.strftime("%H:%M",
                                                 time.localtime(now + 7800)))
        self.assertEqual(r["r7t"], time.strftime("%a %H:%M",
                                                 time.localtime(now + 270000)))

    def test_percent_is_clamped(self):
        # A window can read over 100 while it's being enforced; the device's
        # gauge is a 0..100 bar, so clamp rather than overflow it.
        self.write(140, -3)
        r = bh._rate_limits()
        self.assertEqual(r["r5"], 100)
        self.assertEqual(r["r7"], 0)

    def test_missing_reset_is_blank_not_epoch_zero(self):
        # A window with a percent but no resets_at must not render as 1970.
        with open(self.tmp.name, "w", encoding="utf-8") as f:
            json.dump({"five_hour": {"pct": 10}}, f)
        self.assertEqual(bh._rate_limits()["r5t"], "")

    def test_stale_file_ignored(self):
        # The statusline hasn't run in over an hour -> the windows have moved on.
        # Better no gauge than a confidently wrong one.
        self.write(72, 41, age=7200)
        self.assertEqual(bh._rate_limits(), {})

    def test_missing_file_is_empty(self):
        os.unlink(self.tmp.name)
        self.assertEqual(bh._rate_limits(), {})
        open(self.tmp.name, "w").close()  # so tearDown's unlink still works

    def test_no_five_hour_means_no_gauge(self):
        # Nothing to anchor the card on -> the device keeps its token counters.
        with open(self.tmp.name, "w", encoding="utf-8") as f:
            json.dump({"seven_day": {"pct": 41, "resets_at": time.time()}}, f)
        self.assertEqual(bh._rate_limits(), {})


if __name__ == "__main__":
    unittest.main()

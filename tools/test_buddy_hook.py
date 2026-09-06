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


class TestAntigravity(unittest.TestCase):
    """_from_agy()/_scan_agy(): the second harness's payload and transcript."""

    COMMON = {"conversationId": "ec33ebf9-0cba-4100-8142-c61503f6c587",
              "workspacePaths": ["E:/claude-buddy-cyd"],
              "transcriptPath": "/tmp/t.jsonl",
              "modelName": "auto"}

    def test_pre_invocation_maps_to_prompt_submit(self):
        d = bh._from_agy(dict(self.COMMON), ["PreInvocation"])
        self.assertEqual(d["hook_event_name"], "UserPromptSubmit")
        # camelCase in, snake_case out -- the rest of the script reads these
        self.assertEqual(d["session_id"], self.COMMON["conversationId"])
        self.assertEqual(d["transcript_path"], "/tmp/t.jsonl")
        self.assertEqual(bh._project(d), "claude-buddy-cyd")
        self.assertNotIn("_tick", d)  # no tool ran -> don't inflate the burst

    def test_post_tool_use_carries_tool_and_ticks_the_burst(self):
        # agy's PostToolUse payload has no tool name at all; hooks.json's
        # matcher supplies it as argv, already spelled the Claude way.
        d = bh._from_agy(dict(self.COMMON, stepIdx=5), ["PostToolUse", "Edit"])
        self.assertEqual(d["hook_event_name"], "PostToolUse")
        self.assertEqual(d["tool_name"], "Edit")
        self.assertTrue(d["_tick"])
        self.assertNotIn("tool_response", d)

    def test_tool_error_reuses_the_claude_wince_shape(self):
        d = bh._from_agy(dict(self.COMMON, error="exit status 1"),
                         ["PostToolUse", "Bash"])
        self.assertTrue(d["tool_response"]["error"])

    def test_pre_tool_use_maps_to_pre_tool_use(self):
        d = bh._from_agy(dict(self.COMMON, stepIdx=2), ["PreToolUse", "Bash"])
        self.assertEqual(d["hook_event_name"], "PreToolUse")
        self.assertEqual(d["tool_name"], "Bash")

    def test_unmirrored_and_missing_events_are_dropped(self):
        for argv in (["PostInvocation"], [""], []):
            self.assertIsNone(bh._from_agy(dict(self.COMMON), argv))

    def test_missing_workspace_does_not_crash(self):
        d = bh._from_agy({"conversationId": "x"}, ["Stop"])
        self.assertEqual(d["cwd"], "")

    def _transcript(self, *lines):
        tmp = tempfile.NamedTemporaryFile("w", delete=False, suffix=".jsonl",
                                          encoding="utf-8")
        for o in lines:
            tmp.write(json.dumps(o) + "\n")
        tmp.close()
        self.addCleanup(os.unlink, tmp.name)
        return tmp.name

    def test_scan_counts_turns_and_tool_calls_but_never_tokens(self):
        p = self._transcript(
            {"step_index": 0, "source": "USER_EXPLICIT", "type": "USER_INPUT"},
            {"step_index": 1, "source": "MODEL", "type": "PLANNER_RESPONSE",
             "tool_calls": [{"name": "run_command"}, {"name": "view_file"}]},
            {"step_index": 2, "source": "MODEL", "type": "GENERIC"},
            {"step_index": 3, "source": "MODEL", "type": "PLANNER_RESPONSE",
             "tool_calls": [{"name": "write_to_file"}]},
            {"step_index": 4, "source": "SYSTEM", "type": "SYSTEM_MESSAGE"},
        )
        self.assertEqual(bh._scan_agy(p),
                         {"tok": 0, "tools": 3, "turns": 2})

    def test_scan_survives_a_half_written_last_line(self):
        p = self._transcript({"step_index": 0, "source": "MODEL",
                              "type": "PLANNER_RESPONSE"})
        with open(p, "a", encoding="utf-8") as f:
            f.write('{"step_index":1,"source":"MOD')
        self.assertEqual(bh._scan_agy(p)["turns"], 1)

    def test_scan_of_a_missing_file_is_none(self):
        # None -> _today_stats bails and the device keeps its last snapshot,
        # rather than reporting a session that suddenly did nothing.
        self.assertIsNone(bh._scan_agy(os.path.join(tempfile.gettempdir(),
                                                    "no-such-transcript.jsonl")))


class TestIntensityTick(unittest.TestCase):
    """_intensity() counts a call when the caller says so, not per event name."""

    def setUp(self):
        self.tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".json")
        self.tmp.close()
        os.unlink(self.tmp.name)
        self._saved = bh.RT_STATE
        bh.RT_STATE = self.tmp.name
        self.addCleanup(lambda: setattr(bh, "RT_STATE", self._saved))

    def tearDown(self):
        if os.path.exists(self.tmp.name):
            os.unlink(self.tmp.name)

    def test_tick_accumulates_and_agents_track_subagents(self):
        self.assertEqual(bh._intensity(True, "Bash"), (1, 0))
        self.assertEqual(bh._intensity(True, "Task"), (2, 1))
        # a non-tool event reports the window without adding to it
        self.assertEqual(bh._intensity(False, ""), (2, 1))


if __name__ == "__main__":
    unittest.main()

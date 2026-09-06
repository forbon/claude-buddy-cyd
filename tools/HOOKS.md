# CYD Buddy — hooks setup

The buddy is driven entirely by agent **hooks** (no official Hardware Buddy
feature needed) — Claude Code below, and Antigravity (`agy`) alongside it in
§5. A tiny helper (`buddy_hook.py`) POSTs hook events to a
local **bridge** (`buddy_bridge.py`, spawned on demand, exits when Claude goes
quiet) which relays them to the CYD over **Bluetooth LE**. By default it is a
**passive stats dashboard** — every status hook is non-blocking and never
affects a session. One **optional** hook (`PermissionRequest`, §2.1) is
synchronous and lets you **approve a tool call by tapping the device** instead
of the terminal; it always fails open, so the session is never stuck if the
device is off.

## 1. Config (device secret; NOT committed)

`~/.claude/buddy.json`:
```json
{ "token": "<the token shown on the device>" }
```
The CYD shows its `token` under long-press → the **Menu**, below the tiles
(also on the serial boot log as `[auth] token=…`). Optional
keys: `"port"` (move the bridge off `127.0.0.1:8787`), `"budget"` (daily token
gauge), `"host"` (advanced: point the hook at a bridge on **another** machine,
e.g. `"192.0.2.10:8787"` — see §4). One PC-side dependency:
`python -m pip install bleak`.

## 2. Hooks (`~/.claude/settings.json`)

All events are non-blocking (`async: true`), so they never slow Claude. Add the
buddy helper to the events you want the device to react to:

```json
{
  "hooks": {
    "SessionStart":     [{ "hooks": [{ "type": "command", "command": "python \"<repo>/tools/buddy_hook.py\"", "async": true, "timeout": 10 }] }],
    "UserPromptSubmit": [{ "hooks": [{ "type": "command", "command": "python \"<repo>/tools/buddy_hook.py\"", "async": true, "timeout": 10 }] }],
    "PreToolUse":       [{ "hooks": [{ "type": "command", "command": "python \"<repo>/tools/buddy_hook.py\"", "async": true, "timeout": 10 }] }],
    "PostToolUse":      [{ "hooks": [{ "type": "command", "command": "python \"<repo>/tools/buddy_hook.py\"", "async": true, "timeout": 10 }] }],
    "Stop":             [{ "hooks": [{ "type": "command", "command": "python \"<repo>/tools/buddy_hook.py\"", "async": true, "timeout": 10 }] }],
    "SessionEnd":       [{ "hooks": [{ "type": "command", "command": "python \"<repo>/tools/buddy_hook.py\"", "async": true, "timeout": 10 }] }],
    "Notification":     [{ "hooks": [{ "type": "command", "command": "python \"<repo>/tools/buddy_hook.py\"", "async": true, "timeout": 10 }] }]
  }
}
```
Replace `<repo>` with the absolute path to this checkout — **or**, for a setup
that doesn't depend on the repo, copy the single file `buddy_hook.py` to
`~/.claude/buddy_hook.py` and point the command there (see §4). `PreToolUse`/
`PostToolUse` make the activity + tool counter update live on every tool call.

## 2.1 Optional: approve tool calls on the device (`PermissionRequest`)

Want to tap **Allow / Deny** on the gadget instead of the terminal? Add **one
more** hook — but this one is **synchronous** (no `async`), because Claude waits
for your tap:

```json
"PermissionRequest": [{ "hooks": [{ "type": "command", "command": "python \"<repo>/tools/buddy_hook.py\"", "timeout": 30 }] }]
```

`PermissionRequest` fires **only when a permission prompt would appear** (not on
every tool), so it never slows ordinary auto-approved calls. When it fires the
device shows the tool name with **Allow / Deny** buttons; your tap is returned to
Claude as the permission decision. **Fail-open guarantees:** if the device is
unreachable, or you don't tap within ~26 s, the hook prints nothing and Claude
falls back to the **normal terminal prompt** — you're never blocked. Leave this
hook out entirely to keep the device purely a dashboard.

You can also switch it off **on the device** — long-press → **Menu → Settings →
Ask on device: off**. The device then answers the bridge with `pass` the moment
the ask arrives, so the hook returns immediately and you get the terminal prompt
without sitting through the timeout. Handy when you walk away from the desk but
don't want to touch `settings.json`.

## 2.2 Optional: plan-limit gauges (status line)

Want the card's two headline numbers to be **how much of your 5-hour and weekly
limits you've spent**, instead of today's and all-time tokens? That data exists
in exactly one place on your machine: the JSON Claude Code pipes into your
**status line**. It is not in hook payloads, not in the transcripts, and there is
no `claude usage` command — so without a status line there is nothing to show,
and the device keeps its token counters.

Have your status line script write the two windows to `~/.claude/buddy_rl.json`:

```json
{"five_hour": {"pct": 72.4, "resets_at": 1788625885},
 "seven_day": {"pct": 41,   "resets_at": 1788888085}}
```

`pct` is `rate_limits.<window>.used_percentage` and `resets_at` is that window's
`resets_at` (Unix epoch seconds), both straight out of the status line's stdin
JSON. The buddy hook reads the file on its next event and forwards the
percentages plus each window's reset time, formatted in your local timezone
(the device has no clock) — so the status line does **no** network I/O and stays
fast. PowerShell:

```powershell
if ($data.rate_limits) {
    $rl = @{
        five_hour = @{ pct = $data.rate_limits.five_hour.used_percentage; resets_at = $data.rate_limits.five_hour.resets_at }
        seven_day = @{ pct = $data.rate_limits.seven_day.used_percentage; resets_at = $data.rate_limits.seven_day.resets_at }
    } | ConvertTo-Json -Compress
    $rlPath = Join-Path $HOME '.claude\buddy_rl.json'
    try {
        if (-not (Test-Path $rlPath) -or (Get-Content $rlPath -Raw -EA Stop) -ne $rl) {
            Set-Content -Path $rlPath -Value $rl -Encoding utf8 -NoNewline -EA Stop
        }
    } catch { }   # a side channel must never break the status line
}
```

**Fail-safe by design:** the file is ignored when it's missing or older than an
hour, `rate_limits` is absent for API-key users, and a missing `five_hour` drops
the gauge entirely. In every one of those cases the card falls back to Today /
Total tokens rather than showing a confident `0%`.

## 3. What it sends

Per event the helper pushes the current activity (a rotating whimsical verb
while busy) plus today's usage rollup read from the session transcript: tokens
(today + all-time), tool calls, assistant turns, and session count. The
transcript is scanned **incrementally** (per-session byte offset persisted in
`buddy_tokens.json`), so events stay fast even on a session whose transcript
has grown to tens of MB. Today's
counts persist in `~/.claude/buddy_tokens.json` and reset at local midnight.
It also stamps each event with the PC-local **date**, which the device uses to
key its on-device 30-day usage history (the trends card) — the device itself
has no clock. An older helper without the date simply leaves the trends card
empty; everything else still works.

## 4. Use it from another computer (no repo needed)

The flashed device is **fully standalone** — firmware and the animation pack
live in its own flash, so it needs no PC, no repo, and no cloud; it just boots
and advertises over BLE. Two ways to drive it from elsewhere:

**A. Another machine with its own Bluetooth** (you carried the buddy over):
repeat the normal setup there — copy **two** files, `buddy_hook.py` and
`buddy_bridge.py` (repo-independent home: `~/.claude/`; keep them side by side
— the hook spawns the bridge from its own directory), `pip install bleak`,
create `buddy.json` with the `token`, add the hooks from §2 pointing at that
copy (absolute path; on Windows `%USERPROFILE%\.claude\buddy_hook.py`).

**B. A machine without Bluetooth reach** (remote box, VM): on the PC that sits
near the buddy, run the bridge listening beyond localhost —
`python buddy_bridge.py --listen 0.0.0.0` — and on the remote machine put
`"host": "<that-pc>:8787"` into `buddy.json` (reachable over LAN or a mesh VPN
such as Tailscale). The remote machine only needs `buddy_hook.py`; with `host`
set it never tries to spawn a local bridge.

Requirements: **Python 3 on `PATH`** (plus `bleak` wherever a bridge runs).
`buddy_tokens.json` is created automatically on first run.

Each machine keeps its **own** `buddy_tokens.json`, so today/all-time counts are
per-machine, not merged. If two machines push at once, the device shows whichever
pushed last.

## 5. Antigravity (`agy`) as a second harness

The same helper also runs as an **Antigravity lifecycle hook**, so both agents
drive the one device. Nothing changes on the CYD or in the bridge; only the
hook config and a payload translation at the door differ.

Copy [`hooks.agy.json`](hooks.agy.json) to **`~/.gemini/config/hooks.json`**
(the shared location the `/hooks` command uses; a workspace-local
`<workspace>/.agents/hooks.json` also works) and replace `<repo>` with the
absolute path to this checkout:

```json
{ "cyd-buddy": {
    "PreInvocation": [ { "command": "python <repo>/tools/buddy_hook.py agy PreInvocation", "timeout": 8 } ],
    "PostToolUse":   [ { "matcher": "run_command|manage_task",
                         "hooks": [ { "command": "python <repo>/tools/buddy_hook.py agy PostToolUse Bash", "timeout": 8 } ] } ],
    "Stop":          [ { "command": "python <repo>/tools/buddy_hook.py agy Stop", "timeout": 8 } ] } }
```

> **Windows tip:** Do not wrap the script path in quotes (`\"...\"`) in `hooks.json`. On Windows, Antigravity executes commands via `cmd /c` using Go's `os/exec`, which escapes quotes into `\"`, causing Python to fail with `[Errno 22] Invalid argument`. Use forward slashes without quotes (e.g. `python E:/claude-buddy-cyd/tools/buddy_hook.py agy PreInvocation`).

(Abridged — the shipped file has all five `PostToolUse` matcher groups.) Two
oddities of agy's contract are why the event name and the tool name are passed
as **arguments**:

- The payload carries **no event name** at all (no `hook_event_name`).
- `PostToolUse`'s payload **omits the tool** — only the `matcher` in
  `hooks.json` knows which tool fired. So each matcher group names the
  equivalent Claude tool (`Bash`, `Edit`, `Read`, `WebFetch`, `Task`) and the
  existing activity map does the rest.

Everything else is renaming: `conversationId` → `session_id`,
`workspacePaths[0]` → `cwd`, `transcriptPath` → `transcript_path`,
`error` → the `tool_response` shape the wince logic already reads.

### Which events, and why not the others

| agy event | wired | device state |
| :--- | :--- | :--- |
| `PreInvocation` | yes | thinking / running |
| `PreToolUse` | yes | real-time activity switch (typing, building, reading, etc.) before tool runs |
| `PostToolUse` | yes | updates stats; `error` → wince |
| `Stop` | yes | done + celebrate, "your turn" nudge |
| `PostInvocation` | **no** | duplicates `PostToolUse` for our purposes |

`PreToolUse` immediately responds with `{"decision": "ask"}` so Antigravity falls
back to normal user permissions (fail-open), while pushing the active tool's clip to
the buddy device in real time before long commands or tasks execute.

agy has no `SessionStart`/`SessionEnd`/`Notification` equivalent, but `ask_question`
is mapped to an amber notification reaction ("NEEDS YOU") on the buddy.

### Tokens: not available

Antigravity's `transcript.jsonl` is a step log
(`step_index`/`source`/`type`/`tool_calls`) and — checked against real logs —
contains **no token usage of any kind**. There is no local usage file and no
usage field in the hook payload either. So agy sessions contribute **tool calls,
turns and the session count**, and `tok` stays `0`: the card shows the token
numbers it can actually source rather than an invented one. Both harnesses key
their sessions by UUID, so they roll up into the same card without colliding.

### Cost of running it

agy hooks are **synchronous and block the agent loop** (there is no `async`
flag like Claude Code's). That is fine here because the bridge answers `/event`
with `202` immediately — it never waits for the BLE write. With no bridge
running the connect is refused instantly, the bridge is spawned, and that one
event is dropped. Timeout is set to 8 s against the helper's own 5 s HTTP
timeout.

## 6. Behaviour / safety

- Bridge missing → the hook spawns it and drops that one event (the next event
  heals the display). Device off or out of range → the bridge accepts events
  and quietly discards them. Either way nothing blocks or breaks a session.
- The optional approval hook (§2.1) **fails open**: a disconnected device or a
  no-tap timeout yields no decision, so Claude shows its normal prompt. It can
  only *grant* permission you'd otherwise be asked for — it never auto-runs a
  tool Claude wasn't already about to ask about.
- The helper bypasses the system HTTP proxy (the bridge is on localhost).
- The token is a shared secret; treat `buddy.json` as private (it is not part of
  this repo).

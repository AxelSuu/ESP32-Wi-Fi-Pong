# R1 — Controller Descriptors Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **PROJECT RULE (overrides the skill's "commit/build" steps):** Per `CLAUDE.md`, the **agent
> never commits, builds, flashes, or monitors hardware.** The agent's loop is: make edits → run
> **host** tests (`make -C test/host run`, host gcc, allowed) → hand off to the user. Every
> "Checkpoint" below is a **user** action (review diff, commit, build, flash). Do not run
> `git commit`, `idf.py`, or any hardware command.

**Goal:** Replace the six hand-rolled per-game control surfaces in `index.html` with one generic
renderer driven by a `controls` descriptor the firmware sends in the `active` message, so every
future game ships as pure firmware (zero frontend edits).

**Architecture:** Add a `const char *controls` field to the `game_module_t` vtable — a small,
trusted JSON-array string each game owns. The `active` WS message carries it verbatim
(`proto_fmt_active` gains a `controls` argument). The frontend gains `renderControls(spec)`,
which builds a widget stack (joystick / tilt / dpad / btn / pick) into a single `data-mode="game"`
surface. The existing per-game `data-mode` divs are deleted; existing games are re-expressed as
descriptors to prove the system before any new game depends on it.

**Tech Stack:** C (ESP-IDF v6.0, `-Wall -Werror -Wextra`), dependency-free `proto.c`, host test
harness (`test/host/`, gcc), vanilla JS/CSS in `spiffs_image/index.html` (SPIFFS asset).

**Widget vocabulary (closed):** `joystick` → `move`; `tilt` → `tilt` (+ a `dpad` for L/R
fallback); `dpad` with a `dirs` subset of `up/down/left/right` → `input`; `btn` `{label, ev}`
→ `input`; `pick` → the ◀ PICK ▶ row (`nav -1` / `select` / `nav +1`). `back` (MENU) and the
connection/⚙ chrome stay global, outside the descriptor.

---

## File structure (what changes)

- `main/proto.h` / `main/proto.c` — `proto_fmt_active` gains a `controls` param (embeds it raw).
- `test/host/test_proto.c` — update the two `proto_fmt_active` assertions.
- `main/game_module.h` — add `const char *controls;` to `game_module_t`.
- `main/network.h` / `main/network.c` — `net_broadcast_active` gains `controls`; grow its buffer.
- `main/engine.c` — pass `g->controls` at the two call sites.
- `main/pong.c`, `snake.c`, `racer.c`, `breakout.c`, `tron.c`, `survivor.c` — add `.controls`.
- `spiffs_image/index.html` — delete the 6 per-game surfaces; add one `data-mode="game"`
  surface, `renderControls()` + widget builders, supporting CSS; wire `case 'active'`.
- `TEST_PLAN.md` — add a §13 (R1) acceptance section.

---

## Task 1: Extend `proto_fmt_active` with a `controls` argument

**Files:**
- Modify: `main/proto.h:32`, `main/proto.c:58-62`
- Test: `test/host/test_proto.c:68-70,86-88`

- [ ] **Step 1: Update the failing test first**

In `test/host/test_proto.c`, replace the existing active block (around lines 68-70):

```c
    proto_fmt_active(b, sizeof b, "pong", 1, "[{\"w\":\"btn\"}]");
    CHECK(strcmp(b, "{\"v\":1,\"t\":\"active\",\"game\":\"pong\",\"players\":1,"
                    "\"controls\":[{\"w\":\"btn\"}]}") == 0, "active");
```

And the second call (around line 86-88):

```c
    proto_fmt_active(b, sizeof b, "tron", 2, "[{\"w\":\"pick\"}]");
    CHECK(strcmp(b, "{\"v\":1,\"t\":\"active\",\"game\":\"tron\",\"players\":2,"
                    "\"controls\":[{\"w\":\"pick\"}]}") == 0, "active tron");
```

- [ ] **Step 2: Run host tests to verify the build fails**

Run: `make -C test/host run`
Expected: **compile error** — `proto_fmt_active` called with 5 args but declared with 4
(`too many arguments to function 'proto_fmt_active'`).

- [ ] **Step 3: Update the declaration**

In `main/proto.h`, replace line 32:

```c
int proto_fmt_active(char *buf, size_t cap, const char *game_id, int players,
                     const char *controls);
```

- [ ] **Step 4: Update the implementation**

In `main/proto.c`, replace the `proto_fmt_active` function (lines 58-62):

```c
int proto_fmt_active(char *buf, size_t cap, const char *game_id, int players,
                     const char *controls)
{
    // `controls` is a trusted JSON-array literal owned by the game module; it is
    // embedded raw (not quoted) so the client can parse it as the widget stack.
    return snprintf(buf, cap,
                    "{\"v\":%d,\"t\":\"active\",\"game\":\"%s\",\"players\":%d,"
                    "\"controls\":%s}",
                    PROTO_SCHEMA_VERSION, game_id, players, controls);
}
```

- [ ] **Step 5: Run host tests to verify they pass**

Run: `make -C test/host run`
Expected: PASS — `active` and `active tron` checks green, all other suites still green.

- [ ] **Step 6: Checkpoint (user)**

Review the diff and commit when satisfied (agent does not commit):

```bash
git add main/proto.h main/proto.c test/host/test_proto.c
git commit -m "proto: carry a controls descriptor in the active message"
```

---

## Task 2: Add `controls` to the vtable and the broadcaster

**Files:**
- Modify: `main/game_module.h:29-42`, `main/network.h:25`, `main/network.c:194-199`,
  `main/engine.c:101,265`

- [ ] **Step 1: Add the vtable field**

In `main/game_module.h`, add the field after `bool scored;` (line 33), inside the
`game_module_t` struct:

```c
    bool        scored;        // true = has a numeric high score (false = win-based)
    const char *controls;      // JSON-array control descriptor for the phone (see proto)
```

- [ ] **Step 2: Update the broadcaster declaration**

In `main/network.h`, replace line 25:

```c
void net_broadcast_active(const char *game_id, int players, const char *controls);  // controller morph
```

- [ ] **Step 3: Update the broadcaster implementation**

In `main/network.c`, replace `net_broadcast_active` (lines 194-199). Note the larger buffer —
descriptor strings push the message to ~150 bytes:

```c
void net_broadcast_active(const char *game_id, int players, const char *controls)
{
    char buf[256];
    proto_fmt_active(buf, sizeof buf, game_id, players, controls);
    ws_broadcast(buf);
}
```

- [ ] **Step 4: Update the two engine call sites**

In `main/engine.c` line 101:

```c
    net_broadcast_active(g->id, g->min_players, g->controls);
```

In `main/engine.c` line 265:

```c
        net_broadcast_active(s_active->id, s_active->min_players, s_active->controls);  // re-morph
```

- [ ] **Step 5: Verify host tests still build/pass**

Run: `make -C test/host run`
Expected: PASS. (`game_module.h` is included by the host game stubs; the new field is optional
in initializers, so nothing breaks yet — games get their `.controls` in Task 3.)

- [ ] **Step 6: Checkpoint (user)**

```bash
git add main/game_module.h main/network.h main/network.c main/engine.c
git commit -m "engine: thread per-game controls descriptor into the active broadcast"
```

---

## Task 3: Author `controls` descriptors for the six existing games

Each game gains one `.controls` initializer line. These are the exact strings (C-escaped). Add
each line inside the game's `const game_module_t … = { … };` initializer, right after the
`.scored` line.

**Files:** `main/pong.c:235`, `main/snake.c:131`, `main/racer.c:141`, `main/breakout.c:169`,
`main/tron.c:120`, `main/survivor.c:328`

- [ ] **Step 1: Pong** — two action buttons (▲ UP / ▼ DOWN). In `main/pong.c`, after `.scored`:

```c
    .controls    = "[{\"w\":\"btn\",\"label\":\"&#9650; UP\",\"ev\":\"up\"},"
                   "{\"w\":\"btn\",\"label\":\"&#9660; DOWN\",\"ev\":\"down\"}]",
```

- [ ] **Step 2: Snake** — 4-way dpad. In `main/snake.c`, after `.scored`:

```c
    .controls    = "[{\"w\":\"dpad\",\"dirs\":[\"up\",\"left\",\"right\",\"down\"]}]",
```

- [ ] **Step 3: Racer** — tilt + L/R fallback. In `main/racer.c`, after `.scored`:

```c
    .controls    = "[{\"w\":\"tilt\"},{\"w\":\"dpad\",\"dirs\":[\"left\",\"right\"]}]",
```

- [ ] **Step 4: Breakout** — same as Racer. In `main/breakout.c`, after `.scored`:

```c
    .controls    = "[{\"w\":\"tilt\"},{\"w\":\"dpad\",\"dirs\":[\"left\",\"right\"]}]",
```

- [ ] **Step 5: Tron** — two turn buttons. In `main/tron.c`, after `.scored`:

```c
    .controls    = "[{\"w\":\"btn\",\"label\":\"&#8634; LEFT\",\"ev\":\"left\"},"
                   "{\"w\":\"btn\",\"label\":\"RIGHT &#8635;\",\"ev\":\"right\"}]",
```

- [ ] **Step 6: Survivor** — joystick + pick row. In `main/survivor.c`, after `.scored`:

```c
    .controls    = "[{\"w\":\"joystick\"},{\"w\":\"pick\"}]",
```

- [ ] **Step 7: Verify host tests still pass**

Run: `make -C test/host run`
Expected: PASS (these `.c` files compile in the host game suite; the new field initializes fine).

- [ ] **Step 8: Checkpoint (user)**

```bash
git add main/pong.c main/snake.c main/racer.c main/breakout.c main/tron.c main/survivor.c
git commit -m "games: declare phone control descriptors per game"
```

---

## Task 4: Generic frontend renderer (replaces the per-game surfaces)

This task is **not host-testable** — it is verified by the user's hardware playtest (Task 5).
Make the edits exactly as shown.

**Files:** `spiffs_image/index.html`

- [ ] **Step 1: Replace the six per-game surfaces with one generic surface**

In `spiffs_image/index.html`, delete the six blocks `data-mode="pong"`, `"snake"`, `"racer"`,
`"breakout"`, `"survivor"`, `"tron"` (lines 169-232) and replace all of them with this single
block:

```html
    <!-- GAME (controls built dynamically from the active message's `controls` descriptor) -->
    <div class="surface" data-mode="game">
      <div id="gamectl"></div>
      <button class="pill warn" onclick="back()">MENU</button>
    </div>
```

Leave the `menu`, `waiting`, `gameover`, and `info` surfaces untouched.

- [ ] **Step 2: Add the renderer + widget builders**

In the `<script>` block, immediately **after** the `renderMenu(...)` function (ends at line 382),
add:

```js
    // --- generic controller: build the active game's surface from its descriptor ---
    function renderControls(spec) {
      const host = document.getElementById('gamectl');
      host.innerHTML = '';
      let widgets = [];
      try { widgets = Array.isArray(spec) ? spec : JSON.parse(spec); } catch (e) { widgets = []; }
      widgets.forEach(w => {
        if (w.w === 'joystick')   host.appendChild(buildJoystick());
        else if (w.w === 'tilt')  host.appendChild(buildTilt());
        else if (w.w === 'dpad')  host.appendChild(buildDpad(w.dirs || []));
        else if (w.w === 'btn')   host.appendChild(buildButton(w.label || '?', w.ev || 'primary'));
        else if (w.w === 'pick')  host.appendChild(buildPick());
      });
    }

    function buildJoystick() {
      const pad = document.createElement('div'); pad.id = 'joypad';
      const base = document.createElement('div'); base.id = 'joybase';
      const thumb = document.createElement('div'); thumb.id = 'joythumb';
      base.appendChild(thumb);
      base.addEventListener('touchstart', joyStart, { passive: false });
      base.addEventListener('touchmove',  joyMove,  { passive: false });
      base.addEventListener('touchend',   joyEnd,   { passive: false });
      base.addEventListener('mousedown',  joyStart);
      base.addEventListener('mousemove',  joyMove);
      base.addEventListener('mouseup',    joyEnd);
      pad.appendChild(base);
      return pad;
    }

    function buildTilt() {
      const wrap = document.createElement('div'); wrap.className = 'tiltwrap';
      const bar = document.createElement('div'); bar.id = 'tiltbar';
      const dot = document.createElement('div'); dot.id = 'tiltdot';
      bar.appendChild(dot);
      const btn = document.createElement('button');
      btn.className = 'primary tilt-toggle';
      btn.textContent = tiltArmed ? 'TILT ON' : 'ENABLE TILT';
      btn.onclick = enableTilt;
      wrap.appendChild(bar); wrap.appendChild(btn);
      return wrap;
    }

    function buildDpad(dirs) {
      const glyph = { up: '&#9650;', down: '&#9660;', left: '&#9664;', right: '&#9654;' };
      const has = d => dirs.indexOf(d) >= 0;
      const pad = document.createElement('div'); pad.className = 'dpad';
      // 3x3 grid: up top-center, left/right middle, down bottom-center; empty cells stay blank
      [null, 'up', null, 'left', null, 'right', null, 'down', null].forEach(d => {
        const cell = document.createElement('div'); cell.className = 'dcell';
        if (d && has(d)) {
          const b = document.createElement('button'); b.className = 'accent';
          b.innerHTML = glyph[d]; b.onclick = () => press(d);
          cell.appendChild(b);
        }
        pad.appendChild(cell);
      });
      return pad;
    }

    function buildButton(label, ev) {
      const b = document.createElement('button'); b.className = 'accent gamebtn';
      b.innerHTML = label; b.onclick = () => press(ev);
      return b;
    }

    function buildPick() {
      const row = document.createElement('div'); row.className = 'pickrow';
      const mk = (cls, html, fn) => {
        const b = document.createElement('button'); b.className = cls; b.innerHTML = html;
        b.onclick = fn; return b;
      };
      row.appendChild(mk('accent',  '&#9664;', () => nav(-1)));
      row.appendChild(mk('primary', 'PICK',    () => select()));
      row.appendChild(mk('accent',  '&#9654;', () => nav(1)));
      return row;
    }
```

- [ ] **Step 3: Wire the `active` message to the renderer**

In `onMessage`, replace the `case 'active':` line (line 433):

```js
        case 'active':  renderControls(m.controls); setSurface('game'); break;
```

- [ ] **Step 4: Add supporting CSS**

In the `<style>` block, add these rules next to the existing joystick rules (after the
`.pickrow button { … }` rule). The `#joypad`, `#joybase`, `#joythumb`, `.pickrow`, `#tiltbar`,
`#tiltdot`, `.tilt-toggle` rules already exist and are reused as-is:

```css
    /* generic controller host + widgets */
    #gamectl { flex: 1; display: flex; flex-direction: column; gap: var(--gap); min-height: 0; }
    .tiltwrap { display: flex; flex-direction: column; gap: var(--gap); }
    .dpad { display: grid; grid-template-columns: repeat(3, 1fr); gap: var(--gap); flex: 0 0 auto; }
    .dpad .dcell { display: flex; }
    .dpad .dcell button { flex: 1; min-height: 56px; }
    .gamebtn { flex: 1; min-height: 56px; font-size: 22px; }
```

- [ ] **Step 5: Sanity-check the markup/JS coherence**

Run: `grep -n "data-mode=\"game\"\|renderControls\|buildJoystick\|buildDpad\|case 'active'" spiffs_image/index.html`
Expected: the `data-mode="game"` surface, all builder definitions, and the updated `case 'active'`
line are present; **no** remaining `data-mode="pong"|"snake"|"racer"|"breakout"|"survivor"|"tron"`
(verify: `grep -nE 'data-mode="(pong|snake|racer|breakout|survivor|tron)"' spiffs_image/index.html`
returns nothing).

- [ ] **Step 6: Checkpoint (user)**

```bash
git add spiffs_image/index.html
git commit -m "controller: render game controls generically from the descriptor"
```

---

## Task 5: Acceptance — host green + hardware playtest handoff

**Files:** `TEST_PLAN.md`

- [ ] **Step 1: Add a TEST_PLAN section**

Append to `TEST_PLAN.md`:

```markdown
## 13. Controller descriptors (R1)

| # | Step | Expect |
|---|------|--------|
| 13.1 | Launch each game and look at the phone | Controls render from the descriptor: Survivor = joystick + ◀PICK▶; Pong = ▲UP/▼DOWN; Snake = 4-way dpad; Racer/Breakout = tilt + ◀▶; Tron = LEFT/RIGHT turns |
| 13.2 | Play each game through its controls | Every input still works exactly as before the refactor (move/turn/fire/tilt/nav/select) |
| 13.3 | iOS: a tilt game (Racer) → ENABLE TILT | Permission prompt fires; tilt steers; the dpad L/R fallback still works |
| 13.4 | Switch game A → MENU → game B | The previous controls are cleared and B's controls render fresh (no leftover widgets) |
| 13.5 | Survivor level-up | ◀ PICK ▶ row drives the upgrade choice (nav/select) as before |
```

- [ ] **Step 2: Run the full host suite**

Run: `make -C test/host run`
Expected: PASS — all suites green (game logic, proto incl. the new `controls` field, gfx).

- [ ] **Step 3: Hand off to the user (agent stops here)**

Tell the user: this changes `index.html` (SPIFFS), so it needs a **USB** flash, not OTA:

```bash
source $IDF_PATH/export.sh
idf.py build flash monitor
```

Then verify TEST_PLAN §13 on the phone (every existing game controls correctly through the new
generic surface). Once confirmed, R1 is done and R2 (Descender) becomes pure firmware.

- [ ] **Step 4: Checkpoint (user)**

```bash
git add TEST_PLAN.md
git commit -m "test: R1 controller-descriptor acceptance matrix"
```

---

## Self-review notes

- **Spec coverage:** R1 of the spec (vtable `controls` field, `active` carries `controls`,
  closed widget vocabulary joystick/tilt/dpad/btn + **pick**, one generic renderer, existing
  games converted to prove it, USB-reflash caveat) — all covered by Tasks 1-5. The `pick` widget
  is an explicit, documented addition to the spec's vocabulary (Survivor's upgrade row needs
  `nav`/`select`, which `btn`→`input` can't express).
- **No new `input_kind_t`:** confirmed — every descriptor maps to an existing wire message
  (`move`/`tilt`/`input`/`nav`/`select`).
- **Type consistency:** `proto_fmt_active(buf, cap, game_id, players, controls)` is used
  identically in `proto.h`, `proto.c`, `test_proto.c`, and `net_broadcast_active`;
  `game_module_t.controls` is read in `engine.c` and set in all six game `.c` files.
- **Buffer:** `net_broadcast_active` buffer raised 64 → 256; the longest message (Tron/Pong with
  entity labels) is ~150 bytes.
- **Deferred to later phases:** Descender/puzzler/runner games (R2-R4), `rng.h`, and classic
  retirement (R5) get their own plans when reached.
```
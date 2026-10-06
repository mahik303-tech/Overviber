# Lua skins (LUA tab)

The Modern skin's **LUA** tab is a page that a Lua 5.4 script lays out and
draws. The script creates the page's controls, draws into canvases, reads and
writes the synth's parameters and follows the active palette. Everything else
in the editor stays as it is: the LUA tab is one more tab, not a replacement
for the C++ skin.

The tab has pages, chosen in its toolbar:

| Page | File | What it is |
|------|------|------------|
| OSC, FILTER / VCA, ENV, LFO / ARP, AFX, MOD MATRIX, SETTINGS | `osc.lua`, `filter.lua`, `env.lua`, `lfo.lua`, `afx.lua`, `matrix.lua`, `settings.lua` | the seven native tabs rebuilt in Lua, for comparison |
| SKIN.LUA | `skin.lua` | your own page (the built-in example until you write one) |

Each page runs its file from the script folder

| Platform | Folder |
|----------|--------|
| Windows  | `%USERPROFILE%\Documents\Overviber\LUA\` |
| macOS    | `~/Documents/Overviber/LUA/` |
| Linux    | `~/Documents/Overviber/LUA/` |

while that file exists, else its built-in version. **FOLDER** writes the
current page's built-in script there (if the file does not exist yet) and
opens the folder. Save the file and the page reloads within a second;
**RELOAD** runs it again at once. **LOG** shows the script's `log()` output and
its errors; it opens by itself when a script fails.

`lib.lua`, a helper library written in Lua, runs before every page. A
`lib.lua` in the script folder replaces the built-in one (changes reload the
page too). It defines the global table `lx`; see "The helper library" below.

The Lua pages set the same parameters as the native tabs, so both stay in
step. What the Lua API cannot reach stays native-only: wave editing and wave
files, the preset and kit browsers, split/layer routing, fonts, window scale
and the editor's own settings. Each page's header comment lists what it
mirrors, what it adds and what it leaves out.

## What a script can do

```lua
local w, h = ui.size()                       -- the page's size in pixels

ui.card{ title = "FILTER", badge = "LUA", x = 0, y = 0, w = 400, h = 300 }

local curve = ui.canvas{
  id = "curve", x = 10, y = 35, w = 380, h = 180,
  paint = function(g, w, h)
    g:fillAll("cardBg")
    g:setColour("visualizerCurve")
    g:beginPath()
    for i = 0, 100 do g:lineTo(i / 100 * w, h - params.get("cpCutoff") / 999 * h) end
    g:stroke(2)
  end,
  mouse = function(event, x, y) if event == "drag" then params.set("cpCutoff", x / 380 * 999) end end,
}

ui.knob{ param = "cpCutoff", mode = "cutoff", caption = "CUTOFF", x = 30, y = 230 }
params.onChange("cpCutoff", function(value) curve:repaint() end)
```

### `ui`

| Call | Meaning |
|------|---------|
| `ui.size()` | width and height of the page |
| `ui.canvas{ id, x, y, w, h, paint, mouse }` | a surface the script draws on |
| `ui.knob{ param, x, y, size, caption, mode, id }` | a knob bound to a continuous parameter; `mode` is `percent`, `bipolar`, `cutoff`, `time`, `semitones` or `raw` |
| `ui.button{ text, x, y, w, h, toggle, onClick, id }` | a button; `onClick(toggled)` |
| `ui.label{ text, x, y, w, h, size, bold, colour, align, id }` | a text label |
| `ui.card{ title, badge, x, y, w, h, id }` | a section card behind the other widgets |
| `ui.setFrameRate(hz)` | 0 (the default) means no animation; at most 60 |
| `ui.onFrame(function(dt, time) end)` | called while the frame rate is above 0 and the tab is open |

Every `ui.*` call returns a handle with `repaint()`, `setBounds(x, y, w, h)`,
`getBounds()`, `setVisible(v)`, `setText(t)`, `getText()`, `setToggled(v)`,
`isToggled()` and `getId()`.

`paint(g, w, h)` is called whenever the canvas needs redrawing,
`mouse(event, x, y, wheel)` with `event` one of `down`, `drag`, `up`, `double`,
`wheel`, `move`, `enter` or `exit`.

### `g` (inside `paint`)

`setColour(c [, alpha])`, `setOpacity(a)`, `fillAll([c])`, `fillRect`,
`drawRect`, `fillRoundedRect`, `drawRoundedRect`, `drawLine`, `fillEllipse`,
`drawEllipse`, `drawText(text, x, y, w, h [, align [, size [, bold]]])`,
`textWidth(text [, size [, bold]])`, `setGradient{ x1, y1, x2, y2, radial, stops }`,
`beginPath`, `moveTo`, `lineTo`, `quadTo`, `cubicTo`, `arc(cx, cy, r, from, to)`,
`closePath`, `stroke([thickness])`, `fill()`, `save`, `restore`, `translate`,
`rotate`, `scale`, `clip`.

Colours are `"#rgb"`, `"#rrggbb"`, `"#rrggbbaa"`, `"rgb(r, g, b)"`,
`"rgba(r, g, b, a)"` or the name of a theme role.

### `params`

| Call | Meaning |
|------|---------|
| `params.get(id)` | 0..999 for a continuous parameter, the step for a stepped one |
| `params.set(id, value)` | sets it, exactly as a knob of the skin does (the host sees the automation) |
| `params.getText(id)` | the host's own text, e.g. `"42 %"` |
| `params.display(id [, value])` | the text the native skin's knob shows, e.g. `"2.53 kHz"`, `"200 ms"`, `"+12.0 st"` |
| `params.getInfo(id)` | `{ id, name, kind, min, max, bipolar, choices }` |
| `params.list()` | every parameter ID |
| `params.onChange(id, function(value, id) end)` | returns a listener id |
| `params.removeListener(listenerId)` | |

The IDs are the plugin's host parameter IDs (`cpCutoff`, `cpAmpAtt`,
`spFilterModel`, `matrixSlot0_depth`, ...); `vst/Tests/fixtures/plugin-params/params.txt`
lists them all with their ranges.

### `theme`

`theme.get()` returns the palette as a table (`accent`, `windowBg`, `cardBg`,
`textBody`, `visualizerCurve`, ... and `name`), `theme.colour(role [, alpha])`
one colour, and `theme.onChange(fn)` calls `fn` when the user picks another
palette in SETTINGS.

`theme.list()` returns the preset palettes of SETTINGS, each a table of its
colours and `name`; `theme.select(i)` applies palette `i` as a click in
SETTINGS does (and saves the choice).

### `synth` (display data and the voice mixer)

| Call | Meaning |
|------|---------|
| `synth.voices()` | the six voices' levels, 0..1 |
| `synth.scope([n])` | the latest `n` (up to 2048) samples of the output, oldest first |
| `synth.arp()` | `{ valid, tick, step, gate, notes, pattern }`: the arpeggiator as it plays (`pattern` has 16 steps, -1 = none) |
| `synth.bpm()` | the tempo the arpeggiator follows |
| `synth.wave("A" or "B" [, points])` | the oscillator's wave, -1..1 |
| `synth.waveName("A" or "B")` | its file name |
| `synth.openWaveBrowser("A" or "B")` | the skin's wave browser |
| `synth.fader(voice [, dB])`, `synth.pan(voice [, -1..1])`, `synth.mute([on])` | the voice mixer of FILTER / VCA |
| `synth.envMs(pot [, slow])`, `synth.lfoHz(pot [, range])` | an envelope stage's time and an LFO's rate, as the engine computes them |

### `matrix` and `afx`

`matrix.get(slot)` returns `{ source, via, dest, depth, curve, enabled }`
(slots 1..8, depth -100..100), `matrix.set(slot, fields)` changes the given
fields like the MOD MATRIX tab does, and `matrix.sources()` /
`matrix.destinations()` name the ids (`[0]` is "None").

`afx.pad(i)` returns `{ name, customized }` (pads 1..16), `afx.padForNote(n)`
the pad a key plays, `afx.setKey(note, pad)` moves a key, and
`afx.map("octaves" | "chromatic" | "default" | "all", pad)` maps them all.
The selected pad and AFX mode are parameters (`spAFXSelectedSlot`,
`spEngineMode`).

`print(...)` and `log(...)` write to the tab's log.

## The helper library (`lx`)

`lib.lua` builds everything the pages draw out of the calls above:

| Part | Calls |
|------|-------|
| widgets drawn in Lua | `lx.dial`, `lx.choice`, `lx.toggle`, `lx.button`, `lx.hslider`, `lx.text`, `lx.section`, `lx.subhead` |
| animation | `lx.tween(obj, key, to, seconds, ease, onStep)`, `lx.loop(name, fn, hz)`, `lx.stop(name)`, `lx.ease.*` |
| drawing | `lx.panel`, `lx.grid`, `lx.glow`, `lx.area`, `lx.glyphs.*` |
| colour | `lx.col`, `lx.mix`, `lx.shade`, `lx.hsv` |
| values | `lx.watch`, `lx.snapshot`, `lx.apply`, `lx.morph`, `lx.noteName` |

`lib.lua` owns `ui.onFrame`: pages animate with `lx.tween` and `lx.loop`. The
frame rate is the highest a running loop asks for (60 while a tween runs) and
drops to 0 when nothing moves, so a still page costs no CPU.

## Animation and drawing quality

Lua pages draw with the same JUCE renderer as the native skin, so lines, text
and gradients are anti-aliased exactly alike and scale with the window. The
differences:

- Animation is Lua's strength here: values glide into place (`lx.tween`),
  curves morph into their new shape, highlights slide between choices,
  meters and scopes run from live data. Frames run only while something
  moves, at up to 60 per second.
- Every frame is a Lua call plus a software repaint of the canvases that
  changed. Large canvases redrawn 30 to 60 times a second cost more CPU than
  the native displays, which cache more; the pages therefore repaint only
  what moves and stop their loops when nothing does.
- There are no images, blur or real shadows; glow is drawn as layered
  strokes (`lx.glow`). Text is single-line.
- A callback is limited to 200 ms: a heavy computation (the FFT on FILTER /
  VCA takes well under a millisecond) is fine, an endless loop is stopped.

## Limits, and why they exist

A skin script runs inside the editor, so a bad script must not be able to take
the plugin or the DAW down with it. The engine therefore:

- opens only the `base`, `string`, `table`, `math`, `utf8` and `coroutine`
  libraries. There is no `io`, `os`, `package` or `debug`, and no `load`,
  `loadfile`, `dofile`, `require`, `collectgarbage` or `string.dump`: a script
  cannot read files, start programs, load binary chunks or load C libraries;
- loads scripts as text only;
- stops any call that runs too long (1 s while loading, 200 ms per callback)
  and says `time limit exceeded`. `pcall`, `xpcall` and `coroutine.resume`
  cannot swallow that error;
- caps the whole state at 16 MB;
- catches every error. A `paint` or callback that fails is switched off until
  the next reload (a failed canvas says `LUA ERROR (SEE LOG)`), the rest of the
  page keeps working, and the editor never sees an exception;
- runs only on the message thread, never on the audio thread, and never while
  the editor opens: the script starts the first time the tab is shown;
- hands out no `lua_State` to anything that outlives the tab. Reloading or
  closing the editor destroys the state together with every listener and the
  frame callback, so nothing can call into a dead script.

Without `ui.setFrameRate(hz)` there is no animation and the tab costs nothing
between parameter changes. With it, the page redraws up to 60 times a second
while the tab is open, which costs UI time like any other animation.

`vst/Tests/LuaTabScenarioTest.cpp` checks all of this, including endless loops,
runaway memory, a graphics object kept past its paint call and a script
reloaded while listeners are registered. It also loads every page in the
editor, clicks every button, sends every mouse event to every canvas and runs
frames, and expects no Lua error. `LuaTabScenarioTest --pages <dir> [<script dir>]`
renders each page to `<dir>/lua_<page>.png`, reading the pages from
`<script dir>` when given (no rebuild needed while editing them).

## Where the pieces are

| Path | Contents |
|------|----------|
| `vst/Source/lua/` | Lua 5.4.7 (MIT), without the file, OS, module and debug libraries |
| `vst/Source/ui/lua/LuaEngine.*` | the sandboxed state, the limits, the graphics object, colours |
| `vst/Source/ui/lua/LuaTab.*` | the tab: widgets, parameters, theme, reloading |
| `vst/Source/ui/lua/LuaDefaultSkin.lua` | the built-in example (SKIN.LUA) |
| `vst/Source/ui/lua/pages/` | `lib.lua` and the seven Lua pages |
| `vst/Tests/LuaTabScenarioTest.cpp` | the test |

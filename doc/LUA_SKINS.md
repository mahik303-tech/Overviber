# Lua skins (LUA tab)

The Modern skin's **LUA** tab is a page that a Lua 5.4 script lays out and
draws. The script creates the page's controls, draws into canvases, reads and
writes the synth's parameters and follows the active palette. Everything else
in the editor stays as it is: the LUA tab is one more tab, not a replacement
for the C++ skin.

The tab shows the built-in example until a script of your own exists.
**SCRIPT FOLDER** writes that example to

| Platform | File |
|----------|------|
| Windows  | `%USERPROFILE%\Documents\Overviber\LUA\skin.lua` |
| macOS    | `~/Documents/Overviber/LUA/skin.lua` |
| Linux    | `~/Documents/Overviber/LUA/skin.lua` |

and opens the folder. Save the file and the tab reloads it within a second;
**RELOAD** runs it again at once. **LOG** shows the script's `log()` output and
its errors; it opens by itself when a script fails.

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
`mouse(event, x, y, wheel)` with `event` one of `down`, `drag`, `up`, `double`
or `wheel`.

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
| `params.getText(id)` | the host's own text, e.g. `"2.53 kHz"` |
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

`print(...)` and `log(...)` write to the tab's log.

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
reloaded while listeners are registered.

## Where the pieces are

| Path | Contents |
|------|----------|
| `vst/Source/lua/` | Lua 5.4.7 (MIT), without the file, OS, module and debug libraries |
| `vst/Source/ui/lua/LuaEngine.*` | the sandboxed state, the limits, the graphics object, colours |
| `vst/Source/ui/lua/LuaTab.*` | the tab: widgets, parameters, theme, reloading |
| `vst/Source/ui/lua/LuaDefaultSkin.lua` | the built-in example |
| `vst/Tests/LuaTabScenarioTest.cpp` | the test |

-- lib.lua: the helper library of the Lua pages. It runs before every page
-- (Documents/Overviber/LUA/lib.lua replaces this built-in copy) and defines
-- one global table, `lx`:
--
--   animation   lx.tween, lx.loop, lx.stop, lx.ease: the frame rate is 0
--               (no CPU) unless something animates
--   colour      lx.col, lx.mix, lx.hsv, lx.shade
--   drawing     lx.panel, lx.grid, lx.glow, lx.area, lx.glyph
--   widgets     lx.section, lx.subhead, lx.dial, lx.choice, lx.toggle,
--               lx.button, lx.hslider, lx.text (all drawn by Lua on canvases)
--   values      lx.watch, lx.snapshot, lx.apply, lx.morph, lx.noteName
--
-- Pages use lx.loop instead of ui.onFrame (lib.lua owns the frame callback).

lx = {}
local floor, max, min, abs, pi = math.floor, math.max, math.min, math.abs, math.pi

-- =========================================================== math, easing
function lx.clamp(v, a, b) return v < a and a or (v > b and b or v) end
function lx.lerp(a, b, t) return a + (b - a) * t end
function lx.round(v) return floor(v + 0.5) end

lx.ease = {
  linear = function(t) return t end,
  outCubic = function(t) t = 1 - t; return 1 - t * t * t end,
  inOutCubic = function(t) return t < 0.5 and 4 * t * t * t or 1 - (-2 * t + 2) ^ 3 / 2 end,
  inOutSine = function(t) return -(math.cos(pi * t) - 1) / 2 end,
  outBack = function(t) local c = 1.70158; t = t - 1; return 1 + (c + 1) * t * t * t + c * t * t end,
  outElastic = function(t)
    if t <= 0 or t >= 1 then return t end
    return 2 ^ (-10 * t) * math.sin((t * 10 - 0.75) * (2 * pi / 3)) + 1
  end,
}

-- =========================================================== animation
-- Tweens and loops share the page's one frame callback. The frame rate is the
-- highest any running loop asks for (60 while a tween runs) and 0 when
-- nothing moves, so a still page costs nothing between parameter changes.
local tweens, loops = {}, {}
lx.time = 0

local function updateRate()
  local hz = next(tweens) and 60 or 0
  for _, l in pairs(loops) do hz = max(hz, l.hz) end
  if hz ~= lx.frameRate then
    lx.frameRate = hz
    ui.setFrameRate(hz)
  end
end
lx.frameRate = 0

-- lx.tween(obj, key, to, seconds [, ease [, onStep]]): animates obj[key].
-- A new tween on the same obj/key replaces the running one (from where it is).
function lx.tween(obj, key, to, seconds, ease, onStep)
  local id = tostring(obj) .. "." .. key
  if (seconds or 0) <= 0 then
    obj[key] = to
    tweens[id] = nil
    if onStep then onStep(to) end
    updateRate()
    return
  end
  tweens[id] = { obj = obj, key = key, from = obj[key] or to, to = to, t = 0, d = seconds,
                 ease = ease or lx.ease.outCubic, step = onStep }
  updateRate()
end

-- lx.loop(name, fn(dt, time) [, hz=60]): calls fn every frame until lx.stop(name)
-- or until fn returns false.
function lx.loop(name, fn, hz)
  loops[name] = { fn = fn, hz = hz or 60 }
  updateRate()
end
function lx.stop(name)
  loops[name] = nil
  updateRate()
end
function lx.running(name) return loops[name] ~= nil end

ui.onFrame(function(dt, time)
  lx.time = time
  dt = min(dt, 0.1)
  for id, tw in pairs(tweens) do
    tw.t = tw.t + dt
    local k = min(1, tw.t / tw.d)
    tw.obj[tw.key] = tw.from + (tw.to - tw.from) * tw.ease(k)
    if tw.step then tw.step(tw.obj[tw.key]) end
    if k >= 1 then tweens[id] = nil end
  end
  for name, l in pairs(loops) do
    if l.fn(dt, time) == false then loops[name] = nil end
  end
  updateRate()
end)

-- =========================================================== colours
local cache = {}
local function rgba(c)
  local hit = cache[c]
  if hit then return hit[1], hit[2], hit[3], hit[4] end
  local s = c
  if not s:find("^#") then s = theme.colour(c) end
  local r, g, b = tonumber(s:sub(2, 3), 16), tonumber(s:sub(4, 5), 16), tonumber(s:sub(6, 7), 16)
  local a = #s >= 9 and tonumber(s:sub(8, 9), 16) or 255
  cache[c] = { r, g, b, a }
  return r, g, b, a
end
lx.rgba = rgba
theme.onChange(function() cache = {} end)

local function hex(r, g, b, a)
  r, g, b, a = lx.clamp(floor(r + 0.5), 0, 255), lx.clamp(floor(g + 0.5), 0, 255), lx.clamp(floor(b + 0.5), 0, 255),
               lx.clamp(floor((a or 255) + 0.5), 0, 255)
  return string.format("#%02x%02x%02x%02x", r, g, b, a)
end

-- lx.col(role or colour [, alpha])
function lx.col(c, alpha)
  local r, g, b, a = rgba(c)
  return hex(r, g, b, a * (alpha or 1))
end
-- lx.mix(a, b, t [, alpha]): t = 0 is a, 1 is b
function lx.mix(a, b, t, alpha)
  local r1, g1, b1, a1 = rgba(a)
  local r2, g2, b2, a2 = rgba(b)
  return hex(r1 + (r2 - r1) * t, g1 + (g2 - g1) * t, b1 + (b2 - b1) * t, (a1 + (a2 - a1) * t) * (alpha or 1))
end
-- lx.shade(c, f [, alpha]): f < 0 darker, f > 0 lighter
function lx.shade(c, f, alpha)
  return f < 0 and lx.mix(c, "#000000", -f, alpha) or lx.mix(c, "#ffffff", f, alpha)
end
-- lx.hsv(h 0..1, s, v [, alpha])
function lx.hsv(h, s, v, alpha)
  h = (h % 1) * 6
  local i = floor(h)
  local f = h - i
  local p, q, t = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
  local r, g, b
  if i == 0 then r, g, b = v, t, p elseif i == 1 then r, g, b = q, v, p elseif i == 2 then r, g, b = p, v, t
  elseif i == 3 then r, g, b = p, q, v elseif i == 4 then r, g, b = t, p, v else r, g, b = v, p, q end
  return hex(r * 255, g * 255, b * 255, (alpha or 1) * 255)
end

-- =========================================================== drawing
-- The dark display background of the native skin's curves and scopes.
function lx.panel(g, w, h, radius)
  g:setGradient{ x1 = 0, y1 = 0, x2 = 0, y2 = h,
                 stops = { { 0, lx.shade("windowBg", -0.15) }, { 1, lx.shade("windowBg", -0.35) } } }
  g:fillRoundedRect(0, 0, w, h, radius or 3)
  g:setColour("visualizerGrid", 0.8)
  g:drawRoundedRect(0.5, 0.5, w - 1, h - 1, radius or 3, 1)
end

function lx.grid(g, x, y, w, h, cols, rows, alpha)
  g:setColour("visualizerGrid", alpha or 1)
  for i = 1, cols - 1 do local gx = floor(x + w * i / cols) + 0.5; g:drawLine(gx, y, gx, y + h, 1) end
  for i = 1, rows - 1 do local gy = floor(y + h * i / rows) + 0.5; g:drawLine(x, gy, x + w, gy, 1) end
end

-- Strokes the current path with a soft glow: wide, faint strokes under the line.
function lx.glow(g, colour, thickness, strength)
  strength = strength or 1
  for _, s in ipairs({ { 7, 0.06 }, { 4.5, 0.10 }, { 2.6, 0.18 } }) do
    g:setColour(colour, s[2] * strength)
    g:stroke(thickness * s[1])
  end
  g:setColour(colour)
  g:stroke(thickness)
end

-- lx.area(g, points, baseY, colour [, alpha]): a curve given as {x1, y1, x2, y2, ...},
-- filled down to baseY with a fading gradient, then stroked with a glow.
function lx.area(g, pts, baseY, colour, top, bottom, thickness)
  local n = #pts
  if n < 4 then return end
  g:beginPath()
  g:moveTo(pts[1], baseY)
  for i = 1, n, 2 do g:lineTo(pts[i], pts[i + 1]) end
  g:lineTo(pts[n - 1], baseY)
  g:closePath()
  local minY = baseY
  for i = 2, n, 2 do minY = min(minY, pts[i]) end
  if minY == baseY then minY = baseY - 1 end
  g:setGradient{ x1 = 0, y1 = minY, x2 = 0, y2 = baseY,
                 stops = { { 0, lx.col(colour, top or 0.35) }, { 1, lx.col(colour, bottom or 0.03) } } }
  g:fill()
  g:beginPath()
  g:moveTo(pts[1], pts[2])
  for i = 3, n, 2 do g:lineTo(pts[i], pts[i + 1]) end
  lx.glow(g, colour, thickness or 1.8)
end

-- Small line drawings for buttons; name -> function(g, x, y, w, h)
lx.glyphs = {}
local function wave(g, x, y, w, h, f)
  g:beginPath()
  for i = 0, 24 do
    local t = i / 24
    local v = f(t)
    if i == 0 then g:moveTo(x + t * w, y + h / 2 - v * h / 2) else g:lineTo(x + t * w, y + h / 2 - v * h / 2) end
  end
  g:stroke(1.5)
end
lx.glyphs.pulse = function(g, x, y, w, h) wave(g, x, y, w, h, function(t) return (t < 0.02 or (t > 0.5 and t < 0.98)) and -0.8 or 0.8 end) end
lx.glyphs.triangle = function(g, x, y, w, h) wave(g, x, y, w, h, function(t) return 0.8 * (1 - 4 * abs(((t + 0.25) % 1) - 0.5)) end) end
lx.glyphs.sine = function(g, x, y, w, h) wave(g, x, y, w, h, function(t) return 0.8 * math.sin(t * 2 * pi) end) end
lx.glyphs.saw = function(g, x, y, w, h) wave(g, x, y, w, h, function(t) return 0.8 * (2 * ((t * 2) % 1) - 1) end) end
lx.glyphs.revsaw = function(g, x, y, w, h) wave(g, x, y, w, h, function(t) return 0.8 * (1 - 2 * ((t * 2) % 1)) end) end
lx.glyphs.random = function(g, x, y, w, h)
  local steps = { 0.2, 0.7, -0.4, 0.5, -0.8, 0.1 }
  wave(g, x, y, w, h, function(t) return steps[min(6, floor(t * 6) + 1)] end)
end
lx.glyphs.noise = function(g, x, y, w, h)
  wave(g, x, y, w, h, function(t) return 0.8 * math.sin(t * 61) * math.cos(t * 23 + 1) end)
end
lx.glyphs.grit = function(g, x, y, w, h)
  wave(g, x, y, w, h, function(t) return 0.7 * math.sin(t * 2 * pi) + 0.15 * math.sin(t * 40) end)
end
lx.glyphs.pwm = function(g, x, y, w, h) wave(g, x, y, w, h, function(t) return (t > 0.25 and t < 0.6) and 0.8 or -0.8 end) end
lx.glyphs.fm = function(g, x, y, w, h) wave(g, x, y, w, h, function(t) return 0.8 * math.sin(t * 2 * pi * (1 + 3 * t)) end) end
lx.glyphs.morph = function(g, x, y, w, h)
  wave(g, x, y, w, h, function(t) return 0.8 * (t < 0.5 and math.sin(t * 2 * pi) or (2 * t - 1.5)) end)
end
lx.glyphs.fold = function(g, x, y, w, h)
  wave(g, x, y, w, h, function(t) local v = 1.8 * math.sin(t * 2 * pi); if v > 0.8 then v = 1.6 - v end; if v < -0.8 then v = -1.6 - v end; return v end)
end
lx.glyphs.crush = function(g, x, y, w, h)
  wave(g, x, y, w, h, function(t) return 0.8 * floor(math.sin(t * 2 * pi) * 2.5 + 0.5) / 2.5 end)
end
lx.glyphs.off = function(g, x, y, w, h) g:drawLine(x, y + h / 2, x + w, y + h / 2, 1.5) end
-- envelope curves: exp / lin, fast / slow
local function envGlyph(lin, slow)
  return function(g, x, y, w, h)
    local a = slow and 0.35 or 0.2
    g:beginPath()
    for i = 0, 20 do
      local t = i / 20
      local v
      if t < a then v = lin and t / a or 1 - (1 - t / a) ^ 3 else v = lin and 1 - (t - a) / (1 - a) or ((1 - (t - a) / (1 - a)) ^ 2.5) end
      if i == 0 then g:moveTo(x + t * w, y + h - v * h) else g:lineTo(x + t * w, y + h - v * h) end
    end
    g:stroke(1.4)
  end
end
lx.glyphs.expFast, lx.glyphs.expSlow = envGlyph(false, false), envGlyph(false, true)
lx.glyphs.linFast, lx.glyphs.linSlow = envGlyph(true, false), envGlyph(true, true)
-- filter slopes
local function slope(n)
  return function(g, x, y, w, h)
    g:beginPath()
    g:moveTo(x, y + h * 0.25)
    g:lineTo(x + w * 0.45, y + h * 0.25)
    g:quadTo(x + w * 0.62, y + h * 0.25, x + w * (0.55 + 0.45 * (4 - n) / 4 + 0.1), y + h * 0.95)
    g:stroke(1.5)
  end
end
lx.glyphs.lp24, lx.glyphs.lp18, lx.glyphs.lp12, lx.glyphs.lp6 = slope(4), slope(3), slope(2), slope(1)

-- =========================================================== layout widgets
-- lx.section{ x, y, w, h, title, badge }: a card; returns the inner area (x, y, w, h)
function lx.section(t)
  ui.card{ id = t.id, title = t.title, badge = t.badge, x = t.x, y = t.y, w = t.w, h = t.h }
  return t.x + 10, t.y + 32, t.w - 20, t.h - 40
end

-- lx.subhead{ x, y, w, text }: a small heading with a rule, as in the native cards
function lx.subhead(t)
  return ui.canvas{ id = t.id, x = t.x, y = t.y, w = t.w, h = t.h or 14, paint = function(g, w, h)
    g:setColour("textMuted")
    g:drawText(t.text, 0, 0, w, h, "left", 9, true)
    local tw = g:textWidth(t.text, 9, true) + 8
    if tw < w then
      g:setColour("cardBorder", 0.9)
      g:drawLine(tw, floor(h / 2) + 0.5, w, floor(h / 2) + 0.5, 1)
    end
  end }
end

-- lx.text{ x, y, w, h, text = string or function, size, bold, colour, align }: a label that
-- repaints with lx.watch / handle:repaint() when `text` is a function
function lx.text(t)
  return ui.canvas{ id = t.id, x = t.x, y = t.y, w = t.w, h = t.h or 16, paint = function(g, w, h)
    local s = type(t.text) == "function" and t.text() or t.text
    g:setColour(t.colour or "textBody")
    g:drawText(s or "", 0, 0, w, h, t.align or "left", t.size or 11, t.bold)
  end }
end

-- lx.watch(ids, handle or function): repaints the handle (or calls the function)
-- when any of the parameters changes; ids is a string or a list
function lx.watch(ids, target)
  if type(ids) == "string" then ids = { ids } end
  local fn = type(target) == "function" and target or function() target:repaint() end
  for _, id in ipairs(ids) do params.onChange(id, fn) end
end

-- ----------------------------------------------------------- dial
-- lx.dial{ param, x, y, size=44, w=80, caption, default, format = function(v) -> text,
--          colour = "accent" }
-- A knob drawn in Lua: gradient body, glowing value arc, animated when the
-- value changes from elsewhere (presets, automation). Drag up/down, wheel,
-- double click resets.
function lx.dial(t)
  local info = params.getInfo(t.param)
  local bipolar = info.bipolar
  local size = t.size or 44
  local w = t.w or max(size + 20, 76)
  local h = size + (t.caption and 32 or 18)
  local state = { shown = params.get(t.param), hover = 0, drag = false }
  local default = t.default or (bipolar and 500 or nil)
  local startY, startV
  local handle
  local A0, A1 = -2.36, 2.36

  local function text(v)
    if t.format then return t.format(v) end
    return params.display(t.param, v)
  end

  handle = ui.canvas{
    id = t.id or ("dial_" .. t.param), x = t.x, y = t.y, w = w, h = h,
    paint = function(g, cw, ch)
      local cx, cy, r = cw / 2, size / 2 + 1, size / 2 - 4
      local v = lx.clamp(state.shown / 999, 0, 1)
      local col = t.colour or "accent"
      -- track
      g:beginPath(); g:arc(cx, cy, r, A0, A1)
      g:setColour("knobTrack"); g:stroke(3.2)
      -- value arc with glow
      local from = bipolar and 0 or A0
      local to = A0 + (A1 - A0) * v
      if abs(to - from) > 0.01 then
        g:beginPath(); g:arc(cx, cy, r, math.min(from, to), math.max(from, to))
        lx.glow(g, col, 3.2, 0.6 + 0.6 * state.hover)
      end
      -- body
      local br = r - 5
      g:setGradient{ x1 = cx - br, y1 = cy - br, x2 = cx + br * 0.6, y2 = cy + br,
                     stops = { { 0, lx.shade("buttonBg", 0.18 + 0.1 * state.hover) }, { 1, lx.shade("windowBg", -0.25) } } }
      g:fillEllipse(cx - br, cy - br, br * 2, br * 2)
      g:setColour("buttonBorder", 0.9)
      g:drawEllipse(cx - br, cy - br, br * 2, br * 2, 1)
      -- needle
      local a = to
      local sx, sy = math.sin(a), -math.cos(a)
      g:setColour("knobNeedle")
      g:drawLine(cx + sx * br * 0.25, cy + sy * br * 0.25, cx + sx * (br - 2), cy + sy * (br - 2), 2)
      -- value and caption
      g:setColour(state.drag and col or "textBody")
      g:drawText(text(lx.round(state.shown)), 0, size + 1, cw, 14, "centre", 10.5, true)
      if t.caption then
        g:setColour("textMuted")
        g:drawText(t.caption, 0, size + 15, cw, 13, "centre", 9, true)
      end
    end,
    mouse = function(event, x, y, wheel)
      if event == "down" then
        state.drag, startY, startV = true, y, params.get(t.param)
        handle:repaint()
      elseif event == "drag" and state.drag then
        local v = lx.clamp(startV + (startY - y) * 999 / 160, 0, 999)
        state.shown = v
        params.set(t.param, v)
        handle:repaint()
      elseif event == "up" then
        state.drag = false
        -- the listener only sees values that differ at its next poll: settle on the real value
        lx.tween(state, "shown", params.get(t.param), 0.15, nil, function() handle:repaint() end)
      elseif event == "double" and default then
        params.set(t.param, default)
      elseif event == "wheel" then
        params.set(t.param, lx.clamp(params.get(t.param) + (wheel > 0 and 10 or -10), 0, 999))
      elseif event == "enter" then
        lx.tween(state, "hover", 1, 0.15, nil, function() handle:repaint() end)
      elseif event == "exit" then
        lx.tween(state, "hover", 0, 0.3, nil, function() handle:repaint() end)
      end
    end,
  }
  params.onChange(t.param, function(v)
    if state.drag then return end
    lx.tween(state, "shown", v, 0.28, lx.ease.outCubic, function() handle:repaint() end)
  end)
  if t.watch then lx.watch(t.watch, handle) end   -- e.g. a range switch that changes the text
  return handle
end

-- ----------------------------------------------------------- choice
-- lx.choice{ x, y, w, h, items = { "A", "B" }, cols, rowH, gap,
--            param = "spX" (value = values[i] or i - 1) | get = fn() -> i, set = fn(i),
--            glyphs = { name or fn, ... }, size = 10.5, onSelect = fn(i) }
-- A row/grid of buttons in one canvas; the highlight glides to the selection.
function lx.choice(t)
  local items = t.items
  if not items and t.param then items = params.getInfo(t.param).choices end
  local n = #items
  local cols = t.cols or n
  local rows = math.ceil(n / cols)
  local gap = t.gap or 4
  local rowH = t.rowH or 22
  local w = t.w
  local h = t.h or rows * rowH + (rows - 1) * gap
  local cellW = (w - (cols - 1) * gap) / cols
  local cellH = (h - (rows - 1) * gap) / rows
  local state = { hx = 0, hy = 0, hover = -1, flash = 0 }
  local handle, select

  local function selected()
    if t.get then return t.get() end
    local v = params.get(t.param)
    if t.values then
      for i, x in ipairs(t.values) do if x == v then return i end end
      return 0
    end
    return v + 1
  end
  local function cell(i)
    local c, r = (i - 1) % cols, floor((i - 1) / cols)
    return c * (cellW + gap), r * (cellH + gap)
  end
  local function hit(x, y)
    local c = floor(x / (cellW + gap))
    local r = floor(y / (cellH + gap))
    local i = r * cols + c + 1
    if c < 0 or c >= cols or r < 0 or i > n then return 0 end
    return i
  end
  do
    local s = selected()
    if s >= 1 then state.hx, state.hy = cell(s) end
  end

  handle = ui.canvas{
    id = t.id, x = t.x, y = t.y, w = w, h = h,
    paint = function(g, cw, ch)
      local sel = selected()
      for i = 1, n do
        local x, y = cell(i)
        local hov = i == state.hover
        g:setColour(hov and lx.shade("buttonBg", 0.08) or "buttonBg")
        g:fillRoundedRect(x, y, cellW, cellH, 2)
        g:setColour("buttonBorder", hov and 1 or 0.7)
        g:drawRoundedRect(x + 0.5, y + 0.5, cellW - 1, cellH - 1, 2, 1)
      end
      -- the gliding highlight
      if sel >= 1 then
        local x, y = state.hx, state.hy
        g:setGradient{ x1 = 0, y1 = y, x2 = 0, y2 = y + cellH,
                       stops = { { 0, lx.col("accent", 0.55 + 0.3 * state.flash) }, { 1, lx.col("accentDark", 0.85) } } }
        g:fillRoundedRect(x, y, cellW, cellH, 2)
        g:setColour("accent")
        g:drawRoundedRect(x + 0.5, y + 0.5, cellW - 1, cellH - 1, 2, 1.2)
      end
      for i = 1, n do
        local x, y = cell(i)
        local on = i == sel
        local colour = on and "textTitle" or "textBody"
        local label = items[i]
        local glyph = t.glyphs and t.glyphs[i]
        if type(glyph) == "string" then glyph = lx.glyphs[glyph] end
        if glyph then
          g:setColour(on and "textTitle" or "accent", on and 1 or 0.85)
          local gw = min(cellH * 1.3, cellW * 0.4)
          if label == nil or label == "" then
            glyph(g, x + (cellW - gw) / 2, y + cellH * 0.25, gw, cellH * 0.5)
          else
            local tw = g:textWidth(label, t.size or 10.5, true)
            local total = gw + 6 + tw
            local gx = x + max(4, (cellW - total) / 2)
            glyph(g, gx, y + cellH * 0.28, gw, cellH * 0.44)
            g:setColour(colour)
            g:drawText(label, gx + gw + 6, y, cellW - (gx - x) - gw - 6, cellH, "left", t.size or 10.5, true)
          end
        else
          g:setColour(colour)
          g:drawText(label, x + 2, y, cellW - 4, cellH, "centre", t.size or 10.5, true)
        end
      end
    end,
    mouse = function(event, x, y)
      if event == "down" then
        local i = hit(x, y)
        if i >= 1 then
          if t.set then t.set(i) elseif t.values then params.set(t.param, t.values[i]) else params.set(t.param, i - 1) end
          if t.onSelect then t.onSelect(i) end
          select()
        end
      elseif event == "move" or event == "enter" then
        local i = hit(x, y)
        if i ~= state.hover then state.hover = i; handle:repaint() end
      elseif event == "exit" then
        state.hover = -1
        handle:repaint()
      end
    end,
  }
  -- Moves the highlight to the current selection, animated.
  function select()
    local s = selected()
    if s < 1 then handle:repaint(); return end
    local x, y = cell(s)
    local repaint = function() handle:repaint() end
    lx.tween(state, "hx", x, 0.22, lx.ease.outCubic, repaint)
    lx.tween(state, "hy", y, 0.22, lx.ease.outCubic, repaint)
    state.flash = 1
    lx.tween(state, "flash", 0, 0.5, lx.ease.outCubic, repaint)
  end
  if t.param then params.onChange(t.param, select) end
  return handle, select   -- select(): call after a change the page made through get/set
end

-- ----------------------------------------------------------- toggle
-- lx.toggle{ x, y, w, h=18, text, param (0/1) | get = fn() -> bool, set = fn(bool) }
function lx.toggle(t)
  local function on()
    if t.get then return t.get() end
    return params.get(t.param) ~= 0
  end
  local state = { k = on() and 1 or 0 }
  local handle, sync
  handle = ui.canvas{
    id = t.id, x = t.x, y = t.y, w = t.w, h = t.h or 18,
    paint = function(g, w, h)
      local s = h - 6
      local y0 = (h - s) / 2
      g:setColour("buttonBg"); g:fillRoundedRect(2, y0, s, s, 2)
      g:setColour("buttonBorder"); g:drawRoundedRect(2.5, y0 + 0.5, s - 1, s - 1, 2, 1)
      if state.k > 0.01 then
        local i = (s - 4) * state.k
        g:setColour("accent", 0.25 * state.k)
        g:fillRoundedRect(2 + s / 2 - i / 2 - 3, y0 + s / 2 - i / 2 - 3, i + 6, i + 6, 3)
        g:setColour("accent")
        g:fillRoundedRect(2 + s / 2 - i / 2, y0 + s / 2 - i / 2, i, i, 1.5)
      end
      g:setColour(on() and "textTitle" or "textMuted")
      g:drawText(t.text or "", s + 9, 0, w - s - 9, h, "left", 9.5, true)
    end,
    mouse = function(event)
      if event == "down" then
        local v = not on()
        if t.set then t.set(v) else params.set(t.param, v and 1 or 0) end
        sync()
      end
    end,
  }
  function sync()
    lx.tween(state, "k", on() and 1 or 0, 0.18, lx.ease.outBack, function() handle:repaint() end)
  end
  if t.param then params.onChange(t.param, sync) end
  return handle, sync
end

-- ----------------------------------------------------------- button
-- lx.button{ x, y, w, h=22, text, onClick = fn(), glyph, active = fn() -> bool }
-- Clicks send a ripple across the button.
function lx.button(t)
  local state = { ripple = 1, rx = 0, ry = 0, hover = 0 }
  local handle
  handle = ui.canvas{
    id = t.id, x = t.x, y = t.y, w = t.w, h = t.h or 22,
    paint = function(g, w, h)
      local active = t.active and t.active()
      g:setColour(active and "accentDark" or lx.shade("buttonBg", 0.08 * state.hover))
      g:fillRoundedRect(0, 0, w, h, 2)
      if state.ripple < 1 then
        g:save()
        g:clip(0, 0, w, h)
        local r = state.ripple * w * 1.1
        g:setColour("accent", 0.45 * (1 - state.ripple))
        g:fillEllipse(state.rx - r, state.ry - r, r * 2, r * 2)
        g:restore()
      end
      g:setColour(active and "accent" or "buttonBorder", active and 1 or 0.7 + 0.3 * state.hover)
      g:drawRoundedRect(0.5, 0.5, w - 1, h - 1, 2, 1)
      g:setColour(active and "textTitle" or "textBody")
      g:drawText(t.text or "", 2, 0, w - 4, h, "centre", t.size or 10.5, true)
    end,
    mouse = function(event, x, y)
      if event == "down" then
        state.rx, state.ry, state.ripple = x, y, 0
        lx.tween(state, "ripple", 1, 0.45, lx.ease.outCubic, function() handle:repaint() end)
        if t.onClick then t.onClick() end
      elseif event == "enter" then
        lx.tween(state, "hover", 1, 0.12, nil, function() handle:repaint() end)
      elseif event == "exit" then
        lx.tween(state, "hover", 0, 0.25, nil, function() handle:repaint() end)
      end
    end,
  }
  return handle
end

-- ----------------------------------------------------------- horizontal slider
-- lx.hslider{ x, y, w, h=20, param, caption, format = fn(v) }
function lx.hslider(t)
  local state = { shown = params.get(t.param), drag = false }
  local handle
  handle = ui.canvas{
    id = t.id, x = t.x, y = t.y, w = t.w, h = t.h or 20,
    paint = function(g, w, h)
      g:setColour("buttonBg"); g:fillRoundedRect(0, 0, w, h, 2)
      local v = lx.clamp(state.shown / 999, 0, 1)
      g:setGradient{ x1 = 0, y1 = 0, x2 = w, y2 = 0, stops = { { 0, lx.col("accentDark", 0.6) }, { 1, lx.col("accent", 0.9) } } }
      g:fillRoundedRect(1, 1, (w - 2) * v, h - 2, 2)
      g:setColour("buttonBorder"); g:drawRoundedRect(0.5, 0.5, w - 1, h - 1, 2, 1)
      g:setColour("textTitle")
      local value = t.format and t.format(state.shown) or params.display(t.param, lx.round(state.shown))
      g:drawText((t.caption and (t.caption .. "  ") or "") .. value, 0, 0, w, h, "centre", 10.5, true)
    end,
    mouse = function(event, x)
      if event == "down" or event == "drag" then
        state.drag = true
        state.shown = lx.clamp(x / t.w, 0, 1) * 999
        params.set(t.param, state.shown)
        handle:repaint()
      elseif event == "up" then
        state.drag = false
      end
    end,
  }
  params.onChange(t.param, function(v)
    if not state.drag then lx.tween(state, "shown", v, 0.25, nil, function() handle:repaint() end) end
  end)
  return handle
end

-- =========================================================== values
-- lx.snapshot(ids) -> { id = value }, lx.apply(snapshot)
function lx.snapshot(ids)
  local s = {}
  for _, id in ipairs(ids) do s[id] = params.get(id) end
  return s
end
function lx.apply(s) for id, v in pairs(s) do params.set(id, v) end end
-- lx.morph(a, b, t): continuous parameters glide, stepped ones switch at the middle
function lx.morph(a, b, t)
  for id, va in pairs(a) do
    local vb = b[id]
    if vb ~= nil then
      if params.getInfo(id).kind == "continuous" then params.set(id, va + (vb - va) * t)
      else params.set(id, t < 0.5 and va or vb) end
    end
  end
end

local NOTES = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }
function lx.noteName(n) return NOTES[n % 12 + 1] .. (floor(n / 12) - 1) end
function lx.isBlack(n) local k = n % 12; return k == 1 or k == 3 or k == 6 or k == 8 or k == 10 end

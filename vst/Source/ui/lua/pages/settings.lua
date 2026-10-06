-- SETTINGS in Lua: the native tab's MIDI & MPE choices and palette presets,
-- plus what Lua adds:
--   * palette chips drawn in each palette's own colours (theme.list());
--     hovering one cross-fades a live miniature of the editor (cards, knob,
--     buttons, curve, meters) and the palette's role colours into it, a click
--     applies it (theme.select);
--   * LUA ENGINE: the runtime (_VERSION, pages, parameters, palettes, the
--     frame rate lx asks for right now) and, while MEASURE is on, a live
--     frame-time graph with fps, min / avg / max. Nothing animates otherwise,
--     so the page runs at 0 frames per second when idle.
-- Native only (no Lua API): split / layer routing, role colour editing
-- (HSB knobs, picker), user palettes and "Save Palette As", fonts, font size,
-- window size, startup default, classic skin, editor behaviour (filter
-- memory, 8-bit spectrum, waterfall), debug mode and copying the state.
-- Everything fits the 1068 x 544 page: no scrolling.

local W, H = ui.size()
local GAP = 6
local floor, max, min, abs = math.floor, math.max, math.min, math.abs

-- =========================================================== MIDI & MPE
local MIDI_H = 122
local mx, my, mw = lx.section{ id = "midiCard", title = "MIDI & MPE", badge = "PRESET", x = 0, y = 0, w = W, h = MIDI_H }
local HALF = (mw - 24) / 2
local RX = mx + HALF + 24
local function block(x, y, w, title, param, items)
  lx.subhead{ x = x, y = y, w = w, text = title }
  lx.choice{ id = "choice_" .. param, param = param, items = items, x = x, y = y + 16, w = w, h = 22, size = 10 }
end
block(mx, my, HALF, "MPE MODE", "spMPEMode", { "OFF (STANDARD MIDI)", "MPE 6 VOICES (CH 2-7)", "MPE FULL (CH 2-15)" })
block(mx, my + 44, HALF, "TIMBRE / SLIDE (CC 74) TO", "spTimbreTarget", { "OFF", "PITCH", "CUTOFF", "VOLUME", "WAVEMOD", "LFO 1", "LFO 2" })
block(RX, my, HALF, "PITCH BEND RANGE (MPE)", "spMPEPitchBendRange", { "2 ST", "12 ST", "24 ST", "48 ST", "96 ST" })
block(RX, my + 44, HALF, "RELEASE VELOCITY", "spReleaseVelocityAmt", { "OFF", "LOW", "MID", "HIGH" })

-- =========================================================== SKIN & PALETTE
local BOT_Y = MIDI_H + GAP
local BOT_H = H - BOT_Y
local SKIN_W = 660
local sx, sy, sw, sh = lx.section{ id = "themeCard", title = "SKIN & PALETTE", badge = "APPEARANCE", x = 0, y = BOT_Y, w = SKIN_W, h = BOT_H }

local PALETTES = theme.list()
local NP = #PALETTES
local function shortName(p) return string.upper((p.name:gsub("%s*%b()", ""))) end
local function currentIndex()
  local name = theme.get().name
  for i, p in ipairs(PALETTES) do if p.name == name then return i end end
  return 0
end

-- the preview's colours: a cross-fade from one role table to another
local ROLES = { "accent", "accentDark", "accentGlow", "windowBg", "cardBg", "cardHeader", "cardBorder", "knobTrack", "knobNeedle",
                "buttonBg", "buttonBorder", "textTitle", "textBody", "textMuted", "visualizerGrid", "visualizerCurve",
                "visualizerFill", "meterActive" }
local function snapshotTheme()
  local t, cur = {}, theme.get()
  for _, r in ipairs(ROLES) do t[r] = cur[r] end
  t.name = cur.name
  return t
end
local fade = { from = snapshotTheme(), to = snapshotTheme(), k = 1 }
local hoverChip = 0
local chips, preview, swatches

local function pc(role, alpha)
  local a, b = fade.from[role] or "#000000", fade.to[role] or "#000000"
  if fade.k >= 1 then return lx.col(b, alpha) end
  return lx.mix(a, b, fade.k, alpha)
end
local function fadeTo(target)
  if fade.to.name == target.name and fade.k >= 1 then return end
  local now = {}
  for _, r in ipairs(ROLES) do now[r] = pc(r) end
  fade.from, fade.to, fade.k = now, target, 0
  lx.tween(fade, "k", 1, 0.35, lx.ease.inOutSine, function() preview:repaint(); swatches:repaint() end)
end

lx.subhead{ x = sx, y = sy, w = 236, text = "PALETTE" }
lx.subhead{ x = sx + 248, y = sy, w = sw - 248, text = "PREVIEW  (HOVER A PALETTE)" }
local CHIP_W, CHIP_Y, AREA_H = 236, sy + 16, 240
local CHIP_H = min(25, (AREA_H - (NP - 1) * 3) / max(1, NP))
local CHIP_GAP = NP > 1 and min(8, (AREA_H - NP * CHIP_H) / (NP - 1)) or 0

chips = ui.canvas{
  id = "palettes", x = sx, y = CHIP_Y, w = CHIP_W, h = AREA_H,
  paint = function(g, w, h)
    local cur = currentIndex()
    for i, p in ipairs(PALETTES) do
      local y = (i - 1) * (CHIP_H + CHIP_GAP)
      local hov = i == hoverChip
      -- the chip in the palette's own colours
      g:setGradient{ x1 = 0, y1 = y, x2 = w, y2 = y,
                     stops = { { 0, lx.col(p.cardHeader) }, { 1, lx.col(p.windowBg) } } }
      g:fillRoundedRect(0, y, w, CHIP_H, 3)
      g:setColour(p.accent); g:fillRoundedRect(0, y, 4 + (hov and 3 or 0), CHIP_H, 2)
      -- swatches: chassis, panel, accent
      local s = CHIP_H - 10
      g:setColour(p.windowBg); g:fillRect(12, y + 5, s, s)
      g:setColour(p.cardBg); g:fillRect(12 + s, y + 5, s, s)
      g:setColour(p.accent); g:fillRect(12 + 2 * s, y + 5, s, s)
      g:setColour(p.cardBorder); g:drawRect(12, y + 5, s * 3, s, 1)
      g:setColour(i == cur and p.accent or p.textTitle)
      g:drawText(shortName(p), 20 + 3 * s, y, w - 44 - 3 * s, CHIP_H, "left", 10, true)
      g:setColour(i == cur and p.accent or p.cardBorder, (i == cur or hov) and 1 or 0.8)
      g:drawRoundedRect(0.5, y + 0.5, w - 1, CHIP_H - 1, 3, (i == cur or hov) and 1.5 or 1)
      if i == cur then
        g:setColour(p.accent)
        g:beginPath(); g:moveTo(w - 20, y + CHIP_H / 2); g:lineTo(w - 16, y + CHIP_H / 2 + 4); g:lineTo(w - 9, y + CHIP_H / 2 - 4)
        g:stroke(2)
      end
    end
  end,
  mouse = function(event, x, y)
    local i = floor(y / (CHIP_H + CHIP_GAP)) + 1
    if i < 1 or i > NP or y - (i - 1) * (CHIP_H + CHIP_GAP) > CHIP_H then i = 0 end
    if event == "move" or event == "enter" or event == "drag" then
      if i ~= hoverChip then
        hoverChip = i
        chips:repaint()
        if i > 0 then fadeTo(PALETTES[i]) else fadeTo(snapshotTheme()) end
      end
    elseif event == "exit" then
      hoverChip = 0
      chips:repaint()
      fadeTo(snapshotTheme())
    elseif event == "down" and i > 0 then
      theme.select(i)
    end
  end,
}

-- ---- the miniature editor, drawn in the faded palette
local function miniKnob(g, cx, cy, r, v)
  g:beginPath(); g:arc(cx, cy, r, -2.36, 2.36); g:setColour(pc("knobTrack")); g:stroke(2.5)
  g:beginPath(); g:arc(cx, cy, r, -2.36, -2.36 + 4.72 * v)
  g:setColour(pc("accent", 0.2)); g:stroke(6)
  g:setColour(pc("accent")); g:stroke(2.5)
  local br = r - 4
  g:setColour(pc("buttonBg")); g:fillEllipse(cx - br, cy - br, br * 2, br * 2)
  g:setColour(pc("buttonBorder")); g:drawEllipse(cx - br, cy - br, br * 2, br * 2, 1)
  local a = -2.36 + 4.72 * v
  g:setColour(pc("knobNeedle"))
  g:drawLine(cx + math.sin(a) * br * 0.3, cy - math.cos(a) * br * 0.3, cx + math.sin(a) * (br - 1), cy - math.cos(a) * (br - 1), 2)
end
local function miniCard(g, x, y, w, h, title)
  g:setColour(pc("cardBg")); g:fillRoundedRect(x, y, w, h, 3)
  g:setColour(pc("cardHeader")); g:fillRoundedRect(x, y, w, 16, 3); g:fillRect(x, y + 10, w, 6)
  g:setColour(pc("accent")); g:fillRect(x, y + 16, w, 1)
  g:setColour(pc("cardBorder")); g:drawRoundedRect(x + 0.5, y + 0.5, w - 1, h - 1, 3, 1)
  g:setColour(pc("textTitle")); g:drawText(title, x + 6, y, w - 12, 16, "left", 8.5, true)
end
local function miniButton(g, x, y, w, h, text, on)
  g:setColour(on and pc("accentDark") or pc("buttonBg")); g:fillRoundedRect(x, y, w, h, 2)
  g:setColour(on and pc("accent") or pc("buttonBorder")); g:drawRoundedRect(x + 0.5, y + 0.5, w - 1, h - 1, 2, 1)
  g:setColour(on and pc("textTitle") or pc("textBody")); g:drawText(text, x, y, w, h, "centre", 7.5, true)
end

local PV_X = sx + 248
preview = ui.canvas{
  id = "preview", x = PV_X, y = CHIP_Y, w = sw - 248, h = AREA_H,
  paint = function(g, w, h)
    g:setColour(pc("windowBg")); g:fillRoundedRect(0, 0, w, h, 4)
    -- tab bar
    local tabs = { "OSC", "FILTER", "ENV", "LFO", "AFX", "MATRIX" }
    local tw = (w - 16 - 5 * 4) / 6
    for i, t in ipairs(tabs) do miniButton(g, 8 + (i - 1) * (tw + 4), 6, tw, 14, t, i == 2) end
    -- left card: knobs and buttons
    local cy0, ch0 = 28, h - 36
    miniCard(g, 8, cy0, 132, ch0, "FILTER")
    miniKnob(g, 42, cy0 + 50, 18, 0.72)
    miniKnob(g, 106, cy0 + 50, 18, 0.35)
    g:setColour(pc("textBody")); g:drawText("2.4 kHz", 14, cy0 + 72, 56, 10, "centre", 7.5, true)
    g:drawText("35 %", 78, cy0 + 72, 56, 10, "centre", 7.5, true)
    g:setColour(pc("textMuted")); g:drawText("CUTOFF", 14, cy0 + 82, 56, 10, "centre", 7, true)
    g:drawText("RESONANCE", 78, cy0 + 82, 56, 10, "centre", 7, true)
    miniButton(g, 16, cy0 + 102, 54, 15, "LADDER", true)
    miniButton(g, 76, cy0 + 102, 56, 15, "SEM", false)
    miniButton(g, 16, cy0 + 121, 54, 15, "24 DB", false)
    miniButton(g, 76, cy0 + 121, 56, 15, "12 DB", false)
    g:setColour(pc("textMuted")); g:drawText("Body text in the muted role", 14, cy0 + ch0 - 20, 120, 12, "left", 7.5, false)
    -- right card: curve and meters
    local rx0, rw0 = 146, w - 154
    miniCard(g, rx0, cy0, rw0, ch0, "RESPONSE")
    local px, py, pw, ph = rx0 + 8, cy0 + 24, rw0 - 46, ch0 - 32
    g:setColour(pc("windowBg")); g:fillRoundedRect(px, py, pw, ph, 2)
    g:setColour(pc("visualizerGrid"))
    for k = 1, 4 do g:drawLine(px + pw * k / 5, py, px + pw * k / 5, py + ph, 1) end
    for k = 1, 2 do g:drawLine(px, py + ph * k / 3, px + pw, py + ph * k / 3, 1) end
    g:beginPath()
    for i = 0, 40 do
      local t = i / 40
      local v = 0.35 + 0.25 * math.exp(-((t - 0.62) * 9) ^ 2) - (t > 0.62 and (t - 0.62) * 1.2 or 0)
      local x, y = px + t * pw, py + ph * (1 - lx.clamp(v, 0.02, 0.98))
      if i == 0 then g:moveTo(x, y) else g:lineTo(x, y) end
    end
    g:setColour(pc("visualizerCurve", 0.25)); g:stroke(5)
    g:setColour(pc("visualizerCurve")); g:stroke(1.8)
    g:lineTo(px + pw, py + ph); g:lineTo(px, py + ph); g:closePath()
    g:setColour(pc("visualizerFill", 0.35)); g:fill()
    -- meters
    for k = 1, 3 do
      local mxk = px + pw + 6 + (k - 1) * 10
      local lv = ({ 0.75, 0.55, 0.9 })[k]
      g:setColour(pc("knobTrack")); g:fillRect(mxk, py, 6, ph)
      g:setColour(pc("meterActive")); g:fillRect(mxk, py + ph * (1 - lv), 6, ph * lv)
    end
    g:setColour(pc("accent"))
    g:drawText(hoverChip > 0 and shortName(PALETTES[hoverChip]) or (shortName(theme.get()) .. "  (CURRENT)"),
               rx0, cy0 + ch0 - 9, rw0 - 8, 9, "right", 7.5, true)
  end,
}

-- ---- the roles of the previewed palette
local SWATCH_ROLES = { { "accent", "ACCENT" }, { "windowBg", "CHASSIS" }, { "cardBg", "PANELS" }, { "cardHeader", "HEADER" },
                       { "cardBorder", "BORDERS" }, { "knobTrack", "DIALS" }, { "visualizerCurve", "CURVE" },
                       { "meterActive", "METER" }, { "textTitle", "TEXT" } }
local SW_Y = CHIP_Y + AREA_H + 8
lx.subhead{ x = sx, y = SW_Y, w = sw, text = "COLOUR OF EACH ROLE  (OF THE PREVIEWED PALETTE)" }
swatches = ui.canvas{
  id = "roles", x = sx, y = SW_Y + 16, w = sw, h = 40,
  paint = function(g, w, h)
    local n = #SWATCH_ROLES
    local cw = (w - (n - 1) * 4) / n
    for i, r in ipairs(SWATCH_ROLES) do
      local x = (i - 1) * (cw + 4)
      local c = pc(r[1])
      g:setColour(c); g:fillRoundedRect(x, 0, cw, 24, 2)
      g:setColour("cardBorder"); g:drawRoundedRect(x + 0.5, 0.5, cw - 1, 23, 2, 1)
      local cr, cg, cb = lx.rgba(c)
      local light = (cr * 0.3 + cg * 0.59 + cb * 0.11) > 140
      g:setColour(light and "#000000" or "#ffffff", 0.85)
      g:drawText(r[2], x, 0, cw, 24, "centre", 8.5, true)
      g:setColour("textMuted")
      g:drawText(string.upper(c:sub(1, 7)), x, 26, cw, 12, "centre", 8, false)
    end
  end,
}

local NOTE_Y = SW_Y + 62
lx.subhead{ x = sx, y = NOTE_Y, w = sw, text = "NATIVE ONLY" }
lx.text{ id = "native1", x = sx + 4, y = NOTE_Y + 15, w = sw - 4, h = 13, size = 9.5, colour = "textMuted",
         text = "Split / layer routing, role colour editing, user palettes, fonts, font size, window size, startup default," }
lx.text{ id = "native2", x = sx + 4, y = NOTE_Y + 28, w = sw - 4, h = 13, size = 9.5, colour = "textMuted",
         text = "classic skin, editor behaviour and debug options have no Lua API: use the native SETTINGS tab for them." }

-- =========================================================== LUA ENGINE
local ex, ey, ew, eh = lx.section{ id = "engineCard", title = "LUA ENGINE", badge = "RUNTIME",
                                   x = SKIN_W + GAP, y = BOT_Y, w = W - SKIN_W - GAP, h = BOT_H }
local PAGES = { "OSC", "FILTER / VCA", "ENV", "LFO / ARP", "AFX", "MOD MATRIX", "SETTINGS", "SKIN.LUA" }
local PARAM_COUNT = #params.list()
lx.subhead{ x = ex, y = ey, w = ew, text = "RUNTIME" }
local facts
facts = ui.canvas{
  id = "facts", x = ex, y = ey + 16, w = ew, h = 92,
  paint = function(g, w, h)
    local rows = {
      { "INTERPRETER", _VERSION .. " (sandboxed)" },
      { "PAGES", #PAGES .. "  (" .. table.concat(PAGES, ", ", 1, 4) .. " ...)" },
      { "PAGE AREA", W .. " x " .. H .. " px" },
      { "PARAMETERS", PARAM_COUNT .. " via params.*,  " .. matrix.count() .. " matrix slots" },
      { "PALETTES", NP .. " presets via theme.list()" },
      { "FRAME RATE", lx.frameRate == 0 and "0 Hz  (idle: nothing animates)" or (lx.frameRate .. " Hz requested by lx") },
    }
    for i, r in ipairs(rows) do
      local y = (i - 1) * 15
      g:setColour("textMuted"); g:drawText(r[1], 4, y, 92, 15, "left", 9, true)
      g:setColour(i == 6 and lx.frameRate > 0 and "accent" or "textBody")
      g:drawText(r[2], 100, y, w - 100, 15, "left", 10, i == 1)
    end
  end,
}

-- frame-time graph
local GY = ey + 116
lx.subhead{ x = ex, y = GY, w = ew - 120, text = "FRAME TIME" }
local HIST = 150
local hist, head, filled = {}, 0, 0
for i = 1, HIST do hist[i] = 0 end
local measuring = false
local graph
local GRAPH_H = ey + eh - (GY + 22) - 2

graph = ui.canvas{
  id = "frameGraph", x = ex, y = GY + 20, w = ew, h = GRAPH_H,
  paint = function(g, w, h)
    lx.panel(g, w, h)
    local top, ph = 26, h - 40
    local MAXMS = 40
    local function yOf(ms) return top + ph * (1 - lx.clamp(ms / MAXMS, 0, 1)) end
    -- reference lines: 60 fps, 30 fps
    for _, ref in ipairs({ { 16.67, "60 FPS" }, { 33.3, "30 FPS" } }) do
      local yy = floor(yOf(ref[1])) + 0.5
      g:setColour("visualizerGrid"); g:drawLine(30, yy, w - 6, yy, 1)
      g:setColour("textMuted"); g:drawText(ref[2], 0, yy - 6, 28, 12, "right", 7.5, true)
    end
    g:setColour("visualizerGrid"); g:drawLine(30, top + ph + 0.5, w - 6, top + ph + 0.5, 1)
    -- stats
    local sum, mn, mxv = 0, 1e9, 0
    for k = 1, filled do
      local v = hist[(head - k) % HIST + 1]
      sum = sum + v; mn = min(mn, v); mxv = max(mxv, v)
    end
    if filled > 1 then
      local pts = {}
      local pw = w - 36
      for k = filled, 1, -1 do
        local v = hist[(head - k) % HIST + 1]
        pts[#pts + 1] = 30 + pw * (1 - (k - 1) / (HIST - 1))
        pts[#pts + 1] = yOf(v)
      end
      lx.area(g, pts, top + ph, "visualizerCurve", 0.3, 0.02, 1.5)
      local avg = sum / filled
      g:setColour("accent")
      g:drawText(string.format("%.1f FPS", 1000 / max(0.1, avg)), 8, 4, 120, 18, "left", 14, true)
      g:setColour("textBody")
      g:drawText(string.format("MIN %.1f  AVG %.1f  MAX %.1f MS", mn, avg, mxv), 0, 6, w - 8, 14, "right", 9, true)
    else
      g:setColour("textMuted")
      g:drawText(measuring and "MEASURING ..." or "TURN ON MEASURE TO RECORD THE FRAME TIMES", 0, 6, w, 14, "centre", 9, true)
    end
    g:setColour("textMuted")
    g:drawText(measuring and "LAST " .. HIST .. " FRAMES AT 60 HZ" or "IDLE: 0 FRAMES PER SECOND", 0, h - 14, w - 8, 12, "right", 8, true)
  end,
}

local function setMeasure(on)
  measuring = on
  if on then
    head, filled = 0, 0
    lx.loop("measure", function(dt)
      head = head % HIST + 1
      hist[head] = dt * 1000
      filled = min(HIST, filled + 1)
      graph:repaint()
    end, 60)
  else
    lx.stop("measure")
  end
  graph:repaint()
  facts:repaint()
end
lx.toggle{ id = "measure", x = ex + ew - 110, y = GY - 2, w = 110, h = 18, text = "MEASURE",
           get = function() return measuring end, set = setMeasure }

-- =========================================================== theme changes
theme.onChange(function()
  if hoverChip == 0 then fadeTo(snapshotTheme()) end
  chips:repaint(); preview:repaint(); swatches:repaint(); facts:repaint(); graph:repaint()
end)


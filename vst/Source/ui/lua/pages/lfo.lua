-- LFO / ARP in Lua: every control of the native tab (shapes, speed, depth,
-- start delay, rate x1..x8, free run / key sync, the five destinations,
-- PITCH TO, the arpeggiator's mode, gate, swing, BPM, octaves, rate, latch
-- and host sync), plus what Lua adds:
--   * oscilloscopes computed here: the LFO shape is drawn over a window of
--     whole cycles, a beam sweeps it at the LFO's real rate (synth.lfoHz)
--     and leaves a fading phosphor trail; a new shape morphs into place;
--   * the scope is a control: drag up/down sets MOD DEPTH, the wheel sets
--     SPEED, the crosshair reads phase and value; RUN starts the beam;
--   * the arpeggiator matrix polls synth.arp() at 30 Hz only while the arp
--     is on; the step cursor glides, the cells show gate length and swing;
--     without held keys it previews the mode's pattern on a C major chord;
--   * the start delay knob moves to the LFO the mod wheel does not drive,
--     and the card badges follow the shape and arp mode.
-- Native-only: editing the arp step pattern (accent / tie / rest clicks).
-- Frame rate is 0 unless a scope runs, the arp is on or a tween plays.

local W, H = ui.size()
local GAP = 6
local COL_W = (W - 2 * GAP) // 3
local TOP_H = 304
local BOT_Y = TOP_H + GAP
local BOT_H = H - BOT_Y
local floor, max, min, abs, pi, sin = math.floor, math.max, math.min, math.abs, math.pi, math.sin

-- A badge drawn over a card header, so it can change with the page's state.
local function badge(id, cx, cy, cw, textFn)
  return ui.canvas{ id = id, x = cx + cw - 218, y = cy + 5, w = 210, h = 16, paint = function(g, w, h)
    local s = textFn()
    local tw = g:textWidth(s, 8.5, true) + 12
    local x = w - tw
    g:setColour("cardHeader"); g:fillRect(x, 0, tw, h)
    g:setColour("accentDark"); g:drawRect(x, 0, tw, h, 1)
    g:setColour("accent"); g:drawText(s, x, 0, tw, h, "centre", 8.5, true)
  end }
end

-- =========================================================== LFO shapes
local SHAPE_GLYPHS = { "pulse", "triangle", "random", "sine", "noise", "saw", "revsaw" }
local SHAPE_NAMES = {}
for i, s in ipairs(params.getInfo("spLFOShape").choices) do SHAPE_NAMES[i] = s:upper() end

local function hash(i)
  local x = sin(i * 12.9898 + 4.1414) * 43758.5453
  return (x - floor(x)) * 2 - 1
end

-- value -1..1 of shape s (0-based) at time tc in cycles
local function shapeValue(s, tc)
  local cyc = floor(tc)
  local t = tc - cyc
  if s == 0 then return t < 0.5 and 1 or -1
  elseif s == 1 then
    if t < 0.25 then return t * 4 elseif t < 0.75 then return 1 - (t - 0.25) * 4 else return -1 + (t - 0.75) * 4 end
  elseif s == 2 then return hash(cyc + 1) * 0.9
  elseif s == 3 then return sin(t * 2 * pi)
  elseif s == 4 then
    local k = tc * 16
    local i = floor(k)
    local a, b = hash(i + 101), hash(i + 102)
    return (a + (b - a) * (k - i)) * 0.9
  elseif s == 5 then return 2 * t - 1
  else return 1 - 2 * t end
end

-- =========================================================== LFO cards
local LFOS = {
  { n = 1, shape = "spLFOShape", speed = "cpLFOFreq", amt = "cpLFOAmt", range = "spLFOSpeed", trig = "spLFOTrig",
    targets = "spLFOTargets", badge = "MAIN",
    depths = { "cpLFOPitchAmt", "cpLFOWModAmt", "cpLFOFilAmt", "cpLFOResAmt", "cpLFOAmpAmt" } },
  { n = 2, shape = "spLFO2Shape", speed = "cpLFO2Freq", amt = "cpLFO2Amt", range = "spLFO2Speed", trig = "spLFO2Trig",
    targets = "spLFO2Targets", badge = "AUX / VIB",
    depths = { "cpLFO2PitchAmt", "cpLFO2WModAmt", "cpLFO2FilAmt", "cpLFO2ResAmt", "cpLFO2AmpAmt" } },
}
local DEPTH_CAPTIONS = { "PITCH", "WAVEMOD", "CUTOFF", "RESO", "VOLUME" }

-- As in the firmware the start delay acts on the LFO the mod wheel does not
-- control (mod wheel on LFO 1 depth -> delay on LFO 2).
local function delayLfo() return params.get("spModwheelTarget") == 0 and 2 or 1 end

local function lfoHz(d) return synth.lfoHz(params.get(d.speed), params.get(d.range)) end

local function buildLfo(d, x)
  local fx, fy, fw = lx.section{ id = "lfo" .. d.n .. "Card", title = "LFO " .. d.n, badge = d.badge, x = x, y = 0, w = COL_W, h = TOP_H }
  local blank = {}
  for i = 1, 7 do blank[i] = "" end
  lx.choice{ id = "shape" .. d.n, param = d.shape, items = blank, glyphs = SHAPE_GLYPHS, x = fx, y = fy, w = fw, h = 26 }

  lx.subhead{ x = fx, y = fy + 32, w = fw, text = "SPEED & AMOUNT" }
  d.speedDial = lx.dial{ param = d.speed, caption = "SPEED / RATE", x = fx, y = fy + 46, size = 38, w = 96, watch = d.range, default = 250 }
  d.amtDial = lx.dial{ param = d.amt, caption = "MOD DEPTH", x = fx + 110, y = fy + 46, size = 38, w = 96, default = 0 }
  d.delayDial = lx.dial{ id = "delay" .. d.n, param = "cpModDelay", caption = "START DELAY", x = fx + 220, y = fy + 46, size = 38, w = 96, default = 0 }
  d.layoutTop = function()
    local hasDelay = delayLfo() == d.n
    local slots = hasDelay and 3 or 2
    local sw = fw / slots
    d.speedDial:setBounds(floor(fx + (sw - 96) / 2), fy + 46, 96, 70)
    d.amtDial:setBounds(floor(fx + sw + (sw - 96) / 2), fy + 46, 96, 70)
    d.delayDial:setBounds(floor(fx + 2 * sw + (sw - 96) / 2), fy + 46, 96, 70)
    d.delayDial:setVisible(hasDelay)
  end
  d.layoutTop()

  lx.choice{ id = "range" .. d.n, param = d.range, items = { "x1", "x2", "x4", "x8" }, x = fx, y = fy + 120, w = 156, h = 22, size = 10 }
  lx.choice{ id = "trig" .. d.n, param = d.trig, items = { "FREE RUN", "KEY SYNC" }, x = fx + 166, y = fy + 120, w = fw - 166, h = 22, size = 10 }

  lx.subhead{ x = fx, y = fy + 148, w = fw, text = "DESTINATIONS" }
  local dw = fw / 5
  for i, p in ipairs(d.depths) do
    lx.dial{ param = p, caption = DEPTH_CAPTIONS[i], x = floor(fx + (i - 1) * dw + (dw - 66) / 2), y = fy + 162, size = 34, w = 66, default = 0 }
  end
  lx.text{ x = fx, y = fy + 234, w = 60, h = 22, text = "PITCH TO", size = 9, bold = true, colour = "textMuted" }
  lx.choice{ id = "targets" .. d.n, param = d.targets, items = { "NONE", "OSC A", "OSC B", "A + B" }, x = fx + 64, y = fy + 234, w = fw - 64, h = 22, size = 10 }
end

-- =========================================================== oscilloscopes
local function buildScope(d, x)
  local sx, sy, sw, sh = lx.section{ id = "scope" .. d.n .. "Card", title = "LFO " .. d.n .. " OSCILLOSCOPE", x = x, y = BOT_Y, w = COL_W, h = BOT_H }
  badge("scopeBadge" .. d.n, x, BOT_Y, COL_W, function() return SHAPE_NAMES[params.get(d.shape) + 1] or "" end)
  local st = { from = params.get(d.shape), to = params.get(d.shape), k = 1, phase = 0, hover = -1, hoverA = 0, run = false }
  local scopeH = sh - 26
  local scope, runSync

  local function windowCycles(hz)
    local c = 1
    while c < hz * 0.3 and c < 64 do c = c * 2 end
    return c
  end
  local function value(tc)
    local a = shapeValue(st.from, tc)
    if st.k >= 1 then return shapeValue(st.to, tc) end
    return a + (shapeValue(st.to, tc) - a) * st.k
  end

  scope = ui.canvas{
    id = "scope" .. d.n, x = sx, y = sy, w = sw, h = scopeH,
    paint = function(g, w, h)
      lx.panel(g, w, h)
      local px, pw = 8, w - 34
      local mid = floor(h / 2) + 0.5
      local amp = (h - 34) / 2
      lx.grid(g, px, 4, pw, h - 8, 8, 4, 0.55)
      g:setColour("visualizerGrid"); g:drawLine(px, mid, px + pw, mid, 1)
      local hz = lfoHz(d)
      local cycles = windowCycles(hz)
      local n = min(256, cycles * 40)
      local pts = {}
      for i = 0, n do
        pts[#pts + 1] = px + i / n * pw
        pts[#pts + 1] = mid - value(i / n * cycles) * amp
      end
      local bx = (st.phase % cycles) / cycles
      if st.run then
        -- dim reference, then the phosphor trail behind the beam
        g:beginPath()
        g:moveTo(pts[1], mid)
        for i = 1, #pts, 2 do g:lineTo(pts[i], pts[i + 1]) end
        g:lineTo(pts[#pts - 1], mid)
        g:closePath()
        g:setColour("visualizerCurve", 0.07); g:fill()
        g:beginPath()
        g:moveTo(pts[1], pts[2])
        for i = 3, #pts, 2 do g:lineTo(pts[i], pts[i + 1]) end
        g:setColour("visualizerCurve", 0.22); g:stroke(1.2)
        local SEG, LEN = 12, 0.5
        for j = SEG - 1, 0, -1 do
          local b = bx - j * LEN / SEG
          local a = b - LEN / SEG
          local alpha = (1 - j / SEG) ^ 1.6
          local function seg(p0, p1)
            local i0, i1 = max(0, floor(p0 * n)), min(n, math.ceil(p1 * n))
            if i1 <= i0 then return end
            g:beginPath()
            g:moveTo(px + p0 * pw, mid - value(p0 * cycles) * amp)
            for i = i0 + 1, i1 - 1 do g:lineTo(pts[2 * i + 1], pts[2 * i + 2]) end
            g:lineTo(px + p1 * pw, mid - value(p1 * cycles) * amp)
            if j == 0 then lx.glow(g, "visualizerCurve", 2, 0.9)
            else g:setColour("visualizerCurve", alpha); g:stroke(2) end
          end
          if a < 0 and b <= 0 then seg(a + 1, b + 1)
          elseif a < 0 then seg(a + 1, 1); seg(0, b)
          else seg(a, b) end
        end
      else
        lx.area(g, pts, mid, "visualizerCurve", 0.3, 0.03, 1.8)
      end
      -- beam
      local v = value(bx * cycles)
      local by = mid - v * amp
      local bxp = px + bx * pw
      g:setColour("accent", 0.25); g:fillEllipse(bxp - 8, by - 8, 16, 16)
      g:setColour("textTitle"); g:fillEllipse(bxp - 3, by - 3, 6, 6)
      -- output meter: value x depth
      local depth = params.get(d.amt) / 999
      local mx = w - 18
      g:setColour("knobTrack"); g:fillRoundedRect(mx, 6, 8, h - 12, 2)
      local my = mid - v * depth * amp
      g:setColour("accent", 0.85); g:fillRect(mx + 1, min(my, mid), 6, abs(my - mid))
      g:setColour("textMuted"); g:drawLine(mx - 3, mid, mx + 11, mid, 1)
      -- readouts
      g:setColour("accent")
      local period = hz > 0 and 1000 / hz or 0
      g:drawText(string.format("%s  |  %s", params.display(d.speed), period >= 1000 and string.format("%.2f s", period / 1000)
                 or string.format("%d ms", lx.round(period))), px + 4, 4, pw - 8, 14, "right", 9.5, true)
      g:setColour("textMuted")
      g:drawText(string.format("WINDOW %d CYCLE%s  |  DEPTH %s  |  %s", cycles, cycles > 1 and "S" or "",
                 params.display(d.amt), params.get(d.trig) == 1 and "KEY SYNC" or "FREE RUN"),
                 px + 4, h - 16, pw - 8, 14, "left", 8.5, true)
      -- crosshair
      if st.hoverA > 0.01 and st.hover >= px and st.hover <= px + pw then
        local p = (st.hover - px) / pw
        local hv = value(p * cycles)
        local hy = mid - hv * amp
        g:setColour("textBody", 0.45 * st.hoverA); g:drawLine(st.hover, 4, st.hover, h - 4, 1)
        g:setColour("textTitle", st.hoverA); g:fillEllipse(st.hover - 3, hy - 3, 6, 6)
        local label = string.format("PHASE %d DEG  %+.2f", floor((p * cycles % 1) * 360), hv)
        local lx0 = st.hover + 8 > w - 130 and st.hover - 126 or st.hover + 8
        g:setColour("windowBg", 0.85 * st.hoverA); g:fillRoundedRect(lx0, 20, 118, 16, 2)
        g:setColour("textTitle", st.hoverA); g:drawText(label, lx0 + 4, 20, 114, 16, "left", 9, true)
      end
    end,
    mouse = function(event, mx, my, wheel)
      if event == "down" then
        st.dragY, st.dragV = my, params.get(d.amt)
      elseif event == "drag" and st.dragY then
        params.set(d.amt, lx.clamp(st.dragV + (st.dragY - my) * 999 / 120, 0, 999))
      elseif event == "up" then
        st.dragY = nil
      elseif event == "wheel" then
        params.set(d.speed, lx.clamp(params.get(d.speed) + (wheel > 0 and 10 or -10), 0, 999))
      end
      if event == "move" or event == "drag" or event == "enter" then
        st.hover = mx
        if st.hoverA < 1 then lx.tween(st, "hoverA", 1, 0.15, nil, function() scope:repaint() end) end
        scope:repaint()
      elseif event == "exit" then
        lx.tween(st, "hoverA", 0, 0.3, nil, function() scope:repaint() end)
      end
    end,
  }

  local function setRun(on)
    st.run = on
    if on then
      lx.loop("scope" .. d.n, function(dt)
        st.phase = st.phase + lfoHz(d) * dt
        if st.phase > 4096 then st.phase = st.phase - 4096 end
        scope:repaint()
      end, 30)
    else
      lx.stop("scope" .. d.n)
    end
    scope:repaint()
  end
  local _
  _, runSync = lx.toggle{ id = "run" .. d.n, x = sx, y = sy + scopeH + 6, w = 70, h = 18, text = "RUN",
                          get = function() return st.run end, set = setRun }
  lx.text{ x = sx + 76, y = sy + scopeH + 6, w = sw - 76, h = 18, size = 8.5, colour = "textMuted", align = "right",
           text = "DRAG: DEPTH  |  WHEEL: SPEED" }

  -- the shape morphs into the new one
  params.onChange(d.shape, function(v)
    if v == st.to then return end
    st.from, st.to, st.k = st.to, v, 0
    lx.tween(st, "k", 1, 0.4, lx.ease.inOutCubic, function() scope:repaint() end)
  end)
  lx.watch({ d.speed, d.range, d.amt, d.trig, d.shape }, scope)
end

for i, d in ipairs(LFOS) do
  local x = (i - 1) * (COL_W + GAP)
  buildLfo(d, x)
  buildScope(d, x)
end
lx.watch("spModwheelTarget", function() for _, d in ipairs(LFOS) do d.layoutTop() end end)

-- =========================================================== arpeggiator
local AX = 2 * (COL_W + GAP)
local AW = W - AX
local ax, ay, aw = lx.section{ id = "arpCard", title = "ARPEGGIATOR", badge = "PLAYBACK", x = AX, y = 0, w = AW, h = TOP_H }
local MODE_ITEMS = { "OFF", "UP", "DOWN", "UP/DOWN", "RANDOM", "PLAYED", "CHORD", "CONVERGE", "DEGREE", "STRUM" }
lx.choice{ id = "arpMode", param = "spArpMode", items = MODE_ITEMS, cols = 5, rowH = 22, gap = 4, x = ax, y = ay, w = aw, h = 48, size = 9.5 }
lx.subhead{ x = ax, y = ay + 54, w = aw, text = "GATE, GROOVE & TEMPO" }
local kw = aw / 3
lx.dial{ param = "cpArpGate", caption = "GATE LEN", x = floor(ax + (kw - 96) / 2), y = ay + 68, size = 38, w = 96, default = 833 }
lx.dial{ param = "cpArpSwing", caption = "SWING", x = floor(ax + kw + (kw - 96) / 2), y = ay + 68, size = 38, w = 96, default = 500 }
lx.dial{ param = "cpArpBpm", caption = "FREE BPM", x = floor(ax + 2 * kw + (kw - 96) / 2), y = ay + 68, size = 38, w = 96, default = 357 }
lx.subhead{ x = ax, y = ay + 142, w = aw, text = "OCTAVES & RATE" }
lx.choice{ id = "arpOct", param = "spArpOctaves", items = { "1 OCTAVE", "2 OCTAVES", "3 OCTAVES", "4 OCTAVES" }, x = ax, y = ay + 156, w = aw, h = 22, size = 9.5 }
local RATES = { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" }
lx.choice{ id = "arpRate", param = "spArpRate", items = RATES, x = ax, y = ay + 182, w = aw, h = 22, size = 9.5 }
lx.subhead{ x = ax, y = ay + 210, w = aw, text = "LATCH & TEMPO SYNC" }
lx.toggle{ id = "arpHold", param = "spArpHold", text = "LATCH / HOLD", x = ax + 2, y = ay + 226, w = aw / 2 - 4, h = 20 }
lx.toggle{ id = "arpSync", param = "spArpSync", text = "HOST SYNC (DAW)", x = ax + aw / 2, y = ay + 226, w = aw / 2, h = 20 }

-- ---- matrix
local mx, my, mw, mh = lx.section{ id = "arpMatrixCard", title = "ARPEGGIATOR MATRIX", x = AX, y = BOT_Y, w = AW, h = BOT_H }
local STEPS_PER_BEAT = { 1, 2, 3, 4, 6, 8 }
local function bpm()
  if params.get("spArpSync") ~= 0 then return synth.bpm() end
  return 20 + params.get("cpArpBpm") / 999 * 280
end
local function stepMs() return 60000 / max(1, bpm()) / STEPS_PER_BEAT[params.get("spArpRate") + 1] end
local function modeName() return MODE_ITEMS[params.get("spArpMode") + 1] or "OFF" end
badge("arpBadge", AX, BOT_Y, AW, function()
  local m = params.get("spArpMode")
  if m == 0 then return "OFF" end
  return string.format("%s (%d OCT)", modeName(), params.get("spArpOctaves") + 1)
end)

-- The mode's pattern on a held C major chord (C4 E4 G4): steps -> list of notes.
local function previewPattern()
  local mode, oct = params.get("spArpMode"), params.get("spArpOctaves") + 1
  local base = { 60, 64, 67 }
  local list = {}
  for o = 0, oct - 1 do for _, n in ipairs(base) do list[#list + 1] = n + 12 * o end end
  local seq = {}
  if mode == 2 then for i = #list, 1, -1 do seq[#seq + 1] = list[i] end
  elseif mode == 3 then
    for i = 1, #list do seq[#seq + 1] = list[i] end
    for i = #list - 1, 2, -1 do seq[#seq + 1] = list[i] end
  elseif mode == 4 then
    for s = 1, 16 do seq[s] = list[floor((hash(s * 7) + 1) / 2 * #list) % #list + 1] end
  elseif mode == 7 then
    local lo, hi = 1, #list
    while lo <= hi do
      seq[#seq + 1] = list[lo]
      if hi ~= lo then seq[#seq + 1] = list[hi] end
      lo, hi = lo + 1, hi - 1
    end
  elseif mode == 6 or mode == 8 or mode == 9 then
    local out = {}
    for s = 1, 16 do
      local o = (s - 1) % oct
      out[s] = { 60 + 12 * o, 64 + 12 * o, 67 + 12 * o }
    end
    return out
  else
    seq = list
  end
  local out = {}
  for s = 1, 16 do out[s] = { seq[(s - 1) % #seq + 1] } end
  return out
end

local arpState = { step = -1, hx = 0, live = false, pclock = 0, hover = -1, pattern = nil, notes = {} }
local matrix
matrix = ui.canvas{
  id = "arpMatrix", x = mx, y = my, w = mw, h = mh,
  paint = function(g, w, h)
    local gx, gy, gw = 0, 0, w
    local gh = h - 36
    g:setColour("windowBg"); g:fillRect(gx, gy, gw, gh)
    g:setColour("cardBorder"); g:drawRect(gx, gy, gw, gh, 1)
    local colW = gw / 16
    local pitchH = gh - 6
    local laneH = pitchH / 4
    for r = 1, 3 do
      g:setColour(lx.shade("cardBorder", 0.12)); g:drawLine(gx, floor(gy + r * laneH) + 0.5, gx + gw, floor(gy + r * laneH) + 0.5, 1)
    end
    for s = 1, 15 do
      local x = floor(gx + s * colW) + 0.5
      g:setColour(s % 4 == 0 and lx.shade("cardBorder", 0.2) or "visualizerGrid")
      g:drawLine(x, gy + 1, x, gy + pitchH, 1)
    end
    for s = 1, 16 do
      g:setColour(s - 1 == arpState.step and "accent" or "textMuted")
      g:drawText(string.format("%02d", s), gx + (s - 1) * colW, gh + 2, colW, 12, "centre", 8.5, true)
    end
    local mode = params.get("spArpMode")
    local steps = arpState.pattern
    if mode ~= 0 and steps then
      -- glide highlight of the current step
      g:setColour("accent", 0.1); g:fillRect(gx + arpState.hx * colW, gy + 1, colW, pitchH - 1)
      local minNote = 127
      for s = 1, 16 do for _, n in ipairs(steps[s]) do if n >= 0 then minNote = min(minNote, n) end end end
      local base = floor(minNote / 12) * 12
      local gate = lx.clamp(params.get("cpArpGate") / 999, 0.1, 1)
      local swing = (params.get("cpArpSwing") / 999 * 2 - 1) * colW
      local nodeH = lx.clamp(laneH * 0.3, 4, 10)
      for s = 1, 16 do
        local cx = gx + (s - 1) * colW + (s % 2 == 0 and swing or 0)
        local cur = s - 1 == arpState.step
        for _, n in ipairs(steps[s]) do
          if n >= 0 then
            local p = lx.clamp((n - base) / 47, 0, 1)
            local y = lx.clamp(gy + pitchH - p * pitchH - nodeH / 2, gy + 2, gy + pitchH - nodeH - 1)
            local cw = max(3, (colW - 4) * gate)
            if cur then
              g:setColour("accent", 0.3); g:fillRoundedRect(cx + 2 - 2, y - 2, cw + 4, nodeH + 4, 2)
              g:setColour("accent"); g:fillRoundedRect(cx + 2, y, cw, nodeH, 1.5)
              g:setColour("textTitle"); g:drawRoundedRect(cx + 2.5, y + 0.5, cw - 1, nodeH - 1, 1.5, 1)
            else
              g:setColour(arpState.live and "accent" or "accentDark", arpState.live and 0.6 or 0.75)
              g:fillRoundedRect(cx + 2, y, cw, nodeH, 1.5)
            end
          end
        end
      end
      if not arpState.live then
        g:setColour("textMuted")
        g:drawText("PREVIEW ON C MAJOR - HOLD KEYS FOR THE LIVE PATTERN", gx + 6, gy + 4, gw - 12, 12, "left", 8.5, true)
      end
    else
      g:setColour("textMuted")
      g:drawText("ARP OFF - CHOOSE A MODE ABOVE", gx, gy, gw, pitchH, "centre", 10, true)
    end
    -- hover readout
    if arpState.hover >= 1 and mode ~= 0 and steps then
      local s = arpState.hover
      local names = {}
      for _, n in ipairs(steps[s]) do if n >= 0 then names[#names + 1] = lx.noteName(n) end end
      local label = string.format("STEP %02d  %s  @ %d ms", s, #names > 0 and table.concat(names, " ") or "-", lx.round((s - 1) * stepMs()))
      local tw = g:textWidth(label, 9, true) + 10
      local x = lx.clamp(gx + (s - 0.5) * colW - tw / 2, 2, gw - tw - 2)
      g:setColour("windowBg", 0.9); g:fillRoundedRect(x, gy + pitchH - 20, tw, 16, 2)
      g:setColour("textTitle"); g:drawText(label, x, gy + pitchH - 20, tw, 16, "centre", 9, true)
    end
    -- status
    g:setColour("textMuted")
    local sync = params.get("spArpSync") ~= 0
    local state = mode == 0 and "DISABLED" or (arpState.live and "PLAYING" or "WAITING FOR KEYS")
    g:drawText(string.format("STATE: %s  |  %s: %d BPM  |  %s = %d MS", state, sync and "SYNC" or "FREE", lx.round(bpm()),
               RATES[params.get("spArpRate") + 1], lx.round(stepMs())), 0, h - 18, w, 14, "centre", 9, true)
  end,
  mouse = function(event, x, y, wheel)
    if event == "move" or event == "enter" or event == "drag" or event == "down" then
      local s = lx.clamp(floor(x / (mw / 16)) + 1, 1, 16)
      if y > mh - 36 then s = -1 end
      if s ~= arpState.hover then arpState.hover = s; matrix:repaint() end
    elseif event == "exit" then
      arpState.hover = -1; matrix:repaint()
    elseif event == "wheel" then
      params.set("spArpRate", lx.clamp(params.get("spArpRate") + (wheel > 0 and 1 or -1), 0, 5))
    end
  end,
}

local function setStep(s)
  if s == arpState.step then return end
  local wrap = arpState.step >= 0 and s < arpState.step
  arpState.step = s
  if wrap then arpState.hx = s; matrix:repaint()
  else lx.tween(arpState, "hx", s, min(0.12, stepMs() / 2000), lx.ease.outCubic, function() matrix:repaint() end) end
  matrix:repaint()
end

local function sameNotes(a, b)
  if #a ~= #b then return false end
  for i = 1, #a do if a[i] ~= b[i] then return false end end
  return true
end

local function poll(dt)
  local a = synth.arp()
  local live = type(a) == "table" and a.valid and type(a.notes) == "table" and #a.notes > 0 and type(a.pattern) == "table"
  if live then
    local changed = not arpState.live or not sameNotes(a.notes, arpState.notes)
    if not changed and arpState.pattern then
      for s = 1, 16 do if arpState.pattern[s][1] ~= a.pattern[s] then changed = true; break end end
    end
    if changed then
      local steps = {}
      for s = 1, 16 do steps[s] = { a.pattern[s] or -1 } end
      arpState.pattern, arpState.notes, arpState.live = steps, a.notes, true
      matrix:repaint()
    end
    setStep(lx.clamp(a.step or 0, 0, 15))
  else
    if arpState.live or not arpState.pattern then
      arpState.live, arpState.notes = false, {}
      arpState.pattern = previewPattern()
      matrix:repaint()
    end
    -- the preview steps at the arp's tempo
    arpState.pclock = arpState.pclock + dt * 1000 / stepMs()
    setStep(floor(arpState.pclock) % 16)
  end
end

local function arpModeChanged()
  arpState.pattern = nil
  if params.get("spArpMode") ~= 0 then
    arpState.live = false
    arpState.pattern = previewPattern()
    lx.loop("arp", poll, 30)
  else
    lx.stop("arp")
    arpState.step, arpState.hx = -1, 0
  end
  matrix:repaint()
end
lx.watch({ "spArpMode", "spArpOctaves" }, function()
  if not arpState.live and params.get("spArpMode") ~= 0 then arpState.pattern = previewPattern() end
  matrix:repaint()
end)
lx.watch("spArpMode", arpModeChanged)
lx.watch({ "spArpRate", "spArpSync", "cpArpBpm", "cpArpGate", "cpArpSwing" }, matrix)
arpModeChanged()

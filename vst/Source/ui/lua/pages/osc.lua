-- OSC in Lua: the native tab's controls (engine, OSC A / B with WaveMod and
-- hard sync, noise, the waveforms and the Elements resonator), plus what Lua adds:
--   * each waveform card shows a preview of the selected WaveMod applied to the
--     wave, computed here, morphing into every new mode / amount; a faint line
--     shows the wave at the envelope's peak (amount + env depth);
--   * drag up/down in a waveform sets the WaveMod amount, wheel nudges it,
--     double click resets; hover reads phase and both sample values;
--   * SCAN runs a playhead through the cycle (a play toggle, stops when off);
--   * the Elements card has a drawing of the resonator's modes computed from
--     model, geometry, brightness, damping and position: click strikes it (the
--     partials ring out by their damping), drag moves geometry / brightness;
--   * the layout is data driven: the engine switch shows and hides cards.
-- The WaveMod previews and the mode drawing are approximations drawn in Lua,
-- not the DSP's output.
-- Native only: wave editing (SMOOTH, INVERT, NORMALIZE), saving wave files.

local W, H = ui.size()
local GAP = 6
local ENGINE_H = 24
local TOP_Y = ENGINE_H + GAP
local TOP_H = 192
local BOT_Y = TOP_Y + TOP_H + GAP
local BOT_H = H - BOT_Y
local COL_W = (W - GAP) // 2
local COL2_X = COL_W + GAP
local floor, max, min, abs, sin, pi = math.floor, math.max, math.min, math.abs, math.sin, math.pi

local MOD_NAMES = { "OFF", "GRIT", "PWM", "FM", "MORPH", "FOLD", "CRUSH" }
local MOD_GLYPHS = { false, "grit", "pwm", "fm", "morph", "fold", "crush" }
local MOD_TEXT = { "OFF", "GRIT (ALIASING)", "PWM (WIDTH)", "FM (SELF PHASE)", "MORPH (PHASE SKEW)", "FOLD", "CRUSH (BITS)" }

local function engine() return params.get("spOscEngine") end

-- =========================================================== engine row
local engineChoice = lx.choice{ id = "engine", param = "spOscEngine", x = 0, y = 0, w = 420, h = ENGINE_H,
                                items = { "DUAL WAVETABLE", "ELEMENTS MODAL", "HYBRID" } }
lx.text{ id = "engineInfo", x = 432, y = 0, w = W - 432 - 262, h = ENGINE_H, size = 9.5, colour = "textMuted",
         text = function()
           local e = engine()
           if e == 1 then return "ELEMENTS PLAYS WITH OSC A'S PITCH AND LEVEL" end
           if e == 2 then return "OSC A AND B PLAY, OSC A ALSO EXCITES ELEMENTS" end
           return "TWO WAVETABLE OSCILLATORS"
         end, align = "right" }
lx.hslider{ id = "noise", param = "cpNoiseVol", x = W - 250, y = 0, w = 250, h = ENGINE_H,
            format = function(v) return "NOISE  " .. lx.round(v / 9.99) .. " %" end }

-- =========================================================== OSC A / B cards
-- Card variants by engine (a card's badge is fixed, so each one is its own card)
local cardsA = {
  ui.card{ id = "oscACard0", title = "OSC A", badge = "WAVETABLE", x = 0, y = TOP_Y, w = COL_W, h = TOP_H },
  ui.card{ id = "oscACard1", title = "OSC A", badge = "PITCH & LEVEL OF ELEMENTS", x = 0, y = TOP_Y, w = COL_W, h = TOP_H },
  ui.card{ id = "oscACard2", title = "OSC A", badge = "WAVETABLE + EXCITER", x = 0, y = TOP_Y, w = COL_W, h = TOP_H },
}
local cardsB = {
  ui.card{ id = "oscBCard0", title = "OSC B", badge = "WAVETABLE", x = COL2_X, y = TOP_Y, w = COL_W, h = TOP_H },
  ui.card{ id = "oscBCard1", title = "OSC B", badge = "NOT USED BY ELEMENTS", x = COL2_X, y = TOP_Y, w = COL_W, h = TOP_H },
}

local DIAL_SIZE, DIAL_W = 40, 92
local scrims = {}

-- One oscillator card's contents; dials = { {param, caption}, ... }
local function oscControls(osc, x0, dials, withSync)
  local ix, iy, iw = x0 + 10, TOP_Y + 32, COL_W - 20
  lx.subhead{ id = "head" .. osc, x = ix, y = iy, w = iw,
              text = osc == "A" and "PITCH, LEVEL & WAVEMODULATION" or "PITCH, DETUNE, LEVEL & WAVEMODULATION" }
  local slot = iw / #dials
  for i, d in ipairs(dials) do
    lx.dial{ param = d[1], caption = d[2], x = floor(ix + (i - 1) * slot + (slot - DIAL_W) / 2), y = iy + 14,
             size = DIAL_SIZE, w = DIAL_W, default = d[3] }
  end
  local selY = iy + 90
  lx.subhead{ id = "selHead" .. osc, x = ix, y = selY, w = withSync and iw - 180 or iw, text = "WAVEMODULATION SELECTOR" }
  if withSync then
    lx.toggle{ id = "sync", param = "spSync", text = "HARD SYNC TO OSC A", x = ix + iw - 170, y = selY - 2, w = 170, h = 18 }
  end
  lx.choice{ id = "wmod" .. osc, param = "sp" .. osc .. "WModType", items = MOD_NAMES, glyphs = MOD_GLYPHS,
             x = ix, y = selY + 18, w = iw, h = 48, cols = 4, size = 10 }
  return ix, iy, iw, slot, selY
end

do
  local ix, iy, iw, slot, selY = oscControls("A", 0, {
    { "cpAVol", "LEVEL" }, { "cpAFreq", "COARSE PITCH" }, { "cpABaseWMod", "WAVEMOD", 500 }, { "cpWModAEnv", "ENV DEPTH", 500 } })
  -- in ELEMENTS mode OSC A's WaveMod has no effect: a veil over it
  scrims[#scrims + 1] = { x = floor(ix + 2 * slot), y = iy + 12, w = floor(2 * slot), h = 76 }
  scrims[#scrims + 1] = { x = ix, y = selY, w = iw, h = 68 }
end
do
  local ix, iy, iw = oscControls("B", COL2_X, {
    { "cpBVol", "LEVEL" }, { "cpBFreq", "COARSE PITCH" }, { "cpDetune", "FINE DETUNE", 500 },
    { "cpBBaseWMod", "WAVEMOD", 500 }, { "cpWModBEnv", "ENV DEPTH", 500 } }, true)
  scrims[#scrims + 1] = { x = ix, y = iy, w = iw, h = TOP_H - 40, text = "NOT USED BY ELEMENTS" }
end
for i, s in ipairs(scrims) do
  s.handle = ui.canvas{ id = "scrim" .. i, x = s.x, y = s.y, w = s.w, h = s.h,
    paint = function(g, w, h)
      g:setColour("cardBg", 0.72); g:fillRect(0, 0, w, h)
      if s.text then
        local tw = g:textWidth(s.text, 11, true) + 24
        g:setColour("windowBg", 0.9); g:fillRoundedRect((w - tw) / 2, h / 2 - 12, tw, 24, 3)
        g:setColour("cardBorder"); g:drawRoundedRect((w - tw) / 2 + 0.5, h / 2 - 11.5, tw - 1, 23, 3, 1)
        g:setColour("textBody")
        g:drawText(s.text, 0, 0, w, h, "centre", 11, true)
      end
    end,
    mouse = function() end }   -- swallows clicks: the controls below are out of use
end

-- =========================================================== WaveMod preview maths
local N = 256
local function sampleAt(base, ph)
  ph = ph % 1
  local x = ph * N
  local i = floor(x)
  local f = x - i
  local a, b = base[i + 1], base[(i + 1) % N + 1]
  return a + (b - a) * f
end
local function fold(x) return 1 - 4 * abs(((x + 1) / 4) % 1 - 0.5) end

-- Approximations of the WaveMod types on one cycle; a = amount -1..1
local function applyMod(base, mode, a, out)
  local m = abs(a)
  for i = 0, N - 1 do
    local p = i / N
    local v
    if mode == 1 then                                  -- grit: sample and hold, fewer steps
      local steps = max(3, floor(N / (1 + m * 28)))
      v = sampleAt(base, floor(p * steps) / steps)
    elseif mode == 2 then                              -- pwm: the cycle's halves get unequal widths
      local wdt = 0.5 + 0.45 * a
      v = sampleAt(base, p < wdt and p * 0.5 / wdt or 0.5 + (p - wdt) * 0.5 / (1 - wdt))
    elseif mode == 3 then                              -- fm: phase modulated by the wave itself
      v = sampleAt(base, p + 0.32 * a * sampleAt(base, p * 2))
    elseif mode == 4 then                              -- morph: phase skew
      v = sampleAt(base, p ^ (2 ^ (-1.6 * a)))
    elseif mode == 5 then                              -- fold
      v = fold(sampleAt(base, p) * (1 + 4 * m))
    elseif mode == 6 then                              -- crush: fewer levels
      local half = 2 ^ ((1 - m) * 7)
      v = floor(sampleAt(base, p) * half + 0.5) / half
    else
      v = base[i + 1]
    end
    out[i + 1] = lx.clamp(v, -1.25, 1.25)
  end
  return out
end

local function readWave(osc)
  local ok, data = pcall(synth.wave, osc, N)
  local out = {}
  local sum = 0
  for i = 1, N do
    local v = ok and type(data) == "table" and tonumber(data[i]) or 0
    out[i] = v
    sum = sum + v * (i % 7 + 1)
  end
  return out, sum
end
local function waveName(osc)
  local ok, name = pcall(synth.waveName, osc)
  if not ok or name == nil or name == "" then return "NO WAVE" end
  return name
end

-- =========================================================== waveform cards
local waves = {}

local function makeWaveCard(osc, x)
  local wv = { osc = osc, scan = false, scanPh = 0, hover = { x = -1, a = 0 }, k = 1, drag = nil }
  wv.card = ui.card{ id = "wave" .. osc .. "Card", title = "WAVEFORM", badge = "OSC " .. osc, x = x, y = BOT_Y, w = COL_W, h = BOT_H }
  local ix, iy, iw, ih = x + 10, BOT_Y + 32, COL_W - 20, BOT_H - 40
  local typeId, amtId, envId = "sp" .. osc .. "WModType", "cp" .. osc .. "BaseWMod", "cpWMod" .. osc .. "Env"
  wv.base, wv.sum = readWave(osc)
  wv.name = waveName(osc)
  wv.shown, wv.from, wv.target, wv.peak = {}, {}, {}, {}
  local function amount() return lx.clamp((params.get(amtId) - 500) / 499, -1, 1) end
  local function peakAmount() return lx.clamp(amount() + (params.get(envId) - 500) / 499, -1, 1) end
  local function compute()
    applyMod(wv.base, params.get(typeId), amount(), wv.target)
    applyMod(wv.base, params.get(typeId), peakAmount(), wv.peak)
  end
  compute()
  for i = 1, N do wv.shown[i] = wv.target[i]; wv.from[i] = wv.target[i] end

  local function blend()
    local k = wv.k
    for i = 1, N do wv.shown[i] = wv.from[i] + (wv.target[i] - wv.from[i]) * k end
  end
  -- morphs from what is drawn into the new target
  function wv.retarget(instant)
    for i = 1, N do wv.from[i] = wv.shown[i] end
    compute()
    if instant then wv.k = 1; blend(); wv.display:repaint(); return end
    wv.k = 0
    lx.tween(wv, "k", 1, 0.4, lx.ease.outCubic, function() blend(); wv.display:repaint() end)
  end
  -- re-reads the wave; true when it changed
  function wv.refresh()
    local base, sum = readWave(osc)
    local name = waveName(osc)
    if sum == wv.sum and name == wv.name then return false end
    wv.base, wv.sum, wv.name = base, sum, name
    wv.retarget(false)
    wv.nameLabel:repaint()
    return true
  end

  -- header row: name, scan, browse
  wv.nameLabel = ui.canvas{ id = "waveName" .. osc, x = ix, y = iy, w = iw - 200, h = 22, paint = function(g, w, h)
    g:setColour("buttonBg"); g:fillRoundedRect(0, 0, 34, h, 2)
    g:setColour("accent"); g:drawRoundedRect(0.5, 0.5, 33, h - 1, 2, 1)
    g:drawText("OSC " .. osc, 0, 0, 34, h, "centre", 9, true)
    g:setColour("textTitle")
    g:drawText(wv.name, 42, 0, w - 42, h, "left", 12, true)
  end }
  wv.scanToggle, wv.scanSync = lx.toggle{ id = "scan" .. osc, x = ix + iw - 190, y = iy + 2, w = 100, h = 18, text = "SCAN CYCLE",
    get = function() return wv.scan end,
    set = function(on)
      wv.scan = on
      if on then
        lx.loop("scan" .. osc, function(dt)
          wv.scanPh = (wv.scanPh + dt * 0.35) % 1
          wv.display:repaint()
        end, 30)
      else
        lx.stop("scan" .. osc)
        wv.display:repaint()
      end
    end }
  wv.browseBtn = lx.button{ id = "browse" .. osc, x = ix + iw - 84, y = iy, w = 84, h = 22, text = "BROWSE", size = 10,
    onClick = function()
      local ok, err = pcall(synth.openWaveBrowser, osc)
      if not ok then log("wave browser: " .. tostring(err)); return end
      -- the browser runs outside the page: watch the wave for a while (4 Hz, 30 s at most)
      local waited = 0
      lx.loop("browse" .. osc, function(dt)
        waited = waited + dt
        if wv.refresh() or waited > 30 then return false end
      end, 4)
    end }

  -- the display
  local dispY, dispH = iy + 28, ih - 28 - 20
  local function yOf(v, h) return h / 2 - v * h * 0.42 end
  wv.display = ui.canvas{ id = "waveDisplay" .. osc, x = ix, y = dispY, w = iw, h = dispH,
    paint = function(g, w, h)
      lx.panel(g, w, h)
      lx.grid(g, 0, 0, w, h, 4, 4, 0.7)
      local mode = params.get(typeId)
      local sx = (w - 2) / (N - 1)
      -- original wave, thin
      g:beginPath()
      for i = 1, N do
        local x, y = 1 + (i - 1) * sx, yOf(wv.base[i], h)
        if i == 1 then g:moveTo(x, y) else g:lineTo(x, y) end
      end
      g:setColour("textMuted", mode == 0 and 0 or 0.55); g:stroke(1)
      -- wave at the envelope peak, faint
      if mode ~= 0 and params.get(envId) ~= 500 then
        g:beginPath()
        for i = 1, N do
          local x, y = 1 + (i - 1) * sx, yOf(wv.peak[i], h)
          if i == 1 then g:moveTo(x, y) else g:lineTo(x, y) end
        end
        g:setColour("accent", 0.35); g:stroke(1.2)
      end
      -- preview
      local pts = {}
      for i = 1, N do pts[#pts + 1] = 1 + (i - 1) * sx; pts[#pts + 1] = yOf(wv.shown[i], h) end
      g:save(); g:clip(0, 0, w, h)
      lx.area(g, pts, h / 2, "visualizerCurve", 0.3, 0.05, 1.8)
      g:restore()
      -- legend
      g:setColour("accent")
      local amt = lx.round((params.get(amtId) - 500) / 4.99)
      g:drawText(mode == 0 and "WAVEMOD OFF" or (MOD_TEXT[mode + 1] .. "  " .. amt .. " %"), 0, 4, w - 8, 14, "right", 10, true)
      if mode ~= 0 then
        g:setColour("textMuted")
        g:drawText("LUA PREVIEW (APPROXIMATION)", 8, 4, 220, 14, "left", 8.5, true)
      end
      -- scan playhead
      local function marker(ph, alpha, label)
        local i = lx.clamp(floor(ph * N) + 1, 1, N)
        local x, y = 1 + (i - 1) * sx, yOf(wv.shown[i], h)
        g:setColour("textBody", 0.45 * alpha); g:drawLine(x, 0, x, h, 1)
        g:setColour("accent", 0.3 * alpha); g:fillEllipse(x - 8, y - 8, 16, 16)
        g:setColour("textTitle", alpha); g:fillEllipse(x - 3.5, y - 3.5, 7, 7)
        if label then
          local bx = x + 8 > w - 150 and x - 158 or x + 8
          g:setColour("windowBg", 0.85 * alpha); g:fillRoundedRect(bx, h - 24, 150, 16, 2)
          g:setColour("textTitle", alpha)
          g:drawText(string.format("%3d %%   %+.2f > %+.2f", floor(ph * 100), wv.base[i], wv.shown[i]),
                     bx + 4, h - 24, 146, 16, "left", 9.5, true)
        end
      end
      if wv.scan then marker(wv.scanPh, 1) end
      if wv.hover.a > 0.01 and wv.hover.x >= 0 then marker(lx.clamp(wv.hover.x / w, 0, 0.999), wv.hover.a, true) end
    end,
    mouse = function(event, x, y, wheel)
      if event == "down" then
        wv.drag = { y = y, v = params.get(amtId) }
      elseif event == "drag" and wv.drag then
        params.set(amtId, lx.clamp(wv.drag.v + (wv.drag.y - y) * 999 / dispH, 0, 999))
      elseif event == "up" then
        wv.drag = nil
      elseif event == "double" then
        params.set(amtId, 500)
      elseif event == "wheel" then
        params.set(amtId, lx.clamp(params.get(amtId) + (wheel > 0 and 10 or -10), 0, 999))
      end
      if event == "move" or event == "drag" or event == "enter" then
        wv.hover.x = x
        if event == "enter" then wv.refresh() end
        if wv.hover.a < 1 then lx.tween(wv.hover, "a", 1, 0.15, nil, function() wv.display:repaint() end) end
        wv.display:repaint()
      elseif event == "exit" then
        lx.tween(wv.hover, "a", 0, 0.3, nil, function() wv.display:repaint() end)
      end
    end }
  wv.hint = lx.text{ id = "waveHint" .. osc, x = ix, y = iy + ih - 16, w = iw, h = 16, size = 8.5, colour = "textMuted",
           text = "DRAG UP/DOWN: WAVEMOD  |  WHEEL  |  DOUBLE CLICK: 0  |  SMOOTH, INVERT, NORMALIZE, SAVE: NATIVE TAB" }

  lx.watch({ typeId, amtId, envId }, function() wv.retarget(false) end)
  wv.widgets = { wv.card, wv.nameLabel, wv.scanToggle, wv.browseBtn, wv.display, wv.hint }
  waves[osc] = wv
  return wv
end

makeWaveCard("A", 0)
makeWaveCard("B", COL2_X)

-- =========================================================== Elements card
local EL_BLOW, EL_STRIKE = "#e0195f", "#0aa6c0"
local elCard = ui.card{ id = "elementsCard", title = "PHYSICAL ACOUSTIC MODELING", badge = "ELEMENTS MODAL RESONATOR",
                        x = 0, y = BOT_Y, w = W, h = BOT_H }
local MED, LARGE = 34, 50
local function elDial(param, caption, size, colour)
  return lx.dial{ param = param, caption = caption, size = size, w = 76, x = 0, y = 0, colour = colour }
end
local exciterRows = {
  { size = MED, dials = { elDial("cpElementsContour", "CONTOUR", MED), elDial("cpElementsBow", "BOW", MED),
                          elDial("cpElementsBlow", "BLOW", MED, EL_BLOW), elDial("cpElementsStrike", "STRIKE", MED, EL_STRIKE) } },
  { size = LARGE, dials = { elDial("cpElementsFlow", "FLOW", LARGE, EL_BLOW), elDial("cpElementsMallet", "MALLET", LARGE, EL_STRIKE) } },
  { size = MED, dials = { elDial("cpElementsBowTimbre", "BOW TIMBRE", MED), elDial("cpElementsBlowTimbre", "BLOW TIMBRE", MED, EL_BLOW),
                          elDial("cpElementsStrikeTimbre", "STRIKE TIMBRE", MED, EL_STRIKE) } },
}
local resonatorRows = {
  { size = LARGE, dials = { elDial("cpElementsGeometry", "GEOMETRY", LARGE), elDial("cpElementsBrightness", "BRIGHTNESS", LARGE) } },
  { size = MED, dials = { elDial("cpElementsDamping", "DAMPING", MED), elDial("cpElementsPosition", "POSITION", MED),
                          elDial("cpElementsSpace", "SPACE", MED) } },
}
local exciterHead = lx.subhead{ id = "exciterHead", x = 0, y = 0, w = 100, text = "EXCITER" }
local resonatorHead = lx.subhead{ id = "resonatorHead", x = 0, y = 0, w = 100, text = "RESONATOR & SPACE" }
local modesHead = lx.subhead{ id = "modesHead", x = 0, y = 0, w = 100, text = "RESONATOR MODES (LUA DRAWING)" }
-- thin vertical rules between the zones
local function vrule(id)
  return ui.canvas{ id = id, x = 0, y = 0, w = 1, h = 10, paint = function(g, w, h)
    g:setColour("cardBorder", 0.9); g:fillRect(0, 0, 1, h)
  end }
end
local rule1, rule2 = vrule("elRule1"), vrule("elRule2")
local modelChoice = lx.choice{ id = "elModel", param = "spElementsModel", items = { "MODAL 64", "NON-LIN STRING", "CHORDS", "OMINOUS CHOIR" },
                               x = 0, y = 0, w = 240, h = 48, cols = 2, size = 10 }

-- ---- resonator modes: partial ratios and levels, computed from the settings
local PARTIALS = 32
local CHORD = { 1, 1.26, 1.5, 2 }
local function partials()
  local model = params.get("spElementsModel")
  local geo, bright = params.get("cpElementsGeometry") / 999, params.get("cpElementsBrightness") / 999
  local damp, pos = params.get("cpElementsDamping") / 999, params.get("cpElementsPosition") / 999
  local ratio, level, decay = {}, {}, {}
  for n = 1, PARTIALS do
    local r
    if model == 0 then        -- modal: stiffness from geometry stretches (or squeezes) the series
      local b = (geo - 0.3) * 0.05
      r = n * math.sqrt(max(0.04, 1 + b * (n * n - 1)))
    elseif model == 2 then    -- chords: harmonics of four chord notes
      local note, h = CHORD[(n - 1) % 4 + 1], (n - 1) // 4 + 1
      r = note * h
    else                      -- string and voice: harmonic
      r = n * (1 + (model == 1 and geo * 0.002 * n or 0))
    end
    local a = r ^ (-(2.2 - 2 * bright))
    a = a * (0.12 + 0.88 * abs(sin(pi * n * (0.05 + 0.45 * pos))))
    if model == 3 then        -- ominous voice: two formants, moved by geometry
      local f1, f2 = 2 + geo * 4, 7 + geo * 10
      a = a * (0.2 + math.exp(-((r - f1) / 1.5) ^ 2) + 0.7 * math.exp(-((r - f2) / 3) ^ 2)) * 2
    end
    ratio[n], level[n] = r, min(1, a)
    decay[n] = (0.15 + 3.5 * (1 - damp) ^ 2) / (1 + r * 0.08 * (1.2 - bright))   -- seconds
  end
  return ratio, level, decay
end
local RMAX = 24
local function ratioX(r, w) return math.log(lx.clamp(r, 1, RMAX)) / math.log(RMAX) * (w - 16) + 8 end
local function levelH(a, h) return lx.clamp((20 * math.log(max(a, 1e-4), 10) + 48) / 48, 0, 1) * (h - 30) end

local modes = { k = 1, ring = 0, hover = { x = -1, a = 0 } }
modes.ratio, modes.level, modes.decay = partials()
modes.fr, modes.fl = {}, {}
modes.sr, modes.sl = {}, {}
for n = 1, PARTIALS do modes.sr[n], modes.sl[n] = modes.ratio[n], modes.level[n] end
local modesView
local function modesBlend()
  for n = 1, PARTIALS do
    modes.sr[n] = modes.fr[n] + (modes.ratio[n] - modes.fr[n]) * modes.k
    modes.sl[n] = modes.fl[n] + (modes.level[n] - modes.fl[n]) * modes.k
  end
end
local function modesRetarget()
  for n = 1, PARTIALS do modes.fr[n], modes.fl[n] = modes.sr[n], modes.sl[n] end
  modes.ratio, modes.level, modes.decay = partials()
  modes.k = 0
  lx.tween(modes, "k", 1, 0.35, lx.ease.outCubic, function() modesBlend(); modesView:repaint() end)
end
local modesDrag
modesView = ui.canvas{ id = "modes", x = 0, y = 0, w = 100, h = 100,
  paint = function(g, w, h)
    lx.panel(g, w, h)
    local base = h - 16
    -- grid: harmonic numbers
    for _, r in ipairs({ 1, 2, 3, 4, 6, 8, 12, 16 }) do
      local x = ratioX(r, w)
      g:setColour("visualizerGrid"); g:drawLine(floor(x) + 0.5, 20, floor(x) + 0.5, base, 1)
      g:setColour("textMuted"); g:drawText(tostring(r), x - 10, base + 1, 20, 14, "centre", 8.5, true)
    end
    local t = modes.ring
    for n = PARTIALS, 1, -1 do
      local r = modes.sr[n]
      if r <= RMAX then
        local x = ratioX(r, w)
        local a = modes.sl[n]
        if t > 0 then a = a * math.exp(-t / modes.decay[n]) end
        local bh = levelH(a, h)
        local warm = lx.clamp(modes.decay[n] / 2, 0, 1)
        g:setColour(lx.mix("accentDark", "accent", warm), 0.35 + 0.5 * warm)
        g:fillRect(x - 1.5, base - bh, 3, bh)
        g:setColour("accent", 0.3 + 0.7 * warm); g:fillEllipse(x - 2.5, base - bh - 2.5, 5, 5)
      end
    end
    g:setColour("textMuted")
    g:drawText("PARTIAL RATIO", 8, 3, 120, 14, "left", 8.5, true)
    g:setColour("accent")
    g:drawText(t > 0 and string.format("RINGING  %.1f s", t) or "CLICK: STRIKE  |  DRAG: GEOMETRY / BRIGHTNESS", 0, 3, w - 8, 14, "right", 8.5, true)
    -- hover: nearest partial
    if modes.hover.a > 0.01 then
      local best, bd = 1, 1e9
      for n = 1, PARTIALS do
        local d = abs(ratioX(modes.sr[n], w) - modes.hover.x)
        if d < bd then best, bd = n, d end
      end
      local x = ratioX(modes.sr[best], w)
      g:setColour("textBody", 0.5 * modes.hover.a); g:drawLine(x, 20, x, base, 1)
      local label = string.format("#%d  x%.2f  %.0f dB  %.1f s", best, modes.sr[best], 20 * math.log(max(modes.sl[best], 1e-4), 10), modes.decay[best])
      local bx = x + 8 > w - 160 and x - 168 or x + 8
      g:setColour("windowBg", 0.85 * modes.hover.a); g:fillRoundedRect(bx, 20, 160, 16, 2)
      g:setColour("textTitle", modes.hover.a); g:drawText(label, bx + 4, 20, 156, 16, "left", 9, true)
    end
  end,
  mouse = function(event, x, y, wheel)
    if event == "down" then
      modesDrag = { x = x, y = y, g = params.get("cpElementsGeometry"), b = params.get("cpElementsBrightness") }
      modes.ring = 0
      local longest = 0
      for n = 1, PARTIALS do longest = max(longest, modes.decay[n]) end
      lx.tween(modes, "ring", min(6, longest * 3), min(6, longest * 3), lx.ease.linear, function(v)
        if v >= min(6, longest * 3) - 1e-6 then modes.ring = 0 end
        modesView:repaint()
      end)
    elseif event == "drag" and modesDrag then
      params.set("cpElementsGeometry", lx.clamp(modesDrag.g + (x - modesDrag.x) * 2, 0, 999))
      params.set("cpElementsBrightness", lx.clamp(modesDrag.b + (modesDrag.y - y) * 3, 0, 999))
    elseif event == "up" then
      modesDrag = nil
    elseif event == "wheel" then
      params.set("cpElementsDamping", lx.clamp(params.get("cpElementsDamping") + (wheel > 0 and 10 or -10), 0, 999))
    end
    if event == "move" or event == "drag" or event == "enter" then
      modes.hover.x = x
      if modes.hover.a < 1 then lx.tween(modes.hover, "a", 1, 0.15, nil, function() modesView:repaint() end) end
      modesView:repaint()
    elseif event == "exit" then
      lx.tween(modes.hover, "a", 0, 0.3, nil, function() modesView:repaint() end)
    end
  end }
lx.watch({ "spElementsModel", "cpElementsGeometry", "cpElementsBrightness", "cpElementsDamping", "cpElementsPosition" }, modesRetarget)

-- Places a row of dials centred in equal slots
local function placeRow(row, x, w, y)
  local slot = w / #row.dials
  local dw = min(floor(slot), 96)
  for i, d in ipairs(row.dials) do
    d:setBounds(floor(x + (i - 1) * slot + (slot - dw) / 2), y, dw, row.size + 32)
  end
end
local function setAllVisible(list, on) for _, h in ipairs(list) do h:setVisible(on) end end
local elementsWidgets = { elCard, exciterHead, resonatorHead, modelChoice, rule1, rule2 }
for _, r in ipairs(exciterRows) do for _, d in ipairs(r.dials) do elementsWidgets[#elementsWidgets + 1] = d end end
for _, r in ipairs(resonatorRows) do for _, d in ipairs(r.dials) do elementsWidgets[#elementsWidgets + 1] = d end end

-- Lays the Elements card out in (x, w): exciter | resonator [| modes when wide]
local function layoutElements(x, w)
  elCard:setBounds(x, BOT_Y, w, BOT_H)
  local ix, iy, iw, ih = x + 10, BOT_Y + 32, w - 20, BOT_H - 40
  local wide = iw > 700
  local zoneW = wide and floor((iw - 24) * 0.34) or floor((iw - 12) / 2)
  local ex, rx, mx = ix, ix + zoneW + 12, ix + 2 * (zoneW + 12)
  rule1:setBounds(rx - 6, iy + 8, 1, ih - 8)
  rule2:setBounds(mx - 6, iy + 8, 1, ih - 8)
  rule2:setVisible(wide)
  exciterHead:setBounds(ex, iy, zoneW, 14)
  resonatorHead:setBounds(rx, iy, zoneW, 14)
  local rowsH = MED + 32 + LARGE + 32 + MED + 32
  local gap = floor((ih - 18 - rowsH) / 3)
  local y1 = iy + 18 + gap // 2
  local y2 = y1 + MED + 32 + gap
  local y3 = y2 + LARGE + 32 + gap
  placeRow(exciterRows[1], ex, zoneW, y1)
  placeRow(exciterRows[2], ex, zoneW, y2)
  placeRow(exciterRows[3], ex, zoneW, y3)
  local cw = min(zoneW, 300)
  modelChoice:setBounds(floor(rx + (zoneW - cw) / 2), y1 + (MED + 32 - 48) // 2, cw, 48)
  placeRow(resonatorRows[1], rx, zoneW, y2)
  placeRow(resonatorRows[2], rx, zoneW, y3)
  modesHead:setVisible(wide)
  modesView:setVisible(wide)
  if wide then
    local mw = ix + iw - mx
    modesHead:setBounds(mx, iy, mw, 14)
    modesView:setBounds(mx, iy + 18, mw, ih - 18)
  end
end

-- =========================================================== engine switching
local function setWaveVisible(osc, on)
  local wv = waves[osc]
  for _, h in ipairs(wv.widgets) do h:setVisible(on) end
  if not on and wv.scan then wv.scan = false; lx.stop("scan" .. osc); wv.scanSync() end
end
local function showEngine()
  local e = engine()
  for i, c in ipairs(cardsA) do c:setVisible(i == e + 1) end
  for i, c in ipairs(cardsB) do c:setVisible(i == (e == 1 and 2 or 1)) end
  for _, s in ipairs(scrims) do s.handle:setVisible(e == 1) end
  setWaveVisible("A", e ~= 1)
  setWaveVisible("B", e == 0)
  setAllVisible(elementsWidgets, e ~= 0)
  if e == 0 then
    modesHead:setVisible(false); modesView:setVisible(false)
  elseif e == 1 then
    layoutElements(0, W)
  else
    layoutElements(COL2_X, COL_W)
  end
end
lx.watch("spOscEngine", showEngine)
showEngine()


-- FILTER / VCA in Lua: the native tab's controls, plus what Lua adds:
--   * the response curve is computed here (complex transfer functions of the
--     ladder, SVF and EQ models) and morphs smoothly into every new setting;
--   * a live spectrum of the output (an FFT written in Lua) behind the curve;
--   * crosshair readout, drag/wheel in the curve, A/B snapshots with a morph slider;
--   * the voice mixer is one canvas: meters with peak hold, faders and pans.
-- The curves are drawings of the filter types, not measurements of the DSP.

local W, H = ui.size()
local GAP = 6
local TOP_H = 236
local LEFT_W = 356
local floor, max, min, log10, sqrt = math.floor, math.max, math.min, math.log10 or function(x) return math.log(x, 10) end, math.sqrt

-- =========================================================== filter models
-- family -> entries { model, semVariant (nil = any), mode, label, glyph }
local FAMILIES = {
  { name = "LADDER", entries = {
      { 0, nil, 0, "24 DB", "lp24" }, { 3, nil, 1, "18 DB", "lp18" }, { 3, nil, 2, "12 DB", "lp12" }, { 3, nil, 3, "6 DB", "lp6" } } },
  { name = "RIPPLES", entries = {
      { 1, 4, 0, "LP 24 DB", "lp24" }, { 1, 4, 2, "BP 12 DB", "lp12" }, { 1, 4, 1, "LP 12 DB", "lp12" } } },
  { name = "SEM", entries = {
      { 1, 3, 2, "HP 12 DB", "lp12" }, { 1, 3, 1, "BP 12 DB", "lp12" }, { 1, 3, 0, "LP 12 DB", "lp12" }, { 1, 3, 3, "NOTCH", "lp12" } } },
  { name = "SHELVES", entries = { { 2, nil, 0, "4-BAND PARAMETRIC" } } },
}

local function currentFamily()
  local m = params.get("spFilterModel")
  if m == 0 or m == 3 then return 1 end
  if m == 1 then return params.get("spSemModel") == 4 and 2 or 3 end
  return 4
end
local function currentEntry(fam)
  fam = fam or currentFamily()
  local m, mode = params.get("spFilterModel"), params.get("spFilterMode")
  for i, e in ipairs(FAMILIES[fam].entries) do
    if e[1] == m and e[3] == mode then return i end
  end
  return 1
end
local function selectEntry(fam, i)
  local e = FAMILIES[fam].entries[i]
  params.set("spFilterModel", e[1])
  if e[2] then params.set("spSemModel", e[2]) end
  params.set("spFilterMode", e[3])
end

-- =========================================================== FILTER card
local fx, fy, fw = lx.section{ id = "filterCard", title = "FILTER", badge = "VCF", x = 0, y = 0, w = LEFT_W, h = TOP_H }

local familyChoice, familySelect
local entryChoices, entrySelects = {}, {}
local cutoffWidgets, eqWidgets = {}, {}
local eqBand = 1
local bandDials = {}

local function updateFamilyViews()
  local fam = currentFamily()
  for i, c in ipairs(entryChoices) do c:setVisible(i == fam) end
  if entrySelects[fam] then entrySelects[fam]() end
  local shelves = fam == 4
  for _, wdg in ipairs(cutoffWidgets) do wdg:setVisible(not shelves) end
  for _, wdg in ipairs(eqWidgets) do wdg:setVisible(shelves) end
  for b, list in ipairs(bandDials) do for _, d in ipairs(list) do d:setVisible(shelves and b == eqBand) end end
end

local names = {}
for i, f in ipairs(FAMILIES) do names[i] = f.name end
familyChoice, familySelect = lx.choice{ id = "family", x = fx, y = fy, w = fw, h = 22, items = names,
  get = currentFamily,
  set = function(i) local keep = currentEntry(i); selectEntry(i, keep) end,
}

-- cutoff & modulation dials (all families but SHELVES)
cutoffWidgets[#cutoffWidgets + 1] = lx.subhead{ x = fx, y = fy + 28, w = 160, text = "CUTOFF & RESONANCE" }
cutoffWidgets[#cutoffWidgets + 1] = lx.subhead{ x = fx + 172, y = fy + 28, w = fw - 172, text = "MODULATION" }
local dialY = fy + 44
for i, d in ipairs({ { "cpCutoff", "CUTOFF" }, { "cpResonance", "RESONANCE" }, { "cpFilKbdAmt", "KEY TRACK" }, { "cpFilEnvAmt", "ENV DEPTH" } }) do
  cutoffWidgets[#cutoffWidgets + 1] = lx.dial{ param = d[1], caption = d[2], x = fx + (i - 1) * (fw / 4) + (fw / 4 - 80) / 2, y = dialY, size = 40, w = 80 }
end

-- the EQ's bands (SHELVES)
local BANDS = {
  { "LOW", { { "cpShelvesLsFreq", "FREQ" }, { "cpShelvesLsGain", "GAIN" } } },
  { "MID LOW", { { "cpShelvesP1Gain", "GAIN" } } },
  { "MID HIGH", { { "cpShelvesP2Freq", "FREQ" }, { "cpShelvesP2Gain", "GAIN" }, { "cpShelvesP2Q", "Q" } } },
  { "HIGH", { { "cpShelvesHsFreq", "FREQ" }, { "cpShelvesHsGain", "GAIN" } } },
}
eqWidgets[#eqWidgets + 1] = lx.subhead{ x = fx, y = fy + 28, w = fw, text = "4-BAND EQ" }
local bandChoice, bandSelect
bandChoice, bandSelect = lx.choice{ id = "eqBand", x = fx, y = fy + 44, w = fw, h = 20, size = 9.5,
  items = { "LOW", "MID LOW", "MID HIGH", "HIGH" },
  get = function() return eqBand end,
  set = function(i)
    eqBand = i
    for b, list in ipairs(bandDials) do for _, d in ipairs(list) do d:setVisible(b == i and currentFamily() == 4) end end
  end,
}
eqWidgets[#eqWidgets + 1] = bandChoice
for b, band in ipairs(BANDS) do
  bandDials[b] = {}
  for i, d in ipairs(band[2]) do
    local dial = lx.dial{ id = "eq" .. b .. "_" .. i, param = d[1], caption = band[1] .. " " .. d[2], size = 40, w = 90,
                          x = fx + (i - 1) * 100, y = fy + 70 }
    bandDials[b][#bandDials[b] + 1] = dial
  end
end

-- filter type: one choice per family, only the current one shows
local typeHead = lx.subhead{ x = fx, y = fy + 120, w = fw, text = "FILTER TYPE" }
for fam, f in ipairs(FAMILIES) do
  local labels, glyphs = {}, {}
  for i, e in ipairs(f.entries) do labels[i] = e[4]; glyphs[i] = e[5] end
  local c, s = lx.choice{ id = "type" .. fam, x = fx, y = fy + 136, w = fw, h = fam == 4 and 22 or 48, cols = fam == 4 and 1 or 2,
                          items = labels, glyphs = fam ~= 4 and glyphs or nil, size = 10,
                          get = function() return currentFamily() == fam and currentEntry(fam) or 0 end,
                          set = function(i) selectEntry(fam, i) end }
  entryChoices[fam], entrySelects[fam] = c, s
end

-- =========================================================== response maths
-- complex numbers as two numbers
local function cmul(a, b, c, d) return a * c - b * d, a * d + b * c end
local function cdiv(a, b, c, d) local q = c * c + d * d; return (a * c + b * d) / q, (b * c - a * d) / q end
local function cabs(a, b) return sqrt(a * a + b * b) end
local function cpow(a, b, n) local r, i = 1, 0; for _ = 1, n do r, i = cmul(r, i, a, b) end; return r, i end

local function cutoffHz(pot) return 20 * 10 ^ (3 * pot / 999) end
local function hzToX(hz, w) return (log10(hz / 20) / 3) * w end
local DB_TOP, DB_BOTTOM = 18, -42

-- Gain in dB at frequency hz for model/mode/semVariant, cutoff fc and resonance r (0..1).
local function responseDb(hz, s)
  if s.model == 2 then
    -- 4-band EQ: shelves and peaks drawn with their usual shapes
    local function g(pot) return (pot - 500) / 499 * 15 end
    local function f(pot, lo, hi) return lo * (hi / lo) ^ (pot / 999) end
    local db = 0
    local lf = f(s.ls, 30, 1000); db = db + g(s.lsg) / (1 + (hz / lf) ^ 2)
    local hf = f(s.hs, 1000, 16000); db = db + g(s.hsg) / (1 + (hf / hz) ^ 2)
    local p1 = 400; local x1 = log10(hz / p1) * 3; db = db + g(s.p1g) * math.exp(-x1 * x1)
    local p2 = f(s.p2, 200, 12000); local q = 0.4 + s.p2q / 999 * 4
    local x2 = log10(hz / p2) * 3 * q; db = db + g(s.p2g) * math.exp(-x2 * x2)
    return db
  end
  local wr, wi = 0, hz / s.fc                     -- s = j w / wc
  local hr, hi
  if s.model == 0 or s.model == 3 then
    -- ladder: (1+s)^(4-n) / ((1+s)^4 + k), n poles at the output tap
    local poles = s.model == 0 and 4 or ({ [0] = 4, 3, 2, 1 })[s.mode] or 4
    local k = s.res * 3.6
    local ar, ai = cpow(1 + wr, wi, 4)
    local nr, ni = cpow(1 + wr, wi, 4 - poles)
    hr, hi = cdiv(nr, ni, ar + k, ai)
  else
    -- state variable 2-pole: s^2 + s/Q + 1
    local q = 0.55 / (1 - s.res * 0.93)
    local s2r, s2i = cmul(wr, wi, wr, wi)
    local dr, di = s2r + wr / q + 1, s2i + wi / q
    local mode = s.mode
    local nr, ni
    if s.sem == 4 then
      -- Ripples: 0 LP4, 1 LP2, 2 BP2
      if mode == 2 then nr, ni = wr / q, wi / q else nr, ni = 1, 0 end
      hr, hi = cdiv(nr, ni, dr, di)
      if mode == 0 then hr, hi = cmul(hr, hi, hr, hi) end
    else
      -- SEM: 0 LP, 1 BP, 2 HP, 3 notch
      if mode == 0 then nr, ni = 1, 0
      elseif mode == 1 then nr, ni = wr / q, wi / q
      elseif mode == 2 then nr, ni = s2r, s2i
      else nr, ni = s2r + 1, s2i end
      hr, hi = cdiv(nr, ni, dr, di)
    end
  end
  return 20 * log10(max(1e-6, cabs(hr, hi)))
end

local function settings(fcPot, resPot)
  return {
    model = params.get("spFilterModel"), mode = params.get("spFilterMode"), sem = params.get("spSemModel"),
    fc = cutoffHz(fcPot or params.get("cpCutoff")), res = (resPot or params.get("cpResonance")) / 999,
    ls = params.get("cpShelvesLsFreq"), lsg = params.get("cpShelvesLsGain"), p1g = params.get("cpShelvesP1Gain"),
    p2 = params.get("cpShelvesP2Freq"), p2g = params.get("cpShelvesP2Gain"), p2q = params.get("cpShelvesP2Q"),
    hs = params.get("cpShelvesHsFreq"), hsg = params.get("cpShelvesHsGain"),
  }
end

local POINTS = 220
local function curveDb(s)
  local out = {}
  for i = 0, POINTS do out[i] = responseDb(20 * 1000 ^ (i / POINTS), s) end
  return out
end

-- =========================================================== FREQUENCY RESPONSE card
local rx, ry, rw, rh = lx.section{ id = "responseCard", title = "FREQUENCY RESPONSE", badge = "LUA: MAGNITUDE + FFT",
                                   x = LEFT_W + GAP, y = 0, w = W - LEFT_W - GAP, h = TOP_H }
local plotH = rh - 28
local shown = curveDb(settings())          -- what is drawn
local from, target = shown, shown
local morph = { k = 1 }
local hover = { x = -1, a = 0 }
local spectrumOn = true
local spectrum = {}                        -- dB per display column, decaying
local response

local function blend()
  for i = 0, POINTS do shown[i] = from[i] + (target[i] - from[i]) * morph.k end
end

-- Recomputes the target curve and morphs into it.
local function retarget()
  local copy = {}
  for i = 0, POINTS do copy[i] = shown[i] end
  from, target = copy, curveDb(settings())
  shown = {}
  morph.k = 0
  blend()
  lx.tween(morph, "k", 1, 0.35, lx.ease.outCubic, function() blend(); response:repaint() end)
end

local function dbToY(db, h) return (DB_TOP - db) / (DB_TOP - DB_BOTTOM) * h end

response = ui.canvas{
  id = "response", x = rx, y = ry, w = rw, h = plotH,
  paint = function(g, w, h)
    lx.panel(g, w, h)
    local px, pw, ph = 28, w - 34, h - 16
    -- grid with labels
    g:setColour("visualizerGrid")
    for _, f in ipairs({ 50, 100, 250, 500, 1000, 2000, 5000, 10000, 20000 }) do
      local x = px + hzToX(f, pw)
      g:drawLine(floor(x) + 0.5, 4, floor(x) + 0.5, ph, 1)
      g:setColour("textMuted")
      g:drawText(f >= 1000 and (floor(f / 1000) .. "k") or tostring(f), x - 20, ph, 40, 14, "centre", 8.5, true)
      g:setColour("visualizerGrid")
    end
    for db = 12, -36, -12 do
      local y = dbToY(db, ph)
      g:drawLine(px, floor(y) + 0.5, px + pw, floor(y) + 0.5, 1)
      g:setColour("textMuted")
      g:drawText((db > 0 and "+" or "") .. db, 0, y - 7, px - 4, 14, "right", 8.5, true)
      g:setColour("visualizerGrid")
    end
    -- live spectrum
    if spectrumOn and #spectrum > 0 then
      local n = #spectrum
      local bw = pw / n
      for i = 1, n do
        local db = spectrum[i]
        if db > DB_BOTTOM then
          local y = dbToY(db, ph)
          local t = lx.clamp((db - DB_BOTTOM) / (DB_TOP - DB_BOTTOM), 0, 1)
          g:setColour(lx.mix("accentDark", "accent", t), 0.22 + 0.35 * t)
          g:fillRect(px + (i - 1) * bw + 0.5, y, max(1, bw - 1), ph - y)
        end
      end
    end
    -- response curve
    local pts = {}
    for i = 0, POINTS do
      pts[#pts + 1] = px + i / POINTS * pw
      pts[#pts + 1] = lx.clamp(dbToY(shown[i], ph), 2, ph)
    end
    g:save(); g:clip(px, 0, pw, ph)
    lx.area(g, pts, ph, "visualizerCurve", 0.32, 0.02, 2)
    g:restore()
    -- cutoff handle
    local fam = currentFamily()
    if fam ~= 4 then
      local fc = cutoffHz(params.get("cpCutoff"))
      local x = px + hzToX(fc, pw)
      local y = lx.clamp(dbToY(responseDb(fc, settings()), ph), 6, ph - 6)
      g:setColour("accent", 0.25); g:fillEllipse(x - 9, y - 9, 18, 18)
      g:setColour("accent"); g:fillEllipse(x - 4.5, y - 4.5, 9, 9)
      g:setColour("textTitle"); g:drawEllipse(x - 4.5, y - 4.5, 9, 9, 1)
    end
    -- readout and crosshair
    g:setColour("accent")
    local head = fam == 4 and "EQ: 4 BANDS" or ("Cutoff: " .. params.display("cpCutoff") .. "  |  Resonance: " .. params.display("cpResonance"))
    g:drawText(head, 0, 4, w - 10, 14, "right", 10, true)
    if hover.a > 0.01 and hover.x >= px then
      local f = 20 * 1000 ^ ((hover.x - px) / pw)
      local db = responseDb(f, settings())
      local y = lx.clamp(dbToY(db, ph), 0, ph)
      g:setColour("textBody", 0.5 * hover.a)
      g:drawLine(hover.x, 0, hover.x, ph, 1)
      g:setColour("textTitle", hover.a)
      g:fillEllipse(hover.x - 3, y - 3, 6, 6)
      local label = (f >= 1000 and string.format("%.2f kHz", f / 1000) or string.format("%d Hz", floor(f))) .. string.format("  %+.1f dB", db)
      local lx0 = hover.x + 8 > w - 120 and hover.x - 128 or hover.x + 8
      g:setColour("windowBg", 0.85 * hover.a); g:fillRoundedRect(lx0, 22, 120, 16, 2)
      g:setColour("textTitle", hover.a); g:drawText(label, lx0 + 4, 22, 116, 16, "left", 9.5, true)
    end
  end,
  mouse = function(event, x, y, wheel)
    local px, pw, ph = 28, rw - 34, plotH - 16
    if (event == "down" or event == "drag") and currentFamily() ~= 4 then
      params.set("cpCutoff", lx.clamp((x - px) / pw, 0, 1) * 999)
      params.set("cpResonance", lx.clamp(1 - y / ph, 0, 1) * 999)
    elseif event == "wheel" then
      params.set("cpResonance", lx.clamp(params.get("cpResonance") + (wheel > 0 and 20 or -20), 0, 999))
    end
    if event == "move" or event == "drag" or event == "enter" then
      hover.x = x
      if hover.a < 1 then lx.tween(hover, "a", 1, 0.15, nil, function() response:repaint() end) end
      response:repaint()
    elseif event == "exit" then
      lx.tween(hover, "a", 0, 0.3, nil, function() response:repaint() end)
    end
  end,
}

-- ---- live spectrum: Hann window + radix-2 FFT, written in Lua
local N = 1024
local hann, re, im = {}, {}, {}
for i = 0, N - 1 do hann[i] = 0.5 - 0.5 * math.cos(2 * math.pi * i / (N - 1)) end
local bitrev = {}
do
  local bits = 10
  for i = 0, N - 1 do
    local r, x = 0, i
    for _ = 1, bits do r = r * 2 + x % 2; x = x // 2 end
    bitrev[i] = r
  end
end
local function fft()
  local size = 2
  while size <= N do
    local half = size // 2
    local step = -2 * math.pi / size
    for start = 0, N - 1, size do
      for k = 0, half - 1 do
        local a = step * k
        local wr, wi = math.cos(a), math.sin(a)
        local i, j = start + k, start + k + half
        local tr = wr * re[j] - wi * im[j]
        local ti = wr * im[j] + wi * re[j]
        re[j], im[j] = re[i] - tr, im[i] - ti
        re[i], im[i] = re[i] + tr, im[i] + ti
      end
    end
    size = size * 2
  end
end
local COLUMNS = 96
for i = 1, COLUMNS do spectrum[i] = DB_BOTTOM - 1 end
local quiet = true

local function updateSpectrum(dt)
  local samples = synth.scope(N)
  for i = 0, N - 1 do re[bitrev[i]] = samples[i + 1] * hann[i]; im[bitrev[i]] = 0 end
  fft()
  local rate = 48000
  local changed = false
  for c = 1, COLUMNS do
    local f0 = 20 * 1000 ^ ((c - 1) / COLUMNS)
    local f1 = 20 * 1000 ^ (c / COLUMNS)
    local b0 = max(1, floor(f0 / rate * N))
    local b1 = max(b0, min(N // 2 - 1, floor(f1 / rate * N)))
    local peak = 0
    for b = b0, b1 do peak = max(peak, re[b] * re[b] + im[b] * im[b]) end
    local db = 10 * log10(peak + 1e-12) - 30      -- -30: window and size normalisation, by eye
    local old = spectrum[c]
    local new = db > old and db or max(db, old - 60 * dt)   -- falls at 60 dB/s
    if math.abs(new - old) > 0.05 then changed = true end
    spectrum[c] = new
  end
  if changed or not quiet then response:repaint() end
  quiet = not changed
end

local function setSpectrum(on)
  spectrumOn = on
  if on then lx.loop("spectrum", function(dt) updateSpectrum(dt) end, 30) else lx.stop("spectrum") end
  response:repaint()
end

-- ---- footer: spectrum switch, A/B snapshots with morph
local footY = ry + plotH + 6
lx.toggle{ id = "spectrumToggle", x = rx, y = footY, w = 160, h = 20, text = "LIVE SPECTRUM (LUA FFT)",
           get = function() return spectrumOn end, set = setSpectrum }

local FILTER_SET = { "cpCutoff", "cpResonance", "cpFilKbdAmt", "cpFilEnvAmt", "spFilterModel", "spFilterMode", "spSemModel" }
local snapA, snapB = nil, nil
local morphState = { t = 0, drag = false }
local morphBar
lx.button{ id = "storeA", x = rx + rw - 330, y = footY, w = 70, h = 20, text = "STORE A", size = 9.5,
           onClick = function() snapA = lx.snapshot(FILTER_SET); morphState.t = 0; morphBar:repaint() end,
           active = function() return snapA ~= nil end }
lx.button{ id = "storeB", x = rx + rw - 255, y = footY, w = 70, h = 20, text = "STORE B", size = 9.5,
           onClick = function() snapB = lx.snapshot(FILTER_SET); morphState.t = 1; morphBar:repaint() end,
           active = function() return snapB ~= nil end }
morphBar = ui.canvas{
  id = "morph", x = rx + rw - 180, y = footY, w = 180, h = 20,
  paint = function(g, w, h)
    g:setColour("buttonBg"); g:fillRoundedRect(0, 0, w, h, 2)
    local ready = snapA and snapB
    if ready then
      g:setGradient{ x1 = 0, y1 = 0, x2 = w, y2 = 0, stops = { { 0, lx.col("accentDark") }, { 1, lx.col("accent") } } }
      g:fillRoundedRect(1, 1, (w - 2) * morphState.t, h - 2, 2)
    end
    g:setColour("buttonBorder"); g:drawRoundedRect(0.5, 0.5, w - 1, h - 1, 2, 1)
    g:setColour(ready and "textTitle" or "textMuted")
    g:drawText(ready and string.format("MORPH A > B  %d %%", lx.round(morphState.t * 100)) or "MORPH: STORE A AND B",
               0, 0, w, h, "centre", 9.5, true)
  end,
  mouse = function(event, x)
    if not (snapA and snapB) then return end
    if event == "down" or event == "drag" then
      morphState.t = lx.clamp(x / 180, 0, 1)
      lx.morph(snapA, snapB, morphState.t)
      morphBar:repaint()
    elseif event == "double" then
      -- animated sweep A -> B -> A
      local sweep = { t = 0 }
      lx.tween(sweep, "t", 1, 2.5, lx.ease.inOutSine, function(v)
        morphState.t = v < 0.5 and v * 2 or 2 - v * 2
        lx.morph(snapA, snapB, morphState.t)
        morphBar:repaint()
      end)
    end
  end,
}

lx.watch({ "cpCutoff", "cpResonance", "spFilterModel", "spFilterMode", "spSemModel", "cpShelvesLsFreq", "cpShelvesLsGain",
           "cpShelvesP1Gain", "cpShelvesP2Freq", "cpShelvesP2Gain", "cpShelvesP2Q", "cpShelvesHsFreq", "cpShelvesHsGain" },
         retarget)
lx.watch({ "spFilterModel", "spSemModel", "spFilterMode" }, function() updateFamilyViews(); familySelect() end)

-- =========================================================== bottom row
local BOT_Y = TOP_H + GAP
local BOT_H = H - BOT_Y
local COL_W = 214

-- ---- TUNING & VOICES
local tx, ty, tw = lx.section{ id = "tuningCard", title = "TUNING & VOICES", badge = "GLOBAL", x = 0, y = BOT_Y, w = COL_W, h = BOT_H }
lx.subhead{ x = tx, y = ty, w = tw, text = "PITCH" }
lx.dial{ param = "cpMasterTune", caption = "MASTER TUNE", x = tx + 8, y = ty + 14, size = 36, w = 84 }
lx.dial{ param = "cpGlide", caption = "GLIDE", x = tx + 104, y = ty + 14, size = 36, w = 84 }
lx.subhead{ x = tx, y = ty + 84, w = tw, text = "QUANTIZE" }
lx.choice{ id = "quantize", param = "spChromaticPitch", items = { "FREE", "SEMITONES", "OCTAVES" }, x = tx, y = ty + 99, w = tw, h = 20, size = 9.5 }
lx.subhead{ x = tx, y = ty + 125, w = tw, text = "UNISON" }
lx.dial{ param = "cpUnisonDetune", caption = "SPREAD", x = tx + 8, y = ty + 139, size = 36, w = 84 }
lx.toggle{ id = "unison", param = "spUnison", text = "UNISON", x = tx + 110, y = ty + 150, w = 90 }
lx.subhead{ x = tx, y = ty + 207, w = tw, text = "VOICES" }
lx.choice{ id = "voiceCount", param = "spVoiceCount", items = { "1", "2", "3", "4", "5", "6" }, x = tx, y = ty + 222, w = tw, h = 20, size = 9.5 }
lx.choice{ id = "priority", param = "spAssignerPriority", items = { "LAST", "LOW", "HIGH" }, x = tx, y = ty + 246, w = tw, h = 20, size = 9.5 }

-- ---- OUTPUT & BUS
local ox, oy, ow = lx.section{ id = "outputCard", title = "OUTPUT & BUS", badge = "MASTER", x = COL_W + GAP, y = BOT_Y, w = COL_W, h = BOT_H }
lx.subhead{ x = ox, y = oy, w = ow, text = "VCA" }
lx.dial{ param = "cpAmpLevel", caption = "LEVEL", x = ox + (ow - 90) / 2, y = oy + 14, size = 48, w = 90 }
lx.subhead{ x = ox, y = oy + 96, w = ow, text = "CONSOLEX BUS" }
lx.dial{ param = "cpMackityInTrim", caption = "DRIVE", x = ox + 8, y = oy + 110, size = 36, w = 84 }
lx.dial{ param = "cpConsoleDiscontinuity", caption = "AIR", x = ox + 104, y = oy + 110, size = 36, w = 84 }
lx.subhead{ x = ox, y = oy + 182, w = ow, text = "MACKITY" }
lx.dial{ param = "cpMackityDrive", caption = "DRIVE", x = ox + 8, y = oy + 196, size = 36, w = 84 }
lx.dial{ param = "cpMackitySend", caption = "SEND", x = ox + 104, y = oy + 196, size = 36, w = 84 }

-- ---- VOICE CONSOLE MIXER: one canvas for six strips and the master
local mxX = 2 * (COL_W + GAP)
local mx, my, mw, mh = lx.section{ id = "mixerCard", title = "VOICE CONSOLE MIXER", badge = "METERS + FADERS IN ONE CANVAS",
                                   x = mxX, y = BOT_Y, w = W - mxX, h = BOT_H }
local STRIPS = 7
local stripW = (mw - (STRIPS - 1) * 5) / STRIPS
local level, peak, peakHold = {}, {}, {}
for v = 1, 7 do level[v], peak[v], peakHold[v] = -60, -60, 0 end
local drag = nil
local mixer

local function dbToMeter(db) return lx.clamp((db + 48) / 60, 0, 1) end   -- -48 .. +12 dB
local FADER_TOP, PAN_Y = 52, 26

mixer = ui.canvas{
  id = "mixer", x = mx, y = my, w = mw, h = mh - 26,
  paint = function(g, w, h)
    local faderH = h - FADER_TOP - 22
    for s = 1, STRIPS do
      local x = (s - 1) * (stripW + 5)
      local master = s == STRIPS
      g:setColour(lx.shade("cardBg", -0.2)); g:fillRoundedRect(x, 0, stripW, h, 3)
      g:setColour("cardBorder"); g:drawRoundedRect(x + 0.5, 0.5, stripW - 1, h - 1, 3, 1)
      g:setColour("textBody")
      g:drawText(master and "MASTER" or ("VOICE " .. s), x + 6, 3, stripW - 12, 14, "left", 9.5, true)
      -- pan
      if not master then
        local pan = synth.pan(s)
        local cx = x + stripW / 2
        g:setColour("knobTrack"); g:fillRoundedRect(x + 8, PAN_Y + 6, stripW - 16, 4, 2)
        local px = cx + pan * (stripW / 2 - 10)
        g:setColour("accent"); g:fillRect(min(cx, px), PAN_Y + 6, math.abs(px - cx), 4)
        g:setColour("knobNeedle"); g:fillEllipse(px - 4, PAN_Y + 4, 8, 8)
        g:setColour("textMuted")
        g:drawText(pan < -0.01 and string.format("L%d", lx.round(-pan * 100)) or (pan > 0.01 and string.format("R%d", lx.round(pan * 100)) or "C"),
                   x, PAN_Y + 12, stripW, 12, "centre", 8.5, true)
      end
      -- meter: gradient bar with peak hold
      local mxx = x + 8
      local mh2 = faderH
      g:setColour(lx.shade("windowBg", -0.3)); g:fillRect(mxx, FADER_TOP, 8, mh2)
      local m = dbToMeter(level[s])
      if m > 0 then
        g:setGradient{ x1 = 0, y1 = FADER_TOP + mh2, x2 = 0, y2 = FADER_TOP,
                       stops = { { 0, lx.col("accentDark") }, { 0.7, lx.col("accent") }, { 0.85, "#e8c547" }, { 1, "#e57373" } } }
        g:fillRect(mxx, FADER_TOP + mh2 * (1 - m), 8, mh2 * m)
      end
      local pk = dbToMeter(peak[s])
      if pk > 0 then
        g:setColour(peak[s] > 0 and "#e57373" or "textTitle", 0.9)
        g:fillRect(mxx, FADER_TOP + mh2 * (1 - pk) - 1, 8, 2)
      end
      -- scale
      g:setColour("textMuted")
      for _, db in ipairs({ 12, 6, 0, -12, -24, -48 }) do
        local yy = FADER_TOP + mh2 * (1 - dbToMeter(db))
        g:drawText((db > 0 and "+" or "") .. db, mxx + 9, yy - 5, 22, 10, "left", 7.5, false)
      end
      -- fader
      if not master then
        local fdb = synth.fader(s)
        local fx2 = x + stripW - 20
        g:setColour("knobTrack"); g:fillRoundedRect(fx2 + 5, FADER_TOP, 3, mh2, 1.5)
        local fy2 = FADER_TOP + mh2 * (1 - dbToMeter(max(-48, fdb)))
        g:setGradient{ x1 = 0, y1 = fy2 - 6, x2 = 0, y2 = fy2 + 6,
                       stops = { { 0, lx.shade("buttonBg", 0.35) }, { 1, lx.shade("buttonBg", -0.1) } } }
        g:fillRoundedRect(fx2 - 2, fy2 - 6, 17, 12, 2)
        g:setColour(drag and drag.strip == s and "accent" or "buttonBorder")
        g:drawRoundedRect(fx2 - 1.5, fy2 - 5.5, 16, 11, 2, 1)
        g:setColour("accent"); g:fillRect(fx2 + 1, fy2 - 0.5, 11, 1.5)
        g:setColour("textBody")
        g:drawText(fdb <= -60 and "-inf" or string.format("%+.1f dB", fdb), x, h - 18, stripW, 14, "centre", 9, true)
      else
        local muted = synth.mute()
        g:setColour(muted and "#e57373" or "buttonBg"); g:fillRoundedRect(x + 26, h - 40, stripW - 34, 16, 2)
        g:setColour(muted and "textTitle" or "textBody")
        g:drawText("MUTE", x + 26, h - 40, stripW - 34, 16, "centre", 9, true)
        g:setColour("textMuted")
        g:drawText("VOICE SUM", x, PAN_Y + 4, stripW, 12, "centre", 8.5, true)
      end
    end
  end,
  mouse = function(event, x, y, wheel)
    local s = floor(x / (stripW + 5)) + 1
    if s < 1 or s > STRIPS then return end
    local sx = (s - 1) * (stripW + 5)
    local h = mh - 26
    local faderH = h - FADER_TOP - 22
    if event == "down" then
      if s == STRIPS then
        if y > h - 42 then synth.mute(not synth.mute()); mixer:repaint() end
        return
      end
      if y >= PAN_Y and y < PAN_Y + 22 then drag = { strip = s, kind = "pan" }
      elseif y >= FADER_TOP - 8 then drag = { strip = s, kind = "fader" } end
    end
    if (event == "down" or event == "drag") and drag then
      if drag.kind == "pan" then
        synth.pan(drag.strip, lx.clamp((x - (drag.strip - 1) * (stripW + 5) - stripW / 2) / (stripW / 2 - 10), -1, 1))
      else
        local t = lx.clamp(1 - (y - FADER_TOP) / faderH, 0, 1)
        synth.fader(drag.strip, t <= 0 and -100 or t * 60 - 48)
      end
      mixer:repaint()
    elseif event == "up" then
      drag = nil
      mixer:repaint()
    elseif event == "double" and s < STRIPS then
      if y < FADER_TOP then synth.pan(s, 0) else synth.fader(s, 0) end
      mixer:repaint()
    elseif event == "wheel" and s < STRIPS then
      synth.fader(s, lx.clamp(synth.fader(s) + (wheel > 0 and 1 or -1), -48, 12))
      mixer:repaint()
    end
  end,
}
lx.toggle{ id = "pad", param = "spMackityReturnPad", text = "MACKITY PAD -6 DB", x = mx, y = my + mh - 22, w = 160 }
lx.text{ id = "mixerHint", x = mx + 170, y = my + mh - 22, w = mw - 170, h = 18, size = 8.5, colour = "textMuted", align = "right",
         text = "DRAG FADERS AND PANS, DOUBLE CLICK RESETS, WHEEL 1 DB" }

-- meters at 30 Hz; the canvas repaints only while something moves
local still = 0
lx.loop("meters", function(dt)
  local v = synth.voices()
  local sum = 0
  local moving = false
  for s = 1, 7 do
    local lin = s < 7 and v[s] or min(1, sum)
    if s < 7 then sum = sum + v[s] * 0.5 end
    local db = lin > 0.0001 and 20 * log10(lin) or -60
    local old = level[s]
    level[s] = db > old and db or max(-60, old - 30 * dt)        -- 30 dB/s release
    if db >= peak[s] then peak[s], peakHold[s] = db, 1.2
    else
      peakHold[s] = peakHold[s] - dt
      if peakHold[s] <= 0 then peak[s] = max(-60, peak[s] - 20 * dt) end
    end
    if level[s] > -59 or peak[s] > -59 or old ~= level[s] then moving = true end
  end
  if moving then still = 0; mixer:repaint()
  elseif still < 2 then still = still + 1; mixer:repaint() end
end, 30)

updateFamilyViews()
setSpectrum(true)

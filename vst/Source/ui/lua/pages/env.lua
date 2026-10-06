-- ENV in Lua: the native tab's three envelopes (FILTER / AMP / WAVEMOD) with
-- their A/D/S/R and velocity dials, the 4-way curve type (exp / lin, fast /
-- slow x4) and LOOP, plus what Lua adds:
--   * the curves are computed from the real stage times (synth.envMs, slow x4
--     included) on a time axis that switches between linear and logarithmic,
--     with time grid labels; every change morphs the curve into its new shape;
--   * draggable nodes (attack, decay/sustain, release), wheel on a stage,
--     double click resets, hover reads the time and level under the cursor;
--   * LOOP draws the repeating attack/decay cycles while the note is held,
--     velocity draws the softest note's curve beside the full one;
--   * HOLD GATE: hold it and a dot runs through the envelope in real time and
--     shows the level; letting go plays the release. The animation stops as
--     soon as nothing moves.
-- The curve shapes are drawings of exponential / linear segments, not
-- measurements of the engine's envelopes.

local W, H = ui.size()
local GAP = 6
local TOP_H = 120
local FOOT_H = 44
local CURVE_Y = TOP_H + GAP
local CURVE_H = H - CURVE_Y - FOOT_H - GAP
local COL_W = (W - 2 * GAP) // 3
local floor, max, min, abs, exp, ln = math.floor, math.max, math.min, math.abs, math.exp, math.log

local ENVS = {
  { key = "Fil", title = "FILTER ENV", badge = "VCF", curve = "FILTER ADSR CURVE", vel = "cpFilVelocity", defaults = { 0, 500, 500, 500 } },
  { key = "Amp", title = "AMP ENV", badge = "VCA", curve = "AMPLIFIER / VCA ADSR CURVE", vel = "cpAmpVelocity", defaults = { 0, 0, 999, 500 } },
  { key = "WMod", title = "WAVEMOD ENV", badge = "SHAPER", curve = "WAVEMOD ADSR CURVE", vel = "cpWModVelocity", defaults = { 0, 500, 500, 500 } },
}
local TYPES = { "EXP  FAST", "EXP  SLOW X4", "LIN  FAST", "LIN  SLOW X4" }
local TYPE_GLYPHS = { "expFast", "expSlow", "linFast", "linSlow" }

local function fmtMs(ms)
  if ms >= 1000 then return string.format("%.2f s", ms / 1000) end
  if ms >= 100 then return string.format("%d ms", floor(ms + 0.5)) end
  if ms >= 10 then return string.format("%.1f ms", ms) end
  return string.format("%.1f ms", ms)
end
local function stageMs(pot, slow)
  local ok, ms = pcall(synth.envMs, pot, slow)
  return (ok and tonumber(ms)) and max(ms, 0.01) or 0.01
end

-- =========================================================== curve shapes
-- c = 0 linear, 1 exponential; u = 0..1 through the stage
local KA, KD = 2.4, 4.5
local EA, ED = 1 - exp(-KA), 1 - exp(-KD)
local function rise(u, c) return (1 - c) * u + c * (1 - exp(-KA * u)) / EA end
local function fall(u, c) return (1 - c) * (1 - u) + c * (exp(-KD * u) - exp(-KD)) / ED end

-- Level of the envelope at display time t, for the shown values sh
local function levelAt(sh, t)
  local a, d, s, c = sh.a, sh.d, sh.s, sh.c
  if t <= a then return rise(t / a, c) end
  if t <= a + d then return s + (1 - s) * fall((t - a) / d, c) end
  local hold = sh.T - a - d - sh.r
  if t <= a + d + hold then
    if sh.loop > 0.01 then
      local ph = (t - a - d) % (a + d)
      local v = ph < a and s + (1 - s) * rise(ph / a, c) or s + (1 - s) * fall((ph - a) / d, c)
      return s + (v - s) * sh.loop
    end
    return s
  end
  return s * fall(lx.clamp((t - a - d - hold) / sh.r, 0, 1), c)
end

local function targetsFor(e)
  local slow = params.get("sp" .. e.key .. "EnvSlow") ~= 0
  local tg = {
    a = stageMs(params.get("cp" .. e.key .. "Att"), slow),
    d = stageMs(params.get("cp" .. e.key .. "Dec"), slow),
    s = params.get("cp" .. e.key .. "Sus") / 999,
    r = stageMs(params.get("cp" .. e.key .. "Rel"), slow),
    c = params.get("sp" .. e.key .. "EnvLin") ~= 0 and 0 or 1,
    loop = params.get("sp" .. e.key .. "EnvLoop") ~= 0 and 1 or 0,
    vel = params.get(e.vel) / 999,
  }
  local hold = max(0.25 * (tg.a + tg.d + tg.r), 0.5)
  if tg.loop > 0 then hold = max(hold, 2.5 * (tg.a + tg.d)) end
  tg.T = tg.a + tg.d + hold + tg.r
  return tg
end
local KEYS = { "a", "d", "s", "r", "c", "loop", "vel", "T" }

-- =========================================================== one column
local function buildColumn(e, x)
  local P = function(stage) return "cp" .. e.key .. stage end
  local slowId, linId, loopId = "sp" .. e.key .. "EnvSlow", "sp" .. e.key .. "EnvLin", "sp" .. e.key .. "EnvLoop"

  -- ---- dials card
  local ix, iy, iw = lx.section{ id = e.key .. "Card", title = e.title, badge = e.badge, x = x, y = 0, w = COL_W, h = TOP_H }
  local velW = 74
  local adsrW = iw - velW - 10
  lx.subhead{ id = e.key .. "AdsrHead", x = ix, y = iy, w = adsrW, text = "ADSR" }
  lx.subhead{ id = e.key .. "VelHead", x = ix + adsrW + 10, y = iy, w = velW, text = "VELOCITY" }
  ui.canvas{ id = e.key .. "Rule", x = ix + adsrW + 4, y = iy - 2, w = 1, h = TOP_H - 34, paint = function(g, w, h)
    g:setColour("cardBorder", 0.9); g:fillRect(0, 0, 1, h)
  end }
  local slot = adsrW / 4
  local stages = { { "Att", "ATTACK", true }, { "Dec", "DECAY", true }, { "Sus", "SUSTAIN", false }, { "Rel", "RELEASE", true } }
  for i, st in ipairs(stages) do
    local id = P(st[1])
    lx.dial{ param = id, caption = st[2], size = 36, w = floor(slot), x = floor(ix + (i - 1) * slot), y = iy + 14,
             default = e.defaults[i],
             format = st[3] and function(v) return fmtMs(stageMs(v, params.get(slowId) ~= 0)) end or nil,
             watch = st[3] and slowId or nil }
  end
  lx.dial{ param = e.vel, caption = "SENSITIVITY", size = 36, w = velW, x = ix + adsrW + 10, y = iy + 14, default = 0 }

  -- ---- curve card
  local cx, cy, cw, ch = lx.section{ id = e.key .. "CurveCard", title = e.curve, badge = "LUA: REAL TIME",
                                     x = x, y = CURVE_Y, w = COL_W, h = CURVE_H }
  local env = { k = 1, logk = 0, hover = { x = -1, y = -1, a = 0 }, drag = nil }
  env.target = targetsFor(e)
  env.from, env.shown = {}, {}
  for _, k in ipairs(KEYS) do env.from[k], env.shown[k] = env.target[k], env.target[k] end
  local view
  local PL, PR, PT, PB = 8, 8, 22, 18   -- plot insets

  local function blend()
    for _, k in ipairs(KEYS) do env.shown[k] = env.from[k] + (env.target[k] - env.from[k]) * env.k end
  end
  local function retarget()
    for _, k in ipairs(KEYS) do env.from[k] = env.shown[k] end
    env.target = targetsFor(e)
    env.k = 0
    lx.tween(env, "k", 1, 0.4, lx.ease.outCubic, function() blend(); view:repaint() end)
  end

  -- time <-> x: linear and logarithmic maps, blended by logk
  local function X(t, pw)
    local T = env.shown.T
    local tau = max(T / 400, 0.02)
    local lin = t / T
    local lg = ln(1 + t / tau) / ln(1 + T / tau)
    return PL + pw * (lin + (lg - lin) * env.logk)
  end
  local function tAt(x, pw)
    local lo, hi = 0, env.shown.T
    for _ = 1, 22 do
      local mid = (lo + hi) / 2
      if X(mid, pw) < x then lo = mid else hi = mid end
    end
    return (lo + hi) / 2
  end
  local function nodes(pw, ph)
    local sh = env.shown
    local hold = sh.T - sh.a - sh.d - sh.r
    local yb = PT + ph
    return {
      { X(sh.a, pw), PT, "A" },
      { X(sh.a + sh.d, pw), yb - sh.s * ph, "D" },
      { X(sh.a + sh.d + hold, pw), yb - sh.s * ph, "S" },
      { X(sh.T, pw), yb, "R" },
    }
  end

  -- ---- gate / play state
  local play = { on = false, t = 0, rel = -1, relLevel = 0, level = 0, trail = {} }
  local gatePad
  local function playLevel(tg)
    local a, d, s, c = tg.a, tg.d, tg.s, tg.c
    if play.rel >= 0 then return play.relLevel * fall(lx.clamp(play.rel / tg.r, 0, 1), c) end
    local t = play.t
    if t <= a then return rise(t / a, c) end
    if t <= a + d then return s + (1 - s) * fall((t - a) / d, c) end
    if tg.loop > 0 then
      local ph = (t - a - d) % (a + d)
      return ph < a and s + (1 - s) * rise(ph / a, c) or s + (1 - s) * fall((ph - a) / d, c)
    end
    return s
  end
  -- where the dot is drawn, as display time
  local function playTime(tg)
    local a, d = tg.a, tg.d
    local hold = tg.T - a - d - tg.r
    if play.rel >= 0 then return a + d + hold + min(play.rel, tg.r) end
    local t = play.t
    if t <= a + d then return t end
    if tg.loop > 0 then return a + d + (t - a - d) % (a + d) end
    return a + d + min(t - a - d, hold)
  end
  local function playStep(dt)
    local tg = env.target
    local ms = dt * 1000
    if play.rel >= 0 then play.rel = play.rel + ms else play.t = play.t + ms end
    play.level = playLevel(tg)
    local tr = play.trail
    tr[#tr + 1] = playTime(tg)
    tr[#tr + 1] = play.level
    if #tr > 48 then table.remove(tr, 1); table.remove(tr, 1) end
    view:repaint(); gatePad:repaint()
    -- done: release over, or sustaining without loop at the end of the hold
    if play.rel >= tg.r then play.rel = -1; play.t = 0; play.done = true; play.trail = {}; view:repaint(); gatePad:repaint(); return false end
    if not play.on then return end
    if play.rel < 0 and tg.loop == 0 and play.t > tg.T - tg.r then return false end
  end
  local function gate(on)
    if on then
      play.on, play.t, play.rel, play.done, play.trail = true, 0, -1, false, {}
    else
      if not play.on then return end
      play.on = false
      play.relLevel = playLevel(env.target)
      play.rel = 0
    end
    lx.loop("gate" .. e.key, playStep, 60)
  end

  view = ui.canvas{ id = e.key .. "Curve", x = cx, y = cy, w = cw, h = ch,
    paint = function(g, w, h)
      lx.panel(g, w, h)
      local sh = env.shown
      local pw, ph = w - PL - PR, h - PT - PB
      local yb = PT + ph
      -- grid: half level and time ticks
      g:setColour("visualizerGrid")
      g:drawLine(PL, floor(PT + ph / 2) + 0.5, PL + pw, floor(PT + ph / 2) + 0.5, 1)
      g:drawLine(PL, PT + 0.5, PL + pw, PT + 0.5, 1)
      local ticks = {}
      if env.logk > 0.5 then
        local tk = 0.1
        while tk < sh.T do ticks[#ticks + 1] = tk; tk = tk * 10 end
      else
        local raw = sh.T / 4
        local p = 10 ^ floor(ln(raw, 10))
        local step = raw / p < 2 and p or (raw / p < 5 and 2 * p or 5 * p)
        local tk = step
        while tk < sh.T and #ticks < 8 do ticks[#ticks + 1] = tk; tk = tk + step end
      end
      for _, tk in ipairs(ticks) do
        local tx = floor(X(tk, pw)) + 0.5
        g:setColour("visualizerGrid"); g:drawLine(tx, PT, tx, yb, 1)
        g:setColour("textMuted", 0.8); g:drawText(fmtMs(tk), tx + 3, yb - 13, 60, 12, "left", 8, false)
      end
      -- the curve, sampled every 2 px plus the nodes
      local nd = nodes(pw, ph)
      local pts, ghost = {}, {}
      local ni = 1
      local step = 2
      local px = PL
      while px <= PL + pw + 0.01 do
        while ni <= 4 and nd[ni][1] <= px do
          local v = ni == 1 and 1 or (ni == 4 and 0 or sh.s)
          pts[#pts + 1] = nd[ni][1]; pts[#pts + 1] = yb - v * ph
          ghost[#ghost + 1] = nd[ni][1]; ghost[#ghost + 1] = yb - v * (1 - sh.vel) * ph
          ni = ni + 1
        end
        local v = levelAt(sh, tAt(px, pw))
        pts[#pts + 1] = px; pts[#pts + 1] = yb - v * ph
        ghost[#ghost + 1] = px; ghost[#ghost + 1] = yb - v * (1 - sh.vel) * ph
        px = px + step
      end
      pts[#pts + 1] = PL + pw; pts[#pts + 1] = yb
      ghost[#ghost + 1] = PL + pw; ghost[#ghost + 1] = yb
      g:save(); g:clip(0, 0, w, h)
      if sh.vel > 0.01 then
        g:beginPath(); g:moveTo(ghost[1], ghost[2])
        for i = 3, #ghost, 2 do g:lineTo(ghost[i], ghost[i + 1]) end
        g:setColour("textMuted", 0.6); g:stroke(1)
      end
      lx.area(g, pts, yb, "visualizerCurve", 0.28, 0.02, 2)
      g:restore()
      -- nodes (A, D/S, R draggable; S marks the release start)
      for i, n in ipairs(nd) do
        local active = env.drag and env.drag.node == i
        if i == 3 then
          g:setColour("accent", 0.6); g:drawEllipse(n[1] - 3, n[2] - 3, 6, 6, 1)
        else
          g:setColour(active and "accent" or "accentDark"); g:fillRect(n[1] - 4.5, n[2] - 4.5, 9, 9)
          g:setColour(active and "textTitle" or "accent"); g:drawRect(n[1] - 4.5, n[2] - 4.5, 9, 9, 1.2)
        end
      end
      -- stage letters
      -- stage letters, merged where stages are too short to label apart ("AD")
      g:setColour("textMuted")
      local prev, text, at = PL, "", nil
      for i, n in ipairs(nd) do
        local mid = (prev + n[1]) / 2
        if at and mid - at < 12 then
          text = text .. ({ "A", "D", "S", "R" })[i]
        else
          if at then g:drawText(text, at - 15, h - 16, 30, 14, "centre", 9.5, true) end
          text, at = ({ "A", "D", "S", "R" })[i], mid
        end
        prev = n[1]
      end
      g:drawText(text, at - 15, h - 16, 30, 14, "centre", 9.5, true)
      -- header: axis switch and summary
      g:setColour(env.logk < 0.5 and "accent" or "textMuted"); g:drawText("LIN", 8, 3, 24, 14, "left", 9, true)
      g:setColour("textMuted"); g:drawText("|", 30, 3, 8, 14, "left", 9, true)
      g:setColour(env.logk >= 0.5 and "accent" or "textMuted"); g:drawText("LOG", 38, 3, 26, 14, "left", 9, true)
      g:setColour("accent")
      local tg = env.target
      g:drawText(string.format("A %s  D %s  S %d %%  R %s%s", fmtMs(tg.a), fmtMs(tg.d), lx.round(tg.s * 100), fmtMs(tg.r),
                               tg.loop > 0 and "  LOOP" or ""), 70, 3, w - 78, 14, "right", 9, true)
      -- the playing dot with its trail
      if play.on or play.rel >= 0 then
        local tr = play.trail
        local cnt = #tr // 2
        for i = 1, cnt do
          local a = i / cnt
          g:setColour("accent", 0.5 * a)
          local tx, ty = X(tr[2 * i - 1], pw), yb - tr[2 * i] * ph
          g:fillEllipse(tx - 1.5 - a, ty - 1.5 - a, 3 + 2 * a, 3 + 2 * a)
        end
        local dx, dy = X(playTime(env.target), pw), yb - play.level * ph
        g:setColour("accent", 0.3); g:fillEllipse(dx - 10, dy - 10, 20, 20)
        g:setColour("textTitle"); g:fillEllipse(dx - 4.5, dy - 4.5, 9, 9)
        g:setColour("textTitle")
        local lbl = lx.round(play.level * 100) .. " %"
        g:drawText(lbl, dx + 8 > w - 50 and dx - 52 or dx + 8, dy - 18, 44, 14, dx + 8 > w - 50 and "right" or "left", 9.5, true)
      end
      -- hover readout
      local hv = env.hover
      if hv.a > 0.01 and hv.x >= PL then
        local label
        local best, bd = nil, 14
        for i, n in ipairs(nd) do
          local dd = math.sqrt((n[1] - hv.x) ^ 2 + (n[2] - hv.y) ^ 2)
          if i ~= 3 and dd < bd then best, bd = i, dd end
        end
        if best == 1 then label = "ATTACK  " .. fmtMs(tg.a)
        elseif best == 2 then label = string.format("DECAY  %s  SUSTAIN %d %%", fmtMs(tg.d), lx.round(tg.s * 100))
        elseif best == 4 then label = "RELEASE  " .. fmtMs(tg.r)
        else
          local t = tAt(lx.clamp(hv.x, PL, PL + pw), pw)
          local v = levelAt(sh, t)
          local hold = sh.T - sh.a - sh.d - sh.r
          local stage = t <= sh.a and "A" or (t <= sh.a + sh.d and "D" or (t <= sh.a + sh.d + hold and "S" or "R"))
          local tt = stage == "R" and ("REL +" .. fmtMs(t - sh.a - sh.d - hold)) or (stage == "S" and "HELD" or fmtMs(t))
          label = string.format("%s  %s  %d %%", stage, tt, lx.round(v * 100))
          g:setColour("textBody", 0.5 * hv.a); g:drawLine(hv.x, PT, hv.x, yb, 1)
          g:setColour("textTitle", hv.a); g:fillEllipse(hv.x - 3, yb - v * ph - 3, 6, 6)
        end
        local bw = g:textWidth(label, 9.5, true) + 10
        local bx = hv.x + 10 + bw > w - 4 and hv.x - 10 - bw or hv.x + 10
        g:setColour("windowBg", 0.88 * hv.a); g:fillRoundedRect(bx, PT + 4, bw, 16, 2)
        g:setColour("textTitle", hv.a); g:drawText(label, bx + 5, PT + 4, bw - 6, 16, "left", 9.5, true)
      end
    end,
    mouse = function(event, mx, my, wheel)
      local pw, ph = cw - PL - PR, ch - PT - PB
      if event == "down" then
        if my < PT - 2 and mx < 66 then
          lx.tween(env, "logk", env.logk < 0.5 and 1 or 0, 0.45, lx.ease.inOutCubic, function() view:repaint() end)
          return
        end
        local nd = nodes(pw, ph)
        local best, bd = 1, 1e9
        for i, n in ipairs(nd) do
          local dd = math.sqrt((n[1] - mx) ^ 2 + (n[2] - my) ^ 2)
          if i ~= 3 and dd < bd then best, bd = i, dd end
        end
        env.drag = { node = best, x = mx, att = params.get(P("Att")), dec = params.get(P("Dec")), rel = params.get(P("Rel")) }
        view:repaint()
      elseif event == "drag" and env.drag then
        local dr = env.drag
        local dpot = (mx - dr.x) * 999 / (pw * 0.6)
        if dr.node == 1 then params.set(P("Att"), lx.clamp(dr.att + dpot, 0, 999))
        elseif dr.node == 2 then
          params.set(P("Dec"), lx.clamp(dr.dec + dpot, 0, 999))
          params.set(P("Sus"), lx.clamp((PT + ph - my) / ph, 0, 1) * 999)
        else params.set(P("Rel"), lx.clamp(dr.rel + dpot, 0, 999)) end
      elseif event == "up" then
        env.drag = nil
        view:repaint()
      elseif event == "double" then
        for i, st in ipairs({ "Att", "Dec", "Sus", "Rel" }) do params.set(P(st), e.defaults[i]) end
      elseif event == "wheel" then
        local sh = env.shown
        local t = tAt(lx.clamp(mx, PL, PL + pw), pw)
        local hold = sh.T - sh.a - sh.d - sh.r
        local id = t <= sh.a and P("Att") or (t <= sh.a + sh.d and P("Dec") or (t <= sh.a + sh.d + hold and P("Sus") or P("Rel")))
        params.set(id, lx.clamp(params.get(id) + (wheel > 0 and 10 or -10), 0, 999))
      end
      if event == "move" or event == "drag" or event == "enter" then
        env.hover.x, env.hover.y = mx, my
        if env.hover.a < 1 then lx.tween(env.hover, "a", 1, 0.15, nil, function() view:repaint() end) end
        view:repaint()
      elseif event == "exit" then
        lx.tween(env.hover, "a", 0, 0.3, nil, function() view:repaint() end)
      end
    end }

  -- ---- footer: curve type, loop, gate
  local fy = H - FOOT_H
  local typeW = floor((COL_W - 8) * 2 / 3)
  local _, selectType = lx.choice{ id = e.key .. "Type", items = TYPES, glyphs = TYPE_GLYPHS, cols = 2, size = 10,
    x = x, y = fy, w = typeW, h = FOOT_H, gap = 4,
    get = function() return (params.get(linId) ~= 0 and 2 or 0) + (params.get(slowId) ~= 0 and 1 or 0) + 1 end,
    set = function(i)
      params.set(slowId, (i - 1) % 2)
      params.set(linId, (i - 1) // 2)
    end }
  lx.watch({ linId, slowId }, selectType)
  local rx, rw = x + typeW + 10, COL_W - typeW - 10
  lx.toggle{ id = e.key .. "Loop", param = loopId, text = "LOOP ENVELOPE", x = rx, y = fy, w = rw, h = 20 }
  local padState = { hover = 0 }
  gatePad = ui.canvas{ id = e.key .. "Gate", x = rx, y = fy + 24, w = rw, h = 20,
    paint = function(g, w, h)
      local active = play.on or play.rel >= 0
      g:setColour(lx.shade("buttonBg", 0.08 * padState.hover)); g:fillRoundedRect(0, 0, w, h, 2)
      if active then
        g:setGradient{ x1 = 0, y1 = 0, x2 = w, y2 = 0, stops = { { 0, lx.col("accentDark", 0.8) }, { 1, lx.col("accent", 0.9) } } }
        g:fillRoundedRect(1, 1, (w - 2) * lx.clamp(play.level, 0, 1), h - 2, 2)
      end
      g:setColour(play.on and "accent" or "buttonBorder", play.on and 1 or 0.7 + 0.3 * padState.hover)
      g:drawRoundedRect(0.5, 0.5, w - 1, h - 1, 2, 1)
      g:setColour(active and "textTitle" or "textBody")
      g:drawText(play.on and "GATE ON" or (play.rel >= 0 and "RELEASE" or "HOLD: GATE"), 0, 0, w, h, "centre", 9.5, true)
    end,
    mouse = function(event)
      if event == "down" then gate(true)
      elseif event == "up" or event == "exit" then
        gate(false)
        if event == "exit" then lx.tween(padState, "hover", 0, 0.25, nil, function() gatePad:repaint() end) end
      elseif event == "enter" then
        lx.tween(padState, "hover", 1, 0.12, nil, function() gatePad:repaint() end)
      end
    end }

  lx.watch({ P("Att"), P("Dec"), P("Sus"), P("Rel"), e.vel, slowId, linId, loopId }, retarget)
end

for i, e in ipairs(ENVS) do buildColumn(e, (i - 1) * (COL_W + GAP)) end


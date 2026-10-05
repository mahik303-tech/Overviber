-- Overviber Lua skin: the built-in example of the LUA tab.
--
-- Copy it with SCRIPT FOLDER (creates Documents/Overviber/LUA/skin.lua) and
-- edit that file; the tab reloads it when it is saved. The API is described
-- in doc/LUA_SKINS.md. The curves are drawings, not models of the DSP.

local W, H = ui.size()
local GAP = 10
local colW = math.floor((W - GAP) / 2)
local topH = math.floor((H - GAP) * 0.58)
local botY = topH + GAP
local botH = H - botY

-- --------------------------------------------------------------- helpers
local function norm(id) return params.get(id) / 999 end

local function cutoffHz(v) return 20 * 10 ^ (3 * v) end      -- as the knob's "cutoff" mode

local function grid(g, w, h, cols, rows)
  g:setColour("visualizerGrid")
  for i = 1, cols - 1 do g:drawLine(w * i / cols, 0, w * i / cols, h, 1) end
  for i = 1, rows - 1 do g:drawLine(0, h * i / rows, w, h * i / rows, 1) end
end

-- --------------------------------------------------------------- filter
ui.card{ id = "filterCard", title = "FILTER RESPONSE", badge = "LUA", x = 0, y = 0, w = colW, h = topH }

local filterCanvas
filterCanvas = ui.canvas{
  id = "filterCurve", x = 10, y = 35, w = colW - 20, h = topH - 125,
  paint = function(g, w, h)
    g:fillAll("cardBg")
    grid(g, w, h, 10, 4)
    local fc = norm("cpCutoff")
    local res = norm("cpResonance")
    -- 24 dB lowpass with a resonance peak, over 20 Hz .. 20 kHz
    local function gain(x)
      local d = (x - fc) * 10
      local peak = res * 15 * math.exp(-(d * d) * (1 + res * 6))
      return (d > 0 and -d * 24 or 0) + peak
    end
    local function y(db) return h * (18 - db) / 66 end
    g:beginPath()
    for i = 0, 200 do
      local x = i / 200
      g:lineTo(x * w, math.max(0, math.min(h, y(gain(x)))))
    end
    g:lineTo(w, h); g:lineTo(0, h); g:closePath()
    g:setGradient{ x1 = 0, y1 = 0, x2 = 0, y2 = h,
                   stops = { { 0, theme.colour("accent", 0.45) }, { 1, theme.colour("accent", 0.02) } } }
    g:fill()
    g:beginPath()
    for i = 0, 200 do
      local x = i / 200
      g:lineTo(x * w, math.max(0, math.min(h, y(gain(x)))))
    end
    g:setColour("visualizerCurve")
    g:stroke(2)
    g:setColour("accent")
    g:fillRect(fc * w - 1, 0, 2, h)
    local hz = cutoffHz(fc)
    local text = hz >= 1000 and string.format("%.2f kHz", hz / 1000) or string.format("%d Hz", math.floor(hz))
    g:setColour("textBody")
    g:drawText(text .. "   RESO " .. params.getText("cpResonance"), 8, 4, w - 16, 16, "right", 11, true)
    g:setColour("textMuted")
    g:drawText("DRAG: CUTOFF / RESONANCE", 8, h - 18, w - 16, 16, "left", 9, true)
  end,
  -- Drag in the curve: x sets the cutoff, y the resonance.
  mouse = function(event, x, y)
    if event == "down" or event == "drag" then
      local _, _, w, h = filterCanvas:getBounds()
      params.set("cpCutoff", math.floor(999 * math.max(0, math.min(1, x / w))))
      params.set("cpResonance", math.floor(999 * math.max(0, math.min(1, 1 - y / h))))
    end
  end,
}

local knobY = topH - 85
ui.knob{ id = "cutoff", param = "cpCutoff", mode = "cutoff", caption = "CUTOFF", x = 30, y = knobY }
ui.knob{ id = "reso", param = "cpResonance", caption = "RESONANCE", x = 110, y = knobY }
ui.knob{ id = "envAmt", param = "cpFilEnvAmt", caption = "ENV AMOUNT", x = 190, y = knobY }
ui.knob{ id = "keyTrack", param = "cpFilKbdAmt", caption = "KEY TRACK", x = 270, y = knobY }

-- --------------------------------------------------------------- envelopes
local envX = colW + GAP
ui.card{ id = "envCard", title = "ENVELOPES", badge = "AMP + FILTER", x = envX, y = 0, w = colW, h = topH }

local function envPoints(prefix, w, h)
  local a = norm(prefix .. "Att") ^ 2
  local d = norm(prefix .. "Dec") ^ 2
  local s = norm(prefix .. "Sus")
  local r = norm(prefix .. "Rel") ^ 2
  local hold = 0.35
  local total = a + d + hold + r + 0.05
  local function px(t) return 6 + t / total * (w - 12) end
  local function py(l) return 8 + (1 - l) * (h - 16) end
  return {
    { px(0), py(0) }, { px(a), py(1) }, { px(a + d), py(s) },
    { px(a + d + hold), py(s) }, { px(a + d + hold + r), py(0) },
  }
end

local envCanvas = ui.canvas{
  id = "envCurves", x = envX + 10, y = 35, w = colW - 20, h = topH - 125,
  paint = function(g, w, h)
    g:fillAll("cardBg")
    grid(g, w, h, 8, 4)
    for _, e in ipairs({ { "cpFil", "textMuted", 1.5 }, { "cpAmp", "visualizerCurve", 2.5 } }) do
      local pts = envPoints(e[1], w, h)
      g:beginPath()
      for _, p in ipairs(pts) do g:lineTo(p[1], p[2]) end
      g:setColour(e[2])
      g:stroke(e[3])
      for i = 2, #pts - 1 do g:fillEllipse(pts[i][1] - 3, pts[i][2] - 3, 6, 6) end
    end
    g:setColour("visualizerCurve")
    g:drawText("AMP", 8, 4, 60, 14, "left", 10, true)
    g:setColour("textMuted")
    g:drawText("FILTER", 50, 4, 80, 14, "left", 10, true)
  end,
}

local envKnobs = { { "cpAmpAtt", "ATTACK" }, { "cpAmpDec", "DECAY" }, { "cpAmpSus", "SUSTAIN" }, { "cpAmpRel", "RELEASE" } }
for i, k in ipairs(envKnobs) do
  ui.knob{ id = "amp" .. i, param = k[1], mode = k[1] == "cpAmpSus" and "percent" or "time",
           caption = k[2], x = envX + 30 + (i - 1) * 80, y = knobY }
end

-- Repaint the curves when their parameters change (also from automation).
for _, id in ipairs({ "cpCutoff", "cpResonance" }) do params.onChange(id, function() filterCanvas:repaint() end) end
for _, prefix in ipairs({ "cpAmp", "cpFil" }) do
  for _, stage in ipairs({ "Att", "Dec", "Sus", "Rel" }) do
    params.onChange(prefix .. stage, function() envCanvas:repaint() end)
  end
end

-- --------------------------------------------------------------- LFO 1 (animated on request)
ui.card{ id = "lfoCard", title = "LFO 1", badge = "ANIMATION", x = 0, y = botY, w = colW, h = botH }

local phase = 0
local shapes = { [0] = "pulse", "triangle", "random", "sine", "noise", "saw", "revsaw" }
local randomSteps = {}
for i = 1, 16 do randomSteps[i] = math.random() * 2 - 1 end

local function lfoValue(shape, p)
  p = p % 1
  if shape == "pulse" then return p < 0.5 and 1 or -1
  elseif shape == "triangle" then return 1 - 4 * math.abs(p - 0.5)
  elseif shape == "sine" then return math.sin(p * 2 * math.pi)
  elseif shape == "saw" then return 2 * p - 1
  elseif shape == "revsaw" then return 1 - 2 * p
  else return randomSteps[math.floor(p * 16) + 1] end
end

local lfoCanvas = ui.canvas{
  id = "lfoWave", x = 10, y = botY + 35, w = colW - 140, h = botH - 45,
  paint = function(g, w, h)
    g:fillAll("cardBg")
    grid(g, w, h, 4, 2)
    local shape = shapes[params.get("spLFOShape")] or "sine"
    local depth = 0.15 + 0.85 * norm("cpLFOAmt")
    g:beginPath()
    for i = 0, 160 do
      local x = i / 160
      g:lineTo(x * w, h / 2 - lfoValue(shape, x * 2) * depth * (h / 2 - 6))
    end
    g:setColour("visualizerCurve")
    g:stroke(2)
    local px = phase * w
    local py = h / 2 - lfoValue(shape, phase * 2) * depth * (h / 2 - 6)
    g:setColour("accent")
    g:fillEllipse(px - 5, py - 5, 10, 10)
    g:setColour("textMuted")
    g:drawText(string.upper(shape), 8, 4, 120, 14, "left", 10, true)
  end,
}
ui.knob{ id = "lfoSpeed", param = "cpLFOFreq", caption = "SPEED", x = colW - 110, y = botY + 40 }

ui.button{
  id = "animate", text = "ANIMATE", toggle = true, x = colW - 120, y = botY + botH - 40, w = 110, h = 28,
  onClick = function(on)
    -- No frame callback unless asked for: the tab then costs nothing between changes.
    ui.setFrameRate(on and 60 or 0)
  end,
}
ui.onFrame(function(dt)
  local hz = 0.1 * 200 ^ norm("cpLFOFreq")       -- a drawing speed, not the LFO's real rate
  phase = (phase + dt * hz) % 1
  lfoCanvas:repaint()
end)
for _, id in ipairs({ "spLFOShape", "cpLFOAmt" }) do params.onChange(id, function() lfoCanvas:repaint() end) end

-- --------------------------------------------------------------- tools
ui.card{ id = "toolsCard", title = "TOOLS", badge = "PARAMS.SET", x = envX, y = botY, w = colW, h = botH }

local FILTER_SET = { "cpCutoff", "cpResonance", "cpFilEnvAmt", "cpFilAtt", "cpFilDec", "cpFilSus", "cpFilRel" }
local undo = {}
local snapshot = nil

local info = ui.label{ id = "toolsInfo", x = envX + 12, y = botY + 112, w = colW - 24, h = botH - 120,
                       size = 11, colour = "textMuted", text = "" }
local function say(text) info:setText(text) end

local function capture()
  local s = {}
  for _, id in ipairs(FILTER_SET) do s[id] = params.get(id) end
  return s
end
local function apply(s) for id, v in pairs(s) do params.set(id, v) end end

local bx, by = envX + 12, botY + 38
ui.button{ id = "randomFilter", text = "RANDOM FILTER", x = bx, y = by, w = 130, h = 28, onClick = function()
  undo[#undo + 1] = capture()
  if #undo > 20 then table.remove(undo, 1) end
  params.set("cpCutoff", math.random(250, 900))
  params.set("cpResonance", math.random(0, 700))
  params.set("cpFilEnvAmt", math.random(350, 999))
  for _, id in ipairs({ "cpFilAtt", "cpFilDec", "cpFilRel" }) do params.set(id, math.random(0, 600)) end
  params.set("cpFilSus", math.random(0, 999))
  say("Filter randomized. UNDO goes back " .. #undo .. " step(s).")
end }
ui.button{ id = "undo", text = "UNDO", x = bx + 140, y = by, w = 80, h = 28, onClick = function()
  local s = table.remove(undo)
  if s then apply(s); say("Undone, " .. #undo .. " step(s) left.") else say("Nothing to undo.") end
end }
ui.button{ id = "storeA", text = "STORE A", x = bx, y = by + 36, w = 100, h = 28, onClick = function()
  snapshot = capture()
  say("Filter stored in A (until the script reloads).")
end }
ui.button{ id = "recallA", text = "RECALL A", x = bx + 110, y = by + 36, w = 110, h = 28, onClick = function()
  if snapshot then undo[#undo + 1] = capture(); apply(snapshot); say("A recalled.") else say("A is empty.") end
end }

local function describe()
  say(string.format("Theme: %s\nFilter model: %s\nCutoff %s, resonance %s",
      theme.get().name, params.getText("spFilterModel"), params.getText("cpCutoff"), params.getText("cpResonance")))
end
describe()
theme.onChange(describe)
params.onChange("spFilterModel", describe)

log("Example skin loaded: " .. #params.list() .. " parameters available")

-- AFX in Lua: the native tab's AFX MODE switch, the 16 pads, the selected
-- pad's name and keys, the 128-key map with click/drag painting and the
-- OCTAVES / CHROMATIC / ALL KEYS / DEFAULT maps, plus what Lua adds:
--   * the pads, the key share ring and the keyboard are computed here from
--     afx.padForNote over all 128 notes (key ranges, counts, summaries);
--   * one canvas for the 4x4 pads: a glow glides to the selected pad, the
--     wheel steps through the pads, hover lights a pad;
--   * a KEY SHARE ring: each pad's slice of the 128 keys, the selected slice
--     pops out, click a slice to select its pad;
--   * the keyboard paints keys onto the selected pad with a flash, shows the
--     note and its pad under the mouse, and a new key map sweeps across the
--     keys from left to right.
-- Native-only (not reachable from Lua): the per-pad sound list (preset
-- browser), the pad LEVEL knob, COPY EDITED SOUND, SAVE KIT / LOAD KIT.
-- Frame rate is 0 unless a tween or a key flash plays.

local W, H = ui.size()
local GAP = 6
local KEY_H = 170
local TOP_H = H - KEY_H - GAP
local KIT_W = 640
local floor, max, min, abs, pi, atan = math.floor, math.max, math.min, math.abs, math.pi, math.atan
local PADS = afx.padCount()

local function badge(id, cx, cy, cw, textFn)
  return ui.canvas{ id = id, x = cx + cw - 168, y = cy + 5, w = 160, h = 16, paint = function(g, w, h)
    local s = textFn()
    local tw = g:textWidth(s, 8.5, true) + 12
    local x = w - tw
    g:setColour("cardHeader"); g:fillRect(x, 0, tw, h)
    g:setColour("accentDark"); g:drawRect(x, 0, tw, h, 1)
    g:setColour("accent"); g:drawText(s, x, 0, tw, h, "centre", 8.5, true)
  end }
end

-- =========================================================== kit model
local PAD_COL = {}
for i = 1, PADS do PAD_COL[i] = lx.hsv(0.52 + (i - 1) / 16, 0.85, 0.95) end
local IVORY, EBONY = "#dfe4e8", "#15181c"
local KEY_COL = {}   -- [pad] = { white, whiteMine, black, blackMine }
for i = 1, PADS do
  KEY_COL[i] = { lx.mix(IVORY, PAD_COL[i], 0.30), lx.mix(IVORY, PAD_COL[i], 0.90),
                 lx.mix(EBONY, PAD_COL[i], 0.35), lx.mix(EBONY, PAD_COL[i], 0.85) }
end

local map, shownMap = {}, {}       -- note -> pad (1..16); shownMap is what the keyboard draws
local keys, names, summary = {}, {}, {}
local noteName = lx.noteName

local function keysText(list)
  local n = #list
  if n == 0 then return "no keys" end
  if n == 1 then return noteName(list[1]) end
  if list[n] - list[1] + 1 == n then return string.format("%s - %s  (%d keys)", noteName(list[1]), noteName(list[n]), n) end
  if n <= 4 then
    local t = {}
    for i, k in ipairs(list) do t[i] = noteName(k) end
    return table.concat(t, ", ")
  end
  return string.format("%d keys, %s to %s", n, noteName(list[1]), noteName(list[n]))
end

local function rescan()
  for p = 1, PADS do keys[p] = {} end
  for n = 0, 127 do
    local p = afx.padForNote(n)
    map[n] = p
    local k = keys[p]
    k[#k + 1] = n
  end
  for p = 1, PADS do
    summary[p] = keysText(keys[p])
    names[p] = afx.pad(p).name
  end
end
rescan()
for n = 0, 127 do shownMap[n] = map[n] end

local function selected() return lx.clamp(params.get("spAFXSelectedSlot") + 1, 1, PADS) end
local function selectPad(p) params.set("spAFXSelectedSlot", lx.clamp(p, 1, PADS) - 1) end

local refreshAll   -- defined below, repaints everything that shows the kit

-- =========================================================== AFX SOUND KIT card
local kx, ky, kw = lx.section{ id = "kitCard", title = "AFX SOUND KIT", badge = "SOUND PER KEY", x = 0, y = 0, w = KIT_W, h = TOP_H }
local function afxOn() return params.get("spEngineMode") ~= 0 end
local modeBtnT = { id = "afxMode", x = kx, y = ky, w = 140, h = 26, text = "AFX MODE: OFF",
                   active = afxOn, onClick = function() params.set("spEngineMode", afxOn() and 0 or 1) end }
local modeBtn = lx.button(modeBtnT)
local modeText = ui.canvas{ id = "afxModeText", x = kx + 150, y = ky, w = kw - 150 - 170, h = 26, paint = function(g, w, h)
  local on = afxOn()
  g:setColour(on and "accent" or "textMuted")
  g:drawText(on and "ON: every key plays the sound of its pad." or "OFF: MIDI channel N plays pad N. Switch on to play the kit.",
             0, 0, w, h, "left", 9.5, true)
end }
lx.text{ x = kx + kw - 165, y = ky, w = 165, h = 26, size = 8.5, colour = "textMuted", align = "right",
         text = "SAVE / LOAD KIT: NATIVE TAB" }
local function syncMode()
  modeBtnT.text = afxOn() and "AFX MODE: ON" or "AFX MODE: OFF"
  modeBtn:repaint(); modeText:repaint()
end
lx.watch("spEngineMode", syncMode)
syncMode()

lx.subhead{ x = kx, y = ky + 34, w = kw, text = "PADS  (CLICK TO SELECT, WHEEL STEPS)" }
local PGAP = 6
local padsY = ky + 50
local padsH = TOP_H - 10 - padsY
local cellW, cellH = (kw - 3 * PGAP) / 4, (padsH - 3 * PGAP) / 4
local function padXY(p) return ((p - 1) % 4) * (cellW + PGAP), floor((p - 1) / 4) * (cellH + PGAP) end
local padState = { hx = 0, hy = 0, flash = 0, hover = -1 }
padState.hx, padState.hy = padXY(selected())

local pads
pads = ui.canvas{
  id = "afxPads", x = kx, y = padsY, w = kw, h = padsH,
  paint = function(g, w, h)
    local sel = selected()
    for p = 1, PADS do
      local x, y = padXY(p)
      local col = PAD_COL[p]
      local hov = p == padState.hover
      g:setColour(lx.mix("buttonBg", col, p == sel and 0.16 or (hov and 0.14 or 0.08)))
      g:fillRoundedRect(x, y, cellW, cellH, 4)
      g:setColour(col); g:fillRoundedRect(x, y, 5, cellH, 2)
      g:setColour("buttonBorder", hov and 1 or 0.8)
      g:drawRoundedRect(x + 0.5, y + 0.5, cellW - 1, cellH - 1, 4, 1)
      local tx = x + 14
      g:setColour(col, p == sel and 1 or 0.75)
      g:drawText(tostring(p), tx, y + 4, cellW - 20, 22, "topLeft", 18, true)
      g:setColour(p == sel and "textTitle" or "textBody")
      g:drawText(names[p] or "", tx, y + 26, cellW - 20, 16, "left", 11, true)
      g:setColour("textMuted")
      g:drawText(summary[p] or "", tx, y + 42, cellW - 20, 14, "left", 9.5, false)
    end
    -- the selection glow glides between pads
    local x, y = padState.hx, padState.hy
    g:setColour("accent", 0.10 + 0.25 * padState.flash)
    g:drawRoundedRect(x - 2, y - 2, cellW + 4, cellH + 4, 6, 4)
    g:setColour("accent")
    g:drawRoundedRect(x + 1, y + 1, cellW - 2, cellH - 2, 4, 2)
  end,
  mouse = function(event, x, y, wheel)
    local c, r = floor(x / (cellW + PGAP)), floor(y / (cellH + PGAP))
    local inside = c >= 0 and c < 4 and r >= 0 and r < 4 and (x - c * (cellW + PGAP)) <= cellW and (y - r * (cellH + PGAP)) <= cellH
    local p = inside and (r * 4 + c + 1) or -1
    if event == "down" and p > 0 then
      selectPad(p)
    elseif event == "wheel" then
      selectPad(selected() + (wheel > 0 and -1 or 1))
    elseif event == "move" or event == "enter" then
      if event == "enter" then rescan() end
      if p ~= padState.hover then padState.hover = p; pads:repaint() end
    elseif event == "exit" then
      padState.hover = -1; pads:repaint()
    end
  end,
}

-- =========================================================== SELECTED PAD card
local SX = KIT_W + GAP
local SW = W - SX
local sx, sy, sw = lx.section{ id = "padCard", title = "SELECTED PAD", x = SX, y = 0, w = SW, h = TOP_H }
badge("padBadge", SX, 0, SW, function() return "PAD " .. selected() end)
local title = ui.canvas{ id = "padTitle", x = sx, y = sy, w = sw, h = 24, paint = function(g, w, h)
  local p = selected()
  g:setColour(PAD_COL[p])
  g:drawText(p .. "   " .. (names[p] or ""), 0, 0, w, h, "left", 16, true)
end }
local hint = lx.text{ x = sx, y = sy + 24, w = sw, h = 16, size = 9.5, colour = "textMuted", text = function()
  return selected() == 1 and "Pad 1 plays the sound you edit in the other tabs."
                          or "Its sound is picked from the preset list in the native AFX tab." end }

lx.subhead{ x = sx, y = sy + 46, w = sw, text = "KEY SHARE  (CLICK A SLICE)" }
-- a ring segment from angle a0 to a1 (radians clockwise from 12 o'clock) as a closed path
local function annulus(g, cx, cy, r0, r1, a0, a1)
  local steps = max(2, math.ceil((a1 - a0) / 0.12))
  g:beginPath()
  for i = 0, steps do
    local a = a0 + (a1 - a0) * i / steps
    local x, y = cx + math.sin(a) * r1, cy - math.cos(a) * r1
    if i == 0 then g:moveTo(x, y) else g:lineTo(x, y) end
  end
  for i = steps, 0, -1 do
    local a = a0 + (a1 - a0) * i / steps
    g:lineTo(cx + math.sin(a) * r0, cy - math.cos(a) * r0)
  end
  g:closePath()
end
local ringState = { pop = 1, hover = -1 }
local RING = 150
local ring
local function sliceAt(x, y)
  local dx, dy = x - RING / 2, y - RING / 2
  local d = math.sqrt(dx * dx + dy * dy)
  if d < RING / 2 - 30 or d > RING / 2 then return -1 end
  local a = atan(dx, -dy)
  if a < 0 then a = a + 2 * pi end
  local k = floor(a / (2 * pi) * 128)
  local acc = 0
  for p = 1, PADS do
    acc = acc + #keys[p]
    if k < acc then return p end
  end
  return -1
end
ring = ui.canvas{
  id = "keyRing", x = sx, y = sy + 62, w = RING, h = RING,
  paint = function(g, w, h)
    local cx, cy = w / 2, h / 2
    local r = w / 2 - 14
    local sel = selected()
    g:beginPath(); g:arc(cx, cy, r, 0, 2 * pi)
    g:setColour("knobTrack"); g:stroke(14)
    local acc = 0
    for p = 1, PADS do
      local n = #keys[p]
      if n > 0 then
        local a0 = acc / 128 * 2 * pi
        local a1 = (acc + n) / 128 * 2 * pi
        local gap = n < 128 and 0.02 or 0
        local mine = p == sel
        local out = mine and 4 * ringState.pop or 0
        annulus(g, cx, cy, r - 7 - out * 0.5, r + 7 + out, a0 + gap, a1 - gap)
        if mine then
          g:setColour(PAD_COL[p], 0.25); g:stroke(5)
          g:setColour(PAD_COL[p]); g:fill()
          g:setColour("textTitle", 0.8); g:stroke(1)
        else
          g:setColour(PAD_COL[p], p == ringState.hover and 0.95 or 0.55); g:fill()
        end
      end
      acc = acc + n
    end
    local p = ringState.hover > 0 and ringState.hover or sel
    g:setColour(PAD_COL[p])
    g:drawText(tostring(p), 0, cy - 20, w, 24, "centre", 20, true)
    g:setColour("textBody")
    g:drawText(#keys[p] .. " / 128 KEYS", 0, cy + 4, w, 14, "centre", 9, true)
  end,
  mouse = function(event, x, y)
    local p = sliceAt(x, y)
    if event == "down" and p > 0 then selectPad(p)
    elseif event == "move" or event == "enter" then
      if p ~= ringState.hover then ringState.hover = p; ring:repaint() end
    elseif event == "exit" then
      ringState.hover = -1; ring:repaint()
    end
  end,
}
-- largest pads, as bars beside the ring
local bars = ui.canvas{
  id = "keyBars", x = sx + RING + 14, y = sy + 66, w = sw - RING - 14, h = RING - 8,
  paint = function(g, w, h)
    local order = {}
    for p = 1, PADS do order[p] = p end
    table.sort(order, function(a, b) return #keys[a] > #keys[b] or (#keys[a] == #keys[b] and a < b) end)
    local rows = 6
    local rh = (h - 26) / rows
    local most = max(1, #keys[order[1]])
    for i = 1, rows do
      local p = order[i]
      local y = (i - 1) * rh
      local n = #keys[p]
      g:setColour(PAD_COL[p]); g:drawText(tostring(p), 0, y, 20, rh - 4, "left", 10, true)
      g:setColour("knobTrack"); g:fillRoundedRect(22, y + 3, w - 60, rh - 10, 2)
      if n > 0 then g:setColour(PAD_COL[p], p == selected() and 1 or 0.65); g:fillRoundedRect(22, y + 3, (w - 60) * n / most, rh - 10, 2) end
      g:setColour("textBody"); g:drawText(tostring(n), w - 34, y, 34, rh - 4, "right", 9.5, true)
    end
    g:setColour("textMuted")
    g:drawText("MOST KEYS  |  SOUND LIST, LEVEL: NATIVE TAB", 0, h - 16, w, 14, "left", 8.5, true)
  end,
}

lx.subhead{ x = sx, y = sy + 218, w = sw, text = "KEYS" }
local keysLabel = lx.text{ x = sx, y = sy + 234, w = sw, h = 18, size = 11, bold = true, colour = "textTitle",
                           text = function() return summary[selected()] or "" end }
lx.text{ x = sx, y = sy + 252, w = sw, h = 16, size = 9.5, colour = "textMuted",
         text = "Paint keys on the keyboard below; ALL KEYS gives this pad every key." }
local bw = (sw - 6) / 2
lx.button{ id = "prevPad", x = sx, y = TOP_H - 8 - 26, w = bw, h = 26, text = "< PREVIOUS PAD", onClick = function() selectPad(selected() - 1) end }
lx.button{ id = "nextPad", x = sx + bw + 6, y = TOP_H - 8 - 26, w = bw, h = 26, text = "NEXT PAD >", onClick = function() selectPad(selected() + 1) end }

-- =========================================================== KEYBOARD card
local KY = TOP_H + GAP
local bx, by, bwid = lx.section{ id = "keyCard", title = "KEYBOARD", badge = "128 KEYS", x = 0, y = KY, w = W, h = KEY_H }
lx.subhead{ x = bx, y = by, w = bwid, text = "KEY MAP  (CLICK OR DRAG KEYS ONTO THE SELECTED PAD)" }
local hoverNote = -1
local hoverText = ui.canvas{ id = "hoverText", x = bx, y = by + 16, w = bwid - 4 * 96 - 3 * 6 - 12, h = 24, paint = function(g, w, h)
  local s
  if hoverNote < 0 then
    g:setColour("textMuted")
    s = "Click or drag over keys to put them on the selected pad."
  else
    local p = map[hoverNote]
    g:setColour(PAD_COL[p]); g:fillRoundedRect(0, 7, 10, 10, 2)
    g:setColour("textBody")
    s = string.format("%s (note %d) plays pad %d: %s", noteName(hoverNote), hoverNote, p, names[p] or "")
  end
  g:drawText(s, hoverNote < 0 and 0 or 16, 0, w - 16, h, "left", 9.5, true)
end }

local WHITE_BEFORE = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 }
local WHITE_KEYS = 75
local kbW, kbH = bwid, KY + KEY_H - 8 - (by + 46)
local keyW = kbW / WHITE_KEYS
local blackW, blackH = keyW * 0.62, kbH * 0.6
local isBlack = lx.isBlack
local function keyRect(n)
  local x = ((n // 12) * 7 + WHITE_BEFORE[n % 12 + 1]) * keyW
  if isBlack(n) then return x - blackW / 2, 0, blackW, blackH end
  return x, 0, keyW, kbH
end
local function noteAt(x, y)
  if x < 0 or x >= kbW or y < 0 or y >= kbH then return -1 end
  local wi = floor(x / keyW)
  if y < blackH then
    for n = 0, 127 do
      if isBlack(n) then
        local kx0, _, kw0 = keyRect(n)
        if x >= kx0 and x < kx0 + kw0 then return n end
      end
    end
  end
  local oct, w7 = wi // 7, wi % 7
  local WHITE_NOTES = { 0, 2, 4, 5, 7, 9, 11 }
  local n = oct * 12 + WHITE_NOTES[w7 + 1]
  return n <= 127 and n or -1
end

local flash = {}
local sweep = { k = 1 }
local keyboard
keyboard = ui.canvas{
  id = "afxKeyboard", x = bx, y = by + 46, w = kbW, h = kbH,
  paint = function(g, w, h)
    local sel = selected()
    g:setColour("cardBorder"); g:fillRect(0, 0, w, h)
    for pass = 0, 1 do
      for n = 0, 127 do
        local black = isBlack(n)
        if (pass == 1) == black then
          local x, y, kw0, kh = keyRect(n)
          if not black then kw0 = kw0 - 1 end
          local p = (n / 127 <= sweep.k) and map[n] or shownMap[n]
          local mine = p == sel
          local cols = KEY_COL[p]
          g:setColour(black and (mine and cols[4] or cols[3]) or (mine and cols[2] or cols[1]))
          g:fillRect(x, y, kw0, kh)
          local f = flash[n]
          if f then g:setColour("#ffffff", 0.7 * f); g:fillRect(x, y, kw0, kh) end
          if mine then
            g:setColour(black and "textTitle" or EBONY)
            g:fillRect(x + kw0 / 2 - 1.5, kh - 8, 3, 3)
          end
          if n == hoverNote then g:setColour("accent"); g:drawRect(x, y, kw0, kh, 2) end
          if not black and n % 12 == 0 then
            g:setColour(EBONY, 0.8)
            g:drawText("C" .. (n // 12 - 1), x - 4, kh - 24, kw0 + 8, 12, "centre", 8, true)
          end
        end
      end
    end
  end,
  mouse = function(event, x, y)
    local n = noteAt(x, y)
    if event == "down" or event == "drag" then
      local sel = selected()
      if n >= 0 and map[n] ~= sel then
        afx.setKey(n, sel)
        map[n], shownMap[n] = sel, sel
        flash[n] = 1
        if not lx.running("keyFlash") then
          lx.loop("keyFlash", function(dt)
            local any = false
            for k, v in pairs(flash) do
              v = v - dt * 3
              if v <= 0 then flash[k] = nil else flash[k] = v; any = true end
            end
            keyboard:repaint()
            return any
          end, 60)
        end
        rescan()
        refreshAll(true)
      end
    end
    if event == "exit" then n = -1 end
    if event ~= "wheel" and n ~= hoverNote then
      hoverNote = n
      keyboard:repaint(); hoverText:repaint()
    end
  end,
}

-- the maps: the new map sweeps across the keyboard
local function applyMap(kind)
  for n = 0, 127 do shownMap[n] = map[n] end
  if kind == "all" then afx.map("all", selected()) else afx.map(kind) end
  rescan()
  sweep.k = 0
  lx.tween(sweep, "k", 1, 0.6, lx.ease.inOutSine, function() keyboard:repaint() end)
  refreshAll(false)
end
local MAPS = { { "OCTAVES", "octaves" }, { "CHROMATIC", "chromatic" }, { "ALL KEYS", "all" }, { "DEFAULT", "default" } }
for i, m in ipairs(MAPS) do
  lx.button{ id = "map" .. m[2], x = bx + bwid - (5 - i) * 96 - (4 - i) * 6, y = by + 17, w = 96, h = 22, text = m[1], size = 10,
             onClick = function() applyMap(m[2]) end }
end

-- =========================================================== refresh
function refreshAll(withKeyboard)
  pads:repaint(); ring:repaint(); bars:repaint(); keysLabel:repaint(); title:repaint(); hoverText:repaint()
  if withKeyboard then keyboard:repaint() end
end

params.onChange("spAFXSelectedSlot", function()
  rescan()
  local x, y = padXY(selected())
  local rp = function() pads:repaint() end
  lx.tween(padState, "hx", x, 0.25, lx.ease.outCubic, rp)
  lx.tween(padState, "hy", y, 0.25, lx.ease.outCubic, rp)
  padState.flash = 1
  lx.tween(padState, "flash", 0, 0.6, lx.ease.outCubic, rp)
  ringState.pop = 0
  lx.tween(ringState, "pop", 1, 0.4, lx.ease.outBack, function() ring:repaint() end)
  hint:repaint()
  refreshAll(true)
end)

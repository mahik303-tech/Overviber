-- MOD MATRIX in Lua: the native tab's controls, plus what Lua adds:
--   * the eight slots are one canvas: enable box, names, a bipolar depth bar
--     you drag (wheel nudges, double click resets) and a selection that glides;
--     depth bars animate into values set by quick assign, clear or presets;
--   * ROUTING GRAPH: the same slots as a node graph, laid out from the data
--     (only the sources and destinations in use), bezier wires as thick as
--     |depth| with dots flowing along them (backwards for negative depth),
--     click a wire to select its slot. The animation loop runs only while the
--     graph is shown, so the page costs no frames otherwise;
--   * BEND SPAN: the pitch bend range drawn on a keyboard, gliding on change.
-- The matrix has no change events: the page reloads it after its own edits,
-- when the pointer enters the slot list, when common sound parameters change
-- (a preset load) and, while the graph runs, four times a second.
-- Native only: the right-click "modulate this knob" menus of the other tabs
-- and the white modulation arcs on their knobs.

local W, H = ui.size()
local GAP = 6
local LEFT_W = 262
local floor, max, min, abs = math.floor, math.max, math.min, math.abs

-- =========================================================== matrix data
local SRC_NAMES, DEST_NAMES = matrix.sources(), matrix.destinations()
local NSRC, NDEST = 0, 0
while SRC_NAMES[NSRC] do NSRC = NSRC + 1 end
while DEST_NAMES[NDEST] do NDEST = NDEST + 1 end

local SRC_SHORT = { "NONE", "MOD WHEEL", "PITCH BEND", "AFTERTOUCH", "SLIDE CC 74", "VELOCITY", "LIFT VEL", "KEY TRACK",
  "BREATH", "EXPRESSION", "FILTER ENV", "AMP ENV", "WMOD ENV", "LFO 1", "LFO 1 +", "LFO 2", "LFO 2 +", "CONSTANT" }
local DEST_SHORT = { "NONE", "PITCH", "PITCH A", "PITCH B", "DETUNE", "WAVEMOD", "WAVEMOD A",
  "WAVEMOD B", "LEVEL A", "LEVEL B", "NOISE", "CUTOFF", "RESONANCE", "AMP (VCA)",
  "GEOMETRY", "BRIGHTNESS", "DAMPING", "POSITION", "SPACE", "BOW", "BLOW",
  "STRIKE", "CONTOUR", "FLOW", "MALLET", "BOW TIMBRE", "BLOW TIMBRE", "STRIKE TIMBRE" }
-- if the engine's lists ever differ, fall back to its own names
local function shortList(short, names, n)
  if #short == n then return short end
  local out = {}
  for i = 0, n - 1 do out[i + 1] = string.upper(names[i]) end
  return out
end
SRC_SHORT = shortList(SRC_SHORT, SRC_NAMES, NSRC)
DEST_SHORT = shortList(DEST_SHORT, DEST_NAMES, NDEST)

-- ids of modSource_t / modDest_t (OvercyclerTypes.h)
local QUICK = {
  { "MOD WHEEL > CUTOFF", 1, 11, 50 },
  { "VELOCITY > LEVEL", 5, 13, 50 },
  { "AFTERTOUCH > CUTOFF", 3, 11, 40 },
  { "LFO 1 > PITCH (VIBRATO)", 13, 1, 10 },
  { "LFO 2 > WAVEMOD", 15, 5, 50 },
  { "KEY TRACK > CUTOFF", 7, 11, 50 },
}

local COUNT = matrix.count()
local slots, shown = {}, {}
local sel = 1
local function sig(s) return s.source .. "," .. s.via .. "," .. s.dest .. "," .. s.depth .. "," .. (s.enabled and 1 or 0) end
local function isEmpty(s) return s.source == 0 and s.dest == 0 end
for i = 1, COUNT do slots[i] = matrix.get(i); shown[i] = slots[i].depth end

local rows, graph, depthBar, selHead          -- canvases, set below
local editorSyncs = {}

local function repaintViews()
  if rows then rows:repaint() end
  if graph then graph:repaint() end
end
local function syncEditor()
  for _, f in ipairs(editorSyncs) do f() end
end

local function animateDepth(i)
  lx.tween(shown, i, slots[i].depth, 0.3, lx.ease.outCubic, function() repaintViews(); if i == sel and depthBar then depthBar:repaint() end end)
end

-- Reads the matrix again; animates what changed elsewhere.
local function refresh()
  local changed = false
  for i = 1, COUNT do
    local s = matrix.get(i)
    if sig(s) ~= sig(slots[i]) then
      slots[i] = s
      animateDepth(i)
      changed = true
    end
  end
  if changed then repaintViews(); syncEditor() end
end

-- Writes part of a slot ({source, via, dest, depth, enabled}) and shows it.
local function setSlot(i, partial, instant)
  matrix.set(i, partial)
  slots[i] = matrix.get(i)
  if instant then shown[i] = slots[i].depth else animateDepth(i) end
  repaintViews()
  if i == sel then syncEditor() end
end

local selAnim = { y = 0 }
local ROW_H = 20
local function selectSlot(i)
  sel = lx.clamp(i, 1, COUNT)
  lx.tween(selAnim, "y", (sel - 1) * ROW_H, 0.2, lx.ease.outCubic, function() if rows then rows:repaint() end end)
  repaintViews()
  syncEditor()
end
selAnim.y = 0

-- A subhead whose text is a function (lx.subhead's is fixed).
local function liveSubhead(t)
  return ui.canvas{ id = t.id, x = t.x, y = t.y, w = t.w, h = 14, paint = function(g, w, h)
    local s = t.text()
    g:setColour("textMuted")
    g:drawText(s, 0, 0, w, h, "left", 9, true)
    local tw = g:textWidth(s, 9, true) + 8
    if tw < w then
      g:setColour("cardBorder", 0.9)
      g:drawLine(tw, floor(h / 2) + 0.5, w, floor(h / 2) + 0.5, 1)
    end
  end }
end

local function depthColour(d) return d < 0 and "#e8a547" or "accent" end

-- =========================================================== PERFORMANCE CONTROLLERS
local cx, cy, cw, ch = lx.section{ id = "controllersCard", title = "PERFORMANCE CONTROLLERS", badge = "MIDI",
                                   x = 0, y = 0, w = LEFT_W, h = H }
local COL1 = 108
local COL2X = cx + COL1 + 12
local COL2W = cw - COL1 - 12
local RH, RG = 17, 3
local function listH(n) return n * RH + (n - 1) * RG end

local y = cy
local function perfSection(title, colA, itemsA, paramA, colB, itemsB, paramB)
  lx.subhead{ x = cx, y = y, w = cw, text = title }
  y = y + 16
  lx.subhead{ x = cx + 8, y = y, w = COL1 - 8, text = colA }
  lx.subhead{ x = COL2X, y = y, w = COL2W, text = colB }
  y = y + 16
  local hA, hB = listH(#itemsA), listH(#itemsB)
  lx.choice{ id = "perf_" .. paramA, param = paramA, items = itemsA, cols = 1, rowH = RH, gap = RG,
             x = cx + 8, y = y, w = COL1 - 8, h = hA, size = 9 }
  lx.choice{ id = "perf_" .. paramB, param = paramB, items = itemsB, cols = 1, rowH = RH, gap = RG,
             x = COL2X, y = y, w = COL2W, h = hB, size = 9 }
  y = y + max(hA, hB) + 8
end
perfSection("PITCH BEND", "RANGE", { "4 SEMI (3RD)", "7 SEMI (5TH)", "12 SEMI (1 OCT)" }, "spBenderRange",
            "DESTINATION", { "OFF", "PITCH", "CUTOFF", "VOLUME", "WAVEMOD" }, "spBenderTarget")
perfSection("MODULATION WHEEL (CC 1)", "INTENSITY", { "MIN", "LOW", "HIGH", "MAX" }, "spModwheelRange",
            "DESTINATION", { "LFO 1 DEPTH", "LFO 2 DEPTH" }, "spModwheelTarget")
perfSection("AFTERTOUCH", "SENSITIVITY", { "MIN", "LOW", "HIGH", "MAX" }, "spPressureRange",
            "DESTINATION", { "OFF", "PITCH", "CUTOFF", "VOLUME", "WAVEMOD", "LFO 1", "LFO 2" }, "spPressureTarget")

-- ---- bend span on a keyboard (computed from spBenderRange / spBenderTarget)
local BEND_SEMIS = { 4, 7, 12 }
local BEND_DEST = { "OFF", "PITCH", "CUTOFF", "VOLUME", "WAVEMOD" }
local span = { k = BEND_SEMIS[params.get("spBenderRange") + 1] or 4 }
local bendHead = liveSubhead{ id = "bendHead", x = cx, y = y, w = cw, text = function()
  return string.format("BEND SPAN  +/- %d ST > %s", lx.round(span.k), BEND_DEST[params.get("spBenderTarget") + 1] or "")
end }
local kbY = y + 16
local keyboard
keyboard = ui.canvas{
  id = "bendSpan", x = cx, y = kbY, w = cw, h = cy + ch - kbY + 4,
  paint = function(g, w, h)
    local kh = h - 10
    local lo, hi = -14, 14                             -- semitones around the played key
    local whites = {}
    for n = lo, hi do if not lx.isBlack(n + 60) then whites[#whites + 1] = n end end
    local kw = w / #whites
    local on = params.get("spBenderTarget") ~= 0
    local col = on and "accent" or "textMuted"
    local function inSpan(n) return abs(n) <= span.k + 0.001 end
    -- span band
    local function xOf(n)
      local i = 0
      for wi, wn in ipairs(whites) do if wn <= n then i = wi end end
      local x = (i - 1) * kw
      if lx.isBlack(n + 60) then x = x + kw end
      return x
    end
    for i, n in ipairs(whites) do
      local x = (i - 1) * kw
      local lit = inSpan(n)
      g:setColour(lit and lx.mix("textBody", col, n == 0 and 1 or 0.45) or lx.shade("textBody", -0.45))
      g:fillRect(x + 0.5, 0, kw - 1, kh)
    end
    for n = lo, hi do
      if lx.isBlack(n + 60) then
        local x = xOf(n) - kw * 0.3
        g:setColour(inSpan(n) and lx.mix("windowBg", col, 0.75) or lx.shade("windowBg", -0.4))
        g:fillRect(x, 0, kw * 0.6, kh * 0.6)
      end
    end
    -- the bend arrows
    local x0 = xOf(0) + kw / 2
    local reach = xOf(lx.round(span.k)) + kw / 2 - x0
    if lx.isBlack(lx.round(span.k) + 60) then reach = reach - kw / 2 end
    g:setColour(col, 0.9)
    g:drawLine(x0 - reach, kh + 5, x0 + reach, kh + 5, 2)
    g:fillEllipse(x0 - reach - 3, kh + 2, 6, 6)
    g:fillEllipse(x0 + reach - 3, kh + 2, 6, 6)
  end,
}
lx.watch("spBenderRange", function(v)
  lx.tween(span, "k", BEND_SEMIS[v + 1] or 4, 0.35, lx.ease.outBack, function() keyboard:repaint(); bendHead:repaint() end)
end)
lx.watch("spBenderTarget", function() keyboard:repaint(); bendHead:repaint() end)

-- =========================================================== MODULATION MATRIX
local mx0 = LEFT_W + GAP
local ix, iy, iw, ih = lx.section{ id = "modMatrixCard", title = "MODULATION MATRIX", badge = COUNT .. " SLOTS",
                                   x = mx0, y = 0, w = W - mx0, h = H }

lx.subhead{ x = ix, y = iy, w = iw - 170, text = "ROUTING  (CLICK A SLOT TO EDIT, DRAG ITS BAR FOR THE DEPTH)" }
local VIEW_Y = iy + 18
local VIEW_H = COUNT * ROW_H

-- ---- the slot rows
local BAR_W = 190
local hoverRow = 0
local drag = nil
local function barX(w) return w - BAR_W - 62 end

rows = ui.canvas{
  id = "routing", x = ix, y = VIEW_Y, w = iw, h = VIEW_H,
  paint = function(g, w, h)
    local bx = barX(w)
    for i = 1, COUNT do
      local s = slots[i]
      local y0 = (i - 1) * ROW_H
      g:setColour(i % 2 == 0 and lx.shade("cardBg", -0.12) or lx.shade("cardBg", 0.03))
      g:fillRect(0, y0, w, ROW_H)
      if i == hoverRow then g:setColour("accent", 0.07); g:fillRect(0, y0, w, ROW_H) end
    end
    -- the gliding selection
    g:setColour("accent", 0.14)
    g:fillRoundedRect(0, selAnim.y, w, ROW_H, 2)
    g:setColour("accent", 0.9)
    g:drawRoundedRect(0.5, selAnim.y + 0.5, w - 1, ROW_H - 1, 2, 1)
    for i = 1, COUNT do
      local s = slots[i]
      local y0 = (i - 1) * ROW_H
      local empty = isEmpty(s)
      -- enable box
      g:setColour("buttonBg"); g:fillRoundedRect(8, y0 + 4, 10, 10, 2)
      g:setColour("buttonBorder"); g:drawRoundedRect(8.5, y0 + 4.5, 9, 9, 2, 1)
      if s.enabled then g:setColour(empty and lx.col("accent", 0.4) or lx.col("accent")); g:fillRoundedRect(10, y0 + 6, 6, 6, 1) end
      g:setColour(empty and "textMuted" or "textTitle")
      g:drawText(tostring(i), 24, y0, 16, ROW_H, "left", 10, true)
      if empty then
        g:setColour("textMuted")
        g:drawText("empty", 44, y0, 200, ROW_H, "left", 10, false)
      else
        local alpha = s.enabled and 1 or 0.45
        g:setColour("textTitle", alpha)
        local src = SRC_NAMES[s.source] or "?"
        g:drawText(src, 44, y0, 250, ROW_H, "left", 10.5, true)
        if s.via ~= 0 then
          local sw = min(250, g:textWidth(src, 10.5, true))
          g:setColour("textMuted", alpha)
          g:drawText("x " .. (SRC_SHORT[s.via + 1] or ""), 50 + sw, 0 + y0, 300 - 50 - sw, ROW_H, "left", 9, true)
        end
        g:setColour("accent", alpha)
        g:drawLine(300, y0 + ROW_H / 2, 312, y0 + ROW_H / 2, 1.2)
        g:drawLine(309, y0 + ROW_H / 2 - 3, 312, y0 + ROW_H / 2, 1.2)
        g:drawLine(309, y0 + ROW_H / 2 + 3, 312, y0 + ROW_H / 2, 1.2)
        g:setColour("textTitle", alpha)
        g:drawText(DEST_NAMES[s.dest] or "?", 318, y0, bx - 326, ROW_H, "left", 10.5, true)
        -- bipolar depth bar
        g:setColour(lx.shade("windowBg", -0.3)); g:fillRect(bx, y0 + 5, BAR_W, ROW_H - 10)
        local mid = bx + BAR_W / 2
        local d = shown[i] / 100
        local col = depthColour(shown[i])
        g:setColour(col, s.enabled and 1 or 0.4)
        g:fillRect(min(mid, mid + d * BAR_W / 2), y0 + 5, abs(d) * BAR_W / 2, ROW_H - 10)
        g:setColour("textMuted", 0.6); g:fillRect(mid - 0.5, y0 + 3, 1, ROW_H - 6)
        g:setColour("textTitle", alpha)
        g:drawText(string.format("%+d %%", lx.round(shown[i])), bx + BAR_W + 6, y0, 52, ROW_H, "right", 10, true)
      end
    end
  end,
  mouse = function(event, x, y, wheel)
    local i = lx.clamp(floor(y / ROW_H) + 1, 1, COUNT)
    local bx = barX(iw)
    local function depthAt(px) return lx.round(lx.clamp((px - bx - BAR_W / 2) / (BAR_W / 2), -1, 1) * 100) end
    if event == "down" then
      if x < 22 then
        setSlot(i, { enabled = not slots[i].enabled })
      elseif x >= bx - 4 and x <= bx + BAR_W + 4 and not isEmpty(slots[i]) then
        drag = i
        if i ~= sel then selectSlot(i) end
        setSlot(i, { depth = depthAt(x) }, true)
      elseif i ~= sel then
        selectSlot(i)
      end
    elseif event == "drag" and drag then
      setSlot(drag, { depth = depthAt(x) }, true)
    elseif event == "up" then
      drag = nil
    elseif event == "double" and x >= bx - 4 and x <= bx + BAR_W + 4 and not isEmpty(slots[i]) then
      setSlot(i, { depth = 0 })
    elseif event == "wheel" and not isEmpty(slots[i]) then
      setSlot(i, { depth = lx.clamp(slots[i].depth + (wheel > 0 and 5 or -5), -100, 100) })
    elseif event == "enter" then
      refresh()
    end
    if event == "move" or event == "enter" then
      if i ~= hoverRow then hoverRow = i; rows:repaint() end
    elseif event == "exit" then
      hoverRow = 0; rows:repaint()
    end
  end,
}

-- ---- the routing graph
local graphOn = false
local NODE_W = 150
local layout = nil          -- built per paint: nodes and wires

local function buildLayout(w, h)
  local srcs, dsts, srcOrder, dstOrder = {}, {}, {}, {}
  for i = 1, COUNT do
    local s = slots[i]
    if not isEmpty(s) then
      if not srcs[s.source] then srcs[s.source] = true; srcOrder[#srcOrder + 1] = s.source end
      if not dsts[s.dest] then dsts[s.dest] = true; dstOrder[#dstOrder + 1] = s.dest end
    end
  end
  table.sort(srcOrder); table.sort(dstOrder)
  local function place(order)
    local pos = {}
    local n = #order
    local nh = min(20, (h - 8) / max(1, n) - 4)
    local step = (h - 8) / max(1, n)
    for k, id in ipairs(order) do pos[id] = { y = 4 + (k - 0.5) * step, h = nh } end
    return pos
  end
  return { srcOrder = srcOrder, dstOrder = dstOrder, src = place(srcOrder), dst = place(dstOrder),
           x0 = 8 + NODE_W, x1 = w - 8 - NODE_W }
end

local function bez(t, x0, y0, x1, y1)
  local xm = (x0 + x1) / 2
  local u = 1 - t
  local a, b, c, d = u * u * u, 3 * u * u * t, 3 * u * t * t, t * t * t
  return a * x0 + b * xm + c * xm + d * x1, a * y0 + b * y0 + c * y1 + d * y1
end

graph = ui.canvas{
  id = "graph", x = ix, y = VIEW_Y, w = iw, h = VIEW_H,
  paint = function(g, w, h)
    lx.panel(g, w, h)
    local L = buildLayout(w, h)
    layout = L
    if #L.srcOrder == 0 then
      g:setColour("textMuted")
      g:drawText("NO ROUTINGS YET: USE QUICK ASSIGN OR THE GRIDS BELOW", 0, 0, w, h, "centre", 10, true)
      return
    end
    -- wires, selected last
    local order = {}
    for i = 1, COUNT do if i ~= sel then order[#order + 1] = i end end
    order[#order + 1] = sel
    for _, i in ipairs(order) do
      local s = slots[i]
      if not isEmpty(s) then
        local y0, y1 = L.src[s.source].y, L.dst[s.dest].y
        local xm = (L.x0 + L.x1) / 2
        local th = 1 + abs(shown[i]) / 100 * 4
        local col = depthColour(shown[i])
        g:beginPath(); g:moveTo(L.x0, y0); g:cubicTo(xm, y0, xm, y1, L.x1, y1)
        if not s.enabled then
          g:setColour("textMuted", 0.35); g:stroke(1)
        elseif i == sel then
          lx.glow(g, col, th, 1)
        else
          g:setColour(col, 0.55); g:stroke(th)
        end
        -- flowing dots: speed and count by depth, backwards when negative
        if s.enabled and shown[i] ~= 0 then
          local dir = shown[i] < 0 and -1 or 1
          local speed = 0.15 + abs(shown[i]) / 100 * 0.45
          local r = 1.5 + th * 0.45
          for k = 0, 2 do
            local t = (lx.time * speed * dir + k / 3) % 1
            local px, py = bez(t, L.x0, y0, L.x1, y1)
            g:setColour("textTitle", 0.85 * math.sin(t * math.pi))
            g:fillEllipse(px - r, py - r, r * 2, r * 2)
          end
        end
        -- slot number at the middle
        local mx, my = bez(0.3 + 0.4 * (i - 1) / max(1, COUNT - 1), L.x0, y0, L.x1, y1)
        g:setColour(i == sel and "accent" or lx.shade("cardBg", 0.1))
        g:fillEllipse(mx - 8, my - 8, 16, 16)
        g:setColour(i == sel and "accent" or "buttonBorder"); g:drawEllipse(mx - 8, my - 8, 16, 16, 1)
        g:setColour(i == sel and "windowBg" or "textBody")
        g:drawText(tostring(i), mx - 8, my - 8, 16, 16, "centre", 9, true)
        if s.via ~= 0 then
          g:setColour("textMuted")
          g:drawText("x " .. SRC_SHORT[s.via + 1], mx + 10, my - 14, 100, 12, "left", 8.5, true)
        end
      end
    end
    -- nodes
    local function node(x, p, label, active)
      g:setColour(active and lx.shade("buttonBg", 0.1) or "buttonBg")
      g:fillRoundedRect(x, p.y - p.h / 2, NODE_W, p.h, 3)
      g:setColour(active and "accent" or "buttonBorder")
      g:drawRoundedRect(x + 0.5, p.y - p.h / 2 + 0.5, NODE_W - 1, p.h - 1, 3, 1)
      g:setColour(active and "textTitle" or "textBody")
      g:drawText(label, x + 6, p.y - p.h / 2, NODE_W - 12, p.h, "centre", min(10, p.h - 4), true)
    end
    local cur = slots[sel]
    for _, id in ipairs(L.srcOrder) do
      node(8, L.src[id], SRC_SHORT[id + 1], not isEmpty(cur) and cur.source == id)
      g:setColour("accent"); g:fillEllipse(L.x0 - 3, L.src[id].y - 3, 6, 6)
    end
    for _, id in ipairs(L.dstOrder) do
      node(L.x1, L.dst[id], DEST_SHORT[id + 1], not isEmpty(cur) and cur.dest == id)
      g:setColour("accent"); g:fillEllipse(L.x1 - 3, L.dst[id].y - 3, 6, 6)
    end
    g:setColour("textMuted")
    g:drawText("SOURCES", 10, 2, 100, 10, "left", 7.5, true)
    g:drawText("DESTINATIONS", w - 110, 2, 100, 10, "right", 7.5, true)
  end,
  mouse = function(event, x, y, wheel)
    if event == "down" and layout then
      -- nearest wire within 8 px
      local best, bestD = nil, 64
      for i = 1, COUNT do
        local s = slots[i]
        if not isEmpty(s) and layout.src[s.source] and layout.dst[s.dest] then
          local y0, y1 = layout.src[s.source].y, layout.dst[s.dest].y
          for k = 0, 24 do
            local px, py = bez(k / 24, layout.x0, y0, layout.x1, y1)
            local d = (px - x) ^ 2 + (py - y) ^ 2
            if d < bestD then best, bestD = i, d end
          end
        end
      end
      if best then selectSlot(best) end
    elseif event == "wheel" and not isEmpty(slots[sel]) then
      setSlot(sel, { depth = lx.clamp(slots[sel].depth + (wheel > 0 and 5 or -5), -100, 100) })
    end
  end,
}
graph:setVisible(false)

local pollT = 0
local function setGraph(on)
  graphOn = on
  rows:setVisible(not on)
  graph:setVisible(on)
  if on then
    refresh()
    pollT = 0
    lx.loop("flow", function(dt)
      pollT = pollT + dt
      if pollT > 0.25 then pollT = 0; refresh() end
      graph:repaint()
    end, 30)
  else
    lx.stop("flow")
    rows:repaint()
  end
end
lx.toggle{ id = "graphToggle", x = ix + iw - 150, y = iy - 2, w = 150, h = 18, text = "ROUTING GRAPH (LUA)",
           get = function() return graphOn end, set = setGraph }

-- =========================================================== selected slot editor
local EY = VIEW_Y + VIEW_H + 6
selHead = liveSubhead{ id = "selHead", x = ix, y = EY, w = iw, text = function()
  local s = slots[sel]
  if isEmpty(s) then return "SELECTED SLOT " .. sel .. "  (EMPTY)" end
  return "SELECTED SLOT " .. sel .. "  (" .. SRC_SHORT[s.source + 1] .. " > " .. DEST_SHORT[s.dest + 1] .. ")"
end }
editorSyncs[#editorSyncs + 1] = function() selHead:repaint() end

local CAP_W = 84
local GX, GW = ix + CAP_W, iw - CAP_W
local rowY = EY + 18
local _, enSync = lx.toggle{ id = "slotOn", x = ix + 4, y = rowY + 2, w = CAP_W - 8, h = 18, text = "SLOT ON",
  get = function() return slots[sel].enabled end,
  set = function(v) setSlot(sel, { enabled = v }) end }
editorSyncs[#editorSyncs + 1] = enSync
lx.button{ id = "clearSlot", x = GX, y = rowY, w = 100, h = 22, text = "CLEAR SLOT",
           onClick = function() setSlot(sel, { source = 0, via = 0, dest = 0, depth = 0, enabled = true }) end }

local depthDrag = false
local DB_X, DB_W = GX + 108, GW - 108
depthBar = ui.canvas{
  id = "depthBar", x = DB_X, y = rowY, w = DB_W, h = 22,
  paint = function(g, w, h)
    g:setColour("buttonBg"); g:fillRoundedRect(0, 0, w, h, 2)
    local d = shown[sel] / 100
    local mid = w / 2
    local col = depthColour(shown[sel])
    g:setGradient{ x1 = mid, y1 = 0, x2 = mid + (d >= 0 and 1 or -1) * w / 2, y2 = 0,
                   stops = { { 0, lx.col(col, 0.35) }, { 1, lx.col(col, 0.9) } } }
    g:fillRect(min(mid, mid + d * (w / 2 - 1)), 1, abs(d) * (w / 2 - 1), h - 2)
    g:setColour(col); g:fillRect(mid + d * (w / 2 - 1) - 1, 1, 2, h - 2)
    g:setColour("textMuted", 0.7); g:fillRect(mid - 0.5, 3, 1, h - 6)
    for k = 1, 3 do
      local tx = w / 2 + k * w / 8
      g:setColour("textMuted", 0.25); g:fillRect(tx, h - 5, 1, 3); g:fillRect(w - tx, h - 5, 1, 3)
    end
    g:setColour(depthDrag and "accent" or "buttonBorder"); g:drawRoundedRect(0.5, 0.5, w - 1, h - 1, 2, 1)
    g:setColour("textTitle")
    g:drawText(string.format("DEPTH  %+d %%", lx.round(shown[sel])), 0, 0, w, h, "centre", 10.5, true)
  end,
  mouse = function(event, x, y, wheel)
    local function at(px) return lx.round(lx.clamp((px - DB_W / 2) / (DB_W / 2), -1, 1) * 100) end
    if event == "down" or (event == "drag" and depthDrag) then
      depthDrag = true
      setSlot(sel, { depth = at(x) }, true)
      depthBar:repaint()
    elseif event == "up" then
      depthDrag = false
      depthBar:repaint()
    elseif event == "double" then
      setSlot(sel, { depth = 0 })
    elseif event == "wheel" then
      setSlot(sel, { depth = lx.clamp(slots[sel].depth + (wheel > 0 and 1 or -1), -100, 100) }, true)
      depthBar:repaint()
    end
  end,
}
editorSyncs[#editorSyncs + 1] = function() depthBar:repaint() end

-- source / via / destination grids
local GRID_RH, GRID_G = 19, 3
local function gridH(n, cols) local r = math.ceil(n / cols); return r * GRID_RH + (r - 1) * GRID_G end
local function grid(id, caption, y0, items, cols, key)
  lx.text{ id = id .. "Cap", x = ix + 8, y = y0 + 1, w = CAP_W - 8, h = 16, text = caption, size = 9, bold = true, colour = "textMuted" }
  local _, sync = lx.choice{ id = id, x = GX, y = y0, w = GW, h = gridH(#items, cols), items = items, cols = cols,
                             rowH = GRID_RH, gap = GRID_G, size = 9,
                             get = function() return slots[sel][key] + 1 end,
                             set = function(i)
                               local p = { [key] = i - 1 }
                               if key ~= "via" and isEmpty(slots[sel]) and slots[sel].depth == 0 then p.depth = 50 end
                               setSlot(sel, p)
                             end }
  editorSyncs[#editorSyncs + 1] = sync
  return y0 + gridH(#items, cols) + 6
end
local gy = rowY + 28
gy = grid("source", "SOURCE", gy, SRC_SHORT, 9, "source")
gy = grid("via", "VIA", gy, SRC_SHORT, 9, "via")
gy = grid("dest", "DESTINATION", gy, DEST_SHORT, 7, "dest")

-- quick assign
lx.subhead{ x = ix, y = gy, w = iw, text = "QUICK ASSIGN  (INTO THE SELECTED SLOT)" }
local QW = (iw - 5 * 6) / 6
for i, q in ipairs(QUICK) do
  lx.button{ id = "quick" .. i, x = ix + (i - 1) * (QW + 6), y = gy + 16, w = QW, h = 22, text = q[1], size = 9,
             onClick = function() setSlot(sel, { source = q[2], via = 0, dest = q[3], depth = q[4], enabled = true }) end }
end
local hy = gy + 44
lx.subhead{ x = ix, y = hy, w = iw, text = "MODULATION FROM ANY TAB" }
lx.text{ id = "hint", x = ix + 8, y = hy + 14, w = iw - 8, h = 14, size = 9.5, colour = "textMuted",
         text = "Right-click a knob on any native tab (cutoff, resonance, pitch, levels, WaveMod, Elements) to modulate it. "
             .. "A modulated knob shows a white arc." }

-- a preset load changes these with the matrix: reload it then
lx.watch({ "cpCutoff", "cpResonance", "cpAmpLevel", "spFilterModel", "cpGlide", "cpMasterTune", "spVoiceCount" }, refresh)


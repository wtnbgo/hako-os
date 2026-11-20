-- Smoke test for LÖVE 12 on hako: renderer info, gamepad state, touch, a beep.
-- START + SELECT (or Escape) quits back to the launcher.

local pads = {}
local touches = {}
local t = 0
local beep

local BUTTONS = { "a", "b", "x", "y", "back", "guide", "start", "leftshoulder",
	"rightshoulder", "leftstick", "rightstick", "dpup", "dpdown", "dpleft", "dpright" }

local function makeBeep()
	local rate, len = 44100, 0.12
	local sd = love.sound.newSoundData(math.floor(rate * len), rate, 16, 1)
	for i = 0, sd:getSampleCount() - 1 do
		local env = 1 - i / sd:getSampleCount()
		sd:setSample(i, math.sin(i / rate * 880 * 2 * math.pi) * 0.3 * env)
	end
	return love.audio.newSource(sd, "static")
end

function love.load()
	love.graphics.setFont(love.graphics.newFont(28))
	beep = makeBeep()
end

function love.joystickadded(j) pads[j] = true end
function love.joystickremoved(j) pads[j] = nil end

function love.gamepadpressed(j, b)
	beep:stop(); beep:play()
	if (b == "start" and j:isGamepadDown("back")) or (b == "back" and j:isGamepadDown("start")) then
		love.event.quit()
	end
end

function love.keypressed(k) if k == "escape" then love.event.quit() end end

function love.touchpressed(id, x, y) touches[id] = { x, y } end
function love.touchmoved(id, x, y) touches[id] = { x, y } end
function love.touchreleased(id) touches[id] = nil end

function love.update(dt) t = t + dt end

function love.draw()
	local w, h = love.graphics.getDimensions()
	love.graphics.clear(0.08, 0.09, 0.14)

	-- spinning square so dropped frames are visible
	love.graphics.push()
	love.graphics.translate(w - 160, h - 160)
	love.graphics.rotate(t)
	love.graphics.setColor(0.3, 0.7, 1.0)
	love.graphics.rectangle("fill", -60, -60, 120, 120)
	love.graphics.pop()

	love.graphics.setColor(1, 1, 1)
	local name, version, vendor, device = love.graphics.getRendererInfo()
	local y = 30
	local function line(s) love.graphics.print(s, 30, y); y = y + 40 end
	local major, minor = love.getVersion()
	line(("LÖVE %d.%d  %s"):format(major, minor, love.system.getOS()))
	line(("%s %s"):format(name, version))
	line(("%s / %s"):format(vendor, device))
	line(("%dx%d  FPS %d"):format(w, h, love.timer.getFPS()))
	y = y + 20
	for j in pairs(pads) do
		line(j:getName() .. "  (" .. j:getGamepadType() .. ")")
		local down = {}
		-- a/b/x/y are positions (south/east/west/north, Xbox names), not the
		-- printed labels; Nintendo-layout pads print B/A/Y/X there.
		local gt = j:getGamepadType()
		-- HAKO_PAD_LAYOUT=label already maps a/b/x/y to the printed letters
		local nintendo = (gt:find("switch") ~= nil or gt:find("joycon") ~= nil)
			and os.getenv("HAKO_PAD_LAYOUT") ~= "label"
		local NLABEL = { a = "B", b = "A", x = "Y", y = "X" }
		for _, b in ipairs(BUTTONS) do
			if j:isGamepadDown(b) then
				down[#down + 1] = (nintendo and NLABEL[b]) and (b .. "[" .. NLABEL[b] .. "]") or b
			end
		end
		line("  " .. (#down > 0 and table.concat(down, " ") or "-"))
		line(("  L(%.2f, %.2f)  R(%.2f, %.2f)  LT %.2f  RT %.2f"):format(
			j:getGamepadAxis("leftx"), j:getGamepadAxis("lefty"),
			j:getGamepadAxis("rightx"), j:getGamepadAxis("righty"),
			j:getGamepadAxis("triggerleft"), j:getGamepadAxis("triggerright")))
	end
	if next(pads) == nil then line("no gamepad") end
	line("START + SELECT: quit")

	love.graphics.setColor(1, 0.5, 0.3)
	for _, p in pairs(touches) do love.graphics.circle("line", p[1], p[2], 50) end
end

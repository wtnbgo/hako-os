// SPDX-License-Identifier: MIT
# include "Input.hpp"

# if __has_include(<SDL3/SDL_gamepad.h>)
#	include <SDL3/SDL_gamepad.h>
#	define HK_HAVE_SDL3 1
# else
#	define HK_HAVE_SDL3 0
# endif

namespace hk
{
	namespace
	{
		// SDL_GamepadButton numbers (Siv3D's SDL3 backend reports buttons in this order)
		enum PadButton : uint8
		{
			South = 0, East = 1, West = 2, North = 3,
			Back = 4, Guide = 5, PadStart = 6, LeftStick = 7, RightStick = 8,
			LeftShoulder = 9, RightShoulder = 10,
			DpadUp = 11, DpadDown = 12, DpadLeft = 13, DpadRight = 14,
		};

		constexpr double StickDeadZone = 0.35;
		constexpr double TriggerThreshold = 0.5;

		struct State
		{
			std::array<bool, static_cast<size_t>(Btn::Count_)> now{};
			std::array<bool, static_cast<size_t>(Btn::Count_)> prev{};
			std::array<double, static_cast<size_t>(Btn::Count_)> heldTime{};
			std::array<double, static_cast<size_t>(Btn::Count_)> nextRepeat{};

			Vec2 stick{ 0, 0 };
			Vec2 rstick{ 0, 0 };
			bool padConnected = false;
			bool padLastUsed = false;
			bool nintendo = false;
			bool layoutChecked = false;

			// pointer
			bool padPointer = false;
			double pointerSpeed = 600.0;
			Vec2 pos{ 0, 0 };
			bool fromPad = false;
			bool pNow = false, pPrev = false;
			Optional<Point> lastCursor;
		};

		State g;

		[[nodiscard]] size_t I(Btn b) { return static_cast<size_t>(b); }

		[[nodiscard]] bool PadButton(const detail::Gamepad_impl& pad, uint8 b)
		{
			return (b < pad.buttons.size()) && pad.buttons[b].pressed();
		}

		[[nodiscard]] double PadAxis(const detail::Gamepad_impl& pad, size_t a)
		{
			return (a < pad.axes.size()) ? pad.axes[a] : 0.0;
		}

		// A / B / X / Y as printed on the pad. SDL knows it from the gamepad type
		// (SDL_GAMECONTROLLERTYPE, e.g. SwitchPro for a Nintendo layout).
		[[nodiscard]] bool DetectNintendoLayout()
		{
		# if HK_HAVE_SDL3
			int count = 0;
			if (SDL_JoystickID* ids = SDL_GetGamepads(&count))
			{
				bool nintendo = false;
				if (0 < count)
				{
					const SDL_GamepadType type = SDL_GetGamepadTypeForID(ids[0]);
					nintendo = (SDL_GetGamepadButtonLabelForType(type, SDL_GAMEPAD_BUTTON_EAST) == SDL_GAMEPAD_BUTTON_LABEL_A);
				}
				SDL_free(ids);
				return nintendo;
			}
		# endif
			return false;
		}
	}

	void Update()
	{
		const double dt = Scene::DeltaTime();
		g.prev = g.now;
		g.now.fill(false);

		// --- pad -------------------------------------------------------------------
		const auto& pad = Gamepad(0);
		g.padConnected = pad.isConnected();
		bool padActivity = false;
		Vec2 padStick{ 0, 0 };

		if (g.padConnected)
		{
			if (not g.layoutChecked)
			{
				g.nintendo = DetectNintendoLayout();
				g.layoutChecked = true;
			}

			const uint8 confirm = (g.nintendo ? East : South);
			const uint8 cancel = (g.nintendo ? South : East);
			const uint8 x = (g.nintendo ? North : West);
			const uint8 y = (g.nintendo ? West : North);

			auto set = [&](Btn b, bool on) { if (on) { g.now[I(b)] = true; padActivity = true; } };
			set(Btn::Confirm, PadButton(pad, confirm));
			set(Btn::Cancel, PadButton(pad, cancel));
			set(Btn::X, PadButton(pad, x));
			set(Btn::Y, PadButton(pad, y));
			set(Btn::L, PadButton(pad, LeftShoulder));
			set(Btn::R, PadButton(pad, RightShoulder));
			set(Btn::ZL, (TriggerThreshold < PadAxis(pad, 4)));
			set(Btn::ZR, (TriggerThreshold < PadAxis(pad, 5)));
			set(Btn::Start, PadButton(pad, PadStart));
			set(Btn::Select, PadButton(pad, Back));
			set(Btn::Home, PadButton(pad, Guide));

			const bool du = PadButton(pad, DpadUp), dd = PadButton(pad, DpadDown);
			const bool dl = PadButton(pad, DpadLeft), dr = PadButton(pad, DpadRight);

			Vec2 ls{ PadAxis(pad, 0), PadAxis(pad, 1) };
			if (ls.length() < StickDeadZone) { ls = Vec2{ 0, 0 }; }
			Vec2 rs{ PadAxis(pad, 2), PadAxis(pad, 3) };
			if (rs.length() < StickDeadZone) { rs = Vec2{ 0, 0 }; }
			g.rstick = rs;

			padStick = ls + Vec2{ (dr - dl), (dd - du) };
			if (1.0 < padStick.length()) { padStick.setLength(1.0); }

			set(Btn::Up, (du || (ls.y < -0.5)));
			set(Btn::Down, (dd || (0.5 < ls.y)));
			set(Btn::Left, (dl || (ls.x < -0.5)));
			set(Btn::Right, (dr || (0.5 < ls.x)));

			if (not padStick.isZero() || not rs.isZero()) { padActivity = true; }
		}
		else
		{
			g.rstick = Vec2{ 0, 0 };
			g.layoutChecked = false;
		}

		// --- keyboard ------------------------------------------------------------------
		bool keyActivity = false;
		auto key = [&](Btn b, bool on) { if (on) { g.now[I(b)] = true; keyActivity = true; } };
		key(Btn::Confirm, (KeyEnter.pressed() || KeySpace.pressed() || KeyZ.pressed()));
		key(Btn::Cancel, (KeyBackspace.pressed() || KeyX.pressed()));
		key(Btn::X, KeyC.pressed());
		key(Btn::Y, KeyV.pressed());
		key(Btn::L, KeyQ.pressed());
		key(Btn::R, KeyW.pressed());
		key(Btn::ZL, KeyA.pressed());
		key(Btn::ZR, KeyS.pressed());
		key(Btn::Start, KeyEnter.pressed());
		key(Btn::Select, KeyTab.pressed());
		key(Btn::Up, KeyUp.pressed());
		key(Btn::Down, KeyDown.pressed());
		key(Btn::Left, KeyLeft.pressed());
		key(Btn::Right, KeyRight.pressed());

		const Vec2 keyStick{ (KeyRight.pressed() - KeyLeft.pressed()), (KeyDown.pressed() - KeyUp.pressed()) };
		g.stick = padStick + keyStick;
		if (1.0 < g.stick.length()) { g.stick.setLength(1.0); }

		if (padActivity) { g.padLastUsed = true; }
		if (keyActivity) { g.padLastUsed = false; }

		// --- key repeat ------------------------------------------------------------------
		for (size_t i = 0; i < g.now.size(); ++i)
		{
			g.heldTime[i] = (g.now[i] ? (g.heldTime[i] + dt) : 0.0);
		}

		// --- pointer -------------------------------------------------------------------
		const Point cursor = Cursor::Pos();
		const bool mouseMoved = (g.lastCursor && (*g.lastCursor != cursor));
		g.lastCursor = cursor;

		if (mouseMoved || MouseL.pressed() || MouseL.up())
		{
			g.fromPad = false;
			g.padLastUsed = false;
			g.pos = Cursor::PosF();
		}
		else if (g.padPointer && not padStick.isZero())
		{
			if (not g.fromPad)
			{
				g.fromPad = true;
			}
			g.pos += (padStick * g.pointerSpeed * dt);
			g.pos.x = Clamp(g.pos.x, 0.0, static_cast<double>(Scene::Width() - 1));
			g.pos.y = Clamp(g.pos.y, 0.0, static_cast<double>(Scene::Height() - 1));
		}
		else if (not g.fromPad)
		{
			g.pos = Cursor::PosF();
		}

		g.pPrev = g.pNow;
		g.pNow = (MouseL.pressed() || (g.padPointer && g.now[I(Btn::Confirm)]));
	}

	bool Down(Btn b) { return g.now[I(b)] && not g.prev[I(b)]; }
	bool Pressed(Btn b) { return g.now[I(b)]; }
	bool Up(Btn b) { return not g.now[I(b)] && g.prev[I(b)]; }

	bool Repeat(Btn b)
	{
		if (Down(b))
		{
			g.nextRepeat[I(b)] = 0.4;
			return true;
		}
		if (g.now[I(b)] && (g.nextRepeat[I(b)] <= g.heldTime[I(b)]))
		{
			g.nextRepeat[I(b)] += 0.08;
			return true;
		}
		return false;
	}

	Vec2 Stick() { return g.stick; }
	Vec2 RStick() { return g.rstick; }
	bool PadConnected() { return g.padConnected; }
	bool PadLastUsed() { return g.padLastUsed; }
	bool NintendoLayout() { return g.nintendo; }

	void SetPadPointer(const bool enabled, const double speed)
	{
		g.padPointer = enabled;
		g.pointerSpeed = speed;
		if (not enabled)
		{
			g.fromPad = false;
		}
		else
		{
			g.pos = Vec2{ Scene::Center() };
		}
	}

	Vec2 Pos() { return g.pos; }

	Vec2 Pos(const Mat3x2& transform)
	{
		return transform.inverse().transformPoint(g.pos);
	}

	bool PDown() { return g.pNow && not g.pPrev; }
	bool PPressed() { return g.pNow; }
	bool PUp() { return not g.pNow && g.pPrev; }
	bool PointerFromPad() { return g.padPointer && g.fromPad; }

	void DrawPadPointer()
	{
		if (not PointerFromPad())
		{
			return;
		}

		const Vec2 p = g.pos;
		const Triangle arrow{ p, p.movedBy(0, 26), p.movedBy(18, 20) };
		arrow.movedBy(2, 2).draw(ColorF{ 0.0, 0.35 });
		arrow.draw(g.pNow ? ColorF{ 1.0, 0.8, 0.3 } : ColorF{ 1.0 });
		arrow.drawFrame(2, ColorF{ 0.1 });
	}

	bool MenuRequested()
	{
		return Down(Btn::Home)
			|| (Pressed(Btn::Start) && Pressed(Btn::Select) && (Down(Btn::Start) || Down(Btn::Select)))
			|| KeyEscape.down();
	}
}

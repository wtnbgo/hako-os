// Siv3D game collection for hako: input shared by all games.
//
// Pad, keyboard, mouse and touch are merged into a few "buttons" and one pointer,
// so every game can be played with the built-in pad of a handheld as well as with
// a mouse / touch screen:
//
//   hk::Down(hk::Btn::Confirm)   pad A (as printed: Nintendo layout = right button),
//                                keyboard Enter / Space / Z
//   hk::Stick()                  left stick + d-pad + arrow keys (length <= 1)
//   hk::Pos(), hk::PDown() ...   the pointer: mouse / touch, or moved by the stick
//                                and "clicked" with A when the game enables it
//                                (hk::SetPadPointer(true))
//
// SPDX-License-Identifier: MIT
# pragma once
# include <Siv3D.hpp>

namespace hk
{
	enum class Btn : uint8
	{
		Confirm,	// A (as printed), Enter / Space / Z
		Cancel,		// B (as printed), Backspace / X
		X,			// X (as printed), C
		Y,			// Y (as printed), V
		L,			// L shoulder, Q
		R,			// R shoulder, W
		ZL,			// left trigger, A
		ZR,			// right trigger, S
		Start,		// START, Enter
		Select,		// SELECT (BACK), Tab
		Home,		// HOME / guide
		Up, Down, Left, Right,	// d-pad, left stick, arrow keys
		Count_
	};

	/// @brief Reads the devices. Call once at the start of every frame (before any game code).
	void Update();

	[[nodiscard]] bool Down(Btn b);
	[[nodiscard]] bool Pressed(Btn b);
	[[nodiscard]] bool Up(Btn b);

	/// @brief Down, then repeated while held (for menus / cursors)
	[[nodiscard]] bool Repeat(Btn b);

	/// @brief Left stick + d-pad + arrow keys, length <= 1 (y down)
	[[nodiscard]] Vec2 Stick();

	/// @brief Right stick (y down)
	[[nodiscard]] Vec2 RStick();

	/// @brief Whether a pad is connected
	[[nodiscard]] bool PadConnected();

	/// @brief The last input came from the pad (for showing pad hints)
	[[nodiscard]] bool PadLastUsed();

	/// @brief Nintendo layout (A right, B bottom) detected for the pad
	[[nodiscard]] bool NintendoLayout();

	// --- the pointer -----------------------------------------------------------------

	/// @brief Let the stick move the pointer and A click with it (off by default)
	void SetPadPointer(bool enabled, double speed = 600.0);

	/// @brief Pointer position in scene coordinates
	[[nodiscard]] Vec2 Pos();

	/// @brief Pointer position seen through a 2D transform (e.g. a Camera2D or a Transformer2D)
	[[nodiscard]] Vec2 Pos(const Mat3x2& transform);

	[[nodiscard]] bool PDown();
	[[nodiscard]] bool PPressed();
	[[nodiscard]] bool PUp();

	/// @brief The pointer is being moved by the pad (draw a cursor)
	[[nodiscard]] bool PointerFromPad();

	template <class Shape>
	[[nodiscard]] bool Over(const Shape& shape) { return shape.intersects(Pos()); }

	template <class Shape>
	[[nodiscard]] bool Clicked(const Shape& shape) { return PDown() && Over(shape); }

	template <class Shape>
	[[nodiscard]] bool PressedOn(const Shape& shape) { return PPressed() && Over(shape); }

	/// @brief Draws a cursor at the pointer when the pad moves it
	void DrawPadPointer();

	// --- leaving a game --------------------------------------------------------------

	/// @brief START + SELECT, HOME, or Escape: back to the menu
	[[nodiscard]] bool MenuRequested();
}

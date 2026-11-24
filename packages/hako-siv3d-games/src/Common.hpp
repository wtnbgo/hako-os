// Siv3D game collection for hako: the game interface and shared helpers.
// SPDX-License-Identifier: MIT
# pragma once
# include <Siv3D.hpp>
# include "Input.hpp"

namespace hk
{
	/// @brief One game. The constructor does what the sample did before its main loop,
	/// update() one turn of the main loop (update and draw).
	class IGame
	{
	public:

		virtual ~IGame() = default;

		virtual void update() = 0;
	};

	struct GameInfo
	{
		/// @brief Number on the Siv3D samples page (https://siv3d.github.io/ja-jp/samples/games/)
		int32 number = 0;

		String title;

		/// @brief Controls, shown when the game starts (one line per item)
		Array<String> help;

		/// @brief Scene size the game was written for (letterboxed on the device)
		Size sceneSize{ 800, 600 };

		/// @brief Let the stick move the pointer and A click (games played with the mouse)
		bool padPointer = false;

		/// @brief Where the original comes from (for the license screen / README)
		String origin;

		std::function<std::unique_ptr<IGame>()> make;
	};

	/// @brief All games, in the order of the samples page
	[[nodiscard]] const Array<GameInfo>& Games();

	/// @brief Shared fonts (created once)
	[[nodiscard]] const Font& UiFont();

	/// @brief A button usable with the pointer (mouse / touch / pad pointer).
	/// Replaces SimpleGUI::Button in the games, which only knows the mouse.
	bool Button(const String& label, const Vec2& center, double width = 160, bool enabled = true);
}

// Defines the factory of a game class, to be listed in Games.cpp
# define HK_GAME_FACTORY(Name, Class) \
	std::unique_ptr<hk::IGame> Name() { return std::make_unique<Class>(); }

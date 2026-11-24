// 5. タイピングゲーム (Typing game)
//
// Based on the Siv3D sample "5. タイピングゲーム"
//   https://siv3d.github.io/ja-jp/samples/games/#5-タイピングゲーム
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: an on-screen keyboard (d-pad + A, or touch) for devices without a
// keyboard; a physical keyboard works as before (TextInput); a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// hako: on-screen keyboard
	class OnScreenKeyboard
	{
	public:

		/// @return the typed character, or none
		Optional<char32> update()
		{
			Optional<char32> typed;

			// d-pad / arrow keys move the focus
			if (hk::Repeat(hk::Btn::Left)) { m_focus.x = (m_focus.x + Cols - 1) % Cols; m_padUsed = true; }
			if (hk::Repeat(hk::Btn::Right)) { m_focus.x = (m_focus.x + 1) % Cols; m_padUsed = true; }
			if (hk::Repeat(hk::Btn::Up)) { m_focus.y = (m_focus.y + Rows - 1) % Rows; m_padUsed = true; }
			if (hk::Repeat(hk::Btn::Down)) { m_focus.y = (m_focus.y + 1) % Rows; m_padUsed = true; }

			// pad A types the focused key (not keyboard Z / Space / Enter: those are typed text)
			const bool padPress = (hk::PadLastUsed() && hk::Down(hk::Btn::Confirm));

			// pad shortcuts: Y = Shift, X = space
			if (hk::PadLastUsed() && hk::Down(hk::Btn::Y)) { m_shift = not m_shift; }
			if (hk::PadLastUsed() && hk::Down(hk::Btn::X)) { typed = U' '; }

			for (int32 y = 0; y < Rows; ++y)
			{
				for (int32 x = 0; x < Cols; ++x)
				{
					const char32 key = keyAt(x, y);
					const RectF r = keyRect(x, y);

					if (key == 0)
					{
						continue;
					}

					const bool focused = (m_padUsed && (Point{ x, y } == m_focus));
					const bool clicked = hk::Clicked(r);

					if (clicked)
					{
						m_padUsed = false;
					}

					if (clicked || (focused && padPress))
					{
						if (key == Shift)
						{
							m_shift = not m_shift;
						}
						else
						{
							typed = (m_shift ? ToUpper(key) : key);
							m_shift = false;
						}
					}
				}
			}

			return typed;
		}

		void draw() const
		{
			const Font& font = hk::UiFont();

			for (int32 y = 0; y < Rows; ++y)
			{
				for (int32 x = 0; x < Cols; ++x)
				{
					const char32 key = keyAt(x, y);

					if (key == 0)
					{
						continue;
					}

					// wide keys are drawn once, from their first cell
					if ((0 < x) && (keyAt(x - 1, y) == key) && ((key == Shift) || (key == U' ')))
					{
						continue;
					}

					RectF r = keyRect(x, y);
					int32 span = 1;
					while (((x + span) < Cols) && (keyAt(x + span, y) == key)) { ++span; }
					r.w = (span * KeySize.x - 6);

					const bool focused = (m_padUsed && (m_focus.y == y) && InRange(m_focus.x, x, (x + span - 1)));
					const bool active = ((key == Shift) && m_shift);
					r.rounded(8).draw(focused ? ColorF{ 0.3, 0.55, 0.95 } : (active ? ColorF{ 0.95, 0.7, 0.3 } : ColorF{ 0.97 }))
						.drawFrame(1, ColorF{ 0.6 });

					const String label = (key == Shift) ? U"Shift" : (key == U' ') ? U"Space" : String(1, (m_shift ? ToUpper(key) : key));
					font(label).drawAt(((key == Shift) || (key == U' ')) ? 22 : 28, r.center(), (focused ? ColorF{ 1.0 } : ColorF{ 0.15 }));
				}
			}
		}

	private:

		static constexpr int32 Cols = 10;
		static constexpr int32 Rows = 4;
		static constexpr char32 Shift = U'\x01';
		static constexpr SizeF KeySize{ 74, 62 };
		static constexpr Vec2 Origin{ 33, 330 };

		Point m_focus{ 0, 0 };
		bool m_shift = true; // the sentences start with a capital letter
		bool m_padUsed = false;

		[[nodiscard]] static char32 keyAt(int32 x, int32 y)
		{
			static const std::array<String, Rows> rows =
			{
				String{ U"qwertyuiop" },
				String{ U"asdfghjkl'" },
				String{ U"zxcvbnm,.!" },
				String{ U"\x01\x01\x01          " },
			};
			return rows[y][x];
		}

		[[nodiscard]] static RectF keyRect(int32 x, int32 y)
		{
			return RectF{ (Origin.x + x * KeySize.x), (Origin.y + y * KeySize.y), (KeySize.x - 6), (KeySize.y - 6) };
		}
	};

	class Typing : public hk::IGame
	{
	public:

		Typing()
		{
			Scene::SetBackground(ColorF{ 0.6, 0.8, 0.7 });
		}

		void update() override
		{
			// テキスト入力（TextInputMode::DenyControl: エンターやタブ、バックスペースは受け付けない）
			TextInput::UpdateText(input, TextInputMode::DenyControl);

			// hako: on-screen keyboard
			if (const auto ch = keyboard.update())
			{
				input.push_back(*ch);
			}

			// 誤った入力が含まれていたら削除する
			while (not target.starts_with(input))
			{
				input.pop_back();
			}

			// 一致したら次の問題へ移る
			if (input == target)
			{
				// 問題文をランダムに選ぶ
				target = texts.choice();

				// 入力文字列をクリアする
				input.clear();
			}

			// 問題文を描画する
			font(target).draw(40, Vec2{ 40, 80 }, ColorF{ 0.98 });

			// 入力中の文字を描画する
			font(input).draw(40, Vec2{ 40, 80 }, ColorF{ 0.12 });

			keyboard.draw();
		}

	private:

		// 問題文のリスト
		const Array<String> texts =
		{
			U"Practice makes perfect.",
			U"Don't cry over spilt milk.",
			U"Faith will move mountains.",
			U"Nothing ventured, nothing gained.",
			U"Bad news travels fast.",
		};

		// 問題文をランダムに選ぶ
		String target = texts.choice();

		// 入力中の文字列
		String input;

		const Font font{ FontMethod::MSDF, 48, Typeface::Bold };

		OnScreenKeyboard keyboard;
	};
}

HK_GAME_FACTORY(MakeTyping, Typing)

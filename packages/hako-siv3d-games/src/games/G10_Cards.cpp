// 10. トランプを描く (Drawing playing cards)
//
// Based on the Siv3D sample "10. トランプを描く"
//   https://siv3d.github.io/ja-jp/samples/games/#10-トランプを描く
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: a card is chosen with the d-pad and turned over with A (touch / mouse
// as before); a game class. The scene is 1280x720 as in the sample (Window::Resize).
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	class Cards : public hk::IGame
	{
	public:

		Cards()
		{
			Scene::SetBackground(Palette::Darkgreen);
		}

		void update() override
		{
			const int32 count = static_cast<int32>(cards.size());

			// hako: d-pad moves the selection (13 cards a row)
			if (hk::Repeat(hk::Btn::Right)) { selected = Min(selected + 1, count - 1); padSelecting = true; }
			if (hk::Repeat(hk::Btn::Left)) { selected = Max(selected - 1, 0); padSelecting = true; }
			if (hk::Repeat(hk::Btn::Down)) { selected = Min(selected + 13, count - 1); padSelecting = true; }
			if (hk::Repeat(hk::Btn::Up)) { selected = Max(selected - 13, 0); padSelecting = true; }

			if (padSelecting && hk::Down(hk::Btn::Confirm))
			{
				cards[selected].flip();
			}

			for (size_t i = 0; i < cards.size(); ++i)
			{
				const Vec2 center{ (100 + i % 13 * 90), (100 + (i / 13) * 130) };

				if (hk::Over(pack.regionAt(center)))
				{
					Cursor::RequestStyle(CursorStyle::Hand);

					if (hk::PDown())
					{
						padSelecting = false;

						// カードをめくる
						cards[i].flip();
					}
				}

				// カードを描画する
				pack(cards[i]).drawAt(center);

				if (padSelecting && (static_cast<int32>(i) == selected))
				{
					pack.regionAt(center).stretched(4).drawFrame(4, Palette::Yellow);
				}
			}
		}

	private:

		// カードの幅が 75 ピクセルで裏面が赤色のカードパックを作成
		const PlayingCard::Pack pack{ 75, Palette::Red };

		// ジョーカーの枚数
		static constexpr int32 NumJokers = 2;

		// 52 枚 + ジョーカーを含むカードを作成する
		Array<PlayingCard::Card> cards = PlayingCard::CreateDeck(NumJokers);

		// hako: the card chosen with the d-pad
		int32 selected = 0;
		bool padSelecting = false;
	};
}

HK_GAME_FACTORY(MakeCards, Cards)

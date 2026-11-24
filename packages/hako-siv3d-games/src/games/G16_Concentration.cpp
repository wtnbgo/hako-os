// 16. 神経衰弱 (Concentration)
//
// Based on games/003 "神経衰弱" of Reputeless/games (CC0 1.0 Universal)
//   https://github.com/Reputeless/games/blob/20cd026327799dbb897c90d36732a32d2ee0934d/games/003/A.md
// Changes for hako: a card is chosen with the d-pad and turned with A (touch / mouse as
// before); a new game with A once all pairs are found; the scene is 1280x720 as in the
// sample (Window::Resize); a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// i 枚目のトランプをどこに描画するか座標を返す関数
	Vec2 IndexToPos(size_t i)
	{
		return{ (100.0 + i % 11 * 90), (100.0 + i / 11 * 130) };
	}

	// 現在の状態をチェックしてカードが 2 枚オモテになっていたら
	// 同じ数字の場合: それらのカードをテーブル上に存在しない設定に
	// 異なる数字の場合: それらをウラに戻す
	// という操作をする関数
	void Update(Optional<size_t>& first, Optional<size_t>& second, Array<PlayingCard::Card>& cards, Array<bool>& cardsOnTable)
	{
		// まだ 2 枚めくっていない場合は何もしない
		if ((not first) || (not second))
		{
			return;
		}

		auto& card1 = cards[*first];
		auto& card2 = cards[*second];

		// カードの数字が一致したら
		if (card1.rank == card2.rank)
		{
			// 2 枚のカードをテーブル上に存在しない設定にする
			cardsOnTable[*first] = false;
			cardsOnTable[*second] = false;
		}
		else
		{
			// 2 枚のカードをウラ面に戻す
			cards[*first].isFaceSide = false;
			cards[*second].isFaceSide = false;
		}

		// 何もめくっていない状態にする
		first.reset();
		second.reset();
	}

	class Concentration : public hk::IGame
	{
	public:

		Concentration()
		{
			// 背景色を設定する
			Scene::SetBackground(Palette::Darkgreen);
			reset();
		}

		void update() override
		{
			const int32 count = static_cast<int32>(cards.size());

			// 2 枚目をめくってから 1 秒以上経過したら
			if (1.0s <= timeFromSecond)
			{
				Update(first, second, cards, cardsOnTable);

				// ストップウォッチを初期状態にする
				timeFromSecond.reset();
			}

			// hako: d-pad moves the selection (11 cards a row)
			if (hk::Repeat(hk::Btn::Right)) { selected = Min(selected + 1, count - 1); padSelecting = true; }
			if (hk::Repeat(hk::Btn::Left)) { selected = Max(selected - 1, 0); padSelecting = true; }
			if (hk::Repeat(hk::Btn::Down)) { selected = Min(selected + 11, count - 1); padSelecting = true; }
			if (hk::Repeat(hk::Btn::Up)) { selected = Max(selected - 11, 0); padSelecting = true; }

			const bool cleared = (not cardsOnTable.contains(true));

			if (cleared)
			{
				if (hk::Down(hk::Btn::Confirm) || hk::PDown())
				{
					reset();
				}
			}
			// ストップウォッチが動いていなければ
			else if (not timeFromSecond.isRunning())
			{
				for (size_t i = 0; i < cards.size(); ++i)
				{
					// カードが存在しない場合、すでにオモテを向いているカードはスキップする
					if ((not cardsOnTable[i]) || cards[i].isFaceSide)
					{
						continue;
					}

					// カードの長方形
					const RectF region = pack.regionAt(IndexToPos(i));

					const bool pointed = hk::Over(region);
					const bool padPicked = (padSelecting && (static_cast<int32>(i) == selected) && hk::Down(hk::Btn::Confirm));

					// カード上にマウスカーソルがあれば (hako: or A on the selected card)
					if (pointed || padPicked)
					{
						if (pointed)
						{
							Cursor::RequestStyle(CursorStyle::Hand);
						}

						// 左クリックしたら
						if ((pointed && hk::PDown()) || padPicked)
						{
							if (pointed && hk::PDown())
							{
								padSelecting = false;
							}

							if (not first)
							{
								first = i;
							}
							else
							{
								second = i;
								timeFromSecond.start();
							}

							// カードをオモテ面にする
							cards[i].isFaceSide = true;
						}

						if (pointed)
						{
							break;
						}
					}
				}
			}

			// 各カードについて
			for (size_t i = 0; i < cards.size(); ++i)
			{
				if (padSelecting && (static_cast<int32>(i) == selected))
				{
					pack.regionAt(IndexToPos(i)).stretched(4).drawFrame(4, Palette::Yellow);
				}

				// もう存在しない場合はスキップする
				if (not cardsOnTable[i])
				{
					continue;
				}

				pack(cards[i]).drawAt(IndexToPos(i));
			}

			if (cleared)
			{
				hk::UiFont()(U"クリア！  A / タッチ: もう一度").drawAt(40, Scene::Center());
			}
		}

	private:

		// トランプカードのデータを初期化 (カードの幅 75 ピクセル, ウラ面の色は暗い赤)
		const PlayingCard::Pack pack{ 75, ColorF{ 0.7, 0.1, 0.0 } };

		Array<PlayingCard::Card> cards;

		// それぞれのカードがテーブル上に存在するかを記録する配列
		Array<bool> cardsOnTable;

		// 1 枚目 / 2 枚目としてめくったカードのインデックス
		Optional<size_t> first, second;

		// 2 枚目をめくってからの経過時間を計るストップウォッチ
		Stopwatch timeFromSecond;

		// hako
		int32 selected = 0;
		bool padSelecting = false;

		void reset()
		{
			// 1 セット (52 枚) のトランプ (ジョーカー 0 枚, ウラ面) をシャッフルする
			cards = PlayingCard::CreateDeck(0, false);
			cards.shuffle();
			cardsOnTable = Array<bool>(cards.size(), true);
			first.reset();
			second.reset();
			timeFromSecond.reset();
		}
	};
}

HK_GAME_FACTORY(MakeConcentration, Concentration)

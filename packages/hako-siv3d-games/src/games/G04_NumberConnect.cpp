// 4. 数つなぎ (Connect the numbers)
//
// Based on the Siv3D sample "4. 数つなぎ"
//   https://siv3d.github.io/ja-jp/samples/games/#4-数つなぎ
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: the bubbles are touched with the pointer, which the stick also moves
// (touch: slide the finger over them); the level is drawn on the screen (no window title
// on the device); a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	struct Bubble
	{
		// バブルの円の半径
		static constexpr int32 Radius = 30;

		// バブルの円
		Circle circle;

		// バブルのインデックス
		int32 index;

		// 接続済みなら true に
		bool connected = false;

		void draw(const Font& font) const
		{
			if (connected)
			{
				circle.drawShadow(Vec2{ 1, 2 }, 10, 3).draw()
					.drawFrame(2, 0, ColorF{ 0.3, 0.6, 1.0 });
			}
			else
			{
				circle.draw();
			}

			font(index + 1).drawAt(36, circle.center, ColorF{ 0.25 });
		}
	};

	// バブルどうしが重なっていないかチェックする
	bool CheckBubbles(const Array<Bubble>& bubbles)
	{
		for (size_t i = 0; i < bubbles.size(); ++i)
		{
			for (size_t k = (i + 1); k < bubbles.size(); ++k)
			{
				// 重なっている
				if (bubbles[i].circle.stretched(5)
					.intersects(bubbles[k].circle.stretched(5)))
				{
					return false;
				}
			}
		}

		return true;
	}

	// 指定した個数のバブルを重ならないように生成する
	Array<Bubble> MakeBubbles(int32 count)
	{
		Array<Bubble> bubbles(count);

		do
		{
			for (int32 i = 0; i < count; ++i)
			{
				// バブルのインデックス
				bubbles[i].index = i;

				// バブルの円
				bubbles[i].circle.set(RandomVec2(Circle{ Scene::Center(), (Scene::Height() / 2 - Bubble::Radius) }), Bubble::Radius);
			}
		} while (not CheckBubbles(bubbles));

		return bubbles;
	}

	// 指定したレベルにおけるバブルの個数
	constexpr int32 GetBubbleCount(int32 level)
	{
		return Min(level, 15);
	}

	// 指定したレベルにおける制限時間（秒）
	constexpr Duration GetTime(int32 level)
	{
		return Duration{ (level <= 15) ? 8.0 : 8.0 - Min((level - 15) * 0.05, 2.0) };
	}

	class NumberConnect : public hk::IGame
	{
	public:

		NumberConnect()
		{
			Scene::SetBackground(Palette::White);
		}

		void update() override
		{
			const double delta = Scene::DeltaTime();

			for (auto& bubble : bubbles)
			{
				if ((bubble.index == connected)
					&& (not bubble.connected)
					&& hk::Over(bubble.circle.stretched(10)))
				{
					// 接続済みにする
					bubble.connected = true;

					// 接続数を増やす
					++connected;

					// エフェクトを追加する
					effect.add([pos = hk::Pos()](double t)
					{
						Circle{ pos, (Bubble::Radius + t * 200) }.drawFrame(2, 0, ColorF{ 0.2, 0.5, 1.0, (1.0 - t * 2.5) });
						return (t < 0.4);
					});

					// バブルの数字に応じて効果音を鳴らす
					sounds[bubble.index].playOneShot(0.8);
				}

				// バブルを円周に沿って移動させる
				bubble.circle.center = OffsetCircular{ Scene::Center(), bubble.circle.center }
					.rotate((IsEven(bubble.index) ? 20_deg : -20_deg) * delta);
			}

			// バブルをすべてつなぐか、時間切れになったら
			if (const bool failed = timer.reachedZero();
				(connected == GetBubbleCount(level)) || failed)
			{
				// レベルを更新する
				level = (failed ? 1 : ++level);

				// 接続数をリセットする
				connected = 0;

				// 制限時間をリセットする
				timer = Timer{ GetTime(level), StartImmediately::Yes };

				// バブルを再生成する
				bubbles = MakeBubbles(GetBubbleCount(level));

				// ハイスコアを更新する
				highScore = Max(highScore, level);

				// タイトルを更新する
				Window::SetTitle(U"Level {} (High score: {})"_fmt(level, highScore));
			}

			// 制限時間を表す背景を描画する
			RectF{ Scene::Width(), (Scene::Height() * timer.progress0_1()) }.draw(HSV{ (level * 30), 0.3, 0.9 });

			// バブルをつなぐ線を描画する
			for (int32 i = 0; i < (connected - 1); ++i)
			{
				Line{ bubbles[i].circle.center, bubbles[i + 1].circle.center }.draw(3, Palette::Orange);
			}

			// バブルを描画する
			for (const auto& bubble : bubbles)
			{
				bubble.draw(font);
			}

			// エフェクトを描画する
			effect.update();

			// hako: the window title is not visible on the device
			font(U"Level {}  (High score: {})"_fmt(level, highScore)).draw(20, Vec2{ 12, 8 }, ColorF{ 0.3 });
		}

	private:

		const Font font{ FontMethod::MSDF, 48, Typeface::Medium };

		Effect effect;

		// 効果音を作成する
		const Array<PianoKey> keys = { PianoKey::C5,  PianoKey::D5, PianoKey::E5, PianoKey::F5, PianoKey::G5,
			PianoKey::A5, PianoKey::B5, PianoKey::C6, PianoKey::D6, PianoKey::E6,
			PianoKey::F6, PianoKey::G6, PianoKey::A6, PianoKey::B6, PianoKey::C7 };
		const Array<Audio> sounds = keys.map([](auto k) { return Audio{ GMInstrument::Glockenspiel, k, 0.3s }; });

		// ハイスコア
		int32 highScore = 0;

		// 現在のレベル
		int32 level = 1;

		// 接続数
		int32 connected = 0;

		// 残り時間のタイマー
		Timer timer{ GetTime(level), StartImmediately::Yes };

		// バブル
		Array<Bubble> bubbles = MakeBubbles(GetBubbleCount(level));
	};
}

HK_GAME_FACTORY(MakeNumberConnect, NumberConnect)

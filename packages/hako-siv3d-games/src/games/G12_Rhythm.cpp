// 12. 音ゲー基礎 (Rhythm game basics)
//
// Based on the Siv3D sample "12. 音ゲー基礎"
//   https://siv3d.github.io/ja-jp/samples/games/#12-音ゲー基礎
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: the chart (notes.txt of the sample) is built into the program; the four
// lanes are also played with the pad (d-pad ← ↓, B, A as printed) and by touching a lane;
// the elapsed time is drawn instead of Print; A / a touch starts again at the end;
// a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// ノート
	struct Note
	{
		// ノートの時刻
		int32 time;

		// 押すべきキーのインデックス (0, 1, 2, 3)
		int32 key;

		// 消えたら false
		bool active = true;
	};

	// hako: the chart of the sample (notes.txt), built in
	Array<Note> LoadNotes()
	{
		// time (ms) and lane of each note, as in notes.txt
		constexpr std::array<std::pair<int32, int32>, 16> Chart =
		{ {
			{ 2000, 0 }, { 2500, 1 }, { 3000, 2 }, { 3500, 3 },
			{ 4000, 3 }, { 4500, 2 }, { 5000, 1 }, { 5500, 0 },
			{ 6000, 0 }, { 6500, 1 }, { 7000, 2 }, { 7500, 3 },
			{ 8000, 3 }, { 8500, 2 }, { 9000, 1 }, { 9500, 0 },
		} };

		Array<Note> notes;

		for (const auto& [time, key] : Chart)
		{
			notes.emplace_back(time, key);
		}

		return notes;
	}

	// ノートの座標を計算する関数
	Vec2 GetNotePos(const Note& note, int32 time)
	{
		const double x = (250 + note.key * 100);
		const double y = (500 - (note.time - time) * 0.25);
		return{ x, y };
	}

	// ノートを押したときのエフェクト
	struct NoteEffect : IEffect
	{
		Vec2 m_start;

		int32 m_score;

		Font m_font;

		NoteEffect(const Vec2& start, int32 score, const Font& font)
			: m_start{ start }
			, m_score{ score }
			, m_font{ font } {}

		bool update(double t) override
		{
			Circle{ m_start, (30 + t * 80) }.drawFrame(15 * (0.5 - t));

			if (m_score == 2)
			{
				m_font(U"Excellent").drawAt(32, m_start.movedBy(0, (-20 - t * 160)), Palette::Orange);
			}
			else if (m_score == 1)
			{
				m_font(U"Good").drawAt(32, m_start.movedBy(0, (-20 - t * 160)), Palette::Skyblue);
			}

			return (t < 0.5);
		}
	};

	class Rhythm : public hk::IGame
	{
	public:

		void update() override
		{
			// 経過時間（ミリ秒）
			const int32 time = stopwatch.ms();

			// hako: which lanes were hit this frame (keyboard A S D F, pad, touch)
			std::array<bool, 4> hit{};
			const std::array<bool, 4> padHit =
			{
				hk::Down(hk::Btn::Left),
				hk::Down(hk::Btn::Down),
				hk::Down(hk::Btn::Cancel),
				hk::Down(hk::Btn::Confirm),
			};

			for (size_t i = 0; i < Keys.size(); ++i)
			{
				const RectF lane{ Arg::bottomCenter((250 + i * 100), 600), 100, 600 };
				hit[i] = (Keys[i].down() || (hk::PadLastUsed() && padHit[i]) || hk::Clicked(lane));
				keyTransitions[i].update(hit[i]);
			}

			for (auto& note : notes)
			{
				// 消えているノートはスキップ
				if (not note.active)
				{
					continue;
				}

				// 現在のタイムとノートのタイムとの差（ミリ秒）
				const int32 diffMillisec = (time - note.time);

				// 差の絶対値が 250 ミリ秒未満なら
				if (Abs(diffMillisec) < 250)
				{
					// ノートに対応するキーが押されていたら
					if (hit[note.key])
					{
						// ノートを消す
						note.active = false;

						// ノートの座標
						const Vec2 notePos = GetNotePos(note, time);

						// エフェクトを追加する
						effect.add<NoteEffect>(Vec2{ notePos.x, 500 }, (Abs(diffMillisec) < 80 ? 2 : 1), font);
					}
				}

				// 250 ミリ秒以上の遅れはミス
				if (note.active && (250 <= diffMillisec))
				{
					// ノートを消す
					note.active = false;
				}
			}

			// hako: start again after the last note
			const bool finished = (notes.back().time + 1500 < time);
			if (finished && (hk::Down(hk::Btn::Confirm) || hk::PDown() || KeySpace.down()))
			{
				notes = LoadNotes();
				stopwatch.restart();
			}

			// 入力を描画する
			for (int32 i = 0; i < 4; ++i)
			{
				const double x = (250 + i * 100);
				RectF{ Arg::bottomCenter(x, 600), 80, 600 }
					.draw(Arg::top = ColorF{ 1.0, 0.0 }, Arg::bottom = ColorF{ 1.0, keyTransitions[i].easeOut() * 0.5 });
			}

			// 長方形を描画する
			Rect{ 0, 480, 800, 40 }.draw(ColorF{ 0.5 });

			// キー名を描画する (hako: the pad buttons when the pad is used)
			static const std::array<String, 4> PadNames = { U"←", U"↓", U"B", U"A" };
			for (int32 i = 0; i < 4; ++i)
			{
				const double x = (250 + i * 100);
				font(hk::PadLastUsed() ? PadNames[i] : Keys[i].name()).drawAt(20, Vec2{ x, 500 }, ColorF{ 0.7 });
			}

			// ノートを描画する
			for (const auto& note : notes)
			{
				// 消えているノートはスキップ
				if (not note.active)
				{
					continue;
				}

				// ノートの座標
				const Vec2 notePos = GetNotePos(note, time);

				// 画面内にあるノートのみ描画する
				if (-100.0 < notePos.y)
				{
					Circle{ notePos, 30 }.draw();
				}
			}

			// エフェクトの描画
			effect.update();

			// hako: the elapsed time (the sample used Print)
			font(time).draw(18, Vec2{ 10, 8 }, ColorF{ 0.8 });

			if (finished)
			{
				font(U"A / タッチ: もう一度").drawAt(28, Vec2{ 400, 300 });
			}
		}

	private:

		// ノート配列
		Array<Note> notes = LoadNotes();

		// 判定キー
		const Array<Input> Keys = { KeyA, KeyS, KeyD, KeyF };

		// キー入力エフェクトのトランジション
		Array<Transition> keyTransitions = Array<Transition>(4, Transition{ 0.0s, 0.2s });

		// 時間測定用ストップウォッチ
		Stopwatch stopwatch{ StartImmediately::Yes };

		// フォント
		const Font font{ FontMethod::MSDF, 48, Typeface::Heavy };

		// エフェクト管理
		Effect effect;
	};
}

HK_GAME_FACTORY(MakeRhythm, Rhythm)

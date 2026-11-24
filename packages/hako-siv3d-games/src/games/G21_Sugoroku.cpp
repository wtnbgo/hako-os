// 21. すごろくの基本 (Sugoroku basics)
//
// Based on the Siv3D sample "21. すごろくの基本"
//   https://siv3d.github.io/ja-jp/samples/games/#21-すごろくの基本
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: the dice are rolled with A as well as the button (touch / mouse);
// after the goal, A or the button starts again; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	/// @brief すごろくのマスの種類
	enum class SquareType
	{
		/// @brief スタート
		Start,

		/// @brief 通常
		Normal,

		/// @brief ゴール
		Goal,
	};

	/// @brief すごろくのマスの情報
	struct SquareInfo
	{
		/// @brief マスの位置
		Point pos;

		/// @brief マスの種類
		SquareType type;
	};

	/// @brief すごろくのマスを描画します。
	void DrawSquares(const Array<SquareInfo>& squares, const Font& font)
	{
		// マスの間の線を描画する
		for (size_t i = 0; i < (squares.size() - 1); ++i)
		{
			Line{ squares[i].pos, squares[i + 1].pos }
				.draw(32, ColorF{ 1.0, 0.95, 0.9 });
		}

		// 各マスについて
		for (const auto& square : squares)
		{
			if (square.type == SquareType::Start)
			{
				// スタートマスを描画する
				RoundRect{ Arg::center = square.pos, 144, 144, 24 }
					.draw(ColorF{ 0.5, 0.5, 0.8 }).drawFrame(4, ColorF{ 0.3 });

				// スタートの文字を描画する
				font(U"START")
					.drawAt(36, square.pos);
			}
			else if (square.type == SquareType::Normal)
			{
				// 通常マスを描画する
				RoundRect{ Arg::center = square.pos, 100, 100, 24 }
					.draw().drawFrame(4, ColorF{ 0.3 });
			}
			else if (square.type == SquareType::Goal)
			{
				// ゴールマスを描画する
				RoundRect{ Arg::center = square.pos, 144, 144, 24 }
					.draw(ColorF{ 0.8, 0.5, 0.5 }).drawFrame(4, ColorF{ 0.3 });

				// ゴールの文字を描画する
				font(U"GOAL")
					.drawAt(36, square.pos);
			}
		}
	}

	class Sugoroku : public hk::IGame
	{
	public:

		Sugoroku()
		{
			// 背景色を設定する
			Scene::SetBackground(ColorF{ 0.6, 0.8, 0.7 });
		}

		void update() override
		{
			const bool atGoal = (playerPos == (squares.size() - 1));

			// サイコロをふるボタン (hako: A also rolls; at the goal it starts again)
			const String label = ((atGoal && canRollDice) ? U"もう一度" : U"サイコロをふる");
			if (hk::Button(label, Vec2{ 140, 60 }, 200, canRollDice)
				|| (canRollDice && hk::Down(hk::Btn::Confirm)))
			{
				if (atGoal)
				{
					playerPos = 0;
				}
				else
				{
					// サイコロの回転を開始する
					diceTimer.start();

					// サイコロをふれないようにする
					canRollDice = false;
				}
			}

			// サイコロが回転中
			if (diceTimer.isRunning())
			{
				// サイコロの目を描画する
				Circle{ 300, 60, 40 }.draw();
				font(U"0/{}"_fmt(Random(1, 6)))
					.drawAt(30, Vec2{ 300, 60 }, ColorF{ 0.11 });
			}

			// サイコロの結果を確定させる
			if (diceTimer.reachedZero())
			{
				// サイコロの結果を決定する
				diceResult = Random(1, 6);

				// サイコロの回転を停止する
				diceTimer.reset();

				// プレイヤーの移動を開始する
				walkTimer.restart();
			}

			// サイコロの結果の表示
			if (diceResult)
			{
				// サイコロの目と歩数を描画する
				Circle{ 300, 60, 40 }.draw();
				font(U"{}/{}"_fmt(walkCount, diceResult))
					.drawAt(30, Vec2{ 300, 60 }, ColorF{ 0.11 });
			}

			// プレイヤーの移動
			if ((walkCount != diceResult) && walkTimer.reachedZero())
			{
				// 歩数を進める
				++walkCount;

				// ゴールの先に進まないようにする
				playerPos = Min((playerPos + 1), (squares.size() - 1));

				// 移動タイマーを再スタートさせる
				walkTimer.restart();
			}

			// 移動が完了したら
			if ((diceResult == walkCount) && walkTimer.reachedZero())
			{
				// サイコロの結果をリセットする
				diceResult = 0;

				// 歩数をリセットする
				walkCount = 0;

				// 移動タイマーをリセットする
				walkTimer.reset();

				// サイコロをふれるようにする
				canRollDice = true;
			}

			// すごろくのマスを描する
			DrawSquares(squares, font);

			// プレイヤーの丸影を描画する
			Ellipse{ squares[playerPos].pos.movedBy(0, 30), 40, 10 }
				.draw(ColorF{ 0.0, 0.2 });

			// プレイヤーを描画する
			playerEmoji.drawAt(squares[playerPos].pos.movedBy(0, -24));
		}

	private:

		// フォント
		const Font font{ FontMethod::MSDF, 30, Typeface::Bold };

		// プレイヤーの絵文字
		const Texture playerEmoji{ U"🐥"_emoji };

		// すごろくのマスの情報
		const Array<SquareInfo> squares = {
			{ {100, 500}, SquareType::Start },
			{ {300, 500}, SquareType::Normal },
			{ {500, 500}, SquareType::Normal },
			{ {700, 500}, SquareType::Normal },
			{ {700, 350}, SquareType::Normal },
			{ {500, 350}, SquareType::Normal },
			{ {300, 350}, SquareType::Normal },
			{ {100, 350}, SquareType::Normal },
			{ {100, 200}, SquareType::Normal },
			{ {300, 200}, SquareType::Normal },
			{ {500, 200}, SquareType::Normal },
			{ {700, 200}, SquareType::Goal },
		};

		// サイコロの回転タイマー
		Timer diceTimer{ 1s };

		// プレイヤーの移動タイマー
		Timer walkTimer{ 0.5s };

		// サイコロをふれるか
		bool canRollDice = true;

		// プレイヤーの位置
		size_t playerPos = 0;

		// サイコロの結果
		int32 diceResult = 0;

		// 歩数
		int32 walkCount = 0;
	};
}

HK_GAME_FACTORY(MakeSugoroku, Sugoroku)

// 1. ブロックくずし (Breakout)
//
// Based on the Siv3D sample "1. ブロックくずし"
//   https://siv3d.github.io/ja-jp/samples/games/#1-ブロックくずし
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: the paddle also moves with the pad (stick / d-pad) and follows
// touch; the ball restarts after it falls or all bricks are gone; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	class Breakout : public hk::IGame
	{
	public:

		Breakout()
		{
			reset();
		}

		void update() override
		{
			// paddle: the pointer (mouse / touch) or the stick
			if (hk::Stick().x != 0.0)
			{
				m_paddleX += (hk::Stick().x * PaddleSpeedPerSec * Scene::DeltaTime());
			}
			else if (not hk::PadLastUsed())
			{
				m_paddleX = hk::Pos().x;
			}
			m_paddleX = Clamp(m_paddleX, 30.0, (Scene::Width() - 30.0));

			// パドル | Paddle
			const Rect paddle{ Arg::center(static_cast<int32>(m_paddleX), 500), 60, 10 };

			const bool playing = (not m_bricks.isEmpty()) && (m_ball.y < (Scene::Height() + 20));

			if (playing)
			{
				// ボールを移動させる | Move the ball
				m_ball.moveBy(m_ballVelocity * Scene::DeltaTime());
			}
			else if (hk::Down(hk::Btn::Confirm) || hk::PDown())
			{
				reset();
			}

			// ブロックを順にチェックする | Check bricks in sequence
			for (auto it = m_bricks.begin(); it != m_bricks.end(); ++it)
			{
				// ブロックとボールが交差していたら | If block and ball intersect
				if (it->intersects(m_ball))
				{
					// ブロックの上辺、または底辺と交差していたら | If ball intersects with top or bottom of the block
					if (it->bottom().intersects(m_ball) || it->top().intersects(m_ball))
					{
						m_ballVelocity.y *= -1;
					}
					else // ブロックの左辺または右辺と交差していたら
					{
						m_ballVelocity.x *= -1;
					}

					// ブロックを配列から削除する（イテレータは無効になる） | Remove the block from the array
					m_bricks.erase(it);

					// これ以上チェックしない | Do not check any more
					break;
				}
			}

			// 天井にぶつかったら | If the ball hits the ceiling
			if ((m_ball.y < 0) && (m_ballVelocity.y < 0))
			{
				m_ballVelocity.y *= -1;
			}

			// 左右の壁にぶつかったら | If the ball hits the left or right wall
			if (((m_ball.x < 0) && (m_ballVelocity.x < 0))
				|| ((Scene::Width() < m_ball.x) && (0 < m_ballVelocity.x)))
			{
				m_ballVelocity.x *= -1;
			}

			// パドルにあたったら | If the ball hits the paddle
			if ((0 < m_ballVelocity.y) && paddle.intersects(m_ball))
			{
				// パドルの中心からの距離に応じてはね返る方向（速度ベクトル）を変える
				m_ballVelocity = Vec2{ (m_ball.x - paddle.center().x) * 10, -m_ballVelocity.y }.setLength(BallSpeedPerSec);
			}

			// すべてのブロックを描画する | Draw all the bricks
			for (const auto& brick : m_bricks)
			{
				// ブロックの Y 座標に応じて色を変える | Change the color of the brick depending on the Y coordinate
				brick.stretched(-1).draw(HSV{ brick.y - 40 });
			}

			// マウスカーソルを非表示にする | Hide the mouse cursor
			Cursor::RequestStyle(CursorStyle::Hidden);

			// ボールを描く | Draw the ball
			m_ball.draw();

			// パドルを描く | Draw the paddle
			paddle.rounded(3).draw();

			if (not playing)
			{
				const String message = (m_bricks.isEmpty() ? U"CLEAR!" : U"MISS");
				hk::UiFont()(message).drawAt(60, Vec2{ 400, 330 });
				hk::UiFont()(U"A / タッチ: もう一度").drawAt(24, Vec2{ 400, 390 }, ColorF{ 0.8 });
			}
		}

	private:

		// 1 つのブロックのサイズ | Size of a single block
		static constexpr Size BrickSize{ 40, 20 };

		// ボールの速さ（ピクセル / 秒） | Ball speed (pixels / second)
		static constexpr double BallSpeedPerSec = 480.0;

		// hako: paddle speed with the pad
		static constexpr double PaddleSpeedPerSec = 700.0;

		Vec2 m_ballVelocity{ 0, -BallSpeedPerSec };

		Circle m_ball{ 400, 400, 8 };

		Array<Rect> m_bricks;

		double m_paddleX = 400.0;

		void reset()
		{
			m_ballVelocity = Vec2{ 0, -BallSpeedPerSec };
			m_ball = Circle{ 400, 400, 8 };
			m_bricks.clear();

			for (int32 y = 0; y < 5; ++y)
			{
				for (int32 x = 0; x < (Scene::Width() / BrickSize.x); ++x)
				{
					m_bricks << Rect{ (x * BrickSize.x), (60 + y * BrickSize.y), BrickSize };
				}
			}
		}
	};
}

HK_GAME_FACTORY(MakeBreakout, Breakout)

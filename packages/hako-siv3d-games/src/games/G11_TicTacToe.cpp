// 11. 三目並べ (Tic-tac-toe)
//
// Based on the Siv3D sample "11. 三目並べ"
//   https://siv3d.github.io/ja-jp/samples/games/#11-三目並べ
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: a cell is chosen with the d-pad and placed with A (touch / mouse as
// before); the Reset button also works with the pad (A); a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// 3 つのマークがつながったかを返す関数
	bool CheckLine(const Grid<int32>& grid, const Point& cellA, const Point& cellB, const Point& cellC)
	{
		const int32 a = grid[cellA];
		const int32 b = grid[cellB];
		const int32 c = grid[cellC];
		return ((a != 0) && (a == b) && (b == c));
	}

	// マークがつながったラインの一覧を返す関数
	Array<std::pair<Point, Point>> CheckLines(const Grid<int32>& grid)
	{
		Array<std::pair<Point, Point>> results;

		// 縦 3 列を調べる
		for (int32 x = 0; x < 3; ++x)
		{
			if (CheckLine(grid, Point{ x, 0 }, Point{ x, 1 }, Point{ x, 2 }))
			{
				results.emplace_back(Point{ x, 0 }, Point{ x, 2 });
			}
		}

		// 横 3 行を調べる
		for (int32 y = 0; y < 3; ++y)
		{
			if (CheckLine(grid, Point{ 0, y }, Point{ 1, y }, Point{ 2, y }))
			{
				results.emplace_back(Point{ 0, y }, Point{ 2, y });
			}
		}

		// 斜め（左上 -> 右下) を調べる
		if (CheckLine(grid, Point{ 0, 0 }, Point{ 1, 1 }, Point{ 2, 2 }))
		{
			results.emplace_back(Point{ 0, 0 }, Point{ 2, 2 });
		}

		// 斜め（右上 -> 左下) を調べる
		if (CheckLine(grid, Point{ 2, 0 }, Point{ 1, 1 }, Point{ 0, 2 }))
		{
			results.emplace_back(Point{ 2, 0 }, Point{ 0, 2 });
		}

		return results;
	}

	class GameBoard
	{
	public:

		// セルの大きさ
		static constexpr int32 CellSize = 150;

		// O マークの値
		static constexpr int32 O_Mark = 1;

		// X マークの値
		static constexpr int32 X_Mark = 2;

		/// @param pointer pointer position on the board (hako: replaces the transformed mouse cursor)
		void update(const Vec2& pointer)
		{
			if (m_gameOver)
			{
				return;
			}

			// hako: d-pad moves the selected cell, A places the mark there
			if (hk::Repeat(hk::Btn::Left)) { m_selected.x = (m_selected.x + 2) % 3; m_padSelecting = true; }
			if (hk::Repeat(hk::Btn::Right)) { m_selected.x = (m_selected.x + 1) % 3; m_padSelecting = true; }
			if (hk::Repeat(hk::Btn::Up)) { m_selected.y = (m_selected.y + 2) % 3; m_padSelecting = true; }
			if (hk::Repeat(hk::Btn::Down)) { m_selected.y = (m_selected.y + 1) % 3; m_padSelecting = true; }

			m_pointer = pointer;

			// 3x3 のセル
			for (auto p : step(Size{ 3, 3 }))
			{
				// セル
				const Rect cell{ (p * CellSize), CellSize };

				// セルのマーク
				const int32 mark = m_grid[p];

				const bool clicked = (hk::PDown() && cell.intersects(pointer));
				const bool padPlaced = (m_padSelecting && (p == m_selected) && hk::Down(hk::Btn::Confirm));

				if (clicked)
				{
					m_padSelecting = false;
				}

				// セルが空白で、なおかつクリックされたら
				if ((mark == 0) && (clicked || padPlaced))
				{
					// セルに現在のマークを書き込む
					m_grid[p] = m_currentMark;

					// 現在のマークを入れ替える
					m_currentMark = ((m_currentMark == O_Mark) ? X_Mark : O_Mark);

					// つながったラインを探す
					m_lines = CheckLines(m_grid);

					// 空白セルが 0 になるか、つながったラインが見つかったら
					if (m_grid.count(0) == 0 || m_lines)
					{
						// ゲーム終了
						m_gameOver = true;
					}

					break;
				}
			}
		}

		// ゲームをリセット
		void reset()
		{
			m_currentMark = O_Mark;

			m_grid.fill(0);

			m_lines.clear();

			m_gameOver = false;
		}

		// 描画
		void draw() const
		{
			drawGridLines();

			drawCells();

			drawResults();
		}

		// ゲームが終了したかを返す
		bool isGameOver() const
		{
			return m_gameOver;
		}

	private:

		// 3x3 の二次元配列 (初期値は全要素 0)
		Grid<int32> m_grid = Grid<int32>(3, 3);

		// これから置くマーク
		int32 m_currentMark = O_Mark;

		// ゲーム終了フラグ
		bool m_gameOver = false;

		// 3 つ連続したラインの一覧
		Array<std::pair<Point, Point>> m_lines;

		// hako: the cell chosen with the d-pad
		Point m_selected{ 1, 1 };
		bool m_padSelecting = false;
		Vec2 m_pointer{ -1, -1 };

		// 格子を描く
		void drawGridLines() const
		{
			// 線を引く
			for (auto i : { 1, 2 })
			{
				Line{ (i * CellSize), 0, (i * CellSize), (3 * CellSize) }
					.draw(4, ColorF{ 0.25 });

				Line{ 0, (i * CellSize), (3 * CellSize), (i * CellSize) }
					.draw(4, ColorF{ 0.25 });
			}
		}

		// セルを描く
		void drawCells() const
		{
			// 3x3 のセル
			for (auto p : step(Size{ 3, 3 }))
			{
				// セル
				const Rect cell{ (p * CellSize), CellSize };

				// セルのマーク
				const int32 mark = m_grid[p];

				// hako: frame of the cell chosen with the d-pad
				if (not m_gameOver && m_padSelecting && (p == m_selected))
				{
					cell.stretched(-6).drawFrame(4, ColorF{ 0.2, 0.5, 1.0 });
				}

				// X マークだったら
				if (mark == X_Mark)
				{
					// X マークを描く
					Shape2D::Cross(CellSize * 0.4, 10, cell.center())
						.draw(ColorF{ 0.2 });

					// このセルはこれ以上処理しない
					continue;
				}
				else if (mark == O_Mark) // O マークだったら
				{
					// 〇 マークを描く
					Circle{ cell.center(), (CellSize * 0.4 - 10) }
					.drawFrame(10, 0, ColorF{ 0.2 });

					// このセルはこれ以上処理しない
					continue;
				}

				// セルがマウスオーバーされたら
				if (!m_gameOver && (cell.intersects(m_pointer) || (m_padSelecting && (p == m_selected))))
				{
					// カーソルを手のアイコンにする
					Cursor::RequestStyle(CursorStyle::Hand);

					// セルの上に半透明の白を描く
					cell.stretched(-2).draw(ColorF{ 1.0, 0.6 });
				}
			}
		}

		// つながったラインを描く
		void drawResults() const
		{
			for (const auto& line : m_lines)
			{
				// つながったラインの始点と終点のセルを取得
				const Rect cellBegin{ line.first * CellSize, CellSize };
				const Rect cellEnd{ line.second * CellSize, CellSize };

				// 線を引く
				Line{ cellBegin.center(), cellEnd.center() }
					.stretched(CellSize * 0.45)
					.draw(LineStyle::RoundCap, 5, ColorF{ 0.6 });
			}
		}
	};

	class TicTacToe : public hk::IGame
	{
	public:

		TicTacToe()
		{
			// 背景色
			Scene::SetBackground(ColorF{ 0.8, 1.0, 0.9 });
		}

		void update() override
		{
			{
				// 2D 描画を移動 (hako: the pointer is moved by hand below)
				const Transformer2D transform{ Mat3x2::Translate(Offset) };

				m_gameBoard.update(hk::Pos() - Offset);

				m_gameBoard.draw();
			}

			// ゲームが終了していたら
			if (m_gameBoard.isGameOver())
			{
				// Reset ボタンを押せばリセット (hako: or A on the pad, from the next press on)
				if (hk::Button(U"Reset", Vec2{ 400, 520 }, 120)
					|| (m_wasOver && hk::Down(hk::Btn::Confirm)))
				{
					m_gameBoard.reset();
				}
			}

			m_wasOver = m_gameBoard.isGameOver();
		}

	private:

		static constexpr Point Offset{ 175, 30 };

		GameBoard m_gameBoard;

		bool m_wasOver = false;
	};
}

HK_GAME_FACTORY(MakeTicTacToe, TicTacToe)

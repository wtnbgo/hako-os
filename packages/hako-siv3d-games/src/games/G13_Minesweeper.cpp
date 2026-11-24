// 13. マインスイーパー (Minesweeper)
//
// Based on "Minesweeper" of Siv3D-Samples (CC0 1.0 Universal)
//   https://github.com/Siv3D/Siv3D-Samples/tree/b29d0155ec1b2b487ce8f347fd769aec2331995d/Samples/Minesweeper
// Changes for hako: a cell cursor on the d-pad (A opens, X flags, START or Y starts a new
// game); on a touch screen a short tap opens and a long press (0.4 s) flags; the right
// mouse button still flags; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// ゲームの状態
	enum class GameState
	{
		Game,		// ゲームが進行中
		Failed,		// ゲームオーバー
		Cleared,	// ゲームクリア
	};

	// セルの状態
	struct CellState
	{
		// 開かれている
		bool opened = false;

		// 旗が立てられている
		bool flagged = false;

		// 爆発した
		bool exploded = false;

		// 島番号（数字のないマスを一気に開くときに使う）
		int32 groupIndex = 0;
	};

	// 周囲のマスへのオフセット
	constexpr Point Offsets[8] =
	{
		{ -1, -1 }, { 0, -1 }, { 1, -1 },
		{ -1,  0 }           , { 1,  0 },
		{ -1,  1 }, { 0,  1 }, { 1,  1 },
	};

	// 指定したマス目の周囲にある 💣 (-1) の個数を返す関数
	int32 GetBombCount(const Grid<int32>& grid, const Point& center)
	{
		if (grid[center] == -1)
		{
			return -1;
		}

		int32 bombCount = 0;

		for (const auto& offset : Offsets)
		{
			const Point pos = (center + offset);

			if (grid.fetch(pos, 0) == -1)
			{
				++bombCount;
			}
		}

		return bombCount;
	}

	// 盤面を生成する関数
	Grid<int32> MakeGame(const Size& size, int32 bombs)
	{
		Grid<int32> grid(size);

		while (bombs)
		{
			const Point pos = RandomPoint((size.x - 1), (size.y - 1));

			if (grid[pos] == 0)
			{
				grid[pos] = -1;
				--bombs;
			}
		}

		for (int32 y = 0; y < size.y; ++y)
		{
			for (int32 x = 0; x < size.x; ++x)
			{
				grid[y][x] = GetBombCount(grid, Point{ x, y });
			}
		}

		return grid;
	}

	// 盤面の状態を作成する関数
	Grid<CellState> MakeStates(const Grid<int32>& grid)
	{
		const Size size = grid.size();

		Grid<CellState> states(size);

		// 各マスの接続状況を管理するデータ構造
		DisjointSet<int32> ds{ states.num_elements() };

		for (int32 y = 0; y < size.y; ++y)
		{
			for (int32 x = 0; x < size.x; ++x)
			{
				const int32 index = static_cast<int32>(y * size.x + x);

				if (grid[y][x] == 0)
				{
					if (int nx = (x + 1);
						(nx < size.x) && (grid[y][nx] == 0))
					{
						ds.merge(index, (index + 1));
					}

					if (int ny = (y + 1);
						(ny < size.y) && (grid[ny][x] == 0))
					{
						ds.merge(index, (index + size.x));
					}
				}
			}
		}

		{
			int32 index = 0;

			for (int32 y = 0; y < size.y; ++y)
			{
				for (int32 x = 0; x < size.x; ++x)
				{
					states[y][x].groupIndex = ds.find(index);
					++index;
				}
			}
		}

		return states;
	}

	// 開いていないセルのブロックを描く関数
	void DrawBlock(const Rect& rect)
	{
		Triangle{ rect.tl(), rect.tr(), rect.bl() }.draw(ColorF{ 1.0 });
		Triangle{ rect.tr(), rect.br(), rect.bl() }.draw(ColorF{ 0.5 });
		rect.stretched(-5).draw(ColorF{ 0.75 });
	}

	// 盤面を描画する関数
	void DrawGame(const Grid<int32>& grid, const Grid<CellState>& states, const Font& font, const Texture& bombTexture, const Texture& flagTexture, const Point& gamePos, const Size& cellSize)
	{
		// 0～8 の数字の色
		constexpr ColorF NumberColors[9] =
		{
			ColorF{ 0, 0, 0 }, ColorF{ 0, 0, 1 }, ColorF{ 0, 0.5, 0 }, ColorF{ 1, 0, 0 },
			ColorF{ 0, 0, 0.5 }, ColorF{ 0.5, 0, 0 }, ColorF{ 0.5, 0, 0 }, ColorF{ 0.5, 0, 0 }, ColorF{ 0.5, 0, 0 }
		};

		for (int32 y = 0; y < grid.height(); ++y)
		{
			for (int32 x = 0; x < grid.width(); ++x)
			{
				const auto& state = states[y][x];
				const Point pos = (gamePos + (cellSize * Point{ x, y }));
				const Rect cell{ pos, cellSize };

				if (state.opened)
				{
					cell.stretched(-1).draw(ColorF{ 0.75 });

					if (const int32 n = grid[y][x];
						n == -1)
					{
						if (state.exploded)
						{
							cell.stretched(-1).draw(ColorF{ 1, 0, 0 });
						}

						bombTexture.resized(36).drawAt(cell.center());
					}
					else if (1 <= n)
					{
						font(n).drawAt(cell.center(), NumberColors[n]);
					}
				}
				else
				{
					DrawBlock(cell);

					if (state.flagged)
					{
						flagTexture.resized(30).drawAt(cell.center());
					}
				}
			}
		}
	}

	// 指定された島番号のマスと、それらに隣接するマスを開く関数
	void OpenGroup(const Grid<int32>& grid, Grid<CellState>& states, const int32 groupIndex)
	{
		for (int32 y = 0; y < grid.height(); ++y)
		{
			for (int32 x = 0; x < grid.width(); ++x)
			{
				auto& state = states[y][x];

				if (state.groupIndex == groupIndex)
				{
					state.opened = true;

					for (const auto& offset : Offsets)
					{
						const Point neighbor = (Point{ x, y } + offset);

						if (grid.inBounds(neighbor) && (not states[neighbor].opened))
						{
							states[neighbor].opened = true;

							if ((grid[neighbor] == 0) && (groupIndex != states[neighbor].groupIndex))
							{
								OpenGroup(grid, states, states[neighbor].groupIndex);
							}
						}
					}
				}
			}
		}
	}

	/// @brief 盤面を更新する関数 (hako: the cell to open / flag is decided by the caller)
	void UpdateGame(GameState& gameState, const Grid<int32>& grid, Grid<CellState>& states, const int32 bombCount, const Optional<Point>& openAt, const Optional<Point>& flagAt)
	{
		if (openAt)
		{
			const Point pos = *openAt;

			if ((not states[pos].opened) && (not states[pos].flagged))
			{
				states[pos].opened = true;

				if (grid[pos] == 0)
				{
					OpenGroup(grid, states, states[pos].groupIndex);
				}

				if (grid[pos] == -1)
				{
					gameState = GameState::Failed;
					states[pos].exploded = true;

					for (int32 y = 0; y < grid.height(); ++y)
					{
						for (int32 x = 0; x < grid.width(); ++x)
						{
							if (grid[y][x] == -1)
							{
								states[y][x].opened = true;
							}
						}
					}
				}
				else if (states.count_if([](const CellState& c) { return (not c.opened); }) == bombCount)
				{
					gameState = GameState::Cleared;
				}
			}
		}
		else if (flagAt && (not states[*flagAt].opened))
		{
			states[*flagAt].flagged = (not states[*flagAt].flagged);
		}
	}

	class Minesweeper : public hk::IGame
	{
	public:

		Minesweeper()
		{
			Scene::SetBackground(ColorF{ 0.5 });
		}

		void update() override
		{
			const Rect gameArea{ GamePos, (GameSize * CellSize - Point{ 1, 1 }) };
			auto cellAt = [&](const Vec2& p) { return Point{ ((p.asPoint() - GamePos) / CellSize) }; };

			// hako: d-pad cell cursor
			if (hk::Repeat(hk::Btn::Left)) { cursor.x = (cursor.x + GameSize.x - 1) % GameSize.x; padCursor = true; }
			if (hk::Repeat(hk::Btn::Right)) { cursor.x = (cursor.x + 1) % GameSize.x; padCursor = true; }
			if (hk::Repeat(hk::Btn::Up)) { cursor.y = (cursor.y + GameSize.y - 1) % GameSize.y; padCursor = true; }
			if (hk::Repeat(hk::Btn::Down)) { cursor.y = (cursor.y + 1) % GameSize.y; padCursor = true; }

			Optional<Point> openAt, flagAt;

			if (padCursor && hk::Down(hk::Btn::Confirm)) { openAt = cursor; }
			if (padCursor && hk::Down(hk::Btn::X)) { flagAt = cursor; }

			// hako: touch / mouse: a short tap opens, a long press flags; right click flags
			if (hk::PDown() && hk::Over(gameArea))
			{
				pressStart = hk::Pos();
				pressTime.restart();
				longPressDone = false;
				padCursor = false;
			}
			if (pressStart)
			{
				if (hk::PPressed() && not longPressDone && (0.4s <= pressTime))
				{
					flagAt = cellAt(*pressStart);
					longPressDone = true;
				}
				else if (hk::PUp())
				{
					if (not longPressDone && hk::Over(gameArea))
					{
						openAt = cellAt(*pressStart);
					}
					pressStart.reset();
				}
			}
			if (gameArea.rightClicked())
			{
				flagAt = cellAt(Cursor::PosF());
			}

			// ゲームが進行中なら盤面を更新
			if (gameState == GameState::Game)
			{
				UpdateGame(gameState, grid, states, BombCount, openAt, flagAt);
			}

			// 顔ボタンが押されたら状態を初期化 (hako: or START / Y)
			if (hk::Clicked(faceButton) || hk::Down(hk::Btn::Start) || hk::Down(hk::Btn::Y))
			{
				grid = MakeGame(GameSize, BombCount);
				states = MakeStates(grid);
				gameState = GameState::Game;
				pressStart.reset();
			}

			// 盤面を描く
			DrawGame(grid, states, font, bombTexture, flagTexture, GamePos, CellSize);

			// hako: the cell cursor, and the long press in progress
			if (padCursor)
			{
				Rect{ (GamePos + CellSize * cursor), CellSize }.drawFrame(4, ColorF{ 1.0, 0.85, 0.1 });
			}
			if (pressStart && hk::PPressed() && not longPressDone)
			{
				Circle{ *pressStart, 30 }.drawArc(0, (Min(pressTime.sF() / 0.4, 1.0) * 360_deg), 0, 6, ColorF{ 1.0, 0.3, 0.2 });
			}

			// UI エリアの背景を描く
			Rect{ Scene::Width(), 80 }.draw(ColorF{ 0.75 });
			{
				DrawBlock(faceButton);
				faceTextures[FromEnum(gameState)].resized(60).drawAt(faceButton.center());
			}
		}

	private:

		// 盤面のマス目の数
		static constexpr Size GameSize{ 20, 13 };

		// 設置する 💣 の個数
		static constexpr int32 BombCount = 30;

		static_assert(BombCount < (GameSize.area() / 4));

		// セルの大きさ
		static constexpr Size CellSize{ 40, 40 };

		// 盤面の描画位置
		static constexpr Point GamePos{ 0, 80 };

		const Font font{ FontMethod::MSDF, 32, Typeface::Bold };

		const Texture bombTexture{ U"💣"_emoji };

		const Texture flagTexture{ U"🚩"_emoji };

		const std::array<Texture, 3> faceTextures = { Texture{ U"🙂"_emoji }, Texture{ U"😵"_emoji }, Texture{ U"😎"_emoji } };

		Grid<int32> grid = MakeGame(GameSize, BombCount);

		Grid<CellState> states = MakeStates(grid);

		GameState gameState = GameState::Game;

		const Rect faceButton{ Arg::center(400, 40), 72 };

		// hako
		Point cursor{ 10, 6 };
		bool padCursor = false;
		Optional<Vec2> pressStart;
		Stopwatch pressTime;
		bool longPressDone = false;
	};
}

HK_GAME_FACTORY(MakeMinesweeper, Minesweeper)

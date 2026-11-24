// 19. 2D RPG のマップと移動の基本 (2D RPG map and movement basics)
//
// Based on games/007 "2D RPG のマップと移動の基本" of Reputeless/games (CC0 1.0 Universal)
//   https://github.com/Reputeless/games/blob/20cd026327799dbb897c90d36732a32d2ee0934d/games/007/A.md
// The map data (layer0.csv, layer1.csv, passable.csv of the sample, CC0) is built in, and
// the movement code is the sample's.
// Changes for hako: the sample draws the map with a map chip set from a third-party site
// (not ours to redistribute) and the character with a sprite sheet from the Siv3D examples;
// here the chips are drawn with shapes and built-in emoji, and the character is an emoji
// turned to the walking direction. Moves with the d-pad / stick or by touching where to go;
// X (or Space) shows the passable cells; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// マップのセルの数（横 20, 縦 15 マス）
	constexpr Size MapSize{ 20, 15 };

	// マップチップのサイズ（ピクセル）
	constexpr int32 MapChipSize = 16;

	// hako: the map data of the sample (layer0.csv, layer1.csv, passable.csv)
	Grid<int32> MakeGrid(std::initializer_list<int32> values)
	{
		Grid<int32> grid(MapSize);
		size_t i = 0;
		for (const int32 v : values)
		{
			grid[i / MapSize.x][i % MapSize.x] = v;
			++i;
		}
		return grid;
	}

	const Grid<int32>& Layer0()
	{
		static const Grid<int32> grid = MakeGrid({
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
			 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32
		});
		return grid;
	}

	const Grid<int32>& Layer1()
	{
		static const Grid<int32> grid = MakeGrid({
			242, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 246, 248, 248, 248, 248, 243,
			240,  88,  88,  88,  88,   0,   0,   0,   0,   0,   0,   0,   0, 985, 240,   0,   0,   0,   0, 240,
			240,  88,  88,  88,  88,   0,   0, 990,   0,   0,   0,   0,   0, 985, 240,  94,   0,   0,  92, 240,
			240,  88,  88,  88,  88,   0,   0,   0,   0,   0,   0,   0,   0, 985, 240,   0,  95,  95,   0, 240,
			240,  88,  88,  88,  88,   0,   0,   0,   0,   0,   0,   0,   0,   0, 240,   0,  95,  95,   0, 240,
			240,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 240,  94,   0,   0,  92, 240,
			240,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 240,   0,   0,   0,   0, 240,
			240,   0,  86,  87,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 250, 248, 244,   0, 252, 254,
			240,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 240,
			240,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 240,
			240,  80,  80,  80,  80,  80,   0, 307,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 240,
			240, 208, 209, 209, 209, 210,   0,   0,   0,   0,   0,  85,   0,   0,   0,   0,   0,   0,   0, 240,
			240, 216, 217, 217, 217, 218,   0,   0,   0,   0,   0,   0,   0,   0,  85,   0, 994, 994, 994, 240,
			240, 224, 225, 225, 225, 226,   0,   0,   0,   0,   0,   0,  85,   0,   0,   0, 994, 994, 994, 240,
			250, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 251
		});
		return grid;
	}

	const Grid<int32>& Passable()
	{
		static const Grid<int32> grid = MakeGrid({
			  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   0,   0,   0,   0,   1,
			  1,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0,   0,   1,   1,   0,   0,   0,   0,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   0,   0,   0,   0,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0,   1,
			  1,   0,   1,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   0,   1,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
			  1,   1,   1,   1,   1,   1,   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   1,   1,   1,   1,
			  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   1,   1,   1,   1,
			  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1
		});
		return grid;
	}

	// hako: draws a map chip by its number in the original chip set, with shapes / emoji
	class MapChip
	{
	public:

		void draw(int32 chipIndex, const Point& pos) const
		{
			const Rect cell{ pos, MapChipSize };

			if (chipIndex == 32) // ground
			{
				cell.draw(ColorF{ 0.45, 0.72, 0.36 });
				if (IsEven((pos.x / MapChipSize) * 7 + (pos.y / MapChipSize) * 3))
				{
					Line{ cell.pos.movedBy(4, 11), cell.pos.movedBy(6, 7) }.draw(1, ColorF{ 0.35, 0.6, 0.28 });
				}
			}
			else if (InRange(chipIndex, 240, 254)) // walls
			{
				cell.draw(ColorF{ 0.55, 0.5, 0.45 });
				Rect{ cell.pos, MapChipSize, 4 }.draw(ColorF{ 0.68, 0.63, 0.57 });
				cell.drawFrame(1, ColorF{ 0.35, 0.3, 0.27 });
			}
			else if (chipIndex == 88) // water
			{
				cell.draw(ColorF{ 0.3, 0.55, 0.9 });
				Line{ cell.pos.movedBy(3, 8), cell.pos.movedBy(9, 8) }.draw(1, ColorF{ 0.7, 0.85, 1.0 });
			}
			else if (chipIndex == 80) // fence
			{
				Rect{ cell.pos.movedBy(0, 6), MapChipSize, 3 }.draw(ColorF{ 0.5, 0.33, 0.18 });
				Rect{ cell.pos.movedBy(6, 3), 3, 12 }.draw(ColorF{ 0.45, 0.3, 0.15 });
			}
			else if (InRange(chipIndex, 208, 226)) // field
			{
				cell.draw(ColorF{ 0.55, 0.4, 0.25 });
				Line{ cell.pos.movedBy(0, 5), cell.pos.movedBy(16, 5) }.draw(1, ColorF{ 0.45, 0.3, 0.18 });
				Line{ cell.pos.movedBy(0, 11), cell.pos.movedBy(16, 11) }.draw(1, ColorF{ 0.45, 0.3, 0.18 });
			}
			else if (const auto it = m_objects.find(chipIndex); it != m_objects.end())
			{
				it->second.resized(MapChipSize).draw(pos);
			}
			else
			{
				RoundRect{ cell.stretched(-2), 3 }.draw(HSV{ (chipIndex * 37), 0.4, 0.8 });
			}
		}

	private:

		HashTable<int32, Texture> m_objects =
		{
			{ 85, Texture{ U"🪨"_emoji } }, { 86, Texture{ U"🌳"_emoji } }, { 87, Texture{ U"🌳"_emoji } },
			{ 92, Texture{ U"🪴"_emoji } }, { 94, Texture{ U"🪴"_emoji } }, { 95, Texture{ U"🪑"_emoji } },
			{ 307, Texture{ U"🪧"_emoji } }, { 985, Texture{ U"🌲"_emoji } }, { 990, Texture{ U"⛲"_emoji } },
			{ 994, Texture{ U"🌻"_emoji } },
		};
	};

	class RpgMap : public hk::IGame
	{
	public:

		RpgMap()
		{
			Scene::SetBackground(ColorF{ 0.0 });
		}

		void update() override
		{
			const auto& mapLayer0 = Layer0();
			const auto& mapLayer1 = Layer1();
			const auto& mapPassable = Passable();

			// レンダーテクスチャを黒でクリア
			renderTexture.clear(ColorF{ 0.0, 1.0 });

			// 現在移動中でない場合、上下左右キーで次に進むセルを変更
			// (hako: d-pad / stick / arrow keys, or touch: towards the touched point)
			if (currentCell == nextCell)
			{
				bool down = hk::Pressed(hk::Btn::Down), up = hk::Pressed(hk::Btn::Up);
				bool left = hk::Pressed(hk::Btn::Left), right = hk::Pressed(hk::Btn::Right);

				if (hk::PPressed())
				{
					const Vec2 target = (hk::Pos() / 2.0 / MapChipSize);
					const Vec2 d = (target - Vec2{ currentCell } - Vec2{ 0.5, 0.5 });
					if (0.6 < d.length())
					{
						if (Abs(d.y) >= Abs(d.x)) { (d.y > 0 ? down : up) = true; }
						else { (d.x > 0 ? right : left) = true; }
					}
				}

				// 移動先のセル座標を設定
				if (down) { ++nextCell.y; direction = 0; }
				else if (up) { --nextCell.y; direction = 3; }
				else if (left) { --nextCell.x; direction = 1; }
				else if (right) { ++nextCell.x; direction = 2; }

				// nextCell をマップの範囲内に収める
				nextCell.x = Clamp(nextCell.x, 0, (MapSize.x - 1));
				nextCell.y = Clamp(nextCell.y, 0, (MapSize.y - 1));

				// 移動先のセルが通行不能な場合
				if (mapPassable[nextCell] == 1)
				{
					nextCell = currentCell;
				}

				// 移動が発生する場合
				if (currentCell != nextCell)
				{
					walkProgress -= 1.0;
				}
				else
				{
					walkProgress = 1.0;
				}
			}

			// 移動中の場合
			if (currentCell != nextCell)
			{
				walkProgress += (Scene::DeltaTime() * WalkSpeed);

				if (1.0 <= walkProgress)
				{
					currentCell = nextCell;
				}
			}

			{
				// renderTexture を描画先として設定
				const ScopedRenderTarget2D rt{ renderTexture };

				// スペースキーを押すと通行可能判定を可視化する (hako: or X)
				const bool showPassable = (KeySpace.pressed() || hk::Pressed(hk::Btn::X));

				for (int32 y = 0; y < MapSize.y; ++y)
				{
					for (int32 x = 0; x < MapSize.x; ++x)
					{
						const Point pos{ (x * MapChipSize), (y * MapChipSize) };

						// 最下層のマップチップ
						if (const int32 chipIndex = mapLayer0[y][x]; chipIndex != 0)
						{
							mapchip.draw(chipIndex, pos);
						}

						// 1 段上のマップチップ
						if (const int32 chipIndex = mapLayer1[y][x]; chipIndex != 0)
						{
							mapchip.draw(chipIndex, pos);
						}

						// 通行可能判定
						if (showPassable && (mapPassable[y][x] != 0))
						{
							Rect{ pos, MapChipSize }.draw(ColorF{ 0, 0, 1.0, 0.4 });
							Shape2D::Cross((MapChipSize * 0.3), 3, Vec2{ (MapChipSize * (x + 0.5)), (MapChipSize * (y + 0.5)) })
								.draw(Palette::Red);
						}
					}
				}

				// キャラクター (hako: an emoji instead of the Siv3D-kun sprite sheet)
				{
					const Vec2 cellPos = currentCell.lerp(nextCell, walkProgress);
					const Vec2 pos = (cellPos * MapChipSize) + Vec2{ 8, 6 };
					const double bob = (InRange(walkProgress, 0.25, 0.75) ? -1.5 : 0.0);
					const auto chara = character.resized(18);
					((direction == 1) ? chara.mirrored() : chara).drawAt(pos.movedBy(0, bob));
				}
			}

			{
				// テクスチャ拡大描画時にフィルタリングをしないサンプラーステートを適用
				const ScopedRenderStates2D sampler{ SamplerState::ClampNearest };

				// renderTexture を 2 倍のサイズでシーンに描画
				renderTexture.scaled(2).draw();
			}
		}

	private:

		MapChip mapchip;

		const Texture character{ U"🧒"_emoji };

		// 現在のセル座標 / 移動先のセル座標
		Point currentCell{ 6, 6 };
		Point nextCell = currentCell;

		// 歩行の速さ (マス/秒)
		static constexpr double WalkSpeed = 2.5;

		// 歩行の進捗 (移動開始: 0.0, nextCell に到達: 1.0)
		double walkProgress = 1.0;

		// 方向: (下: 0, 左: 1, 右: 2, 上: 3)
		int32 direction = 0;

		// マップ描画用のレンダーテクスチャ (320x240 に描いて 2 倍で表示する)
		RenderTexture renderTexture{ 320, 240 };
	};
}

HK_GAME_FACTORY(MakeRpgMap, RpgMap)

// 17. ハノイの塔 (Tower of Hanoi)
//
// Based on games/004 "ハノイの塔" of Reputeless/games (CC0 1.0 Universal)
//   https://github.com/Reputeless/games/blob/20cd026327799dbb897c90d36732a32d2ee0934d/games/004/A.md
// Changes for hako: ← → choose a tower, A picks up / puts down a disk (touch / mouse as
// before); a solved message and a new game with A; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// 移動元, 移動先情報に基づいて円盤の移動操作を行う
	void Update(Optional<size_t>& from, size_t to, std::array<Array<int32>, 3>& towers)
	{
		// 移動元情報が無い場合は何もしない
		if (not from)
		{
			return;
		}

		// 移動元と移動先が同じ、または移動元の塔に円盤が無い場合は選択状態を解除する
		if ((from == to) || (not towers[*from]))
		{
			from.reset();
			return;
		}

		// 移動させようとしている, 選択中の円盤のサイズ
		const int32 diskSize = towers[*from].back();

		if ((not towers[to]) // 移動先の塔が空であるか
			|| (diskSize < towers[to].back())) // 移動先の塔の一番上（末尾）の円盤よりも小さければ
		{
			// 移動先の塔の一番上（末尾）に追加する
			towers[to] << diskSize;

			// 移動元からは削除する
			towers[*from].pop_back();
		}

		// 移動元選択状態を解除する
		from.reset();
	}

	class Hanoi : public hk::IGame
	{
	public:

		Hanoi()
		{
			// 背景色を設定する
			Scene::SetBackground(ColorF{ 0.8, 0.9, 1.0 });
			reset();
		}

		void update() override
		{
			const bool solved = (towers[2].size() == 5);

			if (solved)
			{
				if (hk::Down(hk::Btn::Confirm) || hk::PDown())
				{
					reset();
				}
			}
			else
			{
				// hako: ← → choose a tower, A picks up / puts down
				if (hk::Repeat(hk::Btn::Left)) { cursor = (cursor + 2) % 3; padCursor = true; }
				if (hk::Repeat(hk::Btn::Right)) { cursor = (cursor + 1) % 3; padCursor = true; }

				// 各塔について
				for (size_t i = 0; i < TowerAreas.size(); ++i)
				{
					// 塔の領域上にマウスカーソルがあれば
					if (hk::Over(TowerAreas[i]))
					{
						Cursor::RequestStyle(CursorStyle::Hand);
					}

					const bool clicked = hk::Clicked(TowerAreas[i]);
					if (clicked)
					{
						padCursor = false;
					}

					// 塔の領域がクリックされたら (hako: or A on the tower under the cursor)
					if (clicked || (padCursor && (i == cursor) && hk::Down(hk::Btn::Confirm)))
					{
						if (not from) // 選択状態になっている塔が無い場合
						{
							// 現在の塔に円盤があれば選択状態にする
							if (towers[i])
							{
								from = i;
							}
						}
						else // 選択状態になっている塔がある場合
						{
							Update(from, i, towers);
						}
					}
				}
			}

			// 地面を描画する
			Rect{ 0, GroundY, SceneSize.x, GroundHeight }.draw(GroundColor);

			// すべての塔を描画する
			for (const auto& towerX : TowerXs)
			{
				Rect{ Arg::bottomCenter(towerX, GroundY), 12, 300 }.draw(TowerColor);
			}

			// hako: the tower under the d-pad cursor
			if (padCursor && not solved)
			{
				Triangle{ static_cast<double>(TowerXs[cursor]), (GroundY + 30.0), 30, 0_deg }.draw(ColorF{ 0.9, 0.4, 0.2 });
			}

			// 各塔について
			for (size_t ti = 0; ti < towers.size(); ++ti)
			{
				const auto& tower = towers[ti];

				// 現在の塔が移動元として選択されているか
				const bool selected = (from == ti);

				for (int32 i = 0; i < static_cast<int32>(tower.size()); ++i)
				{
					const int32 diskSize = tower[i];
					const int32 diskWidth = (15 + diskSize * 45);
					const HSV diskColor{ (diskSize * 40), 0.5, 1.0 };
					const bool isTop = (i == static_cast<int32>(tower.size() - 1));

					Rect{ Arg::bottomCenter(TowerXs[ti], GroundY - i * 50), diskWidth, 50 }
						.moveBy(0, (selected && isTop) ? -50 : 0) // 選択中かつ一番上なら, 本来よりも上に移動させる
						.rounded(18)
						.draw(diskColor)
						.drawFrame(4, 0, ColorF{ 0.3, 0.5 });
				}
			}

			if (solved)
			{
				hk::UiFont()(U"完成！  A / タッチ: もう一度").drawAt(36, Vec2{ 400, 100 }, ColorF{ 0.2, 0.3, 0.5 });
			}
		}

	private:

		// 画面のサイズ (800x600)
		static constexpr Size SceneSize = Scene::DefaultSceneSize;

		static constexpr ColorF GroundColor{ 0.7 };
		static constexpr ColorF TowerColor{ 0.5 };
		static constexpr int32 GroundHeight = 100;
		static constexpr int32 GroundY = (SceneSize.y - GroundHeight);

		// 各塔の X 座標
		static constexpr std::array<int32, 3> TowerXs = { 150, 400, 650 };

		// 各塔の領域（このエリアをクリックすると選択される）
		static constexpr std::array<Rect, 3> TowerAreas =
		{
			Rect{ 0, 200, 275, 300 },
			Rect{ 275, 200, 250, 300 },
			Rect{ 525, 200, 275, 300 },
		};

		// 各塔の状態（塔が持つ円盤のサイズを配列で記録）
		std::array<Array<int32>, 3> towers;

		// 移動元の塔のインデックス（選択中の塔）
		Optional<size_t> from;

		// hako
		size_t cursor = 0;
		bool padCursor = false;

		void reset()
		{
			towers = {};
			towers[0] = { 5, 4, 3, 2, 1 };
			from.reset();
		}
	};
}

HK_GAME_FACTORY(MakeHanoi, Hanoi)

// 6. 絵文字タワー (Emoji tower)
//
// Based on the Siv3D sample "6. 絵文字タワー"
//   https://siv3d.github.io/ja-jp/samples/games/#6-絵文字タワー
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: the stick moves the pointer and A drops the animal (touch / mouse as
// before); the right stick moves the camera and L / R zoom; the scene is 1280x720 as in
// the sample (Window::Resize); a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	class EmojiTower : public hk::IGame
	{
	public:

		EmojiTower()
		{
			// 背景色を設定
			Scene::SetBackground(ColorF{ 0.2, 0.7, 1.0 });

			for (const auto& emoji : emojis)
			{
				// 絵文字の画像から形状情報を作成する
				polygons << Emoji::CreateImage(emoji).alphaToPolygonsCentered().simplified(2.0);

				// 絵文字の画像からテクスチャを作成する
				textures << Texture{ Emoji{ emoji } };
			}

			index = Random(polygons.size() - 1);
		}

		void update() override
		{
			accumulatedTime += Scene::DeltaTime();

			while (StepTime <= accumulatedTime)
			{
				// 2D 物理演算のワールドを更新する
				world.update(StepTime);

				accumulatedTime -= StepTime;
			}

			// 地面より下に落ちた物体は削除する
			for (auto it = bodies.begin(); it != bodies.end();)
			{
				if (100 < it->getPos().y)
				{
					// 対応テーブルからも削除
					table.erase(it->id());

					it = bodies.erase(it);
				}
				else
				{
					++it;
				}
			}

			// hako: right stick pans, L / R zoom
			if (const Vec2 r = hk::RStick(); not r.isZero())
			{
				camera.setTargetCenter(camera.getTargetCenter() + (r * 600 * Scene::DeltaTime() / camera.getScale()));
			}
			if (hk::Pressed(hk::Btn::L)) { camera.setTargetScale(camera.getTargetScale() * (1.0 - 1.2 * Scene::DeltaTime())); }
			if (hk::Pressed(hk::Btn::R)) { camera.setTargetScale(camera.getTargetScale() * (1.0 + 1.2 * Scene::DeltaTime())); }

			// 2D カメラを更新する
			camera.update();
			{
				// hako: the pointer in world coordinates (mouse, touch or the pad)
				const Vec2 pointer = hk::Pos(camera.getMat3x2());

				// 2D カメラから Transformer2D を作成する
				const auto t = camera.createTransformer();

				// 左クリックされたら (hako: or A)
				if (hk::PDown())
				{
					// ボディを追加する
					bodies << world.createPolygons(P2Dynamic, pointer, polygons[index], P2Material{ 0.1, 0.0, 1.0 });

					// ボディ ID と絵文字のインデックスの組を対応テーブルに追加する
					table.emplace(bodies.back().id(), std::exchange(index, Random(polygons.size() - 1)));
				}

				// すべてのボディを描画する
				for (const auto& body : bodies)
				{
					textures[table[body.id()]].rotated(body.getAngle()).drawAt(body.getPos());
				}

				// 地面を描画する
				ground.draw(Palette::Green);

				// 現在操作できる絵文字を描画する
				textures[index].drawAt(pointer, ColorF{ 1.0, (0.5 + Periodic::Sine0_1(1s) * 0.5) });
			}

			// 2D カメラの操作を描画する
			camera.draw(Palette::Orange);
		}

	private:

		// 登場する絵文字
		const Array<String> emojis = { U"🐘", U"🐧", U"🐐", U"🐤" };

		Array<MultiPolygon> polygons;

		Array<Texture> textures;

		// 2D 物理演算のシミュレーションステップ（秒）
		static constexpr double StepTime = (1.0 / 200.0);

		// 2D 物理演算のシミュレーション蓄積時間（秒）
		double accumulatedTime = 0.0;

		// 2D 物理演算のワールド
		P2World world;

		// [_] 地面
		const P2Body ground = world.createLine(P2Static, Vec2{ 0, 0 }, Line{ -300, 0, 300, 0 });

		// 動物の物体
		Array<P2Body> bodies;

		// 物体の ID と絵文字のインデックスの対応テーブル
		HashTable<P2BodyID, size_t> table;

		// 絵文字のインデックス
		size_t index = 0;

		// 2D カメラ
		Camera2D camera{ Vec2{ 0, -200 } };
	};
}

HK_GAME_FACTORY(MakeEmojiTower, EmojiTower)

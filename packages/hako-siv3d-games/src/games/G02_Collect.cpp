// 2. 落ちてくるアイテムを拾うゲーム (Catch the falling items)
//
// Based on the Siv3D sample "2. 落ちてくるアイテムを拾うゲーム"
//   https://siv3d.github.io/ja-jp/samples/games/#2-落ちてくるアイテムを拾うゲーム
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: moves with the pad (stick / d-pad) and by touching the left or right
// of the player; A (or a touch) starts a new round after the time is up; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// プレイヤークラス
	struct Player
	{
		Circle circle{ 400, 530, 30 };

		Texture texture{ U"😃"_emoji };

		// プレイヤーの状態を更新する関数
		void update(double deltaTime)
		{
			const double speed = (deltaTime * 400.0);

			// hako: stick / d-pad / arrow keys, or touch (mouse) left / right of the player
			double dir = hk::Stick().x;

			if ((dir == 0.0) && hk::PPressed())
			{
				const double dx = (hk::Pos().x - circle.x);
				dir = (Abs(dx) < 8.0) ? 0.0 : Math::Sign(dx);
			}

			circle.x += (dir * speed);

			// プレイヤーが画面外に出ないようにする
			circle.x = Clamp(circle.x, 30.0, 770.0);
		}

		// プレイヤーを描く関数
		void draw() const
		{
			texture.scaled(0.5).drawAt(circle.center);
		}
	};

	// アイテムクラス
	struct Item
	{
		Circle circle;

		// アイテムの種類（0: キャンディー, 1: ケーキ）
		int32 type;

		void update(double deltaTime)
		{
			// アイテムを下に移動させる
			circle.y += (deltaTime * 200.0);
		}

		// アイテムを描く関数
		void draw(const Array<Texture>& itemTextures) const
		{
			// アイテムの種類に応じたテクスチャを描く
			itemTextures[type].scaled(0.5).rotated(circle.y * 0.3_deg).drawAt(circle.center);
		}
	};

	void UpdateItems(Array<Item>& items, double deltaTime, const Player& player, int32& score)
	{
		// すべてのアイテムの状態を更新する
		for (auto& item : items)
		{
			item.update(deltaTime);
		}

		// 各アイテムについて
		for (auto it = items.begin(); it != items.end();)
		{
			// プレイヤーとアイテムが交差したら
			if (player.circle.intersects(it->circle))
			{
				// スコアを加算する（キャンディー: 10点, ケーキ: 50点）
				score += ((it->type == 0) ? 10 : 50);

				// アイテムを削除する
				it = items.erase(it);
			}
			else
			{
				++it;
			}
		}

		// 地面に落下したアイテムを削除する
		items.remove_if([](const Item& item) { return (580 < item.circle.y); });
	}

	// 背景画面を描く関数
	void DrawBackground()
	{
		// 空を描く
		Rect{ 0, 0, 800, 550 }.draw(Arg::top(0.3, 0.6, 1.0), Arg::bottom(0.6, 0.9, 1.0));

		// 地面を描く
		Rect{ 0, 550, 800, 50 }.draw(ColorF{ 0.3, 0.6, 0.3 });
	}

	// アイテムを描く関数
	void DrawItems(const Array<Item>& items, const Array<Texture>& itemTextures)
	{
		for (const auto& item : items)
		{
			item.draw(itemTextures);
		}
	}

	// UI を描く関数
	void DrawUI(int32 score, double remainingTime, const Font& font)
	{
		// スコアを描く
		font(U"SCORE: {}"_fmt(score)).draw(30, Vec2{ 20, 20 });

		// 残り時間を描く
		font(U"TIME: {:.0f}"_fmt(remainingTime)).draw(30, Arg::topRight(780, 20));

		if (remainingTime <= 0.0)
		{
			font(U"TIME'S UP!").drawAt(80, Vec2{ 400, 270 }, ColorF{ 0.3 });
			font(U"A / タッチ: もう一度").drawAt(26, Vec2{ 400, 340 }, ColorF{ 0.3 });
		}
	}

	class Collect : public hk::IGame
	{
	public:

		Collect()
		{
			reset();
		}

		void update() override
		{
			const double deltaTime = Scene::DeltaTime();

			// 残り時間を減らす
			m_remainingTime = Max((m_remainingTime - deltaTime), 0.0);

			// ゲームが進行している場合
			if (0.0 < m_remainingTime)
			{
				// 蓄積時間を増やす
				m_accumulatedTime += deltaTime;

				// 蓄積時間が周期を超えたら
				if (SpawnInterval < m_accumulatedTime)
				{
					// 新しいアイテムを追加する
					m_items << Item{ Circle{ Random(30.0, 770.0), -30, 30 }, Random(0, 1) };

					// 蓄積時間を周期分減らす
					m_accumulatedTime -= SpawnInterval;
				}

				// プレイヤーの状態を更新する
				m_player.update(deltaTime);

				// すべてのアイテムの状態を更新する
				UpdateItems(m_items, deltaTime, m_player, m_score);
			}
			else
			{
				m_items.clear();

				// hako: play again
				if (hk::Down(hk::Btn::Confirm) || hk::PDown())
				{
					reset();
				}
			}

			// 背景を描く
			DrawBackground();

			// プレイヤーを描く
			m_player.draw();

			// すべてのアイテムを描く
			DrawItems(m_items, m_itemTextures);

			// UI を描く
			DrawUI(m_score, m_remainingTime, m_font);
		}

	private:

		// アイテムが出現する周期（秒）
		static constexpr double SpawnInterval = 0.8;

		const Font m_font{ FontMethod::MSDF, 48, Typeface::Bold };

		// アイテムのテクスチャ配列
		const Array<Texture> m_itemTextures =
		{
			Texture{ U"🍬"_emoji },
			Texture{ U"🍰"_emoji },
		};

		Player m_player;

		Array<Item> m_items;

		double m_accumulatedTime = 0.0;

		int32 m_score = 0;

		double m_remainingTime = 20.0;

		void reset()
		{
			m_player.circle = Circle{ 400, 530, 30 };
			m_items.clear();
			m_items << Item{ Circle{ 200, 200, 30 }, 0 };
			m_items << Item{ Circle{ 600, 100, 30 }, 1 };
			m_accumulatedTime = 0.0;
			m_score = 0;
			m_remainingTime = 20.0;
		}
	};
}

HK_GAME_FACTORY(MakeCollect, Collect)

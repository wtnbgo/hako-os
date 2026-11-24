// 9. クッキークリッカー (Cookie clicker)
//
// Based on the Siv3D sample "9. クッキークリッカー"
//   https://siv3d.github.io/ja-jp/samples/games/#9-クッキークリッカー
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: A clicks the cookie, the d-pad up / down chooses an item and Y buys it
// (touch / mouse as before); the save file is written when leaving the game (and every
// 10 seconds) under the user's data folder instead of next to the program; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	//ゲームのセーブデータ
	struct SaveData
	{
		double cookies;

		Array<int32> itemCounts;

		// シリアライズに対応させるためのメンバ関数を定義する
		template <class Archive>
		void SIV3D_SERIALIZE(Archive& archive)
		{
			archive(cookies, itemCounts);
		}
	};

	// hako: where to save (the program folder is read-only once installed)
	FilePath SavePath()
	{
		return (FileSystem::GetFolderPath(SpecialFolder::LocalAppData) + U"hako-siv3d-games/cookie.save");
	}

	/// @brief アイテムのボタン
	/// @return ボタンが押された場合 true, それ以外の場合は false
	bool Button(const Rect& rect, const Texture& texture, const Font& font, const String& name, const String& desc, int32 count, bool enabled, bool focused)
	{
		if (enabled)
		{
			rect.draw(ColorF{ 0.3, 0.5, 0.9, 0.8 });

			rect.drawFrame(2, 2, ColorF{ 0.5, 0.7, 1.0 });

			if (hk::Over(rect))
			{
				Cursor::RequestStyle(CursorStyle::Hand);
			}
		}
		else
		{
			rect.draw(ColorF{ 0.0, 0.4 });

			rect.drawFrame(2, 2, ColorF{ 0.5 });
		}

		// hako: the item chosen with the d-pad
		if (focused)
		{
			rect.stretched(4).drawFrame(4, ColorF{ 1.0, 0.85, 0.3 });
		}

		texture.scaled(0.5).drawAt(rect.x + 50, rect.y + 50);

		font(name).draw(30, rect.x + 100, rect.y + 15, Palette::White);

		font(desc).draw(18, rect.x + 102, rect.y + 60, Palette::White);

		font(count).draw(50, Arg::rightCenter((rect.x + rect.w - 20), (rect.y + 50)), Palette::White);

		return (enabled && hk::Clicked(rect));
	}

	// クッキーが降るエフェクト
	struct CookieBackgroundEffect : IEffect
	{
		Vec2 m_start;
		double m_angle;
		Texture m_texture;

		CookieBackgroundEffect(const Vec2& start, const Texture& texture)
			: m_start{ start }
			, m_angle{ Random(2_pi) }
			, m_texture{ texture } {}

		bool update(double t) override
		{
			const Vec2 pos = m_start + 0.5 * t * t * Vec2{ 0, 120 };

			m_texture.scaled(0.3).rotated(m_angle).drawAt(pos, ColorF{ 1.0, (1.0 - t / 3.0) });

			return (t < 3.0);
		}
	};

	// クッキーが舞うエフェクト
	struct CookieEffect : IEffect
	{
		Vec2 m_start;
		Vec2 m_velocity;
		double m_scale;
		double m_angle;
		Texture m_texture;

		CookieEffect(const Vec2& start, const Texture& texture)
			: m_start{ start }
			, m_velocity{ Circular{ 80, Random(-40_deg, 40_deg) } }
			, m_scale{ Random(0.2, 0.3) }
			, m_angle{ Random(2_pi) }
			, m_texture{ texture } {}

		bool update(double t) override
		{
			const Vec2 pos = m_start
				+ m_velocity * t + 0.5 * t * t * Vec2{ 0, 120 };

			m_texture.scaled(m_scale).rotated(m_angle).drawAt(pos, ColorF{ 1.0, (1.0 - t) });

			return (t < 1.0);
		}
	};

	// 「+1」が上昇するエフェクト
	struct PlusOneEffect : IEffect
	{
		Vec2 m_start;
		Font m_font;

		PlusOneEffect(const Vec2& start, const Font& font)
			: m_start{ start }
			, m_font{ font } {}

		bool update(double t) override
		{
			m_font(U"+1").drawAt(24, m_start.movedBy(0, t * -120), ColorF{ 1.0, (1.0 - t) });

			return (t < 1.0);
		}
	};

	// アイテムのデータ
	struct Item
	{
		Texture emoji;
		String name;
		int32 initialCost;
		int32 cps;

		// アイテムを count 個持っているときの購入コストを返す
		int32 getCost(int32 count) const
		{
			return initialCost * (count + 1);
		}
	};

	// クッキーのばね
	class CookieSpring
	{
	public:

		void update(double deltaTime, bool pressed)
		{
			m_accumulatedTime += deltaTime;

			while (0.005 <= m_accumulatedTime)
			{
				double force = (-0.02 * m_x);

				if (pressed)
				{
					force += 0.004;
				}

				m_velocity = (m_velocity + force) * 0.92;

				m_x += m_velocity;

				m_accumulatedTime -= 0.005;
			}
		}

		double get() const
		{
			return m_x;
		}

	private:

		double m_x = 0.0;
		double m_velocity = 0.0;
		double m_accumulatedTime = 0.0;
	};

	// クッキーの後光を描く関数
	void DrawHalo(const Vec2& center)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			double startAngle = Scene::Time() * 15_deg + i * 90_deg;
			Circle{ center, 180 }.drawPie(startAngle, 60_deg, ColorF{ 1.0, 0.3 }, ColorF{ 1.0, 0.0 });
		}

		for (int32 i = 0; i < 6; ++i)
		{
			double startAngle = Scene::Time() * -15_deg + i * 60_deg;
			Circle{ center, 180 }.drawPie(startAngle, 40_deg, ColorF{ 1.0, 0.3 }, ColorF{ 1.0, 0.0 });
		}
	}

	// アイテムの所有数をもとに CPS を計算する関数
	int32 CalculateCPS(const Array<Item>& ItemTable, const Array<int32>& itemCounts)
	{
		int32 cps = 0;

		for (size_t i = 0; i < ItemTable.size(); ++i)
		{
			cps += ItemTable[i].cps * itemCounts[i];
		}

		return cps;
	}

	class CookieClicker : public hk::IGame
	{
	public:

		CookieClicker()
		{
			// セーブデータが見つかればそれを読み込む
			Deserializer<BinaryReader> reader{ SavePath() };

			if (reader) // もしオープンに成功したら
			{
				SaveData saveData;

				reader(saveData);

				cookies = saveData.cookies;

				if (saveData.itemCounts.size() == ItemTable.size())
				{
					itemCounts = saveData.itemCounts;
				}
			}
		}

		~CookieClicker() override
		{
			// 終了時にゲームをセーブ (hako: when leaving the game)
			save();
		}

		void update() override
		{
			// クッキーの毎秒の生産量を計算する
			const int32 cps = CalculateCPS(ItemTable, itemCounts);

			// ゲームの経過時間を加算する
			accumulatedTime += Scene::DeltaTime();

			// 0.1 秒以上蓄積していたら
			if (0.1 <= accumulatedTime)
			{
				accumulatedTime -= 0.1;

				// 0.1 秒分のクッキー生産を加算する
				cookies += (cps * 0.1);
			}

			// hako: save every 10 seconds as well (the device may be switched off)
			if (10s <= saveStopwatch)
			{
				save();
				saveStopwatch.restart();
			}

			// 背景のクッキー
			{
				const double cookieBackgroundSpawnTime = cps ? Max(1.0 / Math::Log2(cps * 2), 0.03) : Math::Inf;

				if (cps)
				{
					cookieBackgroundAccumulatedTime += Scene::DeltaTime();
				}

				while (cookieBackgroundSpawnTime <= cookieBackgroundAccumulatedTime)
				{
					effectBackground.add<CookieBackgroundEffect>(RandomVec2(Rect{ 0, -150, 800, 100 }), texture);

					cookieBackgroundAccumulatedTime -= cookieBackgroundSpawnTime;
				}
			}

			// hako: A clicks the cookie
			const bool padClick = hk::Down(hk::Btn::Confirm);
			const bool cookieClicked = (hk::Clicked(CookieCircle) || padClick);

			// クッキーのばねを更新する
			cookieSpring.update(Scene::DeltaTime(), (hk::PressedOn(CookieCircle) || hk::Pressed(hk::Btn::Confirm)));

			// クッキー円上にマウスカーソルがあれば
			if (hk::Over(CookieCircle))
			{
				Cursor::RequestStyle(CursorStyle::Hand);
			}

			// クッキー円が左クリックされたら
			if (cookieClicked)
			{
				++cookies;

				const Vec2 at = (padClick ? RandomVec2(CookieCircle.stretched(-40)) : hk::Pos());

				// クッキーが舞うエフェクトを追加する
				effect.add<CookieEffect>(at.movedBy(Random(-5, 5), Random(-5, 5)), texture);

				// 「+1」が上昇するエフェクトを追加する
				effect.add<PlusOneEffect>(at.movedBy(Random(-5, 5), Random(-15, -5)), font);

				// 背景のクッキーを追加する
				effectBackground.add<CookieBackgroundEffect>(RandomVec2(Rect{ 0, -150, 800, 100 }), texture);
			}

			// hako: d-pad chooses an item, Y buys it
			if (hk::Repeat(hk::Btn::Down)) { focusedItem = Min(focusedItem + 1, static_cast<int32>(ItemTable.size()) - 1); padFocus = true; }
			if (hk::Repeat(hk::Btn::Up)) { focusedItem = Max(focusedItem - 1, 0); padFocus = true; }
			const bool padBuy = (padFocus && hk::Down(hk::Btn::Y));

			// 背景を描く
			Rect{ 0, 0, 800, 600 }.draw(Arg::top = ColorF{ 0.6, 0.5, 0.3 }, Arg::bottom = ColorF{ 0.2, 0.5, 0.3 });

			// 背景で降り注ぐクッキーを描画する
			effectBackground.update();

			// クッキーの後光を描く
			DrawHalo(CookieCircle.center);

			// クッキーの数を整数で表示する
			font(ThousandsSeparate((int32)cookies)).drawAt(60, 170, 100);

			// クッキーの生産量を表示する
			font(U"毎秒: {}"_fmt(cps)).drawAt(24, 170, 160);

			// クッキーを描画する
			texture.scaled(1.5 - cookieSpring.get()).drawAt(CookieCircle.center);

			// エフェクトを描画する
			effect.update();

			for (size_t i = 0; i < ItemTable.size(); ++i)
			{
				// アイテムの所有数
				const int32 itemCount = itemCounts[i];

				// アイテムの現在の価格
				const int32 itemCost = ItemTable[i].getCost(itemCount);

				// アイテム 1 つあたりの CPS
				const int32 itemCps = ItemTable[i].cps;

				const bool focused = (padFocus && (static_cast<int32>(i) == focusedItem));

				// ボタン
				if (Button(Rect{ 340, static_cast<int32>(40 + 120 * i), 420, 100 }, ItemTable[i].emoji,
					font, ItemTable[i].name, U"C{} / {} CPS"_fmt(itemCost, itemCps), itemCount, (itemCost <= cookies), focused)
					|| (focused && padBuy && (itemCost <= cookies)))
				{
					cookies -= itemCost;
					++itemCounts[i];
				}
			}

			font(U"A: クッキー   ↑↓: アイテム   Y: 買う").draw(16, Vec2{ 340, 410 }, ColorF{ 1.0, 0.8 });
		}

	private:

		// クッキーの絵文字
		const Texture texture{ U"🍪"_emoji };

		// アイテムのデータ
		const Array<Item> ItemTable = {
			{ Texture{ U"🌾"_emoji }, U"クッキー農場", 10, 1 },
			{ Texture{ U"🏭"_emoji }, U"クッキー工場", 100, 10 },
			{ Texture{ U"⚓"_emoji }, U"クッキー港", 1000, 100 },
		};

		// 各アイテムの所有数
		Array<int32> itemCounts = Array<int32>(3); // = { 0, 0, 0 }

		// フォント
		const Font font{ FontMethod::MSDF, 48, Typeface::Bold };

		// クッキーのクリック円
		static constexpr Circle CookieCircle{ 170, 300, 100 };

		// エフェクト
		Effect effectBackground, effect;

		// クッキーのばね
		CookieSpring cookieSpring;

		// クッキーの個数
		double cookies = 0;

		// ゲームの経過時間の蓄積
		double accumulatedTime = 0.0;

		// 背景のクッキーの蓄積時間
		double cookieBackgroundAccumulatedTime = 0.0;

		// hako
		int32 focusedItem = 0;
		bool padFocus = false;
		Stopwatch saveStopwatch{ StartImmediately::Yes };

		void save() const
		{
			FileSystem::CreateDirectories(FileSystem::ParentPath(SavePath()));
			Serializer<BinaryWriter> writer{ SavePath() };

			if (writer)
			{
				writer(SaveData{ cookies, itemCounts });
			}
		}
	};
}

HK_GAME_FACTORY(MakeCookieClicker, CookieClicker)

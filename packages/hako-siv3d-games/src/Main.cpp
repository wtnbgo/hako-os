// Siv3D game collection for hako.
//
// The game samples of the Siv3D documentation (https://siv3d.github.io/ja-jp/samples/games/)
// put together in one app, playable with the built-in pad of a handheld and with touch.
// Each game is in games/ with a note on where it comes from; the licenses are shown from
// the menu (assets/licenses/, Siv3D's own list).
//
// SPDX-License-Identifier: MIT
# include <Siv3D.hpp>
# include "Common.hpp"
# include "LicenseView.hpp"

namespace
{
	constexpr Size MenuSize{ 800, 600 };
	constexpr ColorF MenuBackground{ 0.12, 0.13, 0.16 };

	// menu entries: the games, then these
	enum class Extra { Licenses, Quit };

	struct Menu
	{
		int32 selected = 0;
	};

	void EnterMenu()
	{
		Scene::Resize(MenuSize);
		Scene::SetBackground(MenuBackground);
		hk::SetPadPointer(false);
		Window::SetTitle(U"Siv3D ゲーム集");
	}

	// help shown over a game for a few seconds after it starts
	struct HelpOverlay
	{
		Stopwatch shown;
		const hk::GameInfo* info = nullptr;

		void start(const hk::GameInfo& gameInfo)
		{
			info = &gameInfo;
			shown.restart();
		}

		void draw() const
		{
			if (not info)
			{
				return;
			}

			const double t = shown.sF();
			const double alpha = (t < 4.0) ? 1.0 : Max(0.0, 1.0 - (t - 4.0) * 2.0);
			const Font& font = hk::UiFont();

			if (0.0 < alpha)
			{
				const double h = (60 + info->help.size() * 26);
				const RectF box{ Arg::center(Scene::Center().x, (Scene::Height() - h / 2 - 20)), (Scene::Width() - 80), h };
				box.rounded(10).draw(ColorF{ 0.0, 0.7 * alpha });
				font(U"{}. {}"_fmt(info->number, info->title)).draw(24, Vec2{ (box.x + 20), (box.y + 10) }, ColorF{ 1.0, alpha });

				for (size_t i = 0; i < info->help.size(); ++i)
				{
					font(info->help[i]).draw(18, Vec2{ (box.x + 24), (box.y + 46 + i * 26) }, ColorF{ 0.9, alpha });
				}
			}

			// small reminder of how to leave, always visible
			font(U"START+SELECT / HOME / Esc: メニュー").draw(12, Arg::bottomRight((Scene::Width() - 6), (Scene::Height() - 4)), ColorF{ 0.5, 0.5 });
		}
	};

	// returns the index of the chosen entry, or -1
	int32 UpdateMenu(Menu& menu, const int32 entryCount)
	{
		const Font& font = hk::UiFont();
		const auto& games = hk::Games();
		constexpr int32 Rows = 12;
		constexpr double RowH = 40;
		constexpr Vec2 Origin{ 30, 92 };
		constexpr double ColW = 375;

		auto entryRect = [&](int32 i)
		{
			return RectF{ (Origin.x + (i / Rows) * ColW), (Origin.y + (i % Rows) * RowH), (ColW - 10), (RowH - 4) };
		};

		// pad / keyboard
		if (hk::Repeat(hk::Btn::Down)) { menu.selected = (menu.selected + 1) % entryCount; }
		if (hk::Repeat(hk::Btn::Up)) { menu.selected = (menu.selected + entryCount - 1) % entryCount; }
		if (hk::Repeat(hk::Btn::Right)) { menu.selected = Min(menu.selected + Rows, entryCount - 1); }
		if (hk::Repeat(hk::Btn::Left)) { menu.selected = Max(menu.selected - Rows, 0); }

		int32 chosen = -1;

		if (hk::Down(hk::Btn::Confirm))
		{
			chosen = menu.selected;
		}

		font(U"Siv3D ゲーム集").draw(36, Vec2{ 30, 18 }, ColorF{ 0.95 });
		font(U"Siv3D のサンプル「ゲーム」をパッドとタッチで遊べるようにしたもの").draw(15, Vec2{ 34, 64 }, ColorF{ 0.6 });

		for (int32 i = 0; i < entryCount; ++i)
		{
			const RectF r = entryRect(i);

			// mouse / touch
			if (r.mouseOver() && (Cursor::Delta() != Point{ 0, 0 }))
			{
				menu.selected = i;
			}
			if (r.leftClicked())
			{
				menu.selected = i;
				chosen = i;
			}

			const bool selected = (i == menu.selected);
			r.rounded(6).draw(selected ? ColorF{ 0.25, 0.45, 0.8 } : ColorF{ 0.2, 0.21, 0.25 });

			String label;
			bool available = true;

			if (i < static_cast<int32>(games.size()))
			{
				label = U"{:>2}. {}"_fmt(games[i].number, games[i].title);
				available = static_cast<bool>(games[i].make);
			}
			else
			{
				label = ((i == static_cast<int32>(games.size())) ? U"ライセンス" : U"終了");
			}

			font(label).draw(18, Arg::leftCenter((r.x + 12), r.center().y), (available ? ColorF{ 0.95 } : ColorF{ 0.5 }));

			if (not available)
			{
				font(U"準備中").draw(13, Arg::rightCenter((r.rightX() - 10), r.center().y), ColorF{ 0.5 });
			}
		}

		font(U"↑↓←→: 選ぶ   A: 決定   タッチ / マウスでも選べます").draw(14, Arg::bottomLeft(30, (Scene::Height() - 10)), ColorF{ 0.6 });

		return chosen;
	}
}

namespace hk
{
	const Font& UiFont()
	{
		static const Font font{ FontMethod::MSDF, 48, Typeface::Bold };
		return font;
	}

	bool Button(const String& label, const Vec2& center, const double width, const bool enabled)
	{
		const RectF rect{ Arg::center(center), width, 40 };
		const bool over = (enabled && Over(rect));
		rect.rounded(6).draw(enabled ? (over ? ColorF{ 0.35, 0.6, 1.0 } : ColorF{ 0.25, 0.45, 0.85 }) : ColorF{ 0.5 })
			.drawFrame(1, ColorF{ 1.0, 0.4 });
		UiFont()(label).drawAt(20, center, ColorF{ 1.0 });
		return (enabled && Clicked(rect));
	}
}

void Main()
{
	// letterbox the scene (written for 800x600 etc.) on any screen
	Scene::SetResizeMode(ResizeMode::Keep);
	// Escape returns to the menu instead of quitting
	System::SetTerminationTriggers(UserAction::CloseButtonClicked);

	// licenses: Siv3D's own list plus the fonts and the samples (assets/licenses/*.txt).
	// F1 would open a browser; there is none on the device, so the menu has its own screen.
	LicenseManager::DisableDefaultTrigger();
	LicenseManager::AddLicenseFiles(U"assets/licenses");

	LicenseView licenseView;
	Menu menu;
	HelpOverlay help;
	std::unique_ptr<hk::IGame> game;
	const auto& games = hk::Games();
	const int32 entryCount = static_cast<int32>(games.size() + 2);

	EnterMenu();

	while (System::Update())
	{
		hk::Update();

		if (licenseView.isOpen())
		{
			licenseView.update();
			continue;
		}

		if (game)
		{
			if (hk::MenuRequested())
			{
				game.reset();
				help = HelpOverlay{};
				EnterMenu();
				continue;
			}

			game->update();
			help.draw();
			hk::DrawPadPointer();
			continue;
		}

		const int32 chosen = UpdateMenu(menu, entryCount);

		if (chosen < 0)
		{
			continue;
		}

		if (chosen < static_cast<int32>(games.size()))
		{
			const auto& info = games[chosen];

			if (info.make)
			{
				Scene::Resize(info.sceneSize);
				Scene::SetBackground(Palette::DefaultBackground);
				hk::SetPadPointer(info.padPointer);
				Window::SetTitle(U"{}. {}"_fmt(info.number, info.title));
				game = info.make();
				help.start(info);
			}
		}
		else if (chosen == static_cast<int32>(games.size()))
		{
			licenseView.open();
		}
		else
		{
			System::Exit();
		}
	}
}

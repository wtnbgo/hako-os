// 3. 15 パズル (15 puzzle)
//
// Based on the Siv3D sample "3. 15 パズル"
//   https://siv3d.github.io/ja-jp/samples/games/#3-15-パズル
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: the picture is made from a built-in emoji instead of a file chosen in a
// dialog (there is no file dialog on the device); the d-pad slides the piece next to the
// blank into it; touch / mouse drag as before; small numbers on the pieces; solved
// message and a new puzzle with A; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// 2つ のピースが隣り合っているかを判定する
	bool Swappable(int32 a, int32 b)
	{
		return ((a / 4 == b / 4) && (AbsDiff(a, b) == 1))
			|| ((a % 4 == b % 4) && (AbsDiff(a, b) == 4));
	}

	// hako: a square picture from an emoji on a colored background (replaces Dialog::OpenImage)
	Image MakePicture()
	{
		static const Array<String> emojis = { U"🐈", U"🐕", U"🐧", U"🦊", U"🐼", U"🍣", U"🌻", U"🚀" };
		const HSV bg{ Random(360.0), 0.35, 0.95 };
		Image image{ 400, 400, bg.toColor() };

		for (int32 y = 0; y < image.height(); ++y)
		{
			const Color line = HSV{ (bg.h + y * 0.1), 0.35, (0.95 - y * 0.0008) }.toColor();
			for (int32 x = 0; x < image.width(); ++x)
			{
				image[y][x] = line;
			}
		}

		const Image emoji = Emoji::CreateImage(emojis.choice()).scaled(2.6, InterpolationAlgorithm::Lanczos);
		emoji.paint(image, ((image.width() - emoji.width()) / 2), ((image.height() - emoji.height()) / 2));
		return image;
	}

	class FifteenPuzzle : public hk::IGame
	{
	public:

		FifteenPuzzle()
		{
			Scene::SetBackground(ColorF{ 0.8, 0.9, 1.0 });
			newPuzzle();
		}

		void update() override
		{
			Rect{ Offset, (CellSize * 4) }
				.drawShadow(Vec2{ 0, 2 }, 12, 8)
				.draw(ColorF{ 0.25 })
				.drawFrame(0, 8, ColorF{ 0.3, 0.5, 0.7 });

			const bool solved = (pieces == Range(0, 15).asArray());

			if (solved)
			{
				grabbed.reset();

				if (hk::Down(hk::Btn::Confirm))
				{
					newPuzzle();
				}
			}
			else
			{
				// hako: d-pad slides the piece next to the blank into it (in the pressed direction)
				const int32 blank = static_cast<int32>(std::find(pieces.begin(), pieces.end(), 15) - pieces.begin());
				Optional<int32> from;
				if (hk::Repeat(hk::Btn::Up) && (blank / 4 < 3)) { from = (blank + 4); }
				if (hk::Repeat(hk::Btn::Down) && (0 < blank / 4)) { from = (blank - 4); }
				if (hk::Repeat(hk::Btn::Left) && (blank % 4 < 3)) { from = (blank + 1); }
				if (hk::Repeat(hk::Btn::Right) && (0 < blank % 4)) { from = (blank - 1); }
				if (from)
				{
					std::swap(pieces[blank], pieces[*from]);
				}
			}

			if (not hk::PPressed())
			{
				grabbed.reset();
			}

			for (int32 i = 0; i < 16; ++i)
			{
				const int32 pieceID = pieces[i];
				const Rect rect = Rect{ (CellSize * (i % 4)), (CellSize * (i / 4)), CellSize }.movedBy(Offset);

				if ((pieceID == 15) && not solved)
				{
					if (grabbed && hk::Over(rect) && Swappable(i, grabbed.value()))
					{
						std::swap(pieces[i], pieces[grabbed.value()]);
						grabbed = i;
					}

					continue;
				}

				if (not solved && hk::Clicked(rect))
				{
					grabbed = i;
				}

				rect(texture.uv((pieceID % 4 * 0.25), (pieceID / 4 * 0.25), 0.25, 0.25))
					.draw()
					.drawFrame(1, 0, ColorF{ 1.0, 0.75 });

				// hako: small number in the corner
				if (not solved)
				{
					hk::UiFont()(pieceID + 1).draw(16, rect.pos.movedBy(6, 2), ColorF{ 1.0, 0.85 });
				}

				if (grabbed == i)
				{
					rect.draw(ColorF{ 1.0, 0.5, 0.0, 0.3 });
				}

				if (hk::Over(rect))
				{
					Cursor::RequestStyle(CursorStyle::Hand);
				}
			}

			// 見本を描く
			texture.resized(180)
				.draw((Offset.x + CellSize * 4 + 40), Offset.y)
				.drawFrame(0, 4, ColorF{ 0.3, 0.5, 0.7 });

			if (solved)
			{
				hk::UiFont()(U"完成！").drawAt(48, Vec2{ 670, 330 }, ColorF{ 0.2, 0.4, 0.7 });
				hk::UiFont()(U"A: 次のパズル").drawAt(20, Vec2{ 670, 390 }, ColorF{ 0.3 });
			}
		}

	private:

		// ピースのサイズ
		static constexpr int32 CellSize = 100;

		// 位置
		static constexpr Point Offset{ 60, 40 };

		Texture texture;

		Array<int32> pieces;

		// 掴んでいるピースの番号
		Optional<int32> grabbed;

		void newPuzzle()
		{
			// 正方形に切り抜く
			texture = Texture{ MakePicture(), TextureDesc::Mipped };

			// ランダムな操作でパズルをシャッフルする
			pieces = Range(0, 15);
			{
				// 空白の位置
				int32 blankPos = 15;

				for (int32 i = 0; i < 1000; ++i)
				{
					const int32 to = (blankPos + Sample({ -4, -1, 1, 4 }));

					if (InRange(to, 0, 15) && Swappable(blankPos, to))
					{
						std::swap(pieces[blankPos], pieces[to]);
						blankPos = to;
					}
				}
			}

			grabbed.reset();
		}
	};
}

HK_GAME_FACTORY(MakeFifteenPuzzle, FifteenPuzzle)

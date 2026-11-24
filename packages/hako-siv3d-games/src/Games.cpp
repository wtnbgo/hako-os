// The list of games, in the order of the Siv3D samples page.
// SPDX-License-Identifier: MIT
# include "Common.hpp"

std::unique_ptr<hk::IGame> MakeBreakout();
std::unique_ptr<hk::IGame> MakeCollect();
std::unique_ptr<hk::IGame> MakeFifteenPuzzle();
std::unique_ptr<hk::IGame> MakeNumberConnect();
std::unique_ptr<hk::IGame> MakeTyping();
std::unique_ptr<hk::IGame> MakeEmojiTower();
std::unique_ptr<hk::IGame> MakeShooting();
std::unique_ptr<hk::IGame> MakePinball();
std::unique_ptr<hk::IGame> MakeCookieClicker();
std::unique_ptr<hk::IGame> MakeCards();
std::unique_ptr<hk::IGame> MakeTicTacToe();
std::unique_ptr<hk::IGame> MakeRhythm();
std::unique_ptr<hk::IGame> MakeMinesweeper();
std::unique_ptr<hk::IGame> MakeOthello();
std::unique_ptr<hk::IGame> MakeKlondike();
std::unique_ptr<hk::IGame> MakeConcentration();
std::unique_ptr<hk::IGame> MakeHanoi();
std::unique_ptr<hk::IGame> MakeWheel();
std::unique_ptr<hk::IGame> MakeRpgMap();
std::unique_ptr<hk::IGame> MakeAutoTiles();
std::unique_ptr<hk::IGame> MakeSugoroku();
std::unique_ptr<hk::IGame> MakeSlot();

namespace
{
	const String Docs = U"Siv3D docs (MIT)";
	const String Samples = U"Siv3D-Samples (CC0)";
	const String Reputeless = U"Reputeless/games (CC0)";
}

namespace hk
{
	const Array<GameInfo>& Games()
	{
		// number, title, help, scene size, pad pointer, origin, factory
		static const Array<GameInfo> games =
		{
			{ 1, U"ブロックくずし", { U"←→ / スティック: パドル（タッチ・マウスでも動かせます）", U"A: ボールを落としたら、もう一度" },
				Size{ 800, 600 }, false, Docs, MakeBreakout },
			{ 2, U"落ちてくるアイテムを拾うゲーム", { U"←→ / スティック: 移動（プレイヤーの左右をタッチしても）", U"A: 時間切れのあと、もう一度" },
				Size{ 800, 600 }, false, Docs, MakeCollect },
			{ 3, U"15 パズル", { U"十字キー: 空いたところへピースをすべらせる", U"タッチ・マウス: ピースをドラッグ", U"完成したら A で次のパズル" },
				Size{ 800, 600 }, false, Docs, MakeFifteenPuzzle },
			{ 4, U"数つなぎ", { U"1 から順に、数字の玉の上をポインターでなぞる", U"スティック: ポインター（タッチは指でなぞる）" },
				Size{ 800, 600 }, true, Docs, MakeNumberConnect },
			{ 5, U"タイピングゲーム", { U"十字キー + A: 画面のキーボードで入力（タッチでも）", U"Y: Shift   X: スペース   キーボードがあればそのまま打てます" },
				Size{ 800, 600 }, false, Docs, MakeTyping },
			{ 6, U"絵文字タワー", { U"スティック: ポインター   A: 落とす（タッチでも）", U"右スティック: カメラ   L / R: ズーム" },
				Size{ 1280, 720 }, true, Docs, MakeEmojiTower },
			{ 7, U"シューティングゲーム", { U"スティック / 十字キー: 移動（B か R を押している間はゆっくり）", U"タッチ・マウス: ドラッグで移動。弾は自動で出ます" },
				Size{ 800, 600 }, false, Docs, MakeShooting },
			{ 8, U"ピンボール", { U"L / ← : 左のフリッパー   R / → / A: 右のフリッパー", U"タッチ: 画面の左右の半分", U"ボールが落ちたら A で戻す" },
				Size{ 800, 600 }, false, Docs, MakePinball },
			{ 9, U"クッキークリッカー", { U"A: クッキーを押す   ↑↓: アイテムを選ぶ   Y: 買う", U"タッチ・マウスでも操作できます（進み具合は保存されます）" },
				Size{ 800, 600 }, false, Docs, MakeCookieClicker },
			{ 10, U"トランプを描く", { U"十字キー: カードを選ぶ   A: めくる", U"タッチ・マウス: カードをタップ" },
				Size{ 1280, 720 }, false, Docs, MakeCards },
			{ 11, U"三目並べ", { U"十字キー: マスを選ぶ   A: 置く", U"タッチ・マウスでマスを選んでも置けます", U"終わったら A でもう一度" },
				Size{ 800, 600 }, false, Docs, MakeTicTacToe },
			{ 12, U"音ゲー基礎", { U"← ↓ B A: 4 つのレーン（キーボードは A S D F）", U"タッチ: レーンをタップ   最後まで行ったら A でもう一度" },
				Size{ 800, 600 }, false, Docs, MakeRhythm },
			{ 13, U"マインスイーパー", { U"十字キー: マスを選ぶ   A: 開く   X: 旗", U"タッチ: タップで開く、長押しで旗   START / Y: 新しいゲーム" },
				Size{ 800, 600 }, false, Samples, MakeMinesweeper },
			{ 14, U"AI オセロ", { U"十字キー: マスを選ぶ   A: 置く（タッチ・マウスでも）", U"Y: 先手（黒）で開始   X: 後手（白）で開始" },
				Size{ 800, 600 }, false, Samples, MakeOthello },
			{ 15, U"クロンダイク", { U"タッチ・マウス: カードをドラッグ", U"スティックでポインター、A を押したまま動かしてドラッグ", U"X: 山札をめくる   Y: 新しいゲーム" },
				Size{ 800, 800 }, true, Samples, MakeKlondike },
			{ 16, U"神経衰弱", { U"十字キー: カードを選ぶ   A: めくる", U"タッチ・マウス: カードをタップ" },
				Size{ 1280, 720 }, false, Reputeless, MakeConcentration },
			{ 17, U"ハノイの塔", { U"←→: 塔を選ぶ   A: 円盤を持ち上げる / 置く", U"タッチ・マウス: 塔をタップ" },
				Size{ 800, 600 }, false, Reputeless, MakeHanoi },
			{ 18, U"ルーレット", { U"A / ボタン: スタート、ストップ" },
				Size{ 800, 600 }, false, Reputeless, MakeWheel },
			{ 19, U"2D RPG のマップと移動", { U"十字キー / スティック: 歩く（行きたい方をタッチしても）", U"X / Space: 通れないマスを表示" },
				Size{ 640, 480 }, false, Reputeless, MakeRpgMap },
			{ 20, U"オートタイル", { U"スティック: ポインター   A: 置く   B: 消す（マウスは左 / 右ボタン）", U"L / R: タイルの種類   右下のチェック: つながり方" },
				Size{ 1280, 720 }, true, Samples, MakeAutoTiles },
			{ 21, U"すごろくの基本", { U"A / ボタン: サイコロをふる", U"ゴールしたら A でもう一度" },
				Size{ 800, 600 }, false, Docs, MakeSugoroku },
			{ 22, U"スロットマシン", { U"A / タッチ / Space: スタート、リールを順に止める" },
				Size{ 800, 600 }, false, Docs, MakeSlot },
		};
		return games;
	}
}

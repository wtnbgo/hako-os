// 8. ピンボール (Pinball)
//
// Based on the Siv3D sample "8. ピンボール"
//   https://siv3d.github.io/ja-jp/samples/games/#8-ピンボール
//   Copyright (c) 2018 - present, Ryo Suzuki / Copyright (c) 2018 - present, OpenSiv3D Project
//   MIT License (assets/licenses/siv3d-docs.txt)
// Changes for hako: flippers also with L / R (ZL / ZR, d-pad) and by touching the left or
// right half of the screen; a lost ball is put back with A or a touch; a game class.
//
// SPDX-License-Identifier: MIT
# include "../Common.hpp"

namespace
{
	// 外周の枠の頂点リストを作成する関数
	LineString CreateFrame(const Vec2& leftAnchor, const Vec2& rightAnchor)
	{
		Array<Vec2> points = { leftAnchor, Vec2{ -70, -20 } };
		for (int32 i = -30; i <= 30; ++i)
		{
			points << OffsetCircular(Vec2{ 0.0, -120 }, 70, (i * 3_deg));
		}
		points << Vec2{ 70, -20 } << rightAnchor;
		return LineString{ points };
	}

	// 接触しているかに応じて色を決定する関数
	ColorF GetColor(const P2Body& body, const HashSet<P2BodyID>& list)
	{
		return list.contains(body.id()) ? Palette::White : Palette::Orange;
	}

	// 左右フリッパーの軸の座標
	constexpr Vec2 LeftFlipperAnchor{ -25, 10 }, RightFlipperAnchor{ 25, 10 };

	// hako: where the ball starts again
	constexpr Vec2 BallStart{ -40, -120 };

	class Pinball : public hk::IGame
	{
	public:

		Pinball()
		{
			// 背景色を設定する
			Scene::SetBackground(ColorF(0.2, 0.3, 0.4));

			// 固定の枠
			{
				// 外周
				frames << world.createLineString(P2Static, Vec2{ 0, 0 }, CreateFrame(LeftFlipperAnchor, RightFlipperAnchor));
				// 左上の (
				frames << world.createLineString(P2Static, Vec2{ 0, 0 }, LineString{ Range(-25, -10).map([=](int32 i) { return OffsetCircular(Vec2{ 0.0, -120 }, 55, (i * 3_deg)).toVec2(); }) });
				// 右上の )
				frames << world.createLineString(P2Static, Vec2{ 0, 0 }, LineString{ Range(10, 25).map([=](int32 i) { return OffsetCircular(Vec2{ 0.0, -120 }, 55, (i * 3_deg)).toVec2(); }) });
			}

			// バンパー
			{
				// ● x3
				{
					const P2Material material{ .restitution = 1.0, .restitutionThreshold = 0.1 };
					bumpers << world.createCircle(P2Static, Vec2{ 0, -170 }, 5, material);
					bumpers << world.createCircle(P2Static, Vec2{ -20, -150 }, 5, material);
					bumpers << world.createCircle(P2Static, Vec2{ 20, -150 }, 5, material);
				}
				// ▲ x2
				{
					const P2Material material{ .restitution = 0.8, .restitutionThreshold = 0.1 };
					bumpers << world.createTriangle(P2Static, Vec2{ 0, 0 }, Triangle{ -60, -50, -40, -15, -60, -30 }, material);
					bumpers << world.createTriangle(P2Static, Vec2{ 0, 0 }, Triangle{ 60, -50, 60, -30, 40, -15 }, material);
				}
			}

			const P2Material softMaterial{ .density = 0.1, .restitution = 0.0 };

			// 左フリッパー
			leftFlipper = world.createRect(P2Dynamic, LeftFlipperAnchor, RectF{ 0, 0.4, 21, 4.5 }, softMaterial);
			// 左フリッパーのジョイント
			leftJoint = world.createPivotJoint(frames[0], leftFlipper, LeftFlipperAnchor).setLimits(-20_deg, 25_deg).setLimitsEnabled(true);

			// 右フリッパー
			rightFlipper = world.createRect(P2Dynamic, RightFlipperAnchor, RectF{ -21, 0.4, 21, 4.5 }, softMaterial);
			// 右フリッパーのジョイント
			rightJoint = world.createPivotJoint(frames[0], rightFlipper, RightFlipperAnchor).setLimits(-25_deg, 20_deg).setLimitsEnabled(true);

			// スピナー ＋
			spinner = world.createRect(P2Dynamic, Vec2{ -58, -120 }, SizeF{ 20, 1 }, softMaterial).addRect(RectF{ Arg::center(0, 0), 1, 20 }, P2Material{ 0.01, 0.0 });
			// スピナーのジョイント
			spinnerJoint = world.createPivotJoint(frames[0], spinner, Vec2{ -58, -120 }).setMaxMotorTorque(0.05).setMotorSpeed(0).setMotorEnabled(true);

			// 風車の |
			frames << world.createLine(P2Static, Vec2{ 0, 0 }, Line{ -40, -60, -40, -40 });
			// 風車の羽 ／
			windmillWing = world.createRect(P2Dynamic, Vec2{ -40, -60 }, SizeF{ 30, 2 }, P2Material{ 0.1, 0.8 });
			// 風車のジョイント
			windmillJoint = world.createPivotJoint(frames.back(), windmillWing, Vec2{ -40, -60 }).setMotorSpeed(240_deg).setMaxMotorTorque(10000.0).setMotorEnabled(true);

			// 振り子の軸
			pendulumBase = world.createPlaceholder(P2Static, Vec2{ 0, -190 });
			// 振り子 ●
			pendulum = world.createCircle(P2Dynamic, Vec2{ 0, -120 }, 4, P2Material{ 0.1, 1.0 });
			// 振り子のジョイント
			pendulumJoint = world.createDistanceJoint(pendulumBase, Vec2{ 0, -190 }, pendulum, Vec2{ 0, -120 }, 70);

			// エレベーターの上部 ●
			elevatorA = world.createCircle(P2Static, Vec2{ 40, -100 }, 3);
			// エレベーターの床 －
			elevatorB = world.createRect(P2Dynamic, Vec2{ 40, -100 }, SizeF{ 20, 2 });
			// エレベーターのジョイント
			elevatorSliderJoint = world.createSliderJoint(elevatorA, elevatorB, Vec2{ 40, -100 }, Vec2::Down()).setLimits(5, 50).setLimitEnabled(true).setMaxMotorForce(10000).setMotorSpeed(-100);

			// ボール 〇
			ball = world.createCircle(P2Dynamic, BallStart, 4, P2Material{ 0.05, 0.0 });
			ballID = ball.id();
		}

		void update() override
		{
			if (4s < sliderStopwatch)
			{
				// エレベーターの巻き上げを停止
				elevatorSliderJoint.setMotorEnabled(false);
				sliderStopwatch.restart();
			}
			else if (2s < sliderStopwatch)
			{
				// エレベーターの巻き上げ
				elevatorSliderJoint.setMotorEnabled(true);
			}

			// hako: flipper input from the pad, keys and touch (left / right half of the screen)
			const bool touchLeft = (hk::PPressed() && (hk::Pos().x < Scene::Center().x));
			const bool touchRight = (hk::PPressed() && (Scene::Center().x <= hk::Pos().x));
			const bool leftOn = (hk::Pressed(hk::Btn::Left) || hk::Pressed(hk::Btn::L) || hk::Pressed(hk::Btn::ZL) || touchLeft);
			const bool rightOn = (hk::Pressed(hk::Btn::Right) || hk::Pressed(hk::Btn::R) || hk::Pressed(hk::Btn::ZR)
				|| hk::Pressed(hk::Btn::Confirm) || touchRight);

			// hako: the ball fell out below the flippers: put it back
			const bool lost = (60 < ball.getPos().y);
			if (lost && (hk::Down(hk::Btn::Confirm) || hk::Down(hk::Btn::Start) || hk::PDown()))
			{
				ball.setPos(BallStart);
				ball.setVelocity(Vec2{ 0, 0 });
			}

			// ボールと接触しているボディの ID
			HashSet<P2BodyID> collidedIDs;

			// 物理演算ワールドの更新
			for (accumulatedTime += Scene::DeltaTime(); StepTime <= accumulatedTime; accumulatedTime -= StepTime)
			{
				// 振り子の揺れをおさえる抵抗
				pendulum.applyForce(Vec2{ (pendulum.getVelocity().x < 0.0) ? 0.0001 : -0.0001, 0.0 });

				// 左フリッパーの操作
				leftFlipper.applyTorque(leftOn ? -80 : 40);

				// 右フリッパーの操作
				rightFlipper.applyTorque(rightOn ? 80 : -40);

				world.update(StepTime);

				// ボールと接触しているボディの ID を格納
				for (auto&& [pair, collision] : world.getCollisions())
				{
					if (pair.a == ballID)
					{
						collidedIDs.emplace(pair.b);
					}
					else if (pair.b == ballID)
					{
						collidedIDs.emplace(pair.a);
					}
				}
			}

			{
				// 描画用の Transformer2D
				const auto transformer = camera.createTransformer();

				// 枠の描画
				for (const auto& frame : frames)
				{
					frame.draw(Palette::Skyblue);
				}

				// スピナーの描画
				spinner.draw(GetColor(spinner, collidedIDs));

				// バンパーの描画
				for (const auto& bumper : bumpers)
				{
					bumper.draw(GetColor(bumper, collidedIDs));
				}

				// 風車の描画
				windmillWing.draw(GetColor(windmillWing, collidedIDs));

				// 振り子の描画
				pendulum.draw(GetColor(pendulum, collidedIDs));

				// エレベーターの描画
				elevatorA.draw(GetColor(elevatorA, collidedIDs));
				elevatorB.draw(GetColor(elevatorB, collidedIDs));

				// ボールの描画
				ball.draw(Palette::White);

				// フリッパーの描画
				leftFlipper.draw(Palette::Orange);
				rightFlipper.draw(Palette::Orange);

				// ジョイントの可視化
				leftJoint.draw(Palette::Red);
				rightJoint.draw(Palette::Red);
				spinnerJoint.draw(Palette::Red);
				windmillJoint.draw(Palette::Red);
				pendulumJoint.draw(Palette::Red);
				elevatorSliderJoint.draw(Palette::Red);
			}

			if (lost)
			{
				hk::UiFont()(U"A / タッチ: ボールを戻す").drawAt(26, Vec2{ Scene::Center().x, (Scene::Height() - 40) });
			}
		}

	private:

		// 2D 物理演算のシミュレーションステップ（秒）
		static constexpr double StepTime = (1.0 / 200.0);

		// 2D 物理演算のシミュレーション蓄積時間（秒）
		double accumulatedTime = 0.0;

		// 物理演算用のワールド
		P2World world{ 60.0 };

		// 固定の枠
		Array<P2Body> frames;

		// バンパー
		Array<P2Body> bumpers;

		P2Body leftFlipper, rightFlipper, spinner, windmillWing, pendulumBase, pendulum, elevatorA, elevatorB, ball;
		P2PivotJoint leftJoint, rightJoint, spinnerJoint, windmillJoint;
		P2DistanceJoint pendulumJoint;
		P2SliderJoint elevatorSliderJoint;
		P2BodyID ballID = 0;

		// エレベーターのアニメーション用ストップウォッチ
		Stopwatch sliderStopwatch{ StartImmediately::Yes };

		// 2D カメラ
		const Camera2D camera{ Vec2{ 0, -80 }, 2.4 };
	};
}

HK_GAME_FACTORY(MakePinball, Pinball)

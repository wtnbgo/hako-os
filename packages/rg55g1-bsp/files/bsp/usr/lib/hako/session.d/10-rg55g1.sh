# Anbernic RG 55G1 defaults for hako-session (overridden by
# /etc/default/hako-session and /etc/hako/session.d/*.sh).

# The panel is portrait (1080x1920) and mounted like the Retroid Pocket Mini's
# (device tree rotation = <90>), so the same 270 gives a landscape output.
# Checked on the device; changing it also needs the touch matrix in
# /usr/lib/udev/rules.d/72-rg55g1-touch-rotate.rules.
HAKO_ROTATE=270

# built-in pad (singleadc-joypad, "Anbernic RG 55G1 Gamepad", bus 0x19,
# 484b:1101): Nintendo (Switch) face-button labels
SDL_GAMECONTROLLERTYPE=0x484b/0x1101=SwitchPro

# Gamepad mapping for the built-in pad. Unlike the Retroid Pocket Mini, this
# pad reports the left face button as BTN_NORTH (307, old name BTN_X) and the
# top one as BTN_WEST (308, BTN_Y), so x (left) = b2 and y (top) = b3
# (checked with evtest: printed Y = left = 307, printed X = top = 308).
# Buttons in SDL's order: SOUTH EAST NORTH WEST TL TR SELECT START THUMBL
# THUMBR, then KEY_F10 (b10) and KEY_BACK (b11): the two buttons below the
# screen (Android home / back), left = KEY_F10 -> guide, right = KEY_BACK ->
# misc1.
# The GUID has no name CRC (bytes 2-3 = 0), which SDL accepts for any name.
SDL_GAMECONTROLLERCONFIG="190000004b4800000111000000010000,Anbernic RG 55G1 Gamepad,a:b0,b:b1,x:b2,y:b3,back:b6,start:b7,leftstick:b8,rightstick:b9,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a3,righty:a4,lefttrigger:a2,righttrigger:a5,guide:b10,misc1:b11,platform:Linux,"

# GPU: Debian's Mesa (25.0, and 26.1 in backports) does not know the Adreno 613
# ("unsupported GPU id ... chip id 0x..06010300"); it was added in Mesa 26.2.
# Until then cage draws in software and SDL apps use the software renderer.
# GPU apps (raylib, LOVE, Siv3D) do not run. Remove both lines with Mesa >= 26.2.
WLR_RENDERER=pixman
SDL_RENDER_DRIVER=software

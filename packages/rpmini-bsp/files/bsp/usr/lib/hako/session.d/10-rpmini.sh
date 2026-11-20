# Retroid Pocket Mini defaults for hako-session (overridden by
# /etc/default/hako-session and /etc/hako/session.d/*.sh).

# The panel is portrait (960x1280). 270: the top of the image is on the side
# of the panel's natural right edge, so apps see a landscape 1280x960 output.
# Changing it also needs the touch matrix in
# /usr/lib/udev/rules.d/72-rpmini-touch-rotate.rules (override in /etc/udev/rules.d/).
HAKO_ROTATE=270

# built-in pad: Nintendo (Switch) face-button labels
SDL_GAMECONTROLLERTYPE=0x2020/0x3001=SwitchPro

# Gamepad mapping for the built-in pad. SDL's automatic Linux mapping treats
# BTN_NORTH as "x" (its old name BTN_X), which swaps the top and left buttons;
# here x (west/left) = b3 = BTN_WEST and y (north/top) = b2 = BTN_NORTH.
SDL_GAMECONTROLLERCONFIG="0300f353202000000130000001000000,Retroid Pocket Gamepad,a:b0,b:b1,x:b3,y:b2,back:b6,guide:b8,start:b7,leftstick:b9,rightstick:b10,leftshoulder:b4,rightshoulder:b5,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a3,righty:a4,lefttrigger:a6,righttrigger:a7,platform:Linux,"

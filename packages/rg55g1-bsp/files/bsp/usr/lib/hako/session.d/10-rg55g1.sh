# Anbernic RG 55G1 defaults for hako-session (overridden by
# /etc/default/hako-session and /etc/hako/session.d/*.sh).

# The panel is portrait (1080x1920) and mounted like the Retroid Pocket Mini's
# (device tree rotation = <90>), so the same 270 gives a landscape output.
# TO CHECK on the device; changing it also needs the touch matrix in
# /usr/lib/udev/rules.d/72-rg55g1-touch-rotate.rules.
HAKO_ROTATE=270

# The built-in pad (singleadc-joypad, "Anbernic RG 55G1 Gamepad") reports the
# standard BTN_SOUTH/EAST/WEST/NORTH and a hat for the d-pad, so SDL maps it by
# itself. TO CHECK on the device: the printed layout (Xbox-like A at the
# bottom, or Nintendo-like A on the right). For a Nintendo layout add:
#SDL_GAMECONTROLLERTYPE=<vid>/<pid>=SwitchPro

# GPU: Debian's Mesa (25.0, and 26.1 in backports) does not know the Adreno 613
# ("unsupported GPU id ... chip id 0x..06010300"); it was added in Mesa 26.2.
# Until then cage draws in software and SDL apps use the software renderer.
# GPU apps (raylib, LOVE, Siv3D) do not run. Remove both lines with Mesa >= 26.2.
WLR_RENDERER=pixman
SDL_RENDER_DRIVER=software

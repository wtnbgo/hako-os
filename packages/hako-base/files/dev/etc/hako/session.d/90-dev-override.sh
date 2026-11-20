# hako-dev: development override for the game session.
# Put e.g. HAKO_APP=/data/dev/mygame (and HAKO_APP_ARGS=...) in
# /data/dev/session.env, then: systemctl restart hako-session
[ -r /data/dev/session.env ] && . /data/dev/session.env

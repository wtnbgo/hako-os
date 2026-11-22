#!/usr/bin/env python3
"""Draw the boot splash images (Plymouth theme "hako") with Pillow.

    python3 packages/hako-base/splash/make-images.py

Writes logo.png and dot.png into files/session/usr/share/plymouth/themes/hako/.
The images are committed; run this only to change them. Colors follow the
launcher (background 0x14161c, accent 0xffc83c, text 0xeeeeee). The wordmark
uses DejaVu Sans Bold (fonts-dejavu-core).
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'files', 'session', 'usr', 'share', 'plymouth', 'themes', 'hako')
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
ACCENT = (0xff, 0xc8, 0x3c, 255)
TEXT = (0xee, 0xee, 0xee, 255)
S = 4  # supersampling


def box(d, cx, top, w, line):
    """An open box seen slightly from above: front face, top face, two flaps."""
    h = w * 0.62           # front face height
    dy = w * 0.22          # depth of the top face
    x0, x1 = cx - w / 2, cx + w / 2
    y_top, y_front = top + dy * 2, top + dy * 3
    # top face (rim)
    rim = [(x0, y_top), (cx, y_top - dy), (x1, y_top), (cx, y_front)]
    d.polygon(rim, outline=ACCENT, width=line)
    # front faces
    d.line([(x0, y_top), (x0, y_top + h), (cx, y_front + h), (x1, y_top + h), (x1, y_top)], fill=ACCENT, width=line, joint='curve')
    d.line([(cx, y_front), (cx, y_front + h)], fill=ACCENT, width=line)
    # open flaps
    d.line([(x0, y_top), (x0 - w * 0.16, y_top - dy * 1.1), (cx - w * 0.16, y_top - dy * 2.1), (cx, y_top - dy)], fill=ACCENT, width=line, joint='curve')
    d.line([(x1, y_top), (x1 + w * 0.16, y_top - dy * 1.1), (cx + w * 0.16, y_top - dy * 2.1), (cx, y_top - dy)], fill=ACCENT, width=line, joint='curve')
    return y_front + h


def logo():
    W, H = 420 * S, 420 * S
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    bottom = box(d, W / 2, 10 * S, 190 * S, 9 * S)
    f = ImageFont.truetype(FONT, 96 * S)
    tw = d.textlength('hako', font=f)
    d.text(((W - tw) / 2, bottom + 26 * S), 'hako', font=f, fill=TEXT)
    im = im.crop(im.getbbox())
    im = im.resize((im.width // S, im.height // S), Image.LANCZOS)
    im.save(os.path.join(OUT, 'logo.png'), optimize=True)
    print('logo.png', im.size)


def dot():
    r = 7
    im = Image.new('RGBA', (2 * r * S, 2 * r * S), (0, 0, 0, 0))
    ImageDraw.Draw(im).ellipse([0, 0, 2 * r * S - 1, 2 * r * S - 1], fill=ACCENT)
    im = im.resize((2 * r, 2 * r), Image.LANCZOS)
    im.save(os.path.join(OUT, 'dot.png'), optimize=True)
    print('dot.png', im.size)


os.makedirs(OUT, exist_ok=True)
logo()
dot()

#!/usr/bin/env python3
"""Writes the license texts of raylib for the packages that build it in statically
(hako-raylib-hello, hako-raylib-classics):

  raylib.txt               LICENSE of raylib (the submodule raylib/)
  raylib-external.txt      the license notices of the third-party code raylib compiles in
                           (raylib/src/external: the comment blocks that hold the license)

Run again after updating the raylib submodule.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
PKG = os.path.dirname(HERE)
RAYLIB = os.path.join(PKG, 'raylib')

# compiled into raylib on the SDL platform with the default config.h
EXTERNAL = [
    'cgltf.h', 'dr_flac.h', 'dr_mp3.h', 'dr_wav.h', 'glad.h', 'glad_gles2.h', 'jar_mod.h', 'jar_xm.h',
    'm3d.h', 'miniaudio.h', 'par_shapes.h', 'qoa.h', 'qoaplay.c', 'qoi.h', 'rprand.h', 'sdefl.h',
    'sinfl.h', 'stb_image.h', 'stb_image_resize2.h', 'stb_image_write.h', 'stb_perlin.h',
    'stb_rect_pack.h', 'stb_truetype.h', 'stb_vorbis.c', 'tinyobj_loader_c.h', 'vox_loader.h',
]

LICENSE_WORDS = re.compile(r'(?i)\b(license|licence|licensed|copyright|public domain|permission is hereby|unlicense)\b')


def comment_blocks(text):
    for m in re.finditer(r'/\*.*?\*/', text, re.S):
        yield m.group(0)
    # runs of // comments
    for m in re.finditer(r'(?:^[ \t]*//.*\n){3,}', text, re.M):
        yield m.group(0)


def notices(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    out = []
    for block in comment_blocks(text):
        if not LICENSE_WORDS.search(block):
            continue
        if len(block) >= 6000:
            # a long documentation block: keep the part from the license heading on
            lines = block.splitlines()
            start = next(i for i, l in enumerate(lines) if LICENSE_WORDS.search(l))
            for i, l in enumerate(lines):
                if l.strip().lower().lstrip('#').strip() in ('license', 'licence'):
                    start = i
                    break
            block = '\n'.join(lines[start:start + 45])
        if block not in out:
            out.append(block)
    return out


def write(name, body):
    with open(os.path.join(HERE, name), 'w', encoding='utf-8', newline='\n') as f:
        f.write(body.rstrip() + '\n')


write('raylib.txt', 'raylib\nhttps://github.com/raysan5/raylib\n\n'
      + open(os.path.join(RAYLIB, 'LICENSE'), encoding='utf-8-sig').read())

parts = ['Third-party code compiled into raylib (raylib/src/external)\n'
         'The license notices as found in each file.\n']
for name in EXTERNAL:
    path = os.path.join(RAYLIB, 'src', 'external', name)
    if not os.path.exists(path):
        continue
    blocks = notices(path)
    parts.append('=' * 72 + f'\n{name}\n' + '=' * 72)
    parts.extend(blocks if blocks else ['(no license notice found in the file)'])
write('raylib-external.txt', '\n\n'.join(parts))
print('ok')

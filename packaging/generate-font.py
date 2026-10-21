#!/usr/bin/env python3
"""Bake the game's UTF-8 literals into Noto Sans SC glyphs (SIL OFL 1.1)."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('font', type=Path)
args = parser.parse_args()
small = ImageFont.truetype(str(args.font), 12)
normal = ImageFont.truetype(str(args.font), 14)
for font in (small, normal):
    try:
        font.set_variation_by_axes([500])
    except OSError:
        pass
characters = set(chr(c) for c in range(32, 127))
for directory, suffix in [('source', '*.cpp'), ('include', '*.hpp')]:
    for source in (root / directory).glob(suffix):
        if source.name != 'font_data.hpp':
            characters.update(source.read_text())
characters = sorted(c for c in characters if ord(c) >= 32 and ord(c) != 127)
out = ['// Generated from Noto Sans SC, SIL OFL 1.1. See packaging/FONT-LICENSE.txt.',
       '#pragma once', '#include <cstdint>', '#include <cstddef>',
       'struct FontGlyph { uint32_t codepoint; uint8_t width; uint8_t alpha[196]; };',
       'static const FontGlyph FONT_GLYPHS[] = {']
for char in characters:
    latin = ord(char) < 0x2e80
    font = small if latin else normal
    width = max(3, min(14, round(font.getlength(char)))) if latin else 14
    image = Image.new('L', (14, 14))
    bbox = font.getbbox(char)
    ImageDraw.Draw(image).text(((width - (bbox[2] - bbox[0])) // 2 - bbox[0],
                               (14 - (bbox[3] - bbox[1])) // 2 - bbox[1]), char,
                              font=font, fill=255)
    out.append('{%d,%d,{%s}},' % (ord(char), width, ','.join(map(str, image.tobytes()))))
out.extend(['};', 'constexpr size_t FONT_COUNT = sizeof(FONT_GLYPHS) / sizeof(FONT_GLYPHS[0]);'])
(root / 'include/font_data.hpp').write_text('\n'.join(out) + '\n')
print('Generated', len(characters), 'glyphs')

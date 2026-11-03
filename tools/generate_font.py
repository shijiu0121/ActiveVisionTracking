"""Rebuild only the Chinese glyphs used by UI strings; never package a full TTF."""
from pathlib import Path
import re
import argparse
from PIL import Image, ImageFont, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--font', default='C:/Windows/Fonts/msyh.ttc')
    args = parser.parse_args()
    text = '\n'.join((ROOT / 'User/Src' / f).read_text(encoding='utf-8') for f in ('ui.c', 'app.c', 'avt_runtime.c'))
    strings = re.findall(r'"([^"\n]*)"', text)
    chars = sorted({ch for s in strings for ch in s if ord(ch) > 127})
    font = ImageFont.truetype(args.font, 22)
    entries = []
    for ch in chars:
        image = Image.new('L', (24, 24))
        draw = ImageDraw.Draw(image)
        x0, y0, x1, y1 = draw.textbbox((0, 0), ch, font=font)
        draw.text(((24-(x1-x0))//2-x0, (24-(y1-y0))//2-y0), ch, font=font, fill=255)
        values = []
        for y in range(24):
            for start in (0, 8, 16):
                values.append(sum((128 >> bit) for bit in range(8) if image.getpixel((start+bit, y)) >= 100))
        entries.append('    {0x%04x, {%s}}, /* %s */' % (ord(ch), ','.join('0x%02x' % n for n in values), ch))
    body = '''/* Generated local UI glyph subset, 24x24 monochrome; see tools/generate_font.py. */
#include "font_cn.h"
#include <stddef.h>
typedef struct { uint16_t code; uint8_t bits[72]; } Glyph;
static const Glyph glyphs[] = {
%s
};
const uint8_t *FontCN_Find(uint16_t code) {
    unsigned lo = 0, hi = sizeof(glyphs) / sizeof(glyphs[0]);
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        if (glyphs[mid].code < code) lo = mid + 1;
        else hi = mid;
    }
    if (lo < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[lo].code == code) return glyphs[lo].bits;
    return NULL;
}
''' % '\n'.join(entries)
    (ROOT / 'User/Src/font_cn.c').write_text(body, encoding='utf-8')
    print(f'{len(chars)} Chinese/punctuation glyphs; {len(chars)*74} bytes of glyph table')


if __name__ == '__main__':
    main()

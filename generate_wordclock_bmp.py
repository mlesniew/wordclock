#!/usr/bin/python3

MATRIX = '''
    HALFZERO
    YQUARTER
    PASTODOT
    SIXTWONE
    THREETEN
    FIVEIGHT
    TWELFOUR
    SEVENINE
'''.strip()

WORDS = [
    'zero',
    'one',
    'two',
    'three',
    'four',
    'five',
    'six',
    'seven',
    'eight',
    'nine',
    'ten',
    'el-even',
    'twel-ve',
    'a-quarter',
    'half',
    'past',
    'to',
    'dot'
]

MATRIX = MATRIX.strip().upper().replace(' ', '')
WORDS = [w.upper() for w in WORDS]

for idx, word in enumerate(WORDS):
    name = word.replace('-', '').upper()
    print(f'#define BMP_{name:10s} (bitmap[{idx}])')

print()
print('const unsigned char bitmap[][8] = {')
for word_idx, word in enumerate(WORDS):
    name = word.replace('-', '').upper()

    def iter_indices():
        parts = word.split('-')
        idx = 0
        for part in parts:
            idx = MATRIX.index(part, idx)
            assert idx >= 0
            for s, c in enumerate(part):
                yield idx + s
            idx += len(part)

    tab = [[0 for x in range(8)] for y in range(8)]

    for idx in iter_indices():
        x = idx % 9
        y = idx // 9
        tab[y][x] = 1

    t = '{ ' + ', '.join('%3i' % sum(e << (i) for i, e in enumerate(row)) for row in reversed(tab)) + ' }'
    print(f'    {t},        //  {word_idx}  -- {name}')
print('};')

"""
Regenerate the two app images:

  flipchess_10px.png       - 10x10 monochrome FAP icon shown in the
                             apps menu. We draw a clean chess-king
                             silhouette in pixel art.
  icons/FLIPR_128x64.png   - 128x64 monochrome start-screen background.
                             Layout-aware: leaves the top 14 px and
                             bottom 11 px clear because the start-screen
                             scene overlays "Chess", the version, and
                             the Sound/Silent button labels there.

Run with `python scripts/gen_icons.py` from the repo root.
"""

from PIL import Image


# ------------------------------------------------------------------ utils

def from_bitmap(rows, scale=1):
    """Build a 1-bit PIL image from rows of `'X'` (foreground) and `'.'` /
    space (background). Each row must be the same length. `scale` upscales
    each source pixel to a scale x scale block."""
    h = len(rows)
    w = len(rows[0])
    for r in rows:
        assert len(r) == w, f"row length mismatch: {len(r)} vs {w}"

    img = Image.new("1", (w * scale, h * scale), color=1)  # white bg
    px = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == "X":
                for dy in range(scale):
                    for dx in range(scale):
                        px[x * scale + dx, y * scale + dy] = 0  # black fg
    return img


def paste_bitmap(target, rows, ox, oy, scale=1):
    """Paste a `from_bitmap`-style bitmap onto an existing image at (ox, oy)."""
    src = from_bitmap(rows, scale=scale)
    target.paste(src, (ox, oy))


# ------------------------------------------------------------------ 10x10 icon

# A king silhouette in 10x10. The cross sits on top, then the crown
# spreads to a heavy base. Keeping the base solid makes it readable
# even when the menu's selection cursor inverts the icon.
KING_10 = [
    "....XX....",   # cross top
    "..XXXXXX..",   # cross arms
    "....XX....",   # cross stem
    "..X.XX.X..",   # crown points
    "..XXXXXX..",   # crown band
    "..XXXXXX..",   # body upper
    ".XXXXXXXX.",   # body
    ".XXXXXXXX.",   # body
    "XXXXXXXXXX",   # base
    "XXXXXXXXXX",   # base
]


# ------------------------------------------------------------------ 128x64 startscreen

# Eight chess-piece silhouettes, each 12x18 pixels. Drawn in a row across
# the middle of the screen, like a chess set lineup. White pieces are
# represented as solid silhouettes (the Flipper screen has no halftones).

# Pawn: small round head, narrow neck, wide base.
PAWN = [
    "............",
    "....XXXX....",
    "...XXXXXX...",
    "...XXXXXX...",
    "....XXXX....",
    ".....XX.....",
    "....XXXX....",
    "...XXXXXX...",
    "...XXXXXX...",
    "....XXXX....",
    ".....XX.....",
    ".....XX.....",
    "....XXXX....",
    "...XXXXXX...",
    "..XXXXXXXX..",
    ".XXXXXXXXXX.",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
]

# Knight: stylized horse head facing right.
KNIGHT = [
    "............",
    ".....XXX....",
    "....XXXXX...",
    "...XXXXXXX..",
    "..XXX.XXXXX.",
    ".XX...XXXXX.",
    "X.....XXXXXX",
    "......XXXXXX",
    ".....XXXXXXX",
    "....XXXXXXXX",
    "...XXXXXXXXX",
    "..XXXXXXXXXX",
    "..XXXXXXXXXX",
    "..XXXXXXXXXX",
    ".XXXXXXXXXXX",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
]

# Bishop: pointed mitre with a slot, then a wide base.
BISHOP = [
    ".....XX.....",
    "....XXXX....",
    "....XXXX....",
    "....X..X....",
    "...XXXXXX...",
    "...XXXXXX...",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    "...XXXXXX...",
    "....XXXX....",
    ".....XX.....",
    "....XXXX....",
    "...XXXXXX...",
    "..XXXXXXXX..",
    ".XXXXXXXXXX.",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
]

# Rook: castle battlements on top, straight body, wide base.
ROOK = [
    "............",
    ".XX.XX.XX.XX",  # battlements
    ".XX.XX.XX.XX",
    ".XXXXXXXXXX.",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    "...XXXXXX...",
    "...XXXXXX...",
    "...XXXXXX...",
    "...XXXXXX...",
    "...XXXXXX...",
    "...XXXXXX...",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    ".XXXXXXXXXX.",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
]

# Queen: crown with five points, then royal body.
QUEEN = [
    "X..X..X..X..",  # crown points
    "X..X..X..X..",
    "XXXXXXXXXXXX",  # crown band
    ".XXXXXXXXXX.",
    ".XXXXXXXXXX.",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    "...XXXXXX...",
    "...XXXXXX...",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    ".XXXXXXXXXX.",
    ".XXXXXXXXXX.",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
]

# King: cross on top, then a tall body.
KING = [
    ".....XX.....",
    "....XXXX....",
    "...XXXXXX...",
    "....XXXX....",
    ".....XX.....",
    "...X.XX.X...",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    "...XXXXXX...",
    "...XXXXXX...",
    "..XXXXXXXX..",
    "..XXXXXXXX..",
    ".XXXXXXXXXX.",
    ".XXXXXXXXXX.",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
    "XXXXXXXXXXXX",
]


def make_icon():
    img = from_bitmap(KING_10)
    return img


def make_startscreen():
    # Canvas is fully white. We draw within the safe band y=15..52 so the
    # overlay text/buttons stay readable.
    img = Image.new("1", (128, 64), color=1)

    # Eight pieces, each 12 wide, with a 4-pixel gap on each side.
    # 8 * 12 = 96 wide, plus 7 gaps of 2 px = 14, total 110. Center it:
    pieces = [ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK]
    piece_w = 12
    gap = 4
    row_w = len(pieces) * piece_w + (len(pieces) - 1) * gap  # 96 + 28 = 124
    start_x = (128 - row_w) // 2  # 2 px margin each side
    y = 16  # safely below the "Chess v1.12" overlay
    for i, p in enumerate(pieces):
        ox = start_x + i * (piece_w + gap)
        paste_bitmap(img, p, ox, y)

    # A thin baseline under the pieces ties them together visually
    # without intruding into the bottom button-label band (y=53+).
    base_y = y + 18  # piece is 18 tall
    px = img.load()
    for x in range(0, 128):
        px[x, base_y] = 0

    return img


# ------------------------------------------------------------------ main

def main():
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)

    icon = make_icon()
    icon_path = os.path.join(repo, "flipchess_10px.png")
    icon.save(icon_path, optimize=True)
    print(f"wrote {icon_path}  ({icon.size[0]}x{icon.size[1]} 1-bit)")

    start = make_startscreen()
    start_path = os.path.join(repo, "icons", "FLIPR_128x64.png")
    start.save(start_path, optimize=True)
    print(f"wrote {start_path}  ({start.size[0]}x{start.size[1]} 1-bit)")


if __name__ == "__main__":
    main()

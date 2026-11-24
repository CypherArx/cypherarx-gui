import argparse
import pathlib
import struct

from PIL import Image, ImageChops, ImageDraw

GRID = 192
SUPERSAMPLE = 8
ICON_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
PREVIEW_SIZE = 256
PIXELS_SIZE = 20
PIXELS_SCALE = 1
SHEET_SCALE_LIMIT = 128
SHEET_GAP = 24

PALETTES = {
    "ui": {"ground": "#000000", "edge": "#444850", "mark": "#e2e5ef"},
    "amber": {"ground": "#16171a", "edge": "#30333b", "mark": "#ffb224"},
}
DEFAULT_PALETTE = "ui"

TILE_RADIUS = 42
TILE_EDGE_WIDTH = 2.5

TOWER_LEFT = 32
TOWER_RIGHT = 160
CROWN_TOP = 8
CROWN_BOTTOM = 58
CROWN_RADIUS = 4
NECK_TOP = 52
NECK_BOTTOM = 76
NECK_INSET = 8
BODY_TOP = 70
BODY_INSET = 20
BODY_BOTTOM = 184
CENTER_X = 96

DETAILED = {
    "scale": 0.74,
    "gaps": [(56, 72), (88, 104), (120, 136)],
    "gap_depth": 32,
    "keyhole": {"y": 110, "radius": 15, "top": 6, "bottom": 12, "foot": 152},
}
SIMPLIFIED = {
    "scale": 0.8,
    "gaps": [(63, 81), (111, 129)],
    "gap_depth": 34,
    "keyhole": {"y": 112, "radius": 20, "top": 8, "bottom": 16, "foot": 164},
}
DETAILED_FROM = 48

PIXEL_MAPS = {
    16: [
        "................",
        "..###.####.###..",
        "..###.####.###..",
        "..############..",
        "..############..",
        "...##########...",
        "....########....",
        "....########....",
        "....########....",
        "...####..####...",
        "...####..####...",
        "...####..####...",
        "...####..####...",
        "..############..",
        "..############..",
        "................",
    ],
    20: [
        "....................",
        "....................",
        "....................",
        ".....##.####.##.....",
        ".....##.####.##.....",
        ".....##########.....",
        ".....##########.....",
        "......########......",
        "......########......",
        "......##....##......",
        "......##....##......",
        "......###..###......",
        "......###..###......",
        ".....##########.....",
        ".....##########.....",
        ".....##########.....",
        ".....##########.....",
        "....................",
        "....................",
        "....................",
    ],
}


def rgba(colour):
    return tuple(int(colour[i:i + 2], 16) for i in (1, 3, 5)) + (255,)


def tower(variant):
    s = SUPERSAMPLE
    image = Image.new("L", (GRID * s, GRID * s), 0)
    d = ImageDraw.Draw(image)

    def polygon(points, fill=255):
        d.polygon([(x * s, y * s) for x, y in points], fill=fill)

    d.rounded_rectangle((TOWER_LEFT * s, CROWN_TOP * s, TOWER_RIGHT * s, CROWN_BOTTOM * s), CROWN_RADIUS * s, fill=255)
    for left, right in variant["gaps"]:
        d.rectangle((left * s, 0, right * s, variant["gap_depth"] * s), fill=0)
    polygon([(TOWER_LEFT + NECK_INSET, NECK_TOP), (TOWER_RIGHT - NECK_INSET, NECK_TOP),
             (TOWER_RIGHT - BODY_INSET, NECK_BOTTOM), (TOWER_LEFT + BODY_INSET, NECK_BOTTOM)])
    polygon([(TOWER_LEFT + BODY_INSET, BODY_TOP), (TOWER_RIGHT - BODY_INSET, BODY_TOP), (TOWER_RIGHT, BODY_BOTTOM),
             (TOWER_LEFT, BODY_BOTTOM)])
    k = variant["keyhole"]
    d.ellipse(((CENTER_X - k["radius"]) * s, (k["y"] - k["radius"]) * s, (CENTER_X + k["radius"]) * s,
               (k["y"] + k["radius"]) * s), fill=0)
    polygon([(CENTER_X - k["top"], k["y"]), (CENTER_X + k["top"], k["y"]), (CENTER_X + k["bottom"], k["foot"]),
             (CENTER_X - k["bottom"], k["foot"])], fill=0)
    return image


def ground(size, palette):
    big = size * SUPERSAMPLE
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    ImageDraw.Draw(image).rounded_rectangle(
        (0, 0, big - 1, big - 1),
        round(big * TILE_RADIUS / GRID),
        fill=palette["ground"],
        outline=palette["edge"],
        width=max(SUPERSAMPLE, round(big * TILE_EDGE_WIDTH / GRID)),
    )
    return image.resize((size, size), Image.LANCZOS)


def draw_icon(size, palette, variant=None):
    image = ground(size, palette)
    mark = Image.new("RGBA", (size, size), rgba(palette["mark"]))
    if size in PIXEL_MAPS and variant is None:
        mask = Image.new("L", (size, size), 0)
        for y, row in enumerate(PIXEL_MAPS[size]):
            for x, cell in enumerate(row):
                if cell == "#":
                    mask.putpixel((x, y), 255)
        image.paste(mark, (0, 0), mask)
        return image
    if variant is None:
        variant = DETAILED if size >= DETAILED_FROM else SIMPLIFIED
    inner = round(size * variant["scale"])
    mask = Image.new("L", (size, size), 0)
    mask.paste(tower(variant).resize((inner, inner), Image.LANCZOS), ((size - inner) // 2, (size - inner) // 2))
    image.paste(mark, (0, 0), mask)
    return image


def sheet(palette, page):
    frames = [draw_icon(size, palette) for size in ICON_SIZES]
    shown = [f if f.width >= SHEET_SCALE_LIMIT else f.resize((f.width * 4, f.height * 4), Image.NEAREST) for f in frames]
    width = SHEET_GAP + sum(f.width + SHEET_GAP for f in frames)
    width = max(width, SHEET_GAP + sum(f.width + SHEET_GAP for f in shown))
    height = SHEET_GAP * 3 + max(f.height for f in frames) + max(f.height for f in shown)
    out = Image.new("RGBA", (width, height), page)
    x = SHEET_GAP
    for f in frames:
        out.alpha_composite(f, (x, SHEET_GAP))
        x += f.width + SHEET_GAP
    x = SHEET_GAP
    for f in shown:
        out.alpha_composite(f, (x, SHEET_GAP * 2 + max(g.height for g in frames)))
        x += f.width + SHEET_GAP
    return out.convert("RGB")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ico", type=pathlib.Path)
    parser.add_argument("--preview", type=pathlib.Path)
    parser.add_argument("--pixels", type=pathlib.Path)
    parser.add_argument("--sheet", type=pathlib.Path)
    parser.add_argument("--sheet-page", default="#000000")
    parser.add_argument("--palette", choices=sorted(PALETTES), default=DEFAULT_PALETTE)
    args = parser.parse_args()
    palette = PALETTES[args.palette]

    if args.ico:
        frames = [draw_icon(size, palette) for size in ICON_SIZES]
        args.ico.parent.mkdir(parents=True, exist_ok=True)
        frames[-1].save(args.ico, format="ICO", append_images=frames[:-1], sizes=[(s, s) for s in ICON_SIZES])
    if args.preview:
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        draw_icon(PREVIEW_SIZE, palette).save(args.preview)
    if args.pixels:
        small = draw_icon(PIXELS_SIZE, palette)
        small = small.resize((PIXELS_SIZE * PIXELS_SCALE, PIXELS_SIZE * PIXELS_SCALE), Image.NEAREST)
        args.pixels.parent.mkdir(parents=True, exist_ok=True)
        with open(args.pixels, "wb") as out:
            out.write(struct.pack("<II", small.width, small.height))
            out.write(small.tobytes("raw", "BGRA"))
    if args.sheet:
        args.sheet.parent.mkdir(parents=True, exist_ok=True)
        sheet(palette, args.sheet_page).save(args.sheet)


if __name__ == "__main__":
    main()

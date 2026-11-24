import argparse
import pathlib
import struct

from PIL import Image, ImageDraw

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

PIXEL_SHAPES = {
    16: {
        "fill": [(2, 3, 3, 4), (2, 3, 6, 9), (2, 3, 11, 12), (4, 5, 3, 12), (6, 10, 4, 11), (11, 13, 3, 12)],
        "cut": [(7, 10, 7, 8)],
    },
    20: {
        "fill": [(3, 4, 5, 6), (3, 4, 8, 11), (3, 4, 13, 14), (5, 6, 5, 14), (7, 12, 6, 13), (13, 16, 5, 14)],
        "cut": [(9, 10, 8, 11), (11, 12, 9, 10)],
    },
    24: {
        "fill": [(3, 5, 6, 8), (3, 5, 10, 13), (3, 5, 15, 17), (6, 7, 6, 17), (8, 8, 7, 16), (9, 12, 8, 15),
                 (13, 16, 7, 16), (17, 20, 6, 17)],
        "cut": [(10, 10, 11, 12), (11, 12, 10, 13), (13, 15, 11, 12)],
    },
    32: {
        "fill": [(4, 7, 8, 11), (4, 7, 14, 17), (4, 7, 20, 23), (8, 10, 8, 23), (11, 11, 9, 22), (12, 17, 10, 21),
                 (18, 22, 9, 22), (23, 27, 8, 23)],
        "cut": [(14, 14, 14, 17), (15, 17, 13, 18), (18, 18, 14, 17), (19, 20, 15, 16), (21, 22, 14, 17)],
    },
}


def rgba(colour):
    return tuple(int(colour[i:i + 2], 16) for i in (1, 3, 5)) + (255,)


def tower_parts(variant):
    k = variant["keyhole"]
    return {
        "crown": (TOWER_LEFT, CROWN_TOP, TOWER_RIGHT, CROWN_BOTTOM),
        "gaps": [(left, 0, right, variant["gap_depth"]) for left, right in variant["gaps"]],
        "neck": [(TOWER_LEFT + NECK_INSET, NECK_TOP), (TOWER_RIGHT - NECK_INSET, NECK_TOP),
                 (TOWER_RIGHT - BODY_INSET, NECK_BOTTOM), (TOWER_LEFT + BODY_INSET, NECK_BOTTOM)],
        "body": [(TOWER_LEFT + BODY_INSET, BODY_TOP), (TOWER_RIGHT - BODY_INSET, BODY_TOP), (TOWER_RIGHT, BODY_BOTTOM),
                 (TOWER_LEFT, BODY_BOTTOM)],
        "hole": (CENTER_X, k["y"], k["radius"]),
        "stem": [(CENTER_X - k["top"], k["y"]), (CENTER_X + k["top"], k["y"]), (CENTER_X + k["bottom"], k["foot"]),
                 (CENTER_X - k["bottom"], k["foot"])],
    }


def tower(variant):
    s = SUPERSAMPLE
    parts = tower_parts(variant)
    image = Image.new("L", (GRID * s, GRID * s), 0)
    d = ImageDraw.Draw(image)
    d.rounded_rectangle(tuple(v * s for v in parts["crown"]), CROWN_RADIUS * s, fill=255)
    for gap in parts["gaps"]:
        d.rectangle(tuple(v * s for v in gap), fill=0)
    for name in ("neck", "body"):
        d.polygon([(x * s, y * s) for x, y in parts[name]], fill=255)
    cx, cy, r = parts["hole"]
    d.ellipse(((cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s), fill=0)
    d.polygon([(x * s, y * s) for x, y in parts["stem"]], fill=0)
    return image


def pixel_mask(size):
    shape = PIXEL_SHAPES[size]
    mask = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(mask)
    for key, value in (("fill", 255), ("cut", 0)):
        for top, bottom, left, right in shape[key]:
            d.rectangle((left, top, right, bottom), fill=value)
    return mask


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


def draw_icon(size, palette):
    image = ground(size, palette)
    mark = Image.new("RGBA", (size, size), rgba(palette["mark"]))
    if size in PIXEL_SHAPES:
        image.paste(mark, (0, 0), pixel_mask(size))
        return image
    variant = DETAILED if size >= DETAILED_FROM else SIMPLIFIED
    inner = round(size * variant["scale"])
    mask = Image.new("L", (size, size), 0)
    mask.paste(tower(variant).resize((inner, inner), Image.LANCZOS), ((size - inner) // 2, (size - inner) // 2))
    image.paste(mark, (0, 0), mask)
    return image


def svg(palette):
    parts = tower_parts(DETAILED)
    scale = DETAILED["scale"]
    offset = GRID * (1 - scale) / 2

    def points(polygon):
        return " ".join(f"{x},{y}" for x, y in polygon)

    def rect(box, fill, radius=0):
        left, top, right, bottom = box
        return (f'<rect x="{left}" y="{top}" width="{right - left}" height="{bottom - top}" rx="{radius}" '
                f'fill="{fill}"/>')

    cx, cy, r = parts["hole"]
    half = TILE_EDGE_WIDTH / 2
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {GRID} {GRID}">',
        f'<rect x="{half}" y="{half}" width="{GRID - TILE_EDGE_WIDTH}" height="{GRID - TILE_EDGE_WIDTH}" '
        f'rx="{TILE_RADIUS - half}" fill="{palette["ground"]}" stroke="{palette["edge"]}" '
        f'stroke-width="{TILE_EDGE_WIDTH}"/>',
        f'<g transform="translate({offset:g} {offset:g}) scale({scale:g})">',
        rect(parts["crown"], palette["mark"], CROWN_RADIUS),
        *[rect(gap, palette["ground"]) for gap in parts["gaps"]],
        f'<polygon points="{points(parts["neck"])}" fill="{palette["mark"]}"/>',
        f'<polygon points="{points(parts["body"])}" fill="{palette["mark"]}"/>',
        f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{palette["ground"]}"/>',
        f'<polygon points="{points(parts["stem"])}" fill="{palette["ground"]}"/>',
        "</g>",
        "</svg>",
    ]
    return "\n".join(lines) + "\n"


def sheet(palette, page):
    frames = [draw_icon(size, palette) for size in ICON_SIZES]
    shown = [f if f.width >= SHEET_SCALE_LIMIT else f.resize((f.width * 4, f.height * 4), Image.NEAREST) for f in frames]
    width = SHEET_GAP + max(sum(f.width + SHEET_GAP for f in frames), sum(f.width + SHEET_GAP for f in shown))
    top = max(f.height for f in frames)
    out = Image.new("RGBA", (width, SHEET_GAP * 3 + top + max(f.height for f in shown)), page)
    for row, y in ((frames, SHEET_GAP), (shown, SHEET_GAP * 2 + top)):
        x = SHEET_GAP
        for f in row:
            out.alpha_composite(f, (x, y))
            x += f.width + SHEET_GAP
    return out.convert("RGB")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ico", type=pathlib.Path)
    parser.add_argument("--preview", type=pathlib.Path)
    parser.add_argument("--pixels", type=pathlib.Path)
    parser.add_argument("--svg", type=pathlib.Path)
    parser.add_argument("--png", type=pathlib.Path)
    parser.add_argument("--png-size", type=int, default=512)
    parser.add_argument("--sheet", type=pathlib.Path)
    parser.add_argument("--sheet-page", default="#000000")
    parser.add_argument("--palette", choices=sorted(PALETTES), default=DEFAULT_PALETTE)
    args = parser.parse_args()
    palette = PALETTES[args.palette]

    for path in (args.ico, args.preview, args.pixels, args.svg, args.png, args.sheet):
        if path:
            path.parent.mkdir(parents=True, exist_ok=True)
    if args.ico:
        frames = [draw_icon(size, palette) for size in ICON_SIZES]
        frames[-1].save(args.ico, format="ICO", append_images=frames[:-1], sizes=[(s, s) for s in ICON_SIZES])
    if args.preview:
        draw_icon(PREVIEW_SIZE, palette).save(args.preview)
    if args.png:
        draw_icon(args.png_size, palette).save(args.png)
    if args.svg:
        args.svg.write_text(svg(palette), encoding="utf-8", newline="\n")
    if args.pixels:
        small = draw_icon(PIXELS_SIZE, palette)
        small = small.resize((PIXELS_SIZE * PIXELS_SCALE, PIXELS_SIZE * PIXELS_SCALE), Image.NEAREST)
        with open(args.pixels, "wb") as out:
            out.write(struct.pack("<II", small.width, small.height))
            out.write(small.tobytes("raw", "BGRA"))
    if args.sheet:
        sheet(palette, args.sheet_page).save(args.sheet)


if __name__ == "__main__":
    main()

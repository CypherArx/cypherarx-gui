import argparse
import pathlib
import struct

from PIL import Image, ImageDraw

CANVAS = 1024
ICON_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
PREVIEW_SIZE = 256
PIXELS_SIZE = 96

TILE_COLOR = "#0b0d12"
TILE_EDGE_COLOR = "#343a46"
CITADEL_COLOR = "#8fb0ee"

TILE_INSET = 48
TILE_RADIUS = 212
TILE_EDGE_WIDTH = 20

WALL_LEFT = 296
WALL_RIGHT = 728
WALL_TOP = 404
WALL_BOTTOM = 772
WALL_RADIUS = 44
MERLON_TOP = 284
MERLON_COUNT = 3
MERLON_GAP = 48

KEYHOLE_CENTER_Y = 548
KEYHOLE_RADIUS = 58
KEYHOLE_STEM_TOP_HALF_WIDTH = 30
KEYHOLE_STEM_BOTTOM_HALF_WIDTH = 52
KEYHOLE_STEM_BOTTOM = 690


def draw_icon():
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(
        (TILE_INSET, TILE_INSET, CANVAS - TILE_INSET, CANVAS - TILE_INSET),
        radius=TILE_RADIUS,
        fill=TILE_COLOR,
        outline=TILE_EDGE_COLOR,
        width=TILE_EDGE_WIDTH,
    )

    draw.rounded_rectangle(
        (WALL_LEFT, WALL_TOP, WALL_RIGHT, WALL_BOTTOM),
        radius=WALL_RADIUS,
        fill=CITADEL_COLOR,
        corners=(False, False, True, True),
    )
    merlon_width = (WALL_RIGHT - WALL_LEFT - MERLON_GAP * (MERLON_COUNT - 1)) / MERLON_COUNT
    for i in range(MERLON_COUNT):
        left = WALL_LEFT + i * (merlon_width + MERLON_GAP)
        draw.rectangle((left, MERLON_TOP, left + merlon_width, WALL_TOP + 1), fill=CITADEL_COLOR)

    center_x = (WALL_LEFT + WALL_RIGHT) / 2
    draw.ellipse(
        (
            center_x - KEYHOLE_RADIUS,
            KEYHOLE_CENTER_Y - KEYHOLE_RADIUS,
            center_x + KEYHOLE_RADIUS,
            KEYHOLE_CENTER_Y + KEYHOLE_RADIUS,
        ),
        fill=TILE_COLOR,
    )
    draw.polygon(
        (
            (center_x - KEYHOLE_STEM_TOP_HALF_WIDTH, KEYHOLE_CENTER_Y),
            (center_x + KEYHOLE_STEM_TOP_HALF_WIDTH, KEYHOLE_CENTER_Y),
            (center_x + KEYHOLE_STEM_BOTTOM_HALF_WIDTH, KEYHOLE_STEM_BOTTOM),
            (center_x - KEYHOLE_STEM_BOTTOM_HALF_WIDTH, KEYHOLE_STEM_BOTTOM),
        ),
        fill=TILE_COLOR,
    )
    return image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ico", type=pathlib.Path, required=True)
    parser.add_argument("--preview", type=pathlib.Path)
    parser.add_argument("--pixels", type=pathlib.Path)
    args = parser.parse_args()

    image = draw_icon()
    frames = [image.resize((size, size), Image.LANCZOS) for size in ICON_SIZES]
    args.ico.parent.mkdir(parents=True, exist_ok=True)
    frames[-1].save(args.ico, format="ICO", append_images=frames[:-1], sizes=[(s, s) for s in ICON_SIZES])
    if args.preview:
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        image.resize((PREVIEW_SIZE, PREVIEW_SIZE), Image.LANCZOS).save(args.preview)
    if args.pixels:
        small = image.resize((PIXELS_SIZE, PIXELS_SIZE), Image.LANCZOS)
        args.pixels.parent.mkdir(parents=True, exist_ok=True)
        with open(args.pixels, "wb") as out:
            out.write(struct.pack("<II", small.width, small.height))
            out.write(small.tobytes("raw", "BGRA"))


if __name__ == "__main__":
    main()

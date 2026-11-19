"""Convert the three control assemblies from TV-4 factory figure 1 to LVGL."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("figure", type=Path, help="spr_tv4_upr_b.jpg from stanki-katalog.ru/info4")
args = parser.parse_args()
source = Image.open(args.figure).convert("L")
if source.size != (1200, 966):
    raise SystemExit("Use the documented 1200 x 966 edition of figure 1")
crops = {"head": (156, 130, 476, 290), "feedbox": (150, 280, 510, 460),
         "apron": (538, 272, 838, 422)}
lines = ["/* Factory figure 1. Source and figure numbering in docs/tv4-controls.md. */", "#include <lvgl.h>"]
for name, bounds in crops.items():
    grid = source.crop(bounds).resize((224, 112), Image.Resampling.LANCZOS)
    # LVGL indexed palettes use BGRA bytes even on an RGB565 display.
    data = [v for rgb in [(26, 39, 48), (87, 106, 115), (159, 178, 184), (222, 230, 230)]
            for v in (*reversed(rgb), 255)]
    pixels = grid.tobytes()
    for i in range(0, len(pixels), 4):
        value = 0
        for p in pixels[i:i+4]:
            value = (value << 2) | min(3, (p + 20) // 70)
        data.append(value)
    lines.append(f"static const uint8_t pixels_{name}[] = {{")
    lines.extend("    " + ",".join(map(str, data[i:i+28])) + "," for i in range(0,len(data),28))
    lines.extend(["};", f"const lv_img_dsc_t tv4_{name} = {{",
                  "    .header = {.cf=LV_IMG_CF_INDEXED_2BIT, .w=224, .h=112},",
                  f"    .data_size=sizeof(pixels_{name}), .data=pixels_{name}", "};"])
(Path(__file__).resolve().parents[1]/"lib/PanelUI/src/tv4_drawings.c").write_text("\n".join(lines)+"\n",encoding="utf-8")

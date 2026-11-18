"""Package the four completed PlatformIO images and their matching boot files."""
import hashlib
import struct
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from platformio.project.config import ProjectConfig

root = Path(__file__).resolve().parents[1]
config = ProjectConfig.get_instance(str(root / "platformio.ini"))
build = Path(config.get("platformio", "build_dir"))
packages = Path(config.get("platformio", "packages_dir"))
profiles = ("discovery-st7789", "discovery-ili9341", "panel-st7789", "panel-ili9341")
images = {}
for name in ("bootloader.bin", "partitions.bin"):
    copies = [(build / profile / name).read_bytes() for profile in profiles]
    if any(blob != copies[0] for blob in copies):
        raise SystemExit(f"{name} differs across profiles; cannot share this file")
    images[name] = copies[0]
images["boot_app0.bin"] = (packages / "framework-arduinoespressif32/tools/partitions/boot_app0.bin").read_bytes()
entry = struct.Struct("<HBBII16sI")
parts = {}
for offset in range(0, len(images["partitions.bin"]) - entry.size + 1, entry.size):
    magic, kind, subtype, start, size, raw_name, flags = entry.unpack_from(images["partitions.bin"], offset)
    if magic != 0x50AA:
        break
    parts[raw_name.rstrip(b"\0").decode("ascii")] = (kind, subtype, start, size)
expected = {"nvs": (1, 2, 0x9000, 0x5000), "otadata": (1, 0, 0xe000, 0x2000),
            "app0": (0, 0x10, 0x10000, 0x1f0000), "app1": (0, 0x11, 0x200000, 0x1f0000)}
if any(parts.get(name) != layout for name, layout in expected.items()):
    raise SystemExit("Unexpected NVS or OTA layout in the built partition binary")
slot_size = min(parts["app0"][3], parts["app1"][3])
for profile in profiles:
    app = (build / profile / "firmware.bin").read_bytes()
    if not app or app[0] != 0xE9 or len(app) > slot_size:
        raise SystemExit(f"Invalid ESP32 app image for {profile}")
    images[f"{profile}.bin"] = app

version = (root / "include/version.h").read_text().split('"')[1]
release = root / "release"
release.mkdir(exist_ok=True)
archive = release / f"TV4-Digital-v{version}.zip"
with ZipFile(archive, "w", ZIP_DEFLATED) as package:
    for name, blob in images.items():
        package.writestr(name, blob)
    manifest = "".join(f"{hashlib.sha256(blob).hexdigest()}  {name}\n" for name, blob in images.items())
    package.writestr("SHA256SUMS.txt", manifest)
    package.write(root / "docs/flash.md", "FLASH.md")
    package.write(root / "docs/ota.md", "OTA.md")
    package.write(root / "lib/PanelUI/FONT-LICENSE.txt", "licenses/FONT-LICENSE.txt")
    libs = Path(config.get("platformio","libdeps_dir")) / "panel-st7789"
    package.write(libs / "lvgl/LICENCE.txt", "licenses/LVGL.txt")
    package.write(libs / "TFT_eSPI/license.txt", "licenses/TFT_eSPI.txt")
with ZipFile(archive) as package:
    for name, blob in images.items():
        if package.read(name) != blob:
            raise SystemExit(f"Archive verification failed for {name}")
print(archive, archive.stat().st_size, "bytes;", len(images), "verified images")

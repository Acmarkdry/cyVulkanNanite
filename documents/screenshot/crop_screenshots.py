"""
将 documents/screenshot 下的三张截图从 1296x759 以中心裁切到 1280x750。
依赖: pip install Pillow
"""
from pathlib import Path
from PIL import Image

SCRIPT_DIR = Path(__file__).parent
TARGET_W, TARGET_H = 1280, 750

images = [
    "178363dac52e95c115efcfdfac0881e8.png",
    "c338d1d442340f1ca12001f6467335f7.png",
    "ca2099ce9ab1be101aaa37d868e52be9.png",
]

for name in images:
    path = SCRIPT_DIR / name
    img = Image.open(path)
    w, h = img.size
    left = (w - TARGET_W) // 2
    top = (h - TARGET_H) // 2
    cropped = img.crop((left, top, left + TARGET_W, top + TARGET_H))
    cropped.save(path)
    print(f"Cropped {name}: {w}x{h} -> {TARGET_W}x{TARGET_H}")

print("Done.")

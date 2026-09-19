from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
out = Path(__file__).parent / "bangla_preview.png"
font = ImageFont.truetype("/usr/share/fonts/truetype/noto/NotoSansBengali-Regular.ttf", 36)
im = Image.new("RGB", (768, 384), "black")
d = ImageDraw.Draw(im)
lines = ["অ আ ই ঈ উ ঊ", "ক খ গ ঘ ঙ", "চ ছ জ ঝ ঞ", "বাংলা", "বাংলাদেশ", "আমি বাংলা লিখি"]
for i, line in enumerate(lines):
    d.text((18, i*60-8), line, font=font, fill="white")
im.save(out)
print(out)

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).parent
FONT = "/usr/share/fonts/truetype/noto/NotoSansBengali-Regular.ttf"
SIZE = 16
font = ImageFont.truetype(FONT, SIZE)

# SRT timings in milliseconds and exact lyric text from the supplied file.
LYRICS = [
(1180,16119,"[মিউজিক]"),(16119,21680,"একাশে তারা তুই একা গুনিস নে গুনতে দিস"),(21680,27599,"তুই কিছু মোরে একাশে তারা তুই একা গুনিস"),(27599,33680,"নে গুনতে দিস তুই কিছু মোরে ওরে সব ভালো"),(33680,37480,"তুই একা বাসিস নে একটু"),(37480,43520,"ভালোবাসতে দিস মোরে সব ভালো তুই একা বাসিস"),(43520,45480,"নে একটু"),(45480,50800,"ভালোবাসতে দিস মোরে পুরো যোছনা তুই একা"),(50800,55239,"পোহাস নে সঙ্গে মিস রে তুই"),(55239,60960,"মোরে পুরো যোছনা তুই একা অপহাস নে সঙ্গে"),(60960,66720,"নিশ রে তুই মোরে মোরে সব ভালো তুই একা"),(66720,69479,"বাসিস নে একটু"),(69479,75520,"ভালোবাসতে দিস মোরে সব ভালো তুই একা বাসিস"),(75520,77479,"নে একটু"),(77479,82040,"ভালোবাসতে দিস মোরে"),(82440,106580,"[মিউজিক]"),(106580,107080,"[প্রশংসা]"),(107080,110580,"[মিউজিক]"),(110580,112000,"[প্রশংসা]"),(112000,120079,"হৃদয় নয়ে জড়িবি যখন বই নিস রে তুই মোরে"),(120079,126079,"ভাসবো না হই দুজন মিলে স্বপ্ন লোক চল"),(126079,132239,"সুখের ঘরে মোর সব ভালো তুই একা বাসিস নে"),(132239,133560,"একটু"),(133560,139599,"ভালোবাসতে দিস মোরে সব ভালো তুই একা বাসিস"),(139599,141560,"নে একটু"),(141560,146120,"ভালোবাসতে দিস মোরে"),(148360,155360,"[মিউজিক]"),(160239,167239,"দুঃখের বোঝা বই যখন স্মরণ করিস রে তুই"),(167239,174680,"মোরে আসবো ছুটে তোর কাছে যেখানে থাকি আমি"),(174680,181640,"যতদূরে ওরে সব ভালো তুই একা বাসিস নে একটু"),(181640,187680,"ভালোবাসতে দিস মোরে সব ভালো তুই একা বাসিস"),(187680,189640,"নে একটু"),(189640,194879,"ভালোবাসতে দিস মোরে একাশে তারা তুই একা"),(194879,201200,"গুনিস নে গুনিতে দিস তুই কিছু মোরে একাশে"),(201200,206159,"তারা তুই একা গুনিস নে গুনিতে দিস তুই"),(206159,211680,"কিছু মোরে ওরে সব ভালো তুই তুই একা বাসিস"),(211680,213640,"নে একটু"),(213640,219680,"ভালোবাসতে দিস মোরে সব ভালো তুই একা বাসিস"),(219680,221640,"নে একটু"),(221640,227680,"ভালোবাসতে দিস মোরে সব ভালো তুই একা বাসিস"),(227680,229640,"নে একটু"),(229640,235680,"ভালোবাসতে দিস মোরে সব ভালো তুই একা বাসিস"),(235680,237640,"নে একটু"),(237640,240410,"ভালোবাসতে দিস মোরে"),(240410,244629,"[মিউজিক]"),
]

chars = list("অআইঈউঊঋএঐওঔকখগঘঙচছজঝঞটঠডঢণতথদধনপফবভমযরলশষসহড়ঢ়য়ৎংঃঁ")

def render(text):
    box=font.getbbox(text)
    im=Image.new("1", (max(1,box[2]-box[0]+4), max(1,box[3]-box[1]+4)), 0)
    ImageDraw.Draw(im).text((2-box[0],2-box[1]), text, font=font, fill=1)
    bb=im.getbbox()
    return im.crop((max(0,bb[0]-1), max(0,bb[1]-1), min(im.width,bb[2]+1), min(im.height,bb[3]+1))) if bb else im

def wrap(text):
    if text.startswith("["):
        return [text]
    words=text.split()
    rows=[]; cur=""
    for word in words:
        test=word if not cur else cur+" "+word
        if render(test).width <= 124 or not cur:
            cur=test
        else:
            rows.append(cur); cur=word
    if cur: rows.append(cur)
    return rows[:3]

def bitmap(im):
    out=[]
    for y in range(im.height):
        for xb in range((im.width+7)//8):
            v=0
            for bit in range(8):
                x=xb*8+bit
                if x<im.width and im.getpixel((x,y)): v |= 1<<bit
            out.append(v)
    return out

def phrase_image(text):
    rows=wrap(text); ims=[render(r) for r in rows]
    w=max(i.width for i in ims); h=sum(i.height for i in ims)+2*(len(ims)-1)
    canvas=Image.new("1",(min(126,w),min(60,h)),0); y=0
    for im in ims:
        x=max(0,(canvas.width-im.width)//2)
        canvas.paste(im,(x,y)); y+=im.height+2
    return canvas

def header():
    lines=["#pragma once","#include <stdint.h>","typedef struct { uint32_t cp; uint8_t w; uint8_t h; const uint32_t *rows; } bn_glyph_t;"]
    arrays=[]
    for i,ch in enumerate(chars):
        im=render(ch); arr=[]
        for y in range(im.height):
            v=0
            for x in range(im.width):
                if im.getpixel((x,y)): v|=1<<x
            arr.append(v)
        arrays.append((f"bn_g_{i}",ord(ch),im.width,im.height,arr))
    for n,cp,w,h,a in arrays: lines.append(f"static const uint32_t {n}[{h}] = {{"+",".join(f"0x{x:08x}u" for x in a)+"};")
    lines.append(f"static const bn_glyph_t bn_glyphs[{len(arrays)}] = {{")
    for n,cp,w,h,a in arrays: lines.append(f"{{0x{cp:04x},{w},{h},{n}}},")
    lines += ["};",f"#define BN_GLYPH_COUNT {len(arrays)}","typedef struct { const char *label; uint8_t w; uint8_t h; const uint8_t *data; } bn_phrase_t;"]
    for i,(_,_,text) in enumerate(LYRICS):
        im=phrase_image(text); a=bitmap(im); n=f"bn_phrase_{i}"
        lines.append(f"static const uint8_t {n}[{len(a)}] = {{"+",".join(f"0x{x:02x}" for x in a)+"};")
        lines.append(f"#define BN_PHRASE_{i}_W {im.width}\n#define BN_PHRASE_{i}_H {im.height}")
    lines.append(f"static const bn_phrase_t bn_phrases[{len(LYRICS)}] = {{")
    for i,(_,_,text) in enumerate(LYRICS): lines.append(f'{{"{text}",BN_PHRASE_{i}_W,BN_PHRASE_{i}_H,bn_phrase_{i}}},')
    lines += ["};",f"#define BN_PHRASE_COUNT {len(LYRICS)}"]
    return "\n".join(lines)+"\n"

content=header(); (OUT/"bangla_font.h").write_text(content,encoding="utf-8"); (OUT/"main"/"bangla_font.h").write_text(content,encoding="utf-8")
timing = "#pragma once\n#include <stdint.h>\ntypedef struct { uint32_t start_ms; uint32_t end_ms; uint16_t phrase_index; } lyric_t;\nstatic const lyric_t lyrics_timing[] = {\n"+"".join(f"{{{a},{b},{i}}},\n" for i,(a,b,_) in enumerate(LYRICS))+f"}};\n#define LYRICS_COUNT {len(LYRICS)}\n"
(OUT/"lyrics_timing.h").write_text(timing,encoding="utf-8"); (OUT/"main"/"lyrics_timing.h").write_text(timing,encoding="utf-8")
print(f"generated {len(chars)} glyphs and {len(LYRICS)} timed lyrics")

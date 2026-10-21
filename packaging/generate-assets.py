#!/usr/bin/env python3
"""Original geometric art for Many Bricks 3DS, drawn at native banner/icon sizes."""
import argparse
from pathlib import Path
import wave
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument('font', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parent
SCALE=4
BG='#0D1427'; WHITE='#E9F4F4'; MINT='#58E0C1'; MUTED='#879AB5'
COLORS=[MINT,'#7BBDFF','#FF887B','#F2CF75','#B29BED','#76C6CF']

def new(w,h):
    return Image.new('RGB',(w*SCALE,h*SCALE),BG)
def rect(d,box,fill,radius=0):
    box=tuple(int(v*SCALE) for v in box)
    if radius:d.rounded_rectangle(box,radius=int(radius*SCALE),fill=fill)
    else:d.rectangle(box,fill=fill)
def circle(d,x,y,r,fill):d.ellipse(tuple(int(v*SCALE) for v in (x-r,y-r,x+r,y+r)),fill=fill)
def font(size):
    f=ImageFont.truetype(str(args.font),size*SCALE)
    try:f.set_variation_by_axes([600])
    except OSError:pass
    return f

def brick(d,x,y,w,h,color):
    rect(d,(x,y,x+w,y+h),color,0.7)
    rect(d,(x+0.6,y+0.4,x+w-0.6,y+0.8),WHITE)

im=new(48,48);d=ImageDraw.Draw(im)
rect(d,(1,1,46,46),'#1C2941',9);rect(d,(2,2,45,45),BG,8)
for r in range(4):
    for c in range(6):
        if (r==3 and c in (2,3)) or (r==2 and c==3):continue
        brick(d,5+c*6.4,7+r*4.5,5.2,3.2,COLORS[r])
for x,y,r in [(26,25,2),(21,30,1.4),(31,32,1.4),(15,28,1.2)]:circle(d,x,y,r,WHITE)
rect(d,(15,38,34,41),MINT,1.5)
im.resize((48,48),Image.Resampling.LANCZOS).save(root/'icon.png')

im=new(256,128);d=ImageDraw.Draw(im)
for x in range(7,256,13):
    for y in range(8,128,13):circle(d,x,y,0.35,'#2C3C55')
for r in range(7):
    for c in range(7):
        if r>=4 and c in (2,3,4):continue
        brick(d,13+c*10.2,18+r*6.6,8.8,5.1,COLORS[r%6])
for x,y,r in [(47,70,3),(34,80,1.6),(64,88,1.8),(52,93,1.4),(23,92,1.4)]:circle(d,x,y,r,WHITE)
rect(d,(32,108,69,112),MINT,2)
d.text((102*SCALE,21*SCALE),'万砖齐发',font=font(27),fill=WHITE)
d.text((104*SCALE,63*SCALE),'MANY BRICKS 3DS',font=font(11),fill=MINT)
d.text((104*SCALE,87*SCALE),'108 关 · 多球 · 触屏',font=font(10),fill=MUTED)
im.resize((256,128),Image.Resampling.LANCZOS).save(root/'banner.png')
with wave.open(str(root/'silence.wav'),'wb') as f:
    f.setparams((1,2,32000,0,'NONE','not compressed'))
    f.writeframes(bytes(16000))
print('Saved original icon.png (48x48), banner.png (256x128), silence.wav.')

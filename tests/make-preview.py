#!/usr/bin/env python3
"""Render the production CPU renderer at exact 3DS framebuffer dimensions.
The live-play snapshot runs the real game physics after granting split pickups.
Menu/result scenes are staged; these images are not photographs of a 3DS.
"""
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image

root=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='many-bricks-preview-') as temp:
    out=Path(temp);binary=out/'preview'
    subprocess.run([os.environ.get('HOST_CXX','c++'),'-std=c++17','-O2','-Wall','-Wextra','-Werror',
                    '-I'+str(root/'include'),str(root/'tests/preview.cpp'),str(root/'source/game.cpp'),
                    str(root/'source/render.cpp'),'-o',str(binary)],check=True)
    screens=[]
    for state in range(8):
        prefix=out/str(state)
        subprocess.run([str(binary),str(state),str(prefix)],cwd=root,check=True)
        canvas=Image.new('RGB',(400,492),'#070C18')
        with Image.open(str(prefix)+'-top.ppm') as top:canvas.paste(top,(0,0))
        with Image.open(str(prefix)+'-bottom.ppm') as bottom:canvas.paste(bottom,(40,252))
        screens.append(canvas)
    screens[0].resize((800,984),Image.Resampling.NEAREST).save(root/'preview.png')
    sheet=Image.new('RGB',(1600,984),'#070C18')
    for i,screen in enumerate(screens):sheet.paste(screen,((i%4)*400,(i//4)*492))
    sheet.save(root/'screens.png')
    gallery=Image.new('RGB',(1200,480),'#070C18')
    for i in range(6):
        prefix=out/('wall-'+str(i))
        subprocess.run([str(binary),str(i+8),str(prefix)],cwd=root,check=True)
        with Image.open(str(prefix)+'-top.ppm') as top:gallery.paste(top,((i%3)*400,(i//3)*240))
    gallery.resize((2400,960),Image.Resampling.NEAREST).save(root/'walls-preview.png')
print('Saved preview.png, screens.png and walls-preview.png from production rendering.')

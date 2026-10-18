import sys, re
from pathlib import Path
from PIL import Image
for f in sys.argv[1:]:
    p=Path(f); w,h=map(int,re.search(r'(\d+)x(\d+)',p.name).groups())
    data=p.read_bytes(); bpp=len(data)//(w*h)
    mode='BGRA' if bpp==4 else None
    img=Image.frombytes('RGBA',(w,h),data[:w*h*4],'raw','BGRA').convert('RGB')
    img.thumbnail((960,540)); img.save(p.with_suffix('.png')); print(p.with_suffix('.png'), bpp)

"""Render production UITask draw-call captures with repository fonts and T114 scaling."""
from pathlib import Path
import re
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parents[2]
out=root/'docs/t114-display-preview'
source=(root/'src/helpers/ui/OLEDDisplayFonts.cpp').read_text()
fonts={}
for size in (16,24):
    body=source.split(f'const uint8_t ArialMT_Plain_{size}[] PROGMEM = {{')[1].split('};')[0]
    body=re.sub(r'//[^\n]*','',body)
    fonts[size]=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',body))
def color(c):
    if c==1:return (255,255,255)
    return ((c>>11)*255//31,((c>>5)&63)*255//63,(c&31)*255//31)
names=['repeater','gps-green','neighbors','usb','gps-orange','gps-red']
sheet=Image.new('RGB',(1000,990),'#18212c');sd=ImageDraw.Draw(sheet)
for index,name in enumerate(names):
    im=Image.new('RGB',(240,135));dr=ImageDraw.Draw(im)
    for op in (out/(name+'.ops')).read_text().splitlines():
        if op.startswith('T '):
            _,x,y,sz,c,text=op.split(' ',5);x=int(int(x)*1.875);y=int(int(y)*2.109375+1)
            f=fonts[24 if sz=='2' else 16];h=f[1];r=(h+7)//8
            for ch in text:
                j=4+(ord(ch)-f[2])*4;hi,lo,n,w=f[j:j+4];offset=4+f[3]*4+(hi<<8)+lo
                if hi!=255:
                    for i in range(n):
                        for bit in range(8):
                            if f[offset+i]&(1<<bit):
                                px=x+i//r;py=y+(i%r)*8+bit
                                assert 0<=px<240 and 0<=py<135,(name,text,px,py)
                                im.putpixel((px,py),color(int(c)))
                x+=w
        else:
            kind,x,y,w,h,c=op.split();x=int(int(x)*1.875);y=int(int(y)*2.109375+1);w=int(int(w)*1.875);h=int(int(h)*2.109375)
            if w and h:
                assert x>=0 and y>=0 and x+w<=240 and y+h<=135
                dr.rectangle((x,y,x+w-1,y+h-1),**({'fill':color(int(c))} if kind=='F' else {'outline':color(int(c))}))
    im.save(out/(name+'.png'))
    x=10+(index%2)*500;y=15+(index//2)*330
    sd.text((x,y),name.replace('-',' ').title(),fill='white')
    sheet.paste(im.resize((480,270),Image.Resampling.NEAREST),(x,y+30))
sheet.save(out/'preview.png')
print('Six previews rendered; all lit glyph pixels and rectangles fit the physical display.')

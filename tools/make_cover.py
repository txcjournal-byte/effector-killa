import sys, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
F=sys.argv[1]; ui_p=sys.argv[2]; out_p=sys.argv[3]
W=H=1600
yy,xx=np.mgrid[0:H,0:W].astype(np.float32)
bg=np.zeros((H,W,3),np.float32); bg[:]=(4,5,12)
bg+=np.exp(-(((xx-560)/620)**2+((yy-980)/520)**2))[...,None]*np.array([10,70,110])
bg+=np.exp(-(((xx-1080)/600)**2+((yy-1020)/520)**2))[...,None]*np.array([90,20,90])
bg+=np.exp(-(((xx-800)/700)**2+((yy-280)/240)**2))[...,None]*np.array([14,18,48])
im=Image.fromarray(bg.clip(0,255).astype(np.uint8)); d=ImageDraw.Draw(im)
mono=ImageFont.truetype(F+'vfd.ttf',30)
d.text((70,60),'MPE PLUGIN',font=mono,fill=(160,172,200))
t='VST3 · AU'; d.text((W-70-d.textlength(t,font=mono),60),t,font=mono,fill=(160,172,200))
def chrome(im,text,size,cy,shear=0.12):
    f=ImageFont.truetype(F+'display.ttf',size)
    tmp=Image.new('L',(W*2,size*2),0); td=ImageDraw.Draw(tmp)
    tw=td.textlength(text,font=f); td.text((W-tw/2,size*0.3),text,font=f,fill=255)
    tmp=tmp.transform(tmp.size,Image.AFFINE,(1,shear,-shear*size,0,1,0),resample=Image.BICUBIC)
    tmp=tmp.crop(tmp.getbbox()); w,h=tmp.size
    x0,y0=(W-w)//2,int(cy-h/2)
    mask=Image.new('L',(W,H),0); mask.paste(tmp,(x0,y0))
    grad=np.zeros((H,W,3),np.float32)
    stops=[(0,(248,250,255)),(0.46,(214,220,235)),(0.54,(126,138,170)),(1,(238,242,252))]
    for y in range(y0,y0+h):
        tt=(y-y0)/max(1,h)
        for (a,ca),(b,cb) in zip(stops,stops[1:]):
            if a<=tt<=b:
                u=(tt-a)/(b-a); grad[y]=[ca[j]+(cb[j]-ca[j])*u for j in range(3)]
    glow=np.asarray(mask.filter(ImageFilter.GaussianBlur(24)))[...,None]/255.0
    gx=(xx/W)[...,None]
    gcol=np.array([70,242,255])*(1-gx)+np.array([255,79,216])*gx
    base=np.asarray(im).astype(np.float32)+glow*gcol*0.75
    m=np.asarray(mask)[...,None]/255.0
    return Image.fromarray((base*(1-m)+grad*m).clip(0,255).astype(np.uint8))
im=chrome(im,'BEND DEMON',190,270)
d=ImageDraw.Draw(im)
f1=ImageFont.truetype(F+'osd.ttf',44); f2=ImageFont.truetype(F+'vfd.ttf',32)
def centre(t,y,f,col): d.text(((W-d.textlength(t,font=f))/2,y),t,font=f,fill=col)
centre('WRITE IT PLAIN. HEAR IT BEND.',420,f1,(234,240,250))
centre('WIN + MAC  ·  VST3 / AU  ·  FL STUDIO READY',486,f2,(159,230,255))
ui=Image.open(ui_p).convert('RGB'); tw=1300; th=int(ui.height*tw/ui.width)
ui=ui.resize((tw,th),Image.LANCZOS); x,y=(W-tw)//2,568
g=Image.new('L',(W,H),0); ImageDraw.Draw(g).rounded_rectangle((x-6,y-6,x+tw+6,y+th+6),radius=22,fill=255)
g=np.asarray(g.filter(ImageFilter.GaussianBlur(30)))[...,None]/255.0
gx=(xx/W)[...,None]; gcol=np.array([50,200,255])*(1-gx)+np.array([230,60,200])*gx
im=Image.fromarray((np.asarray(im).astype(np.float32)+g*gcol*0.8).clip(0,255).astype(np.uint8))
mask=Image.new('L',(tw,th),0); ImageDraw.Draw(mask).rounded_rectangle((0,0,tw-1,th-1),radius=16,fill=255)
im.paste(ui,(x,y),mask); d=ImageDraw.Draw(im)
d.rounded_rectangle((x-1,y-1,x+tw,y+th),radius=16,outline=(140,190,255),width=2)
centre('TRAPWIN  ·  AUDIO UNDERGROUND',y+th+40,f2,(160,172,200))
im.save(out_p); print('ok',tw,th,y+th)

# Step 1: cleans the original design (Resources/source/design.png) -> Resources/source/cleaned.png:
# removes the painted knob pointers, value arcs, LEDs and fader caps so the plugin can draw them live.
# Usage: pip install pillow numpy opencv-python-headless && python tools/make_background.py
import os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
from PIL import Image; import numpy as np, cv2
img=np.asarray(Image.open(os.path.join(ROOT,'Resources/source/design.png')).convert('RGB')).copy()
a=img.astype(int); R,G,B=a[...,0],a[...,1],a[...,2]
H,W=R.shape; yy,xx=np.mgrid[0:H,0:W]
out=img.copy()
redish=(R>G+18)&(R>B+12)
strong=(R>G+55)
def desat(region):
    m=redish&region
    lum=(0.3*R+0.59*G+0.11*B)[m]
    tint=np.array([1.04,1.0,0.9])
    out[m]=np.clip(lum[:,None]*tint,0,255).astype(np.uint8)
inp=np.zeros((H,W),bool)
knobs=[(570,262),(796,264),(1018,262),(1237,264)]
for cx,cy in knobs:
    d=np.hypot(xx-cx,yy-cy)
    cap=strong&(d<88)
    m=cv2.dilate(cap.astype(np.uint8),np.ones((5,5),np.uint8)).astype(bool)&(d<90)
    ys,xs=np.where(m)
    out[ys,xs]=img[2*cy-ys,2*cx-xs]   # rotate 180 about centre
    inp|=strong&(d>=86)&(d<145)
    desat((d<160)&~m)
leds=[(538,484),(673,480),(838,483),(988,483),(1148,483),(1297,479),(1320,878)]
for lx,ly in leds:
    inp|=(abs(xx-lx)<=13)&(abs(yy-ly)<=13)
for x0,x1 in [(465,564),(601,700),(749,866),(914,1014),(1056,1174),(1218,1328)]:
    desat((xx>=x0-25)&(xx<=x1+25)&(yy>=440)&(yy<=540))
desat((xx>=1265)&(xx<=1365)&(yy>=835)&(yy<=935))
desat((xx>=1180)&(xx<=1380)&(yy>=570)&(yy<=670))
desat((xx>=830)&(xx<=1110)&(yy>=695)&(yy<=795))
inp|=(xx>=1418)&(xx<=1430)&(yy>=842)&(yy<=879)
m8=cv2.dilate(inp.astype(np.uint8)*255,np.ones((5,5),np.uint8))
out=cv2.inpaint(out[...,::-1].copy(),m8,5,cv2.INPAINT_TELEA)[...,::-1].copy()
def patch(x0,x1,y0,y1,shift,feather=6):
    global out
    o=out.astype(float)
    src=o[y0:y1,x0-shift:x1-shift].copy()
    # match brightness to the surroundings of the destination
    ring=np.concatenate([o[y0:y1,x0-12:x0].reshape(-1,3),o[y0:y1,x1:x1+12].reshape(-1,3)])
    src+=ring.mean(0)-src.reshape(-1,3).mean(0)
    h,w=y1-y0,x1-x0
    ax=np.minimum(np.arange(w),np.arange(w)[::-1])/feather
    ay=np.minimum(np.arange(h),np.arange(h)[::-1])/feather
    alpha=np.clip(np.minimum(ax[None,:],ay[:,None]),0,1)[...,None]
    o[y0:y1,x0:x1]=src*alpha+o[y0:y1,x0:x1]*(1-alpha)
    out=np.clip(o,0,255).astype(np.uint8)
patch(656,712,846,928,2*68)
patch(1071,1128,846,928,2*62)
Image.fromarray(out).save(os.path.join(ROOT,'Resources/source/cleaned.png'))

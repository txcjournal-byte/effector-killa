# Step 2: builds the v0.2 background (artwork frame + real plaster texture in the control area)
# and the knob face texture from Resources/source/cleaned.png (run make_background.py first).
# Usage: pip install pillow numpy opencv-python-headless && python tools/make_background_v2.py
import os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
from PIL import Image; import numpy as np, cv2
rng=np.random.default_rng(808)
clean=np.asarray(Image.open(os.path.join(ROOT,'Resources/source/cleaned.png')).convert('RGB')).astype(np.float32)
a=clean; lum=a.mean(2); R,G=a[...,0],a[...,1]
P=48
# candidate patches from the right-hand panel area of the artwork
cands=[]
for y in range(90,990-P,6):
    for x in range(450,1360-P,6):
        p=lum[y:y+P,x:x+P]
        if np.percentile(p,3)<70 or p.mean()<140: continue
        if (R[y:y+P,x:x+P]-G[y:y+P,x:x+P]).max()>30: continue
        if p.std()>30 or (p<80).mean()>0.006: continue
        cands.append((x,y))
print('candidates',len(cands))
X0,Y0,X1,Y1=448,82,1500,988
W,H=X1-X0,Y1-Y0
step=36; ov=P-step
acc=np.zeros((H+P,W+P,3),np.float32); wsum=np.zeros((H+P,W+P,1),np.float32)
ramp=np.minimum(np.arange(P),np.arange(P)[::-1]).astype(np.float32)
ramp=np.clip((ramp+1)/(ov/2+1),0,1)
wmask=(ramp[None,:]*ramp[:,None])[...,None]
target=np.array([180,171,154],np.float32)
for y in range(0,H+1,step):
    for x in range(0,W+1,step):
        cx,cy=cands[rng.integers(len(cands))]
        p=clean[cy:cy+P,cx:cx+P].copy()
        k=rng.integers(4); p=np.rot90(p,k)
        if rng.random()<0.5: p=p[:,::-1]
        p=p-p.reshape(-1,3).mean(0)+target
        acc[y:y+P,x:x+P]+=p*wmask; wsum[y:y+P,x:x+P]+=wmask
tex=(acc/np.maximum(wsum,1e-6))[:H,:W]
# gentle large-scale variation
n=cv2.resize(rng.standard_normal((6,9)).astype(np.float32),(W,H),interpolation=cv2.INTER_CUBIC)
tex*= (1+0.05*n)[...,None]
out=clean.copy(); out[Y0:Y1,X0:X1]=np.clip(tex,0,255)
Image.fromarray(out.astype(np.uint8)).convert('RGB').save(os.path.join(ROOT,'Resources/background.jpg'), quality=92, optimize=True)
# knob face texture from the DISTORT knob
cx,cy,r=1017,262,66
face=clean[cy-r:cy+r,cx-r:cx+r].astype(np.uint8)
yy,xx=np.mgrid[0:2*r,0:2*r]; d=np.hypot(xx-r+0.5,yy-r+0.5)
alpha=np.clip((r-2-d)*0.8,0,1)*255
Image.fromarray(np.dstack([face,alpha.astype(np.uint8)])).save(os.path.join(ROOT,'Resources/knob_face.png'))

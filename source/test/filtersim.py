import numpy as np, sys
rng=np.random.default_rng(1)
def true_pos(t):  # 50 m/s, gentle curve, speed wobble
    v=50+3*np.sin(0.7*t); 
    ang=0.15*t
    R=50/0.15
    return np.array([R*np.sin(ang), R*(1-np.cos(ang))]) 
def true_vel(t,h=1e-4): return (true_pos(t+h)-true_pos(t-h))/(2*h)
noise_ms=float(sys.argv[1]) if len(sys.argv)>1 else 2.0
# BeamNG poses: frame clock st, physics sample at st+delta
st=[0.0]
while st[-1]<12: st.append(st[-1]+rng.uniform(0.011,0.022))
st=np.array(st)
delta=rng.uniform(-noise_ms,noise_ms,len(st))/1000
P=np.array([true_pos(t+d) for t,d in zip(st,delta)])
V=np.array([true_vel(t+d) for t,d in zip(st,delta)])
def hermite(t):
    i=np.searchsorted(st,t); i=max(1,min(i,len(st)-1))
    a,b=i-1,i; h=st[b]-st[a]; s=(t-st[a])/h
    s2,s3=s*s,s*s*s
    h00,h10,h01,h11=2*s3-3*s2+1,s3-2*s2+s,-2*s3+3*s2,s3-s2
    p=h00*P[a]+h10*h*V[a]+h01*P[b]+h11*h*V[b]
    v=V[a]+(V[b]-V[a])*s
    return p,v
# GTA ticks
t=2.0; fts=[]; 
res={}
for mode in ['direct','filter']:
    t=2.0; prev=None; corr=[]; fX=None; tau=0.08; drawn=[]
    rng2=np.random.default_rng(7)
    ftn=0.0074
    while t<11:
        ft=ftn; ftn=rng2.uniform(0.0065,0.0085)   # next physics step
        tB=t-0.045
        X,v=hermite(tB)
        if mode=='direct': Xd,Vd=X,v
        else:
            if fX is None: fX=X.copy()
            else:
                fX=fX+v*ft; e=X-fX; fX=fX+e*(1-np.exp(-ft/tau))
            Xd=fX.copy(); Vd=v+(X-fX)/tau
        if prev is not None:
            pO,pV=prev; corr.append(np.linalg.norm(Xd-(pO+pV*ft)))
        # GTA integrates with ftn; drawn next tick = Xd+Vd*ftn
        drawn.append(Xd+Vd*ftn)
        prev=(Xd,Vd); t+=ftn
    drawn=np.array(drawn); d2=np.linalg.norm(drawn[2:]-2*drawn[1:-1]+drawn[:-2],axis=1)
    err=[np.linalg.norm(drawn[i]-true_pos(2.0+0)) for i in range(1)]
    print(f"{mode:7s} noise ±{noise_ms} ms: correction per tick avg {np.mean(corr)*100:.2f} cm max {np.max(corr)*100:.2f} cm; 2nd diff of drawn avg {np.mean(d2)*100:.2f} cm p95 {np.percentile(d2,95)*100:.2f} cm")

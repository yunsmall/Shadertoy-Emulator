//Technical Collapse - Result of live shader coding performance at NODE - Forum for digital arts in Frankfurt.
//Live coded on stage in 30 minutes while chatting shit. Designed in about 4 hours.
//Video of the live performance: https://youtu.be/ebF2oK1yvO8?t=23274

vec2 z,v,e=vec2(.00035,-.00035);float t,tt,b,bb,g,gg;vec3 np,bp,pp,po,no,al,ld;
float bo(vec3 p,vec3 r){p=abs(p)-r;return max(max(p.x,p.y),p.z);}
mat2 r2(float r){ return mat2(cos(r),sin(r),-sin(r),cos(r));}
vec2 mp( vec3 p,float ga )
{
    float ta=smoothstep(0.,1.,(clamp(sin(p.y*.01+tt*.5),-.25,.25)*2.+.5));
    p=mix(p,p.xzy,ta);
    pp=p;
    pp=vec3(atan(pp.x,pp.z)*8.,abs(abs(pp.y)-25.)-7.,length(pp.xz)-5.-bb);
    pp.yz-=.5*exp(pp.y*.1);
    pp.yz*=r2(.3*exp(pp.y*.1));
    pp.xy=mod(pp.xy+vec2(0,tt*5.),2.)-1.;
    vec2 h,t=vec2(length(pp*vec3(1,.6,1))-.75,5); //PROJECTED SPHERES
    t.x=min(t.x,length(pp.xz+vec2(0,2))-.5*sin(p.y*.2+tt*2.)); //TENTACLES
    h=vec2(length(pp.yz)-1.,3);    //PROJECTED BLACK LINES
    h.x=max(h.x,abs(abs(pp.y)-.3)-.15);
    t=t.x<h.x?t:h; t.x*=0.7;
    bp=p+vec3(0,sin(tt)*5.,0); ////////TOWER
    bp.xz*=r2(tt-ta*3.1);
    h=vec2(bo(bp,vec3(2.+bb,100,2)),8);  //MAIN GREY TOWER
    vec2 d=vec2(bo(bp,vec3(1.8+bb,100,2.2)),6);   //YELLOW BIT
    np=bp;
    np.xy*=r2(.785*ta);
    for(int i=0;i<5;i++){
        np=abs(np)-2.;
        np.yz*=r2(.785);
        if(mod(float(i),2.)>0.)np.xz*=r2(.785*(bb+1.));
        h.x=max(h.x,-(bo(np,vec3(.5,200,2.5))));
        d.x=max(d.x,-(bo(np+vec3(2.,0,0),vec3(.7,200,2.5))));
    }
    t=t.x<h.x?t:h;
    t=t.x<d.x?t:d;
    h=vec2(bo(bp,vec3(1.+bb,100,1.)),3);
    h.x=max(h.x,abs(np.y)-1.7);
    t=t.x<h.x?t:h;
    h=vec2(bo(bp,vec3(1.25+bb,100,.1)),7);   //GLOWY BIT
    pp=p;
    pp.xz*=r2(-ta*5.+tt-sin(p.y*.1)*.5);
    pp.xz=abs(pp.xz)-2.-bb-cos(p.y*.1)*2.;
    pp.y=mod(pp.y+tt*5.,3.)-1.5;
    h.x=min(h.x,.8*length(pp));
    g+=(0.1/(0.1+h.x*h.x*(100.-99.*sin(p.y*.1+tt+bb*6.))))*ga;
    t=t.x<h.x?t:h;
    return t;
}
vec2 tr( vec3 ro,vec3 rd )
{
    vec2 h,t=vec2(.1);
    for(int i=0;i<128;i++){
        h=mp(ro+rd*t.x,1.);
        if(h.x<.0001||t.x>50.) break;
        t.x+=h.x;t.y=h.y;
    }
    if(t.x>50.) t.y=0.;
    return t;
}
#define a(d) clamp(mp(po+no*d,0.).x/d,0.,1.)
#define s(d) smoothstep(0.,1.,mp(po+ld*d,0.).x/d)
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv=(fragCoord.xy/iResolution.xy-0.5)/vec2(iResolution.y/iResolution.x,1);
    tt=mod(iTime,40.);
    bb=max(0.,-3.+floor(tt*.25)+smoothstep(0.,1.,min(fract(tt*.25),.25)*4.));
    b=smoothstep(0.,1.,clamp(sin(tt*.5),-.25,.25)*2.+.5);
    vec3 ro=mix(vec3(sin(tt*.4)*5.,3,-15.-bb),vec3(sin(tt*.4)*15.-bb,10,-4),b);
    vec3 cw=normalize(vec3(0)-ro),cu=normalize(cross(cw,vec3(0,1,0))),
    cv=normalize(cross(cu,cw)),rd=mat3(cu,cv,cw)*normalize(vec3(uv,.5)),co,fo;
    co=fo=vec3(.1,.12,.17)-length(uv)*.1-rd.y*.1;
ld=normalize(vec3(0.0,.3,-0.2));
z=tr(ro,rd);t=z.x;
if(z.y>0.){
po=ro+rd*t;
no=normalize(e.xyy*mp(po+e.xyy,0.).x+e.yyx*mp(po+e.yyx,0.).x+e.yxy*mp(po+e.yxy,0.).x+e.xxx*mp(po+e.xxx,0.).x);
al=vec3(.5);
if(z.y<5.)al=vec3(0.);
if(z.y>5.)al=vec3(1.,.5,0)*(2.-ceil(abs(sin(np.z-np.y))-.05)),no+=.5*ceil(sin(np.z-np.y)),no=normalize(no);
if(z.y>6.)al=vec3(1);
if(z.y>7.)al=vec3(.5)*(2.-ceil(abs(sin(np.z-np.y))-.05));
float dif=max(0.,dot(no,ld)),
fr=pow(1.+dot(no,rd),4.),
sp=pow(max(dot(reflect(-ld,no),-rd),0.),40.);
co=mix(sp+al*(a(.05)*a(.3)+.2)*(dif+s(5.)*.5),fo,min(fr,.5));
co=mix(fo,co,exp(-.00005*t*t*t));
}
co=mix(co,co.zyx,b);
co=mix(co,co.xzy,length(uv));
fragColor = vec4(pow(co+g*.2,vec3(.55)),1);
}
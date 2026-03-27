// Fork of "Relativistic Space Drive" by Amirk. https://shadertoy.com/view/7sKBzz
// 2023-09-26 03:58:36

/*
Notice that, if you boost forward with constant proper acceleration
the objects in front actually seem to recede further away at first,
before halting and then coming towards extremely rapidly.
The apparent speed may look faster than light but that is an
"optical effect" due to retardation.

The biggest challenge, compared to classical dynamics
(galilean transformations), was that, it is not enough to know
our/rocket's position and velocity after certain boosts.
Lorentz transforms do not commute so we have to keep track of
the cumulative total transformation from all the boosts we have
made along the way.
This is done in Buffer A.
*/


//comment this if you do not want the delayed view.
#define RETARD 1.

#define MAX_ITER 1500.
#define MAX_DIST 70.
#define SURF .001

vec4 fourvel = vec4(0,0,0,1);
vec4 position =vec4(0);
vec4 boost=vec4(0,0,0,0);
vec3 orientation=vec3(1,0,0);


vec3 SIZE= vec3(.1);

//ray origin in the moving coords.
vec4 RO, rd;
vec2 m;
mat4 TransformMatrix;

vec3 col = vec3(0);

float halo=0.;
float cylinder;
float cylinder2;
float cylinder3;
float cylinder4;



vec3 color( float s){
    return vec3(1) - vec3(.9,.8,0)*smoothstep(.0,2., s)-
    vec3(0.,.6,.6)*smoothstep(0.,1.5, -s);
}

void updateVel(){
    // Fetch the fourvelocity from the Buffer A
    boost= texelFetch( iChannel0, ivec2(5,5), 0);
    orientation=texelFetch( iChannel0, ivec2(6,6), 0).xyz;
    fourvel=texelFetch( iChannel0, ivec2(0,0), 0 );
}

void updatePos(){
    // Fetch the fourposition from the Buffer B

    position =texelFetch( iChannel0, ivec2(1,0), 0 );
    vec4 cam=vec4(-1,0,0,0);
    if(m!=vec2(0)){
        cam.xy*=rot((m.y-.5)*PI);
        cam.xz*=rot(-(m.x-.5)*2.*PI);
    }
    //position+=TransformMatrix*cam;
}


float sdBox(vec4 p , vec3 s){
    float time=p.w;
    p.xyz=fract(p.xyz)-.5; //this creates the grid of reference cubes
    p.yz*=rot(p.w*.5);
    p.xyz= abs(p.xyz)-s;
    return length(max(p.xyz,0.))+ min(max(p.x,max(p.y,p.z)),0.);
}

float sdCylinder( vec3 p, vec2 h )
{
    vec2 d = abs(vec2(length(p.xz),p.y)) - h;
    float outer= min(max(d.x,d.y),0.0) + length(max(d,0.0));
    vec2 d2 = abs(vec2(length(p.xz),p.y)) - (h+vec2(-.05,.05));
    float inner= min(max(d2.x,d2.y),0.0) + length(max(d2,0.0));

    return max(outer,-inner);
}


float getDist(vec4 q){
    float dist= sdBox(q,SIZE);

    //the cylinders:
    float len= 3.;
    float d= 4.;
    float s=.5;
    q.x-=d;
    cylinder= sdCylinder(q.zxy, vec2(s,len*s));
    q.x+=2.*d;
    cylinder2= sdCylinder(q.zxy, vec2(s,len*s));
    q.x-=d;
    q.z-=d;
    cylinder3= sdCylinder(q.xzy, vec2(s,len*s));
    q.z+=2.*d;
    cylinder4= sdCylinder(q.xzy, vec2(s,len*s));

    dist = min(dist,cylinder);
    dist = min(dist,cylinder2);
    dist = min(dist,cylinder3);
    dist = min(dist,cylinder4);

    return dist;
}



vec4 getRayDir(vec2 uv, vec4 lookAt, float zoom){

    vec3 f= normalize(lookAt.xyz);
    vec3 r= normalize(cross(vec3(0,1,0),f));
    vec3 u= cross(f,r);

    return vec4(normalize(f*zoom+uv.x*r+uv.y*u),lookAt.w/c);

    //the w-component determines how we look into past/future/present.
}

float RayMarch(vec4 ro, vec4 rd, float side){
    float dO=0.;
    float i=0.;
    while(i<MAX_ITER){
        vec4 p= ro+dO*rd; //if rd.w =-c we look back in time as we march further away

        float dS=side*getDist(p);

        dO+=dS;

        if(dO>MAX_DIST||abs(dS)<SURF){
            break;
        }
        i++;
    }

    return dO;
}

vec3 getNormal(vec4 p){
    vec2 e= vec2(0.01,0);
    float d=getDist(p);
    vec3 n = d-vec3(getDist(p- e.xyyy),getDist(p- e.yxyy),getDist(p- e.yyxy));

    return normalize(n);
}

void getMaterial(vec4 p){
    if(cylinder<5.*SURF){
        p.yz*=rot(p.w*.5);
        col=vec3(2,0,.2)*sin(atan(p.y,p.z)*10.)*sin(atan(p.y,p.z)*10.);
    }
    else if(cylinder2<5.*SURF){
        p.yz*=rot(p.w*.5);
        col=vec3(.2,1,0)*sin(atan(p.y,p.z)*10.)*sin(atan(p.y,p.z)*10.);
    }else if(cylinder3<5.*SURF){
        p.xy*=rot(p.w*.5);
        col=vec3(1,0,1)*sin(atan(p.y,p.x)*10.)*sin(atan(p.y,p.x)*10.);;
    }else if (cylinder4<5.*SURF){
        p.xy*=rot(p.w*.5);
        col=vec3(0,.2,1)*sin(atan(p.y,p.x)*10.)*sin(atan(p.y,p.x)*10.);;
    }// else col= vec3(1);
}

mat4 getTransform(){
    mat4 M= mat4(1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1);
    if(iFrame>10){
        for(int j=1; j<=4; j++)
        M[j-1]=texelFetch( iChannel0, ivec2(0, j), 0);
    }
    return M;
}

//////////////ROCKET///////////////////////////////////////////////

void angularRepeat(const float a, inout vec2 v)
{
    float an = atan(v.y,v.x);
    float len = length(v);
    an = mod(an+a*.5,a)-a*.5;
    v = vec2(cos(an),sin(an))*len;
}

void angularRepeat(const float a, const float offset, inout vec2 v)
{
    float an = atan(v.y,v.x);
    float len = length(v);
    an = mod(an+a*.5,a)-a*.5;
    an+=offset;
    v = vec2(cos(an),sin(an))*len;
}

float mBox(vec3 p, vec3 b)
{
    return max(max(abs(p.x)-b.x,abs(p.y)-b.y),abs(p.z)-b.z);
}




float dfRocketBody(vec3 p)
{

    vec3 p2 = p;

    angularRepeat(PI*.25,p2.zy);
    float d = p2.z;
    d = max(d, (rot(PI*-.125)*( p2.xz+vec2(-.7,0))).y);
    d = max(d, (rot(PI*-.25*.75)*(p2.xz+vec2(-0.95,0))).y);
    d = max(d, (rot(PI*-.125*.5)*( p2.xz+vec2(-0.4,0))).y);
    d = max(d, (rot(PI*.125*.25)*( p2.xz+vec2(+0.2,0))).y);
    d = max(d, (rot(PI*.125*.8)*( p2.xz+vec2(.5,0))).y);

    d = max(d,-.8-p.x);

    d -= .5;

    return d;
}

float dfRocketFins(vec3 p)
{

    p.yz*=rot(position.w*(1.+10.*boost.w));
    vec3 pFins = p;
    angularRepeat(PI*.5,pFins.zy);
    pFins -= vec3(-1.0+cos(p.x+.2)*.5,.0,.0);
    pFins.xz*=rot(-PI*.25);
    float scale = 1.0-pFins.z*.6;
    float d =mBox(pFins,vec3(.17,.03,3.0)*scale)*.5;
    return d;
}

float Jet(vec3 p)
{
    float d= length(p.yz);
    if(p.x>0.2)d=20.;

    return d-p.x*.05;
}

float df(vec3 p)
{
    if(boost.xz!=vec2(0)){
        p.xz*=rot(-atan(orientation.z,orientation.x));
    }



    float proxy = mBox(p,vec3(4.5,.8,.8));
    if (proxy>1.)
    return proxy;
    float dRocketBody=   dfRocketBody(p);
    float dRocketFins=   dfRocketFins(p);
    float dJet=  Jet(p);
    if(boost.w==1.&&dJet<dRocketFins*5.&&dJet<.4){
        halo+=.7;
    }

    return min(dRocketBody,dRocketFins);
}

vec3 nf(vec3 p)
{

    vec2 e = vec2(0,0.005);
    return normalize(vec3(df(p+e.yxx),df(p+e.xyx),df(p+e.xxy)));
}


void rocket (inout vec3 color, in vec3 pos, in vec3 dir) {


    float dist,tdist = .0;

    for (int i=0; i<100; i++)
    {
        dist  = df(pos);
        pos  += dist*dir;
        tdist+=dist;
        if (dist<0.00001||dist>7.0)break;
    }

    vec3 normal = nf(pos);

    float ao = df(pos+normal*.125)*8.0 +
    df(pos+normal*.5)*2.0 +
    df(pos+normal*.25)*4.0 +
    df(pos+normal*.06125)*16.0;

    ao=ao*.125+.5;

    if(boost.xz!=vec2(0)){
        pos.xz*=rot(-atan(orientation.z,orientation.x));
    }



    vec3 materialColor = vec3(0);
    vec3 blueColor = vec3(.1,.4,.9);

    float dRocketBody = dfRocketBody(pos);
    float dRocketFins = dfRocketFins(pos);
    float dRocket = min(dRocketBody, dRocketFins);


    float r = dot(pos.yz,pos.yz);


    if (dRocketBody<dRocketFins)
    {

        if (pos.x<-.85)
        if (pos.x<-1.2)
        materialColor = blueColor + vec3(0.03 / r);
        else
        materialColor = vec3(.7,.1,.7);
        else
        {
            if (pos.x>1.0)
            materialColor = vec3 (.7,.1,.7) ;
            else
            materialColor = vec3(.6);
        }
    }
    else
    {
        materialColor = vec3(.7,.1,.7);
        if (length (pos - 0.1 * vec3(0.0, normal.yz)) > length (pos)) {

            materialColor -= vec3(.9,.3,1.5) * min(0.2, pos.x + 1.3) / r;
        }
    }

    if (dist<.1) color =  ao*materialColor;
}

//////////////TEXT///////////////////////////////////////////////

#define C(c) U.x-=.5; O+= mychar(U,c)

vec4 mychar(vec2 p, int c)
{
    if (p.x<.0|| p.x>1. || p.y<0.|| p.y>1.) return vec4(0,0,0,1e5);
    return textureGrad( iChannel2, p/16. + fract( vec2(c, 15-c/16) / 16. ), dFdx(p/16.),dFdy(p/16.) );
}

vec4 text( out vec4 O, vec2 uv )
{
    O = vec4(0.0);
    uv /= iResolution.y;
    vec2 pos = vec2(.0,.9);
    float FontSize = 6.;
    vec2 U = ( uv - pos)*64.0/FontSize;


    float k =abs(fourvel.x/fourvel.w);
    C(115);C(112);C(101);C(101);C(100);C(32);


    C(46);
    C(48+int(10.*fract(k)));
    C(48+int(10.*fract(10.*k))); C(99);


    U.y+=1.1; U.x+=5.;
    C(116);C(105);C(109);C(101);C(32);
    C(44-int(sign(position.w)));

    C(48+int(10.*fract(abs(position.w*.01))));
    C(48+int(10.*fract(abs(position.w*.1))));
    C(46);
    C(48+int(10.*fract(abs(position.w))));
    return O.xxxx;
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{

    vec2 uv = (fragCoord-.5*iResolution.xy)/iResolution.y;
    if(iMouse.xy==vec2(0))
    m = vec2(.5);
    else{
        m = (iMouse.xy-.5)/iResolution.xy;
    }

    updateVel();
    //coordinate transform:
    TransformMatrix=getTransform();
    updatePos();

    //ray's spacetime origin represented in "stationary coordinates":
    RO=position;
    float zoom= 1.;


    vec4 lookAt;

    #ifdef RETARD
        lookAt = vec4(c, 0, 0, -1);
    #else
        lookAt = vec4(c, 0, 0, 0);
    #endif

    if(m!=vec2(0)){
        lookAt.xy*=rot((m.y-.5)*PI);
        lookAt.xz*=rot(-(m.x-.5)*2.*PI);
    }

    //ray in our moving coords:
    vec4 ray= getRayDir(uv, lookAt, zoom);


    //adding the rocket on top
    vec3 r_color = vec3 (0);
    vec3 cam=vec3(-7,1.5,0);

    if(m!=vec2(.5)){
        cam.xy*=rot((m.y-.5)*PI);
        cam.xz*=rot(-(m.x-.5)*2.*PI);
    }
    rocket (r_color, cam, ray.xyz);

    if (length (r_color) > 0.0) {
        fragColor.xyz = r_color;
    }else{



        //ray direction from moving coords to stationary coords:
        rd= TransformMatrix*ray;
        //some rescaling for accuracy:

        #ifdef RETARD
        rd.xyz=normalize(rd.xyz);
        rd.w=-RETARD;
        #else
       rd=normalize(rd);
        #endif

    /* //just some helpfull scaling factors for raymarching:
    if(RETARD>0.){
         vv= max(0., -dot(fourvel.xyzw, rd.xyzw));
    }else{
         vv= abs(dot(fourvel.xyzw, rd.xyzw));
    }
    */


        //RAYMARCH IN SPACETIME calculated in stationary coordinates:
        vec4 p=RO;

        float d= RayMarch(p, rd, 1.);


        if(d<MAX_DIST){ //if we hit an object:
                        p= p+ d*rd;

                        col=color(dot(normalize(rd.xyz), fourvel.xyz));
                        getMaterial(p);

                        vec3 n= getNormal(p);

                        float dif= dot(n, normalize(vec3(-3,2,1)))*.5+.5;
                        col/=length(d*rd)*.2;
                        col*=dif*dif;

        }

        col.xyz+=text(fragColor, fragCoord).xyz;


        fragColor = vec4(col,1.0)+halo*halo*vec4(.4,.2,1,1);

    }

}
//
// A gravitational wave effect by Tom@2016
//

//==================================================
// Stars texture from Kali
// https://www.shadertoy.com/view/XlfGRj
#define iterations 15
#define formuparam 0.53
#define volsteps 13
#define stepsize 0.1
#define zoom   0.800
#define tile   0.850
#define speed  0.010
#define brightness 0.005
#define darkmatter 0.300
#define distfading 0.800
#define saturation 0.850
vec3 kali_stars(vec3 from, vec3 dir)
{
    //volumetric rendering
    float s=0.1,fade=1.;
    vec3 v=vec3(0.);
    for (int r=0; r<volsteps; r++) {
        vec3 p=from+s*dir*.5;
        p = abs(vec3(tile)-mod(p,vec3(tile*2.))); // tiling fold
        float pa,a=pa=0.;
    for (int i=0; i<iterations; i++) {
    p=abs(p)/dot(p,p)-formuparam; // the magic formula
    a+=abs(length(p)-pa); // absolute sum of average change
    pa=length(p);
    }
    float dm=max(0.,darkmatter-a*a*.001); //dark matter
    a*=a*a; // add contrast
    if (r>6) fade*=1.-dm; // dark matter, don't render near
    //v+=vec3(dm,dm*.5,0.);
    v+=fade;
    v+=vec3(s,s*s,s*s*s*s)*a*brightness*fade; // coloring based on distance
    fade*=distfading; // distance fading
    s+=stepsize;
    }
    return mix(vec3(length(v)),v,saturation)*.01;
}
//==================================================

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 coord = fragCoord.xy;
    if (iMouse.z > 0.) {
        float zoom_in = min(1.,200./iResolution.y);
        coord = (coord - iMouse.xy)*zoom_in + iMouse.xy; // zoom-in with mouse
    }
    vec2 q = coord/iResolution.xy;
    vec2 uv = vec2(q.x,q.y*iResolution.y/iResolution.x);

    vec3 e = vec3(vec2(1.)/iResolution.xy,0.);
    float p10 = texture(iChannel0, q-e.zy).x;
    float p01 = texture(iChannel0, q-e.xz).x;
    float p21 = texture(iChannel0, q+e.xz).x;
    float p12 = texture(iChannel0, q+e.zy).x;

    // Totally fake displacement and shading:
    vec3 grad = normalize(vec3(p21 - p01, p12 - p10, 1.));
    float t = iTime + 30.;
    vec3 from = vec3(1.+t*.002,.5+t*.001,0.5);
    vec4 c = vec4(kali_stars(from, vec3(uv*zoom, 1.) + grad*.15), 1.);
    vec3 light = normalize(vec3(.2,-.5,.7));
    float diffuse = dot(grad,light);
    float spec = pow(max(0.,-reflect(light,grad).z),32.);
    vec4 color = mix(c,vec4(.2,.6,1.,1.),.15)*.7 + pow(diffuse*.5+.5,16.)*.3 + spec*.5;
    //*max(diffuse,0.) + spec;

    // Black holing
    // Simulate gravity waves
    float scale = 64.; //iResolution.y*.1;
    float dist = cos(t*.03);
    float phase = t * .7 / (dist+.001);
    vec2 center = iResolution.xy*.5;
    vec2 dpos = vec2(cos(phase),sin(phase))*scale*dist;
    float d = smoothstep(.7*scale,.5,length(center + dpos*.7 - coord));
    d += smoothstep(.5*scale,.5,length(center - dpos*1.4 - coord)); // one black hole is smaller
    color *= max(0.,1.-d*4.);
    fragColor = color;
}
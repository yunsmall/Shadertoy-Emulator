// Fork of "Simple wormhole" by A_Toaster. https://shadertoy.com/view/wfcXzj
// 2025-06-07 16:45:42

#define EPS 0.001
#define MAX_DIST 100.

float sdSphere(float r, vec4 p) {
    return length(p) - r;
}

float sdBox( vec4 p, vec4 b )
{
    vec4 q = abs(p) - b;
    return length(max(q,0.0)) + min(max(q.x,max(q.y,max(q.z,q.w))),0.0);
}

// flattened in the 4th dimension so it doesn't stick into the parallel universe :p
float sdHyperEllipsoid(float r, vec4 p) {
    return length(p.xyz) - r + abs(p.w) * 100.;
}


float scene(vec4 p) {
    return min(
        sdHyperEllipsoid(0.75, p - vec4(1., 2., 0., r_min)),
        sdBox(p - vec4(2., 2., -0.75, -r_min), vec4(vec3(0.4), 0.1) + 0.1)
    );

}


// TODO 3d normal calc. This doesn't work for objects inside the wormhole
vec3 calcNormal( in vec4 p ) // for function f(p)
{
    const float h = 0.0001; // replace by an appropriate value
    const vec2 k = vec2(1,-1);
    return normalize( k.xyy*scene( vec4(p.xyz + k.xyy*h, p.w) ) +
    k.yyx*scene( vec4(p.xyz + k.yyx*h, p.w) ) +
    k.yxy*scene( vec4(p.xyz + k.yxy*h, p.w) ) +
    k.xxx*scene( vec4(p.xyz + k.xxx*h, p.w) ) );
}


bool raymarch(vec4 ro, vec4 rd, int steps, out vec4 hit, out vec4 hit_rd) {
    hit = ro;
    hit_rd = rd;
    for(; steps > 0; --steps) {
        float d = min(scene(hit), space_sdf(hit));
        hit += d * hit_rd;
        space_normalize(hit, hit_rd); // this is the magic part, snaps the 4d position to the 3d hypersurface of the wormhole
        if(d < EPS) {
            return true;
        }
        if(d > MAX_DIST) {
            return false;
        }
    }
    return false;
}


// fov 1 = 90 degrees
vec4 camera(vec2 uv_norm, mat4x4 orientation, float fov) {
    //vec3 forward = orientation.x;
    //vec3 up = orientation.z;
    //vec3 left = orientation.y;

    //return left * fov * uv_norm.x + up * fov * uv_norm.y + forward;
    return normalize(orientation * vec4(1.0, -uv_norm.x * fov, uv_norm.y * fov, 0.));
}


// Star Nest from https://www.shadertoy.com/view/XlfGRj
#define iterations 17
#define formuparam 0.53

#define volsteps 20
#define stepsize 0.1

#define tile   0.850
#define speed  0.010

#define brightness 0.0015
#define darkmatter 0.300
#define distfading 0.730
#define saturation 0.850

vec3 starnest(in vec3 from, in vec3 dir) {
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
    v=mix(vec3(length(v)),v,saturation); //color adjust

    return v * 0.01;
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    deserializeState(iChannel0, iResolution.xy);
    // Normalized pixel coordinates (from -1 to 1)
    vec2 uv = fragCoord/iResolution.xy * vec2(2.) - vec2(1.);
    uv = uv * vec2(iResolution.x / iResolution.y, 1.);

    float pitch = -sin(iTime) * 0.4;

    //mat3x3 cam_orient = mat3x3(cos(pitch),0.0,-sin(pitch),
    //                           0.0,1.0,0.0,
    //                           sin(pitch),0.0,cos(pitch));


    vec4 cam_origin = state.camera[3];


    vec4 rd = camera(uv, state.camera, 0.75);

    vec4 hit_pos;
    vec4 hit_rd;
    bool hit = raymarch(cam_origin, rd, 128, hit_pos, hit_rd);



    // Time varying pixel color
    vec3 col;

    if(hit) {
        vec3 n = calcNormal(hit_pos);
        col = vec3(0.5) + 0.2 * n;

    } else {
        // add w so that the star nest is different in the two universes
        // tiny iTime factor for twinkling when stationary
        col = starnest(vec3(1., normalize(hit_pos.w) * 2., 3. + iTime * 0.0001), hit_rd.xyz);

        if(hit_pos.w < 0.) {
            col *= vec3(1., 0.5, 0.5);
        } else {
            col *= vec3(0.5, 0.5, 1.);
        }

    }
    // Output to screen
    fragColor = vec4(col,1.0);

}
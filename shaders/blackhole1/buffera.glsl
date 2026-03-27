//
// A gravitational wave effect by Tom@2016
//
// The water propagation is based on my previous shader:
// https://www.shadertoy.com/view/Xsd3DB
//
// Originally based on: http://freespace.virgin.net/hugo.elias/graphics/x_water.htm
// A very old Hugo Elias water tutorial :)
//

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    float d = 0.;
    vec3 e = vec3(vec2(1.)/iResolution.xy,0.);
    vec2 q = fragCoord.xy/iResolution.xy;
    vec4 c = texture(iChannel0, q);
    // Boundary conditions
    bool boundary = false;
    vec2 dir = vec2(0.);
    if (fragCoord.y < 1.) { dir.y = 1.; boundary = true; }
    if (fragCoord.y > iResolution.y-1.) { dir.y = -1.; boundary = true; }
    if (fragCoord.x < 1.) { dir.x = 1.; boundary = true; }
    if (fragCoord.x > iResolution.x-1.) { dir.x = -1.; boundary = true; }
    if (iFrame>2 && boundary) {
        d = texture(iChannel0, q+e.xy*dir).x;
        fragColor = vec4(d, c.x, 0, 0);
        return;
    }

    float p11 = c.y;

    float p10 = texture(iChannel0, q-e.zy).x;
    float p01 = texture(iChannel0, q-e.xz).x;
    float p21 = texture(iChannel0, q+e.xz).x;
    float p12 = texture(iChannel0, q+e.zy).x;

    // Simulate gravity waves
    float t = iTime + 30.;
    float scale = 64.; //iResolution.y*.1;
    float dist = cos(t*.03);
    float phase = t * .7 / (dist+.001);
    vec2 center = iResolution.xy*.5;
    vec2 dpos = vec2(cos(phase),sin(phase))*scale*dist;
    d = smoothstep(.7*scale,.5,length(center + dpos*.7 - fragCoord.xy));
    d += smoothstep(.5*scale,.5,length(center - dpos*1.4 - fragCoord.xy)); // one black hole is smaller

    // The actual propagation:
    d += -(p11-.5)*2. + (p10 + p01 + p21 + p12 - 2.);
    d *= .99; // dampening
    d *= float(iFrame>=2); // clear the buffer at iFrame < 2
    d = d*.5 + .5;

    // Put previous state as "y":
    fragColor = vec4(d, c.x, 0, 0);
}
/*

This shader is an attempt at porting smallpt to GLSL.

See what it's all about here:
http://www.kevinbeason.com/smallpt/

The code is based in particular on the slides by David Cline.

Some differences:

- For optimization purposes, the code considers there is
  only one light source (see the commented loop)
- Russian roulette and tent filter are not implemented

I spent quite some time pulling my hair over inconsistent
behavior between Chrome and Firefox, Angle and native. I
expect many GLSL related bugs to be lurking, on top of
implementation errors. Please Let me know if you find any.

Update:
There is a bug in the next event estimation. If the scene
is modified into a furnace test, the result will be correct
when ENABLE_NEXT_EVENT_PREDICTION is undefined, but too
much energy ends up being added when it is defined.
Thanks @bernie_freidin for reporting this bug.

--
Zavie

*/

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    fragColor = vec4(pow(clamp(texture(iChannel0, fragCoord.xy / iResolution.xy).rgb, 0., 1.), vec3(1./2.2)), 1.);
}

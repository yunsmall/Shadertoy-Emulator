/*

Created by Tonny Espeset, 2026
https://www.shadertoy.com/view/NcKXW3

Neural Kitten Video v4

Feel free to use this code, but please keep this credit and link.

Check my other shaders at

https://www.shadertoy.com/user/Espeset


NEURAL VIDEO
------------

One of my first shadertoy shaders ever was the Neural Pet

https://www.shadertoy.com/view/MfSBWK

which used a SIREN based neural network for the visual components. In fact these
kinds of networks are excellent at compressing anything of an organic lossy
nature; images, audio even 3D models and video.

So, in this experiment I had a look at using one as a codec for video, based
on this paper:

https://arxiv.org/pdf/2301.10241

After a lot of tests and crunching I found it's not really a good replacement
for a traditional codec like mpeg, the quality is similar but the training time
is long so it is quite impractical for full length movies in high resolution.
Streaming would also mean training one network per segment.

A nice thing is you get video interpolation for free, the network is continuous
in time, so you can play back a video that was originally 25fps at 120fps.
The catch is it only really knows the frames it was trained on, between them
it blends, so fast motion needs interpolated training frames.

Since my source video had only 24FPS, I first interpolated to 60 FPS using
OpenCVs Deepflow which generates motion vectors from video. These can be used
to generate frames inbetween frames. Not perfect, but good enough for this experiment.

The neural network can consume an infinite FPS basically, and simply gets better at
predicting each frame at any given time, without changing its structure. So, in this
kitten example it means that two SIREN videos of the exact same file size, one trained
on 60FPS the other 24FPS, the former will look a lot smoother within the same budget.

Another nice benefit of a SIREN based video is it has random access to any frame at
any time, while mpeg needs to rebuild each successive frame based on its current state,
meaning it struggles to play backwards or show a random frame in a long video far from
an I-frame (a frame stored as is without being built by incremental changes).

So, this is why we can easily play the video as ping-pong, and at any speed anywhere.
If you look closely, you can see I do an ease in/out before the playback changes direction
for a nice smooth transition. :)


*/

const int VW = 480, VH = 270;

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uvScreen = fragCoord/iResolution.xy;
    float aspect = iResolution.x/iResolution.y;
    vec2 uv = uvScreen;
    if (aspect > 16.0/9.0) uv.x = (uvScreen.x - 0.5)*aspect/(16.0/9.0) + 0.5;
    else uv.y = (uvScreen.y - 0.5)*(16.0/9.0)/aspect + 0.5;
    vec3 col = vec3(0.0);
    if (all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0))))
    {
        int W = int(iResolution.x), H = int(iResolution.y);
        vec2 vid = vec2(min(VW, W), min(VH, H - 1));
        // the decoded video sits bottom-left in Buffer B; sample it bilinearly, half a texel inside its edges
        vec2 q = clamp(uv * vid, vec2(0.5), vid - 0.5);
        col = texture(iChannel0, q / iResolution.xy).rgb;
    }
    fragColor = vec4(col, 1.0);
}

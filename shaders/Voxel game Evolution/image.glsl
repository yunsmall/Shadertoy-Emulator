/*---------------------------------------------------------
	THIS SHADER IS BASED ON  "[SH16C] Voxel Game" by fb39ca4  
  	
	when switching to full screen press L until you get better performance (K for higher resolution)

CONTROLS:
    drag mouse to move view and select blocks
    WASD or arrows to move
    Space to jump
    Double-tap space to start flying, use space and shift to go up and down.

    mouse double click to select/unselect a block (doesn't work well with low framerate
    Q + mouse button to place block 
    E + mouse button to destroy blocks 
	R + mouse button to change shape of a block 
	F + mouse button to rotate a shape on z axis 
	G + mouse button to rotate a shape on y axis
	C + mouse button to select multiple blocks (hold on "C" to clear selection)
	destroy,place, shape,rotate also work on selected blocks, without mouse button

    mouse click on inventory to select a block type
	M to toggle map
	I to toggle inventory (hidden, simple, full)

	O,P to decrease/increase speed of day/night cycles   
    k,L to decrease/increase pixel sizes 
	T to teleport to a random location
    Page Up/Down to increase or decrease zoom 
	F7 enable/disable torch light diffusion (flickering on some GPUs)
	F8 enable/disable water flow

BLOCK MECHANICS:

	 TORCH= light
	 TREE= grows if placed
	 DIAMOND= illumninated if close to GOLD or other illuminated diamonds
	 RED BLOCK= mirror 
	 WATER= semtrasparent, flows downwards
	 SAND = falls if in empty space or with 4 horizontal steps
	 PINK MARBLE= infinite water source

CONFIGURATION:
	see #define settings in "Common" file 

BUFFERS:
    "BUFFER A": actions, collisions, settings, material textures
    "BUFFER B": voxel cache, nearest blocks full height 
    "BUFFER C": surface voxel cache (just one block for every xy position)
    "BUFFER D": rendering, map
    "IMAGE"   : gui, stats

CREDITS:
	Voxel game: @fb39ca4 https://shadertoy.com/view/MtcGDH
	Voxel traversal: @Iq https://www.shadertoy.com/view/4dfGzs
	GLSL Number Printing:  @P_Malin https://www.shadertoy.com/view/4sBSWW
	Textures: @Reinder https://www.shadertoy.com/view/4ds3WS
	encoding/decoding: @Eiffie https://www.shadertoy.com/view/wsBfzW
	grass: @W23 https://www.shadertoy.com/view/MdsGzS
	Noise: @Makio64 https://shadertoy.com/view/Xd3GRf
	water reflection:@Reinder https://www.shadertoy.com/view/MdXGW2

CHANGELOG & TODO: 
	see bottom of the file

//-----------------------------------------------------*/


#ifdef STATS

// ---- 8< ---- GLSL Number Printing - @P_Malin ---- 8< ----
// Creative Commons CC0 1.0 Universal (CC-0) 
// https://www.shadertoy.com/view/4sBSWW
float DigitBin(const in int x)
{
    return x==0?480599.0:x==1?139810.0:x==2?476951.0:x==3?476999.0:x==4?350020.0:x==5?464711.0:x==6?464727.0:x==7?476228.0:x==8?481111.0:x==9?481095.0:0.0;
}

float PrintValue(const in vec2 fragCoord, const in vec2 vPixelCoords, const in vec2 vFontSize, const in float fValue, const in float fMaxDigits, const in float fDecimalPlaces)
{
    vec2 vStringCharCoords = (fragCoord.xy - vPixelCoords) / vFontSize;
    if ((vStringCharCoords.y < 0.0) || (vStringCharCoords.y >= 1.0)) return 0.0;
    float fLog10Value = log2(abs(fValue)) / log2(10.0);
    float fBiggestIndex = max(floor(fLog10Value), 0.0);
    float fDigitIndex = fMaxDigits - floor(vStringCharCoords.x);
    float fCharBin = 0.0;
    if(fDigitIndex > (-fDecimalPlaces - 1.01)) {
        if(fDigitIndex > fBiggestIndex) {
            if((fValue < 0.0) && (fDigitIndex < (fBiggestIndex+1.5))) fCharBin = 1792.0;
        } else {
            if(fDigitIndex == -1.0) {
                if(fDecimalPlaces > 0.0) fCharBin = 2.0;
            } else {
                if(fDigitIndex < 0.0) fDigitIndex += 1.0;
                float fDigitValue = (abs(fValue / (pow(10.0, fDigitIndex))));
                float kFix = 0.0001;
                fCharBin = DigitBin(int(floor(mod(kFix+fDigitValue, 10.0))));
            }
        }
    }
    return floor(mod((fCharBin / pow(2.0, floor(fract(vStringCharCoords.x) * 4.0) + (floor(vStringCharCoords.y * 5.0) * 4.0))), 2.0));
}

#endif


vec4 drawSelectionBox(vec2 c) {
    vec4 o = vec4(0.);
    float d = max(abs(c.x), abs(c.y));
    if (d > 6. && d < 9.) {
        o.a = 1.;
        o.rgb = vec3(0.9);
        if (d < 7.) o.rgb -= 0.3;
        if (d > 8.) o.rgb -= 0.1;
    }
    return o;
}

mat2 inv2(mat2 m) {
    return mat2(m[1][1],-m[0][1], -m[1][0], m[0][0]) / (m[0][0]*m[1][1] - m[0][1]*m[1][0]);
}

vec4 drawInventory(vec2 c) {

    float h= (load(_inventory).r>1.?NUM_ITEM_ROWS:1.);
    float scale = floor(iResolution.y / 128.);
    c /= scale;
    vec2 r = iResolution.xy / scale;
    vec4 o = vec4(0);
    float xStart = (r.x - 16. * NUM_ITEMS) / 2.;
    c.x -= xStart;
    float selected = load(_selectedInventory).r;
    vec2 p = (fract(c / 16.) - .5) * 3.;
    vec2 u = vec2(sqrt(3.)/2.,.5);
    vec2 v = vec2(-sqrt(3.)/2.,.5);
    vec2 w = vec2(0,-1);
    if (c.x < NUM_ITEMS * 16. && c.x >= 0. && c.y < 16.* h ) {
        float slot = floor(c.x / 16.) + NUM_ITEMS*floor(c.y / 16.) ;
        o = getTexture(48., fract(c / 16.));
        vec3 b = vec3(dot(p,u), dot(p,v), dot(p,w));
        vec2 texCoord;
        //if (all(lessThan(b, vec3(1)))) o = vec4(dot(p,u), dot(p,v), dot(p,w),1.);
        float top = 0.;
        float right = 0.;
        if (b.z < b.x && b.z < b.y) {
            texCoord = inv2(mat2(u,v)) * p.xy;
            top = 1.;
        }
        else if(b.x < b.y) {
            texCoord = 1. - inv2(mat2(v,w)) * p.xy;
            right = 1.;
        }
        else {
            texCoord = inv2(mat2(u,w)) * p.xy;
            texCoord.y = 1. - texCoord.y;
        }
        if (all(lessThanEqual(abs(texCoord - .5), vec2(.5)))) {
            float id = getInventory(slot);
            if (id == 3.) id += top;
            o.rgb = getTexture(id, texCoord).rgb * (0.5 + 0.25 * right + 0.5 * top);
            o.a = 1.;
        }
    }
    vec4 selection = drawSelectionBox(c - 8. - vec2(16. * mod(selected,NUM_ITEMS), 16.* floor(selected/NUM_ITEMS)));
    o = mix(o, selection, selection.a);
    return o;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    float pixelSize = load(_pixelSize).r;
    vec2 renderResolution = ceil(iResolution.xy / pixelSize);
    fragColor = texture(iChannel3, fragCoord * renderResolution / iResolution.xy / iResolution.xy);
    //fragColor = texture(iChannel3, fragCoord);

    if(load(_inventory).r>.0){
        vec4 gui = drawInventory(fragCoord);
        fragColor = mix(fragColor, gui, gui.a);
    }

    #ifdef STATS
    //DISPLAY STATS IF F3 IS TOGGGLED
    float stats = load(_stats).r;
    if (stats >0.5) {
        vec3 pos = load(_pos).xyz;


        //POS
        fragColor = mix( fragColor, vec2(1,.5).xyyx, PrintValue(fragCoord, vec2(0., iResolution.y - 20.), vec2(8,15), pos.x, 5.0, 5.0));
        fragColor = mix( fragColor, vec2(1,.5).yxyx, PrintValue(fragCoord, vec2(0., iResolution.y - 40.), vec2(8,15), pos.y, 5.0, 5.0));
        fragColor = mix( fragColor, vec2(1,.5).xxyx, PrintValue(fragCoord, vec2(0., iResolution.y - 60.), vec2(8,15), pos.z, 5.0, 5.0));

        //ANGLE
        fragColor = mix( fragColor, vec2(1,.5).xyyx, PrintValue(fragCoord, vec2(0., iResolution.y -80.), vec2(8,15),  load(_angle).x, 5.0, 2.0));
        fragColor = mix( fragColor, vec2(1,.5).xxxx, PrintValue(fragCoord, vec2(50., iResolution.y -80.), vec2(8,15),  load(_angle).y, 5.0, 2.0));

        //TIME
        fragColor = mix( fragColor, vec2(1,.5).xxxx , PrintValue(fragCoord, vec2(0., iResolution.y -100.), vec2(8,15), load(_time).r, 5.0, 2.0));



        //if (fragCoord.x < 20.) fragColor.rgb = mix(fragColor.rgb, texture(iChannel0, fragCoord / iResolution.xy).rgb, texture(iChannel0, fragCoord / iResolution.xy).a);

        //FRAMERATE, MEMORY RANGE, HEIGHT LIMIT, RAY DISTANCE
        fragColor = mix( fragColor, vec2(1,.5).xxyx, PrintValue(fragCoord, vec2(0.0, 105.), vec2(8,15), load(_pixelSize).r, 5.0, 1.0));
        fragColor = mix( fragColor, vec2(1,.5).xxyx, PrintValue(fragCoord, vec2(0.0, 85.), vec2(8,15), 1./ iTimeDelta, 5.0, 1.0));

        #if SURFACE_CACHE>0
        fragColor = mix( fragColor, vec2(1,.5).yxxx, PrintValue(fragCoord, vec2(0., 65.), vec2(8,15), calcLoadDist_C( iChannelResolution[2].xy), 5.0, 2.0));
        fragColor = mix( fragColor, vec2(1,.5).xxxx, PrintValue(fragCoord, vec2(0., 45.), vec2(8,15),  heightLimit_C, 5.0, 2.0));
        #endif
        fragColor = mix( fragColor, vec2(1,.5).xxxx, PrintValue(fragCoord, vec2(0., 25.), vec2(8,15),  load(_rayDistMax).r, 5.0, 2.0));

    }

    // "BUFFER C" DUMP
    if(load(_stats).g>.5) {
        vec3 offset = floor(vec3(load(_pos).xy, 0.));
        vec4  color= texture(iChannel2,fragCoord / iResolution.xy);
        fragColor = color;
    }
    //"BUFFER A" DUMP

    if(load(_stats).b>.5) fragColor= texture(iChannel0, fragCoord /iResolution.xy/3.);

    #endif
}
/*
CHANGELOG 
	- 20200425: added elevators
	            added repeated towers
                20200425-1902: fixed map key
	- 20200426: new materials; voxel.value to store user actions and prevent override
         	    select from inventory with mouse
    	 	    more realistic elevator, stabilizing adaptive pixelSize and renderDistance
                fix: when placing & destroying a block, it becomes invisible
                fix: tree grows correctly when placed
                water block not solid and semi-transparent 
    - 20200427: structures are placed randomly; water in caves and water swimming
                added optional "#define FAST_COMPILE" (uncomment row32 of file common to reduce compilation time by half)
                Pyramids
                water flow downward
    - 20200428: water refraction and waves
	            fog and clouds
	- 20200429: cut compilation time - removed duplicated call to render() in buffer D
	- 20200501: compilation optimization and fixed inventory bug
	 		    replaced voxel traversal algorithm with the one described in "Voxel Edge Occlusion" by Iq
	 			skeleton for subvoxels
	- 20200502: revised light diffusion and default
	- 20200503: added shapes (change shape with "R")
	            shape rotation ( with "F")
	- 20200504: shape vertical rotation ( with "G") ... not always working
	            disabled unecessary keys
	            multiselection with "C" 
	- 20200505: shadows; working but unfinished
	            fixed shadows (#ifdef SHADOW);now working
	- 20200509: webgl 1.0 compatibility and compilation optimization
	            compilation optimization
	- 20200510: tree detail (can be disable)
	            optimization: discard if unused texels in buffer A & C
	            revised textures (need to refine) 
	            refactoring - merged buffer A & buffer C with better performances
	- 20200511: refactoring in buffer B neightbour scan; grass prototype
	            grass rendering from https://www.shadertoy.com/view/MdsGzS
	- 20200512: configurable cloud density , grass height & pathway
	- 20200513: inventory toggle with "I" 
	            proof of concept: electriciy with gold=source, diamond=wire
	- 20200514: lighing of unconnected blocks or sand with more than 4 horizontal steps
	            falling sand if  more than 4 horizontal steps
	- 20200515: demo mode at start
	- 20200518: minimalist mirror (red block)
	- 20200520: revised encode/decodeVoxel in order to exploit al 64 pixel bits
	- 20200522: refactoring calcOcclusion()
	- 20200523: mouse double click 
	- 20200524: refactoring: reused raycasting in buffer D for mouse pointer
	- 20200526: added buffer C(surface cache) and other refactoring
	- 20200603: added water physics and water source (pink marble) - work in progress
	- 20200608: performance optimization and revised subvoxel rendering with SUBTEXTURE
	- 20200609: refactoring calcOcclusion (less code & reduced compilation time)
	            refactoring reflection (mirror block) and refraction (water)
	- 20200611: heightmap cache in buffer C (much faster and many new possibilities)
	            far trees (work in progress)
	- 20200612: fixed shadows & occlusion  for subvoxels
                map view rotation, detailed buildings, configurable building distance
	- 20200625: variable water level (50% of the territory is flooded)
	- 20200704: more realistic water refraction and reflection (inspired by Venice shader)
	- 20200712: pseudo Fresnel reflection 
	- 20200723: enable/disable torch(F7) and water flow (F8)

TODO LIST:
	- substitute buffer B/C with cubemap  (done but not working https://www.shadertoy.com/view/3t2yWR)
	- more shapes and materials 
	- more menus (shape, rotation, etc..) 
	- circuits (wire, gate, flip-flop, sensor, etc...) --> in a fork
	- portals
    - constructions: bridge, tower,wall, road
	- explosions

*/
// A raytraced Rubik cube

const float PI = 3.141592653589;
const float HALFPI = PI * 0.5;
const float TWOPI = PI * 2.0;
const float SMOOTHUP = 0.05;

const int MAXBOUNCE = 2;
const int ANTIALIAS = 3;	// Decrease when shader is too slow

const float acc = 0.9;		// Mouse acceleration
const float dst = 6.0;		// Camera distance
const float rotationTime = 0.5; // Rotation time in seconds

// Keys
const float KEY_SHIFT = 16.5/256.0;
const float KEY_S = 83.5/256.0;

// Texture storage locations
const ivec2 storeFrom =  ivec2(1, 9);
const ivec2 storeAt =    ivec2(2, 9);
const ivec2 storeVer =   ivec2(3, 9);
const ivec2 storeHor =   ivec2(4, 9);
const ivec2 storeUp =    ivec2(5, 9);
const ivec2 storeFroms = ivec2(6, 9);

const ivec2 storeSide  = ivec2(7, 9);
const ivec2 storeMouXY = ivec2(8, 9);
const ivec2 storeMouZW = ivec2(9, 9);
const ivec2 storeAction= ivec2(10, 9);

void setVector(in ivec2 fragCoord, inout vec4 fragColor, in ivec2 settingCoord, in vec3 settingValue)
{
    if (fragCoord == settingCoord)
    {
        fragColor.rgb = settingValue;
    }
}

vec3 getVector(in sampler2D sampler, in ivec2 settingCoord)
{
    return
    texelFetch(sampler, settingCoord, 0).rgb;
}

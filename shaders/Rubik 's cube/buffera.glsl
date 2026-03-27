bool isKeyPressed(float key)
{
    return texture( iChannel1, vec2(key, 0.3) ).x > 0.5;
}

int getTile(in vec3 ro, in vec3 rd )
{
    vec3 r1 = vec3(-1.0, -1.0, -1.0);
    vec3 r2 = vec3( 1.0,  1.0,  1.0);
    vec3 k1 = (-ro+r1)/rd;
    vec3 k2 = (-ro+r2)/rd;
    vec3 kmin = min(k1, k2);
    vec3 kmax = max(k1, k2);
    float ka = max(max(kmin.x, kmin.y), kmin.z);
    float kb = min(min(kmax.x, kmax.y), kmax.z);
    if (ka > kb)
    return -1;

    int side =
    abs(ka - k1.x) < 0.01 ? 4 :
    abs(ka - k1.y) < 0.01 ? 1 :
    abs(ka - k1.z) < 0.01 ? 0 :
    abs(ka - k2.x) < 0.01 ? 2 :
    abs(ka - k2.y) < 0.01 ? 3 :
    5;

    vec3 pos = ro + ka * rd;
    vec2 uv =
    side == 0 || side == 5 ? pos.xy :
    side == 1 || side == 3 ? pos.xz :
    pos.yz;
    int tile = side * 9 + int((uv.x + 1.0) * 3.0 / 2.0) + int((uv.y + 1.0) * 3.0 / 2.0) * 3;
    return tile;
}

ivec2 tileToVec(int tile)
{
    int side = tile / 9;
    ivec2 offset = ivec2(
    side == 0 || side == 5 ? 3 : side * 3 - 3,
    side == 0 ? 0 : side == 5 ? 6 :  3
    );
    tile -= 9 * side;
    return offset + ivec2(tile % 3, tile / 3);
}

void rotateSide(int side, in ivec2 fragCoord, inout vec4 fragColor)
{
    bool ccw = false;
    if (side > 5)
    {
        side -= 6;
        ccw = true;
    }

    // Rotate plane front
    for (int n = 0; n < 9; n++)
    {
        ivec2 targetTile = tileToVec(side * 9 + n);
        if (fragCoord == targetTile)
        {
            int m = (ccw ? 6 : 2);
            m = n == 1 ? (ccw ? 3 : 5) : m;
            m = n == 2 ? (ccw ? 0 : 8) : m;
            m = n == 3 ? (ccw ? 7 : 1) : m;
            m = n == 4 ? (ccw ? 4 : 4) : m;
            m = n == 5 ? (ccw ? 1 : 7) : m;
            m = n == 6 ? (ccw ? 8 : 0) : m;
            m = n == 7 ? (ccw ? 5 : 3) : m;
            m = n == 8 ? (ccw ? 2 : 6) : m;
            ivec2 sourceTile = tileToVec(side * 9 + m);
            fragColor = texelFetch(iChannel0, sourceTile, 0);
            return;
        }
    }

    // Rotate plane sides
    ivec4 s1, s2;
    if (side == 0) { s1 = ivec4(38, 29, 20, 11); s2 = ivec4(36, 27, 18, 9); }
    if (side == 1) { s1 = ivec4(0, 18, 45, 44); s2 = ivec4(6, 24, 51, 38); }
    if (side == 2) { s1 = ivec4(6, 27, 47, 17); s2 = ivec4(8, 33, 45, 11); }
    if (side == 3) { s1 = ivec4(8, 36, 53, 26); s2 = ivec4(2, 42, 47, 20); }
    if (side == 4) { s1 = ivec4(2, 9, 51, 35); s2 = ivec4(0, 15, 53, 29); }
    if (side == 5) { s1 = ivec4(24, 33, 42, 15); s2 = ivec4(26, 35, 44, 17); }

    ivec2 b = ivec2(s1.w, s2.w);
    for (int n = 0; n < 4; n++)
    {
        ivec2 a =    ivec2(s1.x, s2.x);
        a = n == 1 ? ivec2(s1.y, s2.y) : a;
        a = n == 2 ? ivec2(s1.z, s2.z) : a;
        a = n == 3 ? ivec2(s1.w, s2.w) : a;
        for (int m = 0; m < 3; m++)
        {
            ivec2 aa = ccw ? a : b;
            ivec2 bb = ccw ? b : a;

            ivec2 targetTile = ivec2(0);
            targetTile = m == 0 ? tileToVec(aa.x) : targetTile;
            targetTile = m == 1 ? tileToVec((aa.x + aa.y)/2) : targetTile;
            targetTile = m == 2 ? tileToVec(aa.y) : targetTile;
            if (fragCoord == targetTile)
            {
                ivec2 sourceTile = ivec2(0);
                sourceTile = m == 0 ? tileToVec(bb.x) : sourceTile;
                sourceTile = m == 1 ? tileToVec((bb.x + bb.y)/2) : sourceTile;
                sourceTile = m == 2 ? tileToVec(bb.y) : sourceTile;
                fragColor = texelFetch(iChannel0, sourceTile, 0);
                return;
            }
        }
        b = a;
    }
}



void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    bool init = iFrame < 2;
    vec3 from, at, hor, ver, up;
    vec3 froms;
    vec2 mouXY, mouZW;
    int side, tile;
    float lastTime;
    float angle;
    int action;

    ivec2 ifragCoord = ivec2(fragCoord);

    // Restore fragColor from Buf A texture by default
    fragColor = texelFetch(iChannel0, ifragCoord, 0);

    from = getVector(iChannel0, storeFrom);
    at = getVector(iChannel0, storeAt);
    hor = getVector(iChannel0, storeHor);
    ver = getVector(iChannel0, storeVer);
    up =  getVector(iChannel0, storeUp);
    froms = getVector(iChannel0, storeFroms);
    froms = getVector(iChannel0, storeFroms);

    mouXY = getVector(iChannel0, storeMouXY).xy;
    mouZW = getVector(iChannel0, storeMouZW).xy;

    vec3 v = getVector(iChannel0, storeSide);
    side = int(v.x);
    lastTime = v.y;
    angle = v.z;

    action = int(getVector(iChannel0, storeAction).x);

    // Init tiles
    if (fragCoord.y < 9.0)
    {
        if (init)
        {
            int side =
            fragCoord.y < 3.0 ? (fragCoord.x >= 3.0 && fragCoord.x <= 6.0 ? 0 : -1) :
            fragCoord.y < 6.0 ? (fragCoord.x >= 0.0 && fragCoord.x <= 12.0 ? int(floor(fragCoord.x / 3.0)) + 1 : -1) :
            fragCoord.y < 9.0 ? (fragCoord.x >= 3.0 && fragCoord.x <= 6.0 ? 5 : -1) : -1;
            if (side != -1)
            {
                fragColor =
                side == 0 ? vec4(0.8, 0.8, 0.0, 0.0) :		// Yellow
                side == 1 ? vec4(1.0, 0.0, 0.0, 0.0) :		// Red
                side == 2 ? vec4(0.0, 0.2, 0.9, 0.0) :		// Blue
                side == 3 ? vec4(0.9, 0.4, 0.0, 0.0) :		// Orange
                side == 4 ? vec4(0.0, 0.5, 0.1, 0.0) :		// Green
                vec4(1.0, 1.0, 1.0, 0.0); 		// White
            }
            else
            {
                fragColor = vec4(0.3, 0.3, 0.3, 1.0);
            }
        }
        else
        {
            if (action == 1)
            {
                rotateSide(side, ifragCoord, fragColor);
            }
            else
            {
                if (isKeyPressed(KEY_S))		// Shuffle
                {
                    rotateSide(int(mod(iTime * 17.17, 6.0)), ifragCoord, fragColor);
                }
            }
        }
    }

    // Camera calculation
    else if (ifragCoord.y < 10)
    {
        if (action == 1)
        {
            action = 0;
            side = -1;
        }
        if (init)
        {
            from = vec3(-3.0, 2.0, 2.0);
            at = vec3(0.0, 0.0, 0.0);
            hor = vec3(1.0, 0.0, 0.0);
            ver = vec3(0.0, 0.0, 1.0);
            up =  vec3(0.0, 0.0, 1.0);
            side = -1;
        }

        // iMouse:
        // - Only changes value when LMB is down
        // -  xy: current pixel coords (when LMB is down)
        // -  zw: last clicked position. Negated, except...
        //    - z: When LMB is down
        //    - w: When clicked (only first frame when LMB goes down)
        if (iMouse.z > 0. || init)		// Clicked
        {
            froms = from;
        }

        vec2 d = clamp((iMouse.xy - abs(iMouse.zw)) / iResolution.xy * acc, -0.1, 0.1);
        if(iMouse.z > 0. || init)		// Mouse button down
        {
            from = dst * normalize(from - hor * d.x - ver * d.y);
        }

        if (iMouse.z < 0. && mouZW.x > 0.) // Released
        {
            // Mouse button up and mouse didn't move far
            if (distance(iMouse.xy, abs(mouZW)) < 2.0 && side == -1)
            {
                vec2 p = iMouse.xy/iResolution.xy * 2.0 - 1.0;
                vec3 rd = normalize((at-from) + p.x * hor + p.y * ver);

                tile = getTile(from, rd);
                side = tile / 9;
                if (isKeyPressed(KEY_SHIFT))
                side += 6;
                lastTime = iTime;
                setVector(ifragCoord, fragColor, tileToVec(tile), vec3(1.0));
            }
        }
        if (side != -1)
        {
            float factor = (iTime - lastTime) / rotationTime;
            angle = (sin(factor * PI  - HALFPI)*0.5 + 0.5) * HALFPI;
            if (factor > 1.0)
            {
                action = 1; // rotate color info
            }
        }

        up = up * (1.0 - SMOOTHUP);
        if (abs(ver.x) > abs(ver.y) && abs(ver.x) > abs(ver.z))
        up.x += SMOOTHUP * sign(ver.x);
        else
        if (abs(ver.y) > abs(ver.z))
        up.y += SMOOTHUP * sign(ver.y);
        else
        up.z += SMOOTHUP * sign(ver.z);
        up = normalize(up);
        vec3 look = at - from;
        float dist = length(look);
        float aper = 35.0;  // degrees
        float hsize = tan(aper*PI/180.0)*dist;
        float vsize = hsize * iResolution.y /iResolution.x;
        hor = normalize(cross(look, up)) * hsize;
        ver = normalize(cross(hor, look)) * vsize;

        setVector(ifragCoord, fragColor, storeFrom, from);
        setVector(ifragCoord, fragColor, storeAt, at);
        setVector(ifragCoord, fragColor, storeHor, hor);
        setVector(ifragCoord, fragColor, storeVer, ver);
        setVector(ifragCoord, fragColor, storeUp, up);

        setVector(ifragCoord, fragColor, storeMouXY, vec3(iMouse.x, iMouse.y, 0.0));
        setVector(ifragCoord, fragColor, storeMouZW, vec3(iMouse.z, iMouse.w, 0.0));
        setVector(ifragCoord, fragColor, storeSide, vec3(side, lastTime, angle));
        setVector(ifragCoord, fragColor, storeAction, vec3(action, 0.0, 0.0));
    }
}
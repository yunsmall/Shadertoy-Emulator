vec3 render( in vec3 ro, in vec3 rd )
{
    vec3 v = getVector(iChannel0, storeSide);
    int side = int(v.x);
    float ang = v.z;
    vec3 r1, r2, r1r, r2r;
    vec3 k3, k4;
    vec3 ro1, rd1;
    mat3 rotmat;
    mat3 rotmatn;

    if (side > 5)
    {
        side -= 6;
        ang =- ang;
    }

    r1 = vec3(side == 4 ? -0.333 : -1.0, side == 1 ? -0.333 : -1.0, side == 0 ? -0.333 : -1.0);
    r2 = vec3(side == 2 ? +0.333 : +1.0, side == 3 ? +0.333 : +1.0, side == 5 ? +0.333 : +1.0);

    if (side > -1)		// Rotating plane
    {
        r1r = vec3(side == 4 ? -0.333 : +1.0, side == 1 ? -0.333 : +1.0, side == 0 ? -0.333 : +1.0);
        r2r = vec3(side == 2 ? +0.333 : -1.0, side == 3 ? +0.333 : -1.0, side == 5 ? +0.333 : -1.0);
        bool inv = side == 1 || side == 2 || side == 5;
        float cs = cos(inv ? ang : -ang);
        float sn = sin(inv ? ang : -ang);

        if (side == 2 || side == 4)
        {
            rotmat = mat3(1.0, 0.0, 0.0,
            0.0, cs,  -sn,
            0.0, sn, cs);
            rotmatn = mat3(1.0, 0.0, 0.0,
            0.0, cs,  sn,
            0.0, -sn, cs);
        }
        else
        if (side == 1 || side == 3)
        {
            rotmat = mat3(cs, 0.0, -sn,
            0.0, 1.0, 0.0,
            sn, 0.0, cs);
            rotmatn = mat3(cs, 0.0, sn,
            0.0, 1.0, 0.0,
            -sn, 0.0, cs);
        }
        else
        {
            rotmat = mat3(cs, -sn, 0.0,
            sn, cs, 0.0,
            0.0, 0.0, 1.0);
            rotmatn = mat3(cs, sn, 0.0,
            -sn, cs, 0.0,
            0.0, 0.0, 1.0);
        }
    }

    vec3 finalColor=vec3(0.0);
    float frac = 1.0;

    for (int bounce=0; bounce < MAXBOUNCE; bounce++)
    {
        vec3 k1 = (-ro+r1)/rd;
        vec3 k2 = (-ro+r2)/rd;
        vec3 kmin = min(k1, k2);
        vec3 kmax = max(k1, k2);
        float ka = max(max(kmin.x, kmin.y), kmin.z);
        float kb = min(min(kmax.x, kmax.y), kmax.z);
        float k5 = ka < kb ? ka : 10000.0;
        float k6 = 10000.0;

        if (side > -1)		// Rotating plane
        {
            vec3 ro1 = ro * rotmat;
            vec3 rd1 = rd * rotmat;

            k3 = (-ro1+r1r)/rd1;
            k4 = (-ro1+r2r)/rd1;
            kmin = min(k3, k4);
            kmax = max(k3, k4);
            float kc = max(max(kmin.x, kmin.y), kmin.z);
            float kd = min(min(kmax.x, kmax.y), kmax.z);
            k6 = kc < kd ? kc : 10000.0;
        }
        float k = min(k5, k6);

        vec3 localColor;
        if (k < 10000.0 && k > 0.1)  // Inside
        {
            vec3 s = ro + k * rd;
            vec3 s1 = s;
            if (k == k6)
            {
                k1 = k4;
                k2 = k3;
                s1 *= rotmat;
            }
            int plane =
            k == k1.x ? 4 :
            k == k1.y ? 1 :
            k == k1.z ? 0 :
            k == k2.x ? 2 :
            k == k2.y ? 3 :
            5;

            vec3 n = vec3 (
            plane == 2 ? 1.0 : plane == 4 ? -1.0 : 0.0,
            plane == 3 ? 1.0 : plane == 1 ? -1.0 : 0.0,
            plane == 5 ? 1.0 : plane == 0 ? -1.0 : 0.0
            );
            vec3 nu = vec3 (
            plane == 1 ? 1.0 : plane == 3 ? -1.0 : 0.0,
            plane == 0 || plane == 2 || plane == 5 ? 1.0 : plane == 4 ? -1.0 : 0.0,
            0.0
            );
            vec3 nv = vec3 (
            plane == 0 ? 1.0 : plane == 5 ? -1.0 : 0.0,
            0.0,
            plane >= 1 && plane <= 4 ? 1.0 : 0.0
            );
            if (k == k6)
            {
                n = n * rotmatn;
                nu = nu * rotmatn;
                nv = nv * rotmatn;
            }

            // range [0..3, 0..3]
            vec2 uv = vec2(
            plane == 0 ? s1.y : plane == 1 ? s1.x : plane == 2 ? s1.y :
            plane == 3 ? -s1.x : plane == 4 ? -s1.y : s1.y,
            plane == 0 ? s1.x : plane == 1 ? s1.z : plane == 2 ? s1.z :
            plane == 3 ? s1.z : plane == 4 ? s1.z : -s1.x) * 1.5 + 1.5;
            // offset on the Buf A texture
            vec2 offs = vec2(
            plane == 0 || plane == 5 ? 3.0 : float(plane - 1) * 3.0,
            plane == 0 ? 0.0 : plane == 5 ? 6.0 : 3.0
            );

            // range [-1..+1, -1..+1]
            vec2 f = mod(uv, 1.0) * 2.0 - 1.0;

            // When rotating the inner guts are visible... make them black
            bool inside = max(max(abs(s1.x), abs(s1.y)), abs(s1.z)) < 0.99;

            float p = 0.0;
            if (!inside)
            {
                // Shape of the color strips
                p = smoothstep(0.0, 0.1, max(0.0, 0.35 - length(vec2(pow(abs(f.x), 6.0), pow(abs(f.y), 6.0)))));
                float q = sin(p * PI); // edge

                if (f.x < 0.0) nu = -nu;
                if (f.y < 0.0) nv = -nv;

                vec2 g = smoothstep(0.8, 1.0, abs(f)) * 0.495 + f * q * 0.4;

                float h = 0.495 + 0.5 - max(g.x, g.y);
                n = normalize(n * h + nu * g.x + nv * g.y);
            }

            // Light vector
            vec3 lvec = normalize(s - ro);

            // Mirror vector
            float a = -dot(n, rd);
            rd = rd + 2.0 * a * n;
            ro = s;

            // lighting intensity
            float cc = max(0.2, -dot(n, lvec));

            // fake specular spot
            float cc1 = pow(cc, 100.0);
            float cc2 = pow(cc, 3000.0);

            vec4 tileColor = inside ? vec4(0.0) : texture(iChannel0, (floor(offs + uv) + 0.5) / iResolution.xy);
            localColor = tileColor.rgb * p * cc + vec3(cc2, cc2 * 0.4, cc2 * 0.1) + vec3(cc1) * 0.8;

            //localColor = n * 0.5 + 0.5;
        }
        else
        {
            rd = sin(rd * 5.0 + iTime * 0.11);
            localColor = rd * 0.5 + 0.5;
            ro = rd * 20.0;
            rd = -rd;
        }
        finalColor += frac * localColor;
        frac *= 0.05;
    }
    return finalColor;


}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 from = getVector(iChannel0, storeFrom);
    vec3 at =  getVector(iChannel0, storeAt);
    vec3 hor = getVector(iChannel0, storeHor);
    vec3 ver = getVector(iChannel0, storeVer);
    vec3 up =  getVector(iChannel0, storeUp);
    float time = 15.0 + iTime;

    // render
    float res = 1.0 / float(ANTIALIAS);
    vec3 col = vec3(0.0);
    for (float sx = -0.5; sx < 0.5; sx += res)
    for (float sy = -0.5; sy < 0.5; sy += res)
    {
        vec2 p = (fragCoord + vec2(sx, sy))/iResolution.xy * 2.0 - 1.0;
        vec3 rd = normalize((at-from) + p.x * hor + p.y * ver);
        col += render( from, rd );
    }

    col = pow( col / float(ANTIALIAS * ANTIALIAS), vec3(0.4545) );

    //col = 0.5 * texelFetch(iChannel0, ivec2(fragCoord.xy / 10.0), 0).rgb + 0.5* col;

    fragColor = vec4( col.rgb, 1.0 );

}
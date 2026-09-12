#ifdef MC


int gFrame=0;

//--------------------
//porting of "Marching Cubes" algorithm by Paul Bourke (1994)
//http://paulbourke.net/geometry/polygonise/
struct TRIANGLE {
    vec3 p[3];
} ;


struct GRIDCELL{
    vec3 p[8];
    float val[8];
} ;

const vec3 VertexOffset[8] =vec3[8]
(
vec3(0,0,0), vec3(1,0,0),vec3(1,1,0),vec3(0,1,0),
vec3(0,0,1), vec3(1,0,1),vec3(1,1,1),vec3(0,1,1)
);


//lookup tables retrieved from BufferA
#define edgeTable(i) int(texelFetch(confChannel, ivec2(i,30),0).x)
#define triTableRow(i) ivec4(texelFetch(confChannel, ivec2(i,31),0))
#define triTableVal(tt,j) int((tt[j>>2]&(15*(1<<((j&3)*4))))>>((j&3)*4))

const int  vertexTable[24] =int[24](
0,1,   1,2,  2,3,   3,0,
4,5,   5,6,  6,7,   7,4,
0,4,   1,5,  2,6,   3,7);

/*
   Linearly interpolate the position where an isosurface cuts
   an edge between two vertices, each with their own scalar value
*/
float  VertexWeight(float isolevel,float valp1, float valp2)
{
    return  (isolevel - valp1) / (valp2 - valp1);
}

//input: isolevel value at 8 cube vertexs and isolevel threshold
//output: number of triangles (-1= outside) and list of triangles (up to 5 in worst case)
uvec4  Polygonise(inout GRIDCELL grid,float isolevel,inout TRIANGLE[5] triangles,sampler2D confChannel)
{

/*
      Determine the index into the edge table which
      tells us which vertices are inside of the surface
   */
    int cubeindex = 0;
    for(int i=gFrame;i<8;i++) if (grid.val[i] < isolevel) cubeindex |= 1<<i;

/* Cube is entirely in/out of the surface -1=IN, 0=OUT */
    int e=edgeTable(cubeindex);
    if ( e<= 0) return uvec4(e);

/* Find the vertices where the surface intersects the cube */
    vec3 vertlist[12];
    float vertW[12];


    for(int i=0;i<12;i++)
    if ((e & (1<<i))>0)  {
        vertW[i]= VertexWeight(isolevel,grid.val[vertexTable[i*2]], grid.val[vertexTable[i*2+1]]);

        vertlist[i]= mix( grid.p[vertexTable[i*2]], grid.p[vertexTable[i*2+1]],vertW[i]);
    }
/* Create the triangle */
    uvec4 tridata=uvec4(0u); //x=number of triangles, yzw= tritable

    ivec4 ttr=triTableRow(cubeindex);
    for (int i=gFrame;triTableVal(ttr,i)!=15 && i<15;i+=3) {

        for(int j=gFrame;j<3;j++)   {
            uint k =uint(triTableVal(ttr,(i+j)));
            int idx =(i+j);
            if(idx<8) tridata.y +=  k*( 1u<<(idx*4));
            else tridata.z += k*( 1u<<(idx*4-32));

            tridata.w+=  uint( floor(vertW[k]*4. )  )
            *( 1u<<(idx*2));
            triangles[tridata.x].p[j] = vertlist[k];
        }

        tridata.x++;
    }

    return uvec4(tridata);
}
//-------------------------------------
//Iq
vec2 boxIntersection( in vec3 ro, in vec3 rd, in vec3 rad)
{
    vec3 m = 1.0/rd;
    vec3 n = m*ro;
    vec3 k = abs(m)*rad;
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;

    float tN = max( max( t1.x, t1.y ), t1.z );
    float tF = min( min( t2.x, t2.y ), t2.z );

    if( tN>tF || tF<0.0) return vec2(-1.0);

    //vec3 normal = -sign(rd)*step(t1.yzx,t1.xyz)*step(t1.zxy,t1.xyz);

    return vec2( tN, tF );
}


// triangle degined by vertices v0, v1 and  v2
vec3 triIntersect( in vec3 ro, in vec3 rd, in vec3 v0, in vec3 v1, in vec3 v2 )
{
    vec3 v1v0 = v1 - v0;
    vec3 v2v0 = v2 - v0;
    vec3 rov0 = ro - v0;
    vec3  n = cross( v1v0, v2v0 );

    vec3  q = cross( rov0, rd );
    float d = 1.0/dot( rd, n );
    float u = d*dot( -q, v2v0 );
    float v = d*dot(  q, v1v0 );
    float t = d*dot( -n, rov0 );
    if( u<0.0 || u>1.0 || v<0.0 || (u+v)>1.0 ) t = -1.0;
    return vec3( t, u, v );
}
#endif
//--------------------------
vec2 max24(vec2 a, vec2 b, vec2 c, vec2 d) {
    return max(max(a, b), max(c, d));
}

float lightLevelCurve(float t) {
    t = mod(t, 1200.);
    return 1. - ( smoothstep(400., 700., t) - smoothstep(900., 1200., t));
}

vec3 lightmap(in vec2 light) {
    light = 15. - light;
    if(load(_torch).r>0.5) light.t=13.;

    return clamp(mix(vec3(0), mix(vec3(0.11, 0.11, 0.21), vec3(1), lightLevelCurve(load(_time).r)), pow(.8, light.s)) + mix(vec3(0), vec3(1.3, 1.15, 1), pow(.75, light.t)), 0., 1.);

}

float vertexAo(float side1, float side2, float corner) {
    return 1. - (side1 + side2 + max(corner, side1 * side2)) / 5.0;
}

float opaque(float id) {
    //return id > .5 ? 1. : 0.;
    return  id != 0. && id!= 12. && id!= 26. ? 1. :0.;
}

vec3 calcOcclusion(vec3 r,vec3 n, vec2 uv,voxel vox) {
    #ifndef OCCLUSION
    return vec3(vox.light , .75);
    #else
 	//tangents:
    vec3 s = vec3(step(.1,abs(n.y)), 1.- step( .1, abs(n.y)) ,0.                  );
    vec3 t = vec3(step(.1,abs(n.z)), 0.                   ,1.- step(.1,abs(n.z)  ));

    //neightbours vector
    //v[0],v[1],v[2]
    //v[3],v[4],v[5]
    //v[6],v[7],v[8]
    voxel v[9];

    for (int i =-1; i <=1; i++) {
        for (int j =-1; j <=1  ; j++) {
            getVoxel(r +n + s* float(i)+t*float(j),v[4+ i+3*j +min(iFrame,0) ] ,3 );
        }
    }

    float aom, ao[4];
    vec2 lightm,light[4];
    for(int i=0;i<=3;i++){

        ivec4 ids;
        if(i==0) ids=ivec4(6,7,3,4);
        if(i==1) ids=ivec4(7,8,4,5);
        if(i==2) ids=ivec4(3,4,0,1);
        if(i==3) ids=ivec4(4,5,1,2);
        light[i +min(iFrame,0)] =max24(v[ids.x].light, v[ids.y].light, v[ids.z].light, v[ids.w].light);
    }
    lightm = mix(mix(light[2], light[3], uv.x), mix(light[0], light[1], uv.x), uv.y);

    for(int i=0;i<=3 ;i++){

        ivec3 ids;
        if(i==0) ids=ivec3(7,3,6);
        if(i==1) ids=ivec3(7,5,8);
        if(i==2) ids=ivec3(1,3,0);
        if(i==3) ids=ivec3(1,5,2);;
        ao[i] = vertexAo(opaque(v[ids.x].id), opaque(v[ids.y].id), opaque(v[ids.z].id));
    }
    aom = mix(mix(ao[2], ao[3], uv.x), mix(ao[0], ao[1], uv.x), uv.y);
    if(opaque(v[4].id)>0.) {aom*=0.75;}


    return vec3(lightm , aom);
    #endif

}

// RENDERING

vec3 rayDirection(vec2 angle, vec2 uv, vec2 renderResolution){
    vec3 cameraDir = vec3(sin(angle.y) * cos(angle.x), sin(angle.y) * sin(angle.x), cos(angle.y));
    vec3 cameraPlaneU = vec3(normalize(vec2(cameraDir.y, -cameraDir.x)), 0);
    vec3 cameraPlaneV = cross(cameraPlaneU, cameraDir) * renderResolution.y / renderResolution.x;
    return normalize(cameraDir + uv.x * cameraPlaneU + uv.y * cameraPlaneV);

}

struct rayCastResults {
    bool hit;
    vec3 rayPos;
    vec3 mapPos;
    vec3 normal;
    vec2 uv;
    #ifdef SUBTEXTURE
    vec2 uv_txt;
    #endif
    float dist;
    voxel vox;
    float water;
    float fog;
    bool grass;
    bool mirror;
    vec3 color;
    float fresnel;

};
mat3 rotate(float theta,int axis) {
    float c = cos(theta);
    float s = sin(theta);

    if (axis==1) return mat3(
    vec3(1, 0, 0),
    vec3(0, c, s),
    vec3(0, -s, c)
    );
    if (axis==2) return mat3(
    vec3(c, 0, s),
    vec3(0, 1, 0),
    vec3(-s, 0, c)
    );
    return mat3(
    vec3(c, s, 0),
    vec3(-s, c, 0),
    vec3(0, 0, 1)
    );
}

//From https://github.com/hughsk/glsl-hsv2rgb
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec4 sun(){
    float t = load(_time).r;
    float sunAngle = (t * PI * 2. / 1200.) + PI / 4.;
    const float risingAngle=PI/6.;
    return  vec4(cos(sunAngle)*sin(risingAngle), cos(sunAngle)*cos(risingAngle), sin(sunAngle),lightLevelCurve(t));
}

#ifdef CLOUDS
float fogDensity(vec3 p) {

    float density = 2. - abs(p.z - 80.)*1.7;
    //density += mix(0., 40., pow(.5 + .5 * snoise(p.xy /557. + vec2(0.576, .492)), 2.)) * snoise(p / 31.51 + vec3(0.981, .245, .497));
    density += mix(0., 30., pow(.2 + 1.5 * snoise((p.xy +iTime)/207. + vec2(0.576 +iTime/200., .492)), 2.)) * snoise((p +iTime) / 30.99 + vec3(0.981, .245, .497 +iTime/2000.));

    return clamp(density ,0.,50.);

}
void applyFog( inout vec3  rgb,       // original color of the pixel
in float distance ) // camera to point distance
{

    float fogAmount = 1.0 - exp( -distance*0.015 );
    vec3  fogColor  = vec3(0.5,0.6,0.7)*clamp(sun().w,.1,1.);
    rgb= mix( rgb, fogColor, fogAmount );
}
#endif

//-------------

float noise(in vec2 p) {
    vec2 F = floor(p), f = fract(p);
    f = f * f * (3. - 2. * f);
    return mix(
        mix(hash2(F), 			 hash2(F+vec2(1.,0.)), f.x),
        mix(hash2(F+vec2(0.,1.)), hash2(F+vec2(1.)),	  f.x), f.y);
}

//GRASS ADAPTED FROM POLYANKA by W23
//https://www.shadertoy.com/view/MdsGzS
#ifdef GRASS_DETAIL
const int c_grassmarch_steps = 48;
const float c_gscale = 37.;
const float c_gheight = 0.4;
//const float c_rgslope = 2. / (c_gscale * c_gheight);


vec2 noise2(in vec2 p) {
    vec2 F = floor(p), f = fract(p);
    f = f * f * (3. - 2. * f);
    return mix(
        mix(hash22(F), 			  hash22(F+vec2(1.,0.)), f.x),
        mix(hash22(F+vec2(0.,1.)), hash22(F+vec2(1.)),	f.x), f.y);
}

float fnoise(in vec2 p) {
    return .5 * noise(p) + .25 * noise(p*2.03) + .125 * noise(p*3.99);
}

vec2 wind_displacement(in vec2 p) {
    return noise2(p*.1+iTime)/100. - 0.005;
}

float grass_height(in vec3 p,float gheight) {
    float base_h= 0.15;
    float depth = 1. - (base_h - p.z) / gheight;
    vec2 gpos = (p.xy  + depth * wind_displacement(p.xy));
    return base_h - noise(gpos * c_gscale) * gheight;
}


struct xs_t {
    bool hit;
    vec3 pos;
    float occlusion;
    float dist;
};


xs_t trace_grass(vec3 o, vec3 d,vec3 mapPos,float gheight) {
    bool hit=false;
    float L = .005;
    //float Lmax= 1.8;
    for (int i = 0; i < c_grassmarch_steps; ++i) {
        vec3 pos = o + d * L ;
        float h = grass_height(pos +mod(mapPos,10.),gheight);
        float dh = pos.z - h;
        if (dh < .005) {hit=true; break;}
        L += dh * 2. / (c_gscale * gheight);
        vec3  dist = abs(pos-.5);
        //if (L > Lmax) break;
        if (max(dist.z, max(dist.x,dist.y))>.5) break;
    }
    vec3 pos = o + d * L;
    float occlusion = 1. - 2.*(0. - pos.z) / gheight;
    return xs_t(hit, pos + mod(mapPos,99.),  (hit)?1.:min(1.,occlusion),L);
}

vec3 shade_grass(in xs_t xs) {

    vec2 typepos = xs.pos.xy + wind_displacement(xs.pos.xy);
    float typemask1 = fnoise(2.5*typepos);
    float typemask2 = pow(fnoise(.4*typepos), 3.);
    float typemask3 = step(.71,fnoise(.8*typepos));
    vec3 col1 = vec3( 106./255., 170./255.,  64./255.);
    vec3 col2 = vec3(.7, .73, .3)*.3;
    vec3 col3 = vec3(1., 1., .1);
    vec3 col4 = vec3(1., .4, .7);
    vec3 color = mix(mix(mix(col1, col2, typemask1),
                         col3, typemask2), col4, typemask3) *.8;
    color *= xs.occlusion;
    return color;
}
#endif
//-----------------------------
//water reflection: https://www.shadertoy.com/view/MdXGW2
#define BUMPFACTOR 0.3
#define EPSILON 0.1

float waterHeightMap( vec2 pos ) {
    return 0.9+.2*noise(pos +iTime/3.);
    //better but requires more compilation time
    //return 0.9+.1*snoise(vec3(pos,iTime/3.));
}

float fresnelR(vec3 d, vec3 n)
{
    //float a = clamp(1.0-dot(n,-d), 0.0, 1.0);
    // return clamp(exp((5.0*a)-5.0), 0.0, 1.0);
    return pow( clamp( 1.0 + dot(d,n), 0.0, 1.0 ), 5.0 );
}
//------------------------*/
vec4 VoxelHitPos(vec3 pos, vec3 ro, vec3 rd){
    vec3 ri = 1.0/rd;
    vec3 rs = sign(rd);
    vec3 mini = (pos-ro + 0.5 - 0.5*vec3(rs))*ri;
    float t=  max ( mini.x, max ( mini.y, mini.z ) );
    return vec4(t*rd+ro,t);
}

#ifdef SUBVOXEL
rayCastResults raySubCast(vec3 rayPosOrig, vec3 rayDir, int shape,float rotation,vec3 seed){

    rayCastResults  res;


    vec3 c=vec3(.5);
    float theta1= PI/2.*floor(mod(rotation,4.));
    rayPosOrig = rotate( theta1,3) *(rayPosOrig-c) +c;
    rayDir= rotate( theta1,3)*rayDir;
    float theta2= PI/2.*(floor(rotation/4.));
    rayPosOrig = rotate( theta2,2) *(rayPosOrig-c)+c;
    rayDir= rotate( theta2,2)*rayDir;

    vec3 ro = (rayPosOrig) *N_SUBVOXEL;

    //if(abs(ro.x -N/2.)>N/2. ||abs(ro.y -N/2.)>N/2. ||abs(ro.y -N/2.)>N/2.)return vec4(0.,0.,0.,1.);

    vec3 ri = 1.0/rayDir;
    vec3 rs = sign(rayDir);
    vec3 pos = floor(ro-rayDir*0.002);
    vec3 dis = (pos-ro + 0.5 + rs*0.5) * ri;

    res.hit=false;
    vec3 mm = vec3(0.);
    float t=0.;

    for( int i=0; i<int(N_SUBVOXEL)*3; i++ )
    {
        if(i>=0){
            mm = step(dis.xyz, dis.yzx) * step(dis.xyz, dis.zxy);
        }
        dis += mm * rs * ri;
        pos += mm * rs;

        //if( sdBox( ro+t*rayDir-vec3(N_SUBVOXEL/2.),vec3(N_SUBVOXEL/2.) )>.05) {res.hit=false; break;}

        //float timestep= floor(mod(iTime,N_SUBVOXEL));
        //SHAPES

        //SINGLE BLOCK
        //if( sdBox( pos-vec3(x,x,x) +rs*0.001 ,vec3(.5,.5,.5) )<.01) {res.hit=true; break;}


        if(shape==1){// POLE
                     if( sdBox( pos-vec3(2.,2.,2.) ,vec3(.5,.5,2.5) )<.001) {res.hit=true; break;}

        }else if(shape==2){//STEP 1
                           if(sdBox( pos-vec3(2.,2.,0.)  ,vec3(2.5,2.5,0.5) )<.001) {res.hit=true; break;}

        }else if(shape==3){//STEP 2
                           if( sdBox( pos-vec3(2.,2.,0.) ,vec3(2.5,2.5,1.5) )<.001) {res.hit=true; break;}

        }else if(shape==4){//FENCE 1
                           if( sdBox( pos-vec3(2.,2.,2.)  ,vec3(.5,.5,2.5) )<.001) {res.hit=true; break;}
                           if( sdBox( pos-vec3(2.,2.,4.)  ,vec3(.5,2.5,.5) )<.001) {res.hit=true; break;}

        }else if(shape==5){//FENCE 2
                           if( sdBox( pos-vec3(2.,2.,2.) ,vec3(.5,.5,2.5) )<.001) {res.hit=true; break;}
                           if( sdBox( pos-vec3(1.,2.,4.)  ,vec3(1.5,.5,.5) )<.001) {res.hit=true; break;}
                           if( sdBox( pos-vec3(2.,1.,4.)  ,vec3(.5,1.5,.5) )<.001) {res.hit=true;break;}

        }else if(shape==6){//SLOPE 1
                           if( dot(pos,  vec3(0.,sqrt(2.),sqrt(2.))) -6. <0.001
                           && sdBox( pos-vec3(2.,2.,2.),vec3(2.5,2.5,2.5) )<.001  ) {res.hit=true; break;}

        }else if(shape==7){//PANEL
                           if(sdBox( pos-vec3(0.,2.,2.)  ,vec3(.5,2.5,2.5) )<.001) {res.hit=true; break;}

        }
        #ifdef TREE_DETAIL
        else if(shape==8){//TREE W LEAFS

    if( sdCross( pos-vec3(2.,2.,2.)  ,vec3(.5,.5,1.5) )<.001) {res.hit=true; res.vox.id=10.; break;}
    vec3 applePos= vec3(1.,1.,1.);//floor(hash33(seed)*5.);
    if( sdBox( pos-applePos  ,vec3(.5,.5,.5) )<.01 // && hash13(seed)<.95
    ){res.hit=true; res.vox.id=14.; break;}

    if( sdBox( pos-vec3(2.,2.,2.)  ,vec3(2.5,2.5,2.5) )<.001 && hash13(floor(pos)+seed+.5 )  >.75){res.hit=true; res.vox.id=11.; break;}

    //
    }else if(shape==9){//TRUNK
    vec3 p=pos-vec3(2.,2.,2.);
    //p= vec3(abs(p.x)+abs(p.y),max(p.x,p.y),p.z);
    if(sdBox( p ,vec3(1.5,1.5,2.5) )<.001){res.hit=true; res.vox.id=10.; break;}

    }
    #endif
	}


    if(res.hit){
        res.normal = - mm*rs;
        vec4 hitPos=VoxelHitPos(pos,ro,rayDir);
        res.dist=hitPos.a/N_SUBVOXEL;
        vec3 xyz = hitPos.xyz - pos;
        res.uv = vec2( dot(mm.yzx, xyz), dot(mm.zxy, xyz) );
        if(abs(mm.x)>0.) res.uv=res.uv.yx; //invert xz
        //relative to absolute normals:
        res.normal  = rotate( -theta2,2) * rotate(- theta1,3) *res.normal;
    }
    return res;
}
#endif


vec3    g_n;
vec2    g_uv;

rayCastResults rayCast(vec3 rayPos0, vec3 rayDir,int maxRayDist,vec4 range,int rayType) {

    voxel vox;
    vox.id=0.;
    float waterDist=0.;
    float fog=0.;
    rayCastResults res;
    res.hit = false;
    res.color=vec3(-1.);
    res.fresnel=0.;
    res.mirror=false;
    rayCastResults subRes;
    subRes.hit=false;

    vec3 raySign= sign(rayDir);
    vec3 rayInv = 1./rayDir;
    vec3 rayPos=rayPos0;

    vec3 mapPos=floor(rayPos);
    if ( rayPos.z >= heightLimit_B && rayDir.z<0.){

        //MAP RAY FROM ABOVE
        float nstep= (rayPos.z - heightLimit_B)*rayInv.z;
        mapPos = floor(rayPos-rayDir *nstep+ raySign*0.001);
    }
    vec3 sideDist = (mapPos-rayPos + 0.5 + sign(rayDir)*0.5) *rayInv;
    vec3 mask=vec3(0.);


    //vec3 offset = floor(vec3(load(_pos).xy, 0.));
    voxel currentVoxel;
    getCVoxel( mapPos,currentVoxel,3);
    vec3 hitWater = (currentVoxel.id==12.? rayPos: vec3(0.));
    bool xRay=(currentVoxel.id!=0. && currentVoxel.id!=12.);

    for (int i = 0; i < 1000; i++) {

        if(i>0){
            mask = step(sideDist.xyz, sideDist.yzx) * step(sideDist.xyz, sideDist.zxy);

        }
        sideDist += mask *  raySign *rayInv;
        mapPos += mask *  raySign;

        if ( mapPos.z < 0. ) break;
        if ( mapPos.z >= heightLimit_B && rayDir.z > 0.)  break;

        getVoxel( mapPos, vox ,3 );

        //GRASS
        #ifdef  GRASS_DETAIL
        if(vox.id==0. && vox.life>0. && rayType==1 ){
            vec4 vd =VoxelHitPos(mapPos,rayPos,rayDir);
            res.rayPos= vd.xyz;
            res.dist=vd.a;
            vec3 relativePos = res.rayPos -mapPos;

            float grass = c_gheight*vox.life;
            xs_t xs = trace_grass(relativePos,rayDir,mapPos,grass);

            if (xs.hit ) {

                //color = mix(color, c_skycolor, smoothstep(c_maxdist*.35, c_maxdist, xs.l));
                res.hit = true;
                res.vox=vox;
                res.grass=true;
                res.color=shade_grass(xs);
                res.mapPos = mapPos;
                res.water =waterDist;
                res.fog=fog;
                res.normal = vec3(0,0,1);
                res.dist+=  xs.dist ;
                res.rayPos += rayDir * xs.dist ;
                return res;
            }

        }
        #endif

        #ifdef SUBVOXEL
        if(vox.shape!=0 && vox.id!=0. ){
            //SUB VOXEL

            vec3 hitVoxelPos = VoxelHitPos(mapPos,rayPos,rayDir).xyz;

            if( sdBox( mapPos+vec3(.5) -rayPos,vec3(.5,.5,.5) )<.001) hitVoxelPos=rayPos;
            float rotation= vox.rotation;

            subRes = raySubCast( hitVoxelPos - mapPos ,  rayDir, vox.shape,rotation,mapPos);
            if(subRes.hit && vox.id!=12.) {
                res.hit = true;
                if(subRes.vox.id!=0.) vox.id=subRes.vox.id;
                break;
            }
            else if(vox.id==12. && subRes.hit && rayType!=3) {
                //nothing to do
            }
            else {vox.id=0.;res.hit = false;}
        }

        #endif
#ifdef MC
         if(vox.surface!=0. && rayType==1){
            gFrame=min(iFrame,0);
            vec3 hitVoxelPos = VoxelHitPos(mapPos,rayPos,rayDir).xyz;
            GRIDCELL g;
            float csz=1.;
            float mcid=0.;
            bool surface=false;
            for(int id=0;id<8;id++)
            {
                g.p[id]=mapPos+  VertexOffset[id]*csz;
                voxel vt;
                getCVoxel(g.p[id],vt,3 );
                if(vt.id==3.) vt.id=4.;
                mcid =max(mcid,vt.id);
                g.val[id]= vt.id!=0.?1.:-1.;
                surface = surface || ( g.val[id]*g.val[0]<0.);
            }

            if(surface ){

                TRIANGLE[5] triangles;

                //calculate vertexes & triangles (requires buffer A and B)

                uvec4 tridata = Polygonise(g,0.,triangles,iChannel0);

                int ntriangles=int(tridata.x);
                float t = 1000.0;
                for(int i=min(iFrame,0);i<ntriangles;i++) {
                    vec3 tri =triIntersect( hitVoxelPos,rayDir,triangles[i].p[0],triangles[i].p[1],triangles[i].p[2]);
                    if(tri.x>0.  && tri.x <t) {
                        t=tri.x;
                        g_n=-normalize(cross(triangles[i].p[1]-triangles[i].p[0],triangles[i].p[2]-triangles[i].p[0]));
                        g_uv= tri.yz;
                    }
                }
                if(t< 1000. ) {

                    subRes.hit = true;
                    subRes.mapPos = mapPos;
                    subRes.normal = g_n;
                    subRes.uv=g_uv;
                    subRes.rayPos = hitVoxelPos + rayDir*t;
                    subRes.dist = length(rayPos0 - subRes.rayPos);
                    vox.id=mcid;
                    subRes.vox=vox;
                    subRes.color = getTexture(mcid, g_uv).rgb *(.7 - .3*dot( sun().xyz,g_n));;
                    subRes.water =waterDist;
                    subRes.fog=fog;
                    subRes.grass=true;
                    return subRes;

                    //res.hit = true;
                    //break;
                }else vox.id=0.;
            }

        }
        #endif
        if(vox.id==14. &&rayType!=3){ //&& length(rayPos-mapPos -vec3(0.,0.,1.))<=6.){
                                      //MIRROR

                                      vec3 endRayPos = VoxelHitPos(mapPos,rayPos,rayDir).xyz;
                                      rayDir*= (vec3(1.) - 2.* mask);
                                      rayDir=normalize(rayDir);rayInv=1./rayDir;raySign= sign(rayDir);

                                      sideDist = (mapPos-endRayPos + 0.5 + raySign*0.5) /rayDir;
                                      vox.id=0.;
                                      res.mirror=true;
                                      rayPos=endRayPos;
                                      continue;
        }
        if(vox.id==12.  ){ //vox.life < WATER && vox.life>0.){
                           //ENTERING WATER
                           if(hitWater.z<1.) {

                               // deviate ray xy if intercept water NOT EXACT
                               vec3 endRayPos = VoxelHitPos(mapPos,rayPos,rayDir).xyz;
                               vec3 n=mask;
                               if(subRes.hit) {
                                   endRayPos+=rayDir * subRes.dist;
                                   n=subRes.normal;
                               }
                               hitWater=endRayPos;

                               if(abs(n.z)>0.) {
                                   vec2 coord = hitWater.xy;
                                   vec2 dx = vec2( EPSILON, 0. );
                                   vec2 dy = vec2( 0., EPSILON );
                                   float bumpfactor = BUMPFACTOR ;//* (1. - smoothstep( 0., BUMPDISTANCE, dist) );

                                   vec3 normal = vec3( 0., 0., 1. );
                                   normal.x = -bumpfactor * (waterHeightMap(coord + dx) - waterHeightMap(coord-dx) ) / (2. * EPSILON);
                                   normal.y = -bumpfactor * (waterHeightMap(coord + dy) - waterHeightMap(coord-dy) ) / (2. * EPSILON);
                                   normal = normalize( normal );

                                   vec3 rayDirOld=rayDir;

                                   res.fresnel=fresnelR(rayDir, normal);


                                   rayDir = refract( rayDir, normal ,1.3);
                                   if(res.fresnel>.005){
                                       rayDir = reflect( rayDirOld, normal );
                                       hitWater=vec3(0.,0.,-1.);
                                   }
                               }else if(abs(n.x)>0.) rayDir.yz*=(0.7+.4*noise(endRayPos.yz+iTime));
                               else  rayDir.xz*=(0.7+.4*noise(endRayPos.xz+iTime));
                               rayDir=normalize(rayDir);rayInv=1./rayDir;raySign=sign(rayDir);

                               rayPos=endRayPos;
                               sideDist = (mapPos-endRayPos + 0.5 + raySign*0.5) /rayDir;

                           }
                           subRes.hit=false;
                           //vox.id=0.;
                           continue;
        }
        if( vox.id !=0. && vox.id!=26. && vox.id!=12. ){
            if(xRay) continue;
            else{
                res.hit = true;
                break;
            }
        }

        #ifdef CLOUDS
        //FOG & CLOUDS
        if(CLOUDS>0.) {
            float fogd= fogDensity(mapPos)/4.*CLOUDS;
            if(fogd >4. && rayType!=2) break;
            fog += fogd;
        }
        #endif
        //NO HIT
        xRay=false;
        if(hitWater.z>0. && vox.id==0.)  {waterDist +=length(hitWater-mapPos); hitWater=vec3(-1.);res.fresnel=.001;}

        if(!inRange(mapPos.xy, range) && i> maxRayDist) break;

        if(i > int( load(_rayLimit).r)) break;
    }
    if(hitWater.z>0.)  waterDist +=length(hitWater-mapPos);
    if(hitWater.z<0.)  waterDist =0.;   //reflection


    if(load(_stats).r>0.5){
        vec4 range_B= calcLoadRange_B(rayPos.xy,iResolution.xy,1.);
        if(res.hit && inRange(mapPos.xy, range)  && !inRange(mapPos.xy, range_B)) vox.id = 8.;


        #if SURFACE_CACHE>0
        vec4 range_C1= calcLoadRange_C(rayPos.xy,iResolution.xy,1.);
        vec4 range_C0 = load(_old+_loadRange_C);
        if(res.hit && inRange(mapPos.xy, range_C0)  && !inRange(mapPos.xy, range_C1)) vox.id = 17.;
        #endif
    }

    if(!res.hit  &&rayDir.z < 0. && !inRange(mapPos.xy, range)){
        if(mapPos.z>55.) {vox.id = 0.; res.hit=false;}
        else { vox.id=3.; res.hit = true;}
    }

    res.mapPos = mapPos;
    res.normal = res.hit? -raySign * mask:vec3(0.);
    res.rayPos = VoxelHitPos(mapPos,rayPos,rayDir).xyz;
    res.dist = length(rayPos0 - res.rayPos);
    res.vox=vox;
    res.water =waterDist;
    res.fog=fog;

    if(subRes.hit){

        res.normal=  subRes.normal;
        mask=abs(subRes.normal);
        res.rayPos += rayDir * subRes.dist ;
        res.dist = length(rayPos - res.rayPos);

        #ifdef SUBTEXTURE
        // uv coordinates are relative to subvoxel (more detailed but aliased)
        res.uv_txt = subRes.uv ;
        //return res;
        #endif
    }

    //uv coordinates are relative to block (also with subvoxels)
    if (abs(mask.x) > 0.) {
        res.uv = fract(res.rayPos.yz);
    }
    else if (abs(mask.y) > 0.) {
        res.uv = fract(res.rayPos.xz);
    }
    else {
        res.uv = fract(res.rayPos.yx);
    }
    if(res.hit && !res.grass){
        float textureId = res.vox.id;
        if (textureId == 3.) textureId += res.normal.z;
        vec2 uv_txt= res.uv;
        #ifdef SUBTEXTURE
        if(res.vox.shape!=0) uv_txt= res.uv_txt;
        #endif
        res.color = getTexture(textureId, uv_txt).rgb;

    }
    return res;
}


vec3 skyColor(vec3 rayDir) {

    vec4 s= sun();
    float lightLevel = s.w;

    vec3 sunDir=s.xyz;
    vec3 daySkyColor = vec3(.5,.75,1);
    vec3 dayHorizonColor = vec3(0.8,0.8,0.9);
    vec3 nightSkyColor = vec3(0.1,0.1,0.2) / 2.;

    vec3 skyColor = mix(nightSkyColor, daySkyColor, lightLevel);
    vec3 horizonColor = mix(nightSkyColor, dayHorizonColor, lightLevel);
    float sunVis = smoothstep(.99, 0.995, dot(sunDir, rayDir));
    float moonVis = smoothstep(.999, 0.9995, dot(-sunDir, rayDir));
    return mix(mix(mix(horizonColor, skyColor, clamp(dot(rayDir, vec3(0,0,1)), 0., 1.)), vec3(1,1,0.95), sunVis), vec3(0.8), moonVis);

}



// ---- 8< -------- 8< -------- 8< -------- 8< ----


void render( out vec4 fragColor, vec3 rayPos, vec3 rayDir ,int  maxRayDist, int rayType) {

    vec4 range_B = load(_old+_loadRange_B);
    vec3 sunDir = sun().xyz; sunDir *= sign(sunDir.z);

    rayCastResults rays[2] ;//0=view,1 =shadow
    vec3 ro=rayPos;
    vec3 rd=rayDir;
    int rt=rayType;
    for(int i=0; i<=1;i++){
        rays[i]=rayCast(ro, rd,maxRayDist,range_B,rt);
        if(!rays[i].hit) break;
        if(SHADOW<0.) break;
        ro=rays[i].rayPos +rays[i].normal*0.01;
        rd=sunDir;
        maxRayDist=  25;//inRange(rays[i].rayPos.xy, range_B) ? 25:5;
        rt=3;

    }

    rayCastResults res = rays[0];

    vec3 color = vec3(0.);

    if (res.hit) {


        float shadow =rays[1].hit?SHADOW:0.;

        color=res.color;


        if(rayType==1 ){
            bool hB=(res.vox.ground>=MAX_GROUND && res.vox.id!=0. &&res.vox.buffer==BUFFER_B)
        || (res.vox.id==17. && res.vox.life >0.) ;

        if(hB && HIGHLIGHT>0. ){
        color  *=(fract(iTime*4.)+.5);
        }

        if(res.grass) {
        color *= lightmap( vec2(res.vox.light.s*(1.-shadow*.2),res.vox.light.t)   );
        }else{
        vec3 occ=calcOcclusion(res.mapPos, res.normal, res.uv,res.vox);
        color *= lightmap(vec2(occ.x*(1.-shadow*.2),occ.y)) *occ.z;
        }

        // SELECTION AND MOUSE OVER
        vec4 pick = load(_pick);
        if (res.mapPos == pick.xyz || res.vox.value==2) {
        if (pick.a == 1.) color *= getTexture(32., res.uv).r;
        else if (res.vox.value==2) color = mix(color, vec3(1.,0.,0.), 0.5);

        else color = mix(color, vec3(1), 0.2);
        }
        }else
        {
            //MAP
            color *=  clamp( (res.mapPos.z-30.) /30.,0.,1.);
            color = mix(color, vec3(1), 0.2);

        }

    }
    else color = skyColor(rayDir);

    vec3 wcolor= vec3(.03,.1,.60)* lightmap( vec2(res.vox.light.s,res.vox.light.t)   );
    //if(res.water>0.) color *= pow( wcolor ,vec3(sqrt(res.water)/(7. + res.fresnel*1000.)));
    if(res.water>0.) {
        color *= pow( wcolor ,vec3(sqrt(res.water)/7.));
        color = mix(color,wcolor, clamp(res.fresnel*500.,0.3,1.));
    }
    else if(res.fresnel>0. ) color =mix(wcolor ,color,clamp(res.fresnel*4.,0.,.9));
    if(res.mirror) color *= vec3(.9,.5,.5);
    if(rayType==1) {
        #ifdef CLOUDS
        applyFog(color.rgb,res.fog);
        #endif
        color = pow( color, vec3(0.9) );

    }
    fragColor.rgb = color; //pow(color, vec3(1.));

    if(rayType==3 ) {

        float encodeNormal=14.+ res.normal.x + res.normal.y*3. + res.normal.z*9.;
        fragColor=vec4(res.mapPos,(res.hit && res.dist >1. && res.dist <MAX_PICK_DISTANCE ? encodeNormal:0.));
    }

    //DEBUG:
    //fragColor=vec4( vec2(1.- res.dist /50.),  res.hit?1.:0.,1.);
    //fragColor=vec4( (1.-.5* sign(res.normal))* abs(res.normal) ,1.);
    //fragColor=vec4( res.uv,max(abs(res.uv -.5).x,abs(res.uv-.5).y)<.5?1:0 ,1.);
    //if(res.vox.id==12.) fragColor=vec4(vec2(res.vox.life<2. ? .5:0.),1.- res.vox.life/255.,1.);
}


#define NB 8
float[]
camx = float[]   (2954. , 2952. , 2972. , 2972.,2971. ,2955. ,2955. ,2954.),
camy = float[]   (10139., 10140., 10151.,10151.,10152.,10151.,10153.,10139.),
camz = float[]   (71.   , 83.   , 48.   ,34.   ,50.   ,50.   ,71.   ,71.),
lookx = float[]  (2970. ,2972.  , 2972. ,2952. ,2955. ,2955. ,2954. ,2970.),
looky = float[]  (10152.,10153. , 10154.,10133.,10151.,10150.,10139.,10152.),
lookz = float[]  (55.   , 50.   , 34.   ,27.   ,50.   ,71.   ,71.   ,55.);


mat3 LookAt(in vec3 ro, in vec3 up){
    vec3 fw=normalize(ro),
    rt=normalize(cross(fw,up));
    return mat3(rt, cross(rt,fw),fw);
}

vec3 RD(in vec3 ro, in vec3 cp, vec2 uv, vec2 res) {
    return LookAt(cp-ro, vec3(0,0,1))*normalize(vec3((2.*uv-res.xy)/res.y, 3.5));
}

void getCam(in vec2 uv, in vec2 res, in float time, out vec3 ro, out vec3 rd) {

    vec2 q = uv/res;

    float t = .16* time,
    kt = smoothstep(0.,1.,fract(t));

    // - Interpolate positions  and direction
    int  i0 = int(t)%NB, i1 = i0+1;

    vec3 cp = mix(vec3(lookx[i0],looky[i0],lookz[i0]), vec3(lookx[i1],looky[i1],lookz[i1]), kt);

    ro = mix(vec3(camx[i0],camy[i0],camz[i0]), vec3(camx[i1],camy[i1],camz[i1]), kt),
    ro += vec3(.01*cos(2.*time), .01*cos(time),0.);
    rd = RD(ro, cp, uv, res);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {

    float pixelSize = load(_pixelSize).r;
    vec2 renderResolution = ceil(iResolution.xy / pixelSize);
    if (any(greaterThan(fragCoord, renderResolution))) {
        fragColor = vec4(0);
        return;
    }

    vec3 cameraPos;
    vec3 cameraDir;
    int  rayType = 1;

    #ifdef MAP
    float MAP_SIZE= iResolution.y/8./pixelSize;
    vec2 MapCenter=vec2(iResolution.x/pixelSize -MAP_SIZE , iResolution.y/pixelSize - MAP_SIZE);
    if(abs(load(_map).r-1.) <.1 && distance(fragCoord,MapCenter)<MAP_SIZE) rayType=2;
    if(abs(load(_map).r-2.) <.1) {
        rayType=2;
        MapCenter=vec2(iResolution.x/pixelSize/2. , iResolution.y/pixelSize/2.);
    }

    #endif

    if(max(fragCoord.x,fragCoord.y)<1. ) rayType=3;
    if(rayType==3){
        //MOUSE RAY
        float zoom = pow(10., load(_renderScale).r/10.);///pixelSize;
        vec2 renderResolution = iResolution.xy *zoom;
        vec2 renderCenter=vec2(0.5);
        vec2 uv = (iMouse.xy- renderCenter) / renderResolution - (renderCenter/zoom);//  /pixelSize;
        cameraPos = load(_pos).xyz + vec3(0,0,1.6);
        cameraDir = rayDirection(load(_angle).xy,uv,renderResolution);

    }
    #ifdef MAP
    else if(rayType==2){

// MAP CAMERA
float cameraHeight =1500.;
float zoom = cameraHeight/iResolution.x/pixelSize*(load(_map).r>1.5?1.6:.4);
vec2 renderResolution = iResolution.xy *zoom;
vec2 renderCenter=MapCenter/iResolution.xy*pixelSize;
vec2 uv = (fragCoord.xy- renderCenter) / renderResolution - (renderCenter/zoom/pixelSize);
vec2 angle = vec2(0.,PI);
if(load(_map).r>1.5){
angle=iMouse.xy/iResolution.xy*vec2(PI,-PI/3.)+vec2(0,PI);
}
cameraDir = rayDirection(angle,uv,renderResolution);
vec3 cameraCenterDir = vec3(sin(angle.y) * cos(angle.x), sin(angle.y) * sin(angle.x), cos(angle.y));
cameraPos = load(_pos).xyz -cameraCenterDir* cameraHeight;
}
#endif
    else if(rayType==1)
{
// MAIN CAMERA
float zoom = pow(10., load(_renderScale).r/10.)/pixelSize;
vec2 renderResolution = iResolution.xy *zoom;
vec2 renderCenter=vec2(0.5);
vec2 uv = (fragCoord.xy- renderCenter) / renderResolution - (renderCenter/zoom/pixelSize);
cameraPos = load(_pos).xyz + vec3(0,0,1.6);
cameraDir = rayDirection(load(_angle).xy,uv,renderResolution);

//DEMO VIEW
if(load(_demo).r >.5)
getCam((fragCoord.xy- renderCenter) , renderResolution, iTime, cameraPos, cameraDir);

}

render(fragColor,cameraPos, cameraDir, int(load(_rayDistMax).r),rayType);

//MAP BORDER:
#ifdef MAP
    if(rayType==2){
if(load(_map).r <1.5){
if(abs(distance(fragCoord,MapCenter)-MAP_SIZE)<1.) fragColor.rgb=vec3(0.);
if(distance(fragCoord,MapCenter + vec2(sin( load(_angle).x), -cos( load(_angle).x))*MAP_SIZE )<3.) fragColor.rgb= vec3(1.,0.,0.);
}
}
#endif
    //fragColor = texture(iChannel2, fragCoord / 3. / iResolution.xy);
}
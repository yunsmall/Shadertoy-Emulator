/*
VOXEL MEMORY 2 - SURFACE
  mode = 1 it's just a copy of buffer B, working in a limited z range
  mode = 2 stores onlythe surface block with the height, for a wider area
*/

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    #ifdef EXCLUDE_CACHE
    discard;
    #endif

    #ifndef SURFACE_CACHE
    discard;
    #elif SURFACE_CACHE==2
    vec2 textelCoord = floor(fragCoord);
    vec3 offset = floor(vec3(load(_pos).xy, 0.));
    vec3 voxelCoord = texToVoxCoord(textelCoord, offset,BUFFER_C);

    vec4 newRange_C= calcLoadRange_C(offset.xy,iChannelResolution[1].xy,0.);

    if(!inRange(voxelCoord.xy, newRange_C)) {
        discard;

    }
    voxel vox;
    getVoxel( voxelCoord,vox,2);

    if(voxelCoord.z==0. && vox.ground >100.){
        voxel temp;
        float h= vox.ground-100.;
        getVoxel(vec3(voxelCoord.xy,h),temp,2);
        float id = temp.id;
        if(id !=0.){
            vox=temp;
            vox.ground=h;
        }
        else vox.ground--;
    }

    //NEIGHBOURS
    if(voxelCoord.z==0. && vox.ground<100.){
        vec3 s = vec3(1.,0.,0. );
        vec3 t = vec3(0.,1.,0. );
        voxel v[9];
        for (int i =-1; i <=1; i++) {
            for (int j =-1; j <=1  ; j++) {

                getVoxel(voxelCoord + s* float(i)+t*float(j),v[4+ i+3*j +min(iFrame,0) ] ,2 );

                voxel temp = v[4+ i+3*j ];
                if(i+3*j !=0 && temp.id==10. && temp.ground <100. && temp.ground> vox.ground -TREE_SIZE -1.) {
                    vox.id=11.; vox.shape=8;vox.ground=temp.ground+TREE_SIZE+2.;vox.life=0.;
                }
            }
        }
    }

    fragColor = encodeVoxel(vox);

    #elif SURFACE_CACHE==1
    vec2 textelCoord = floor(fragCoord);
    vec3 offset = floor(vec3(load(_pos).xy, 0.));
    vec3 voxelCoord = texToVoxCoord(textelCoord, offset,BUFFER_C);

    voxelCoord.z+=SURFACE_C;
    //vec4 newrange_B = calcLoadRange_B(offset.xy,iChannelResolution[1].xy,1.);
    vec4 newRange_C= calcLoadRange_C(offset.xy,iChannelResolution[1].xy,0.);
    //if (inRange(voxelCoord.xy,newrange_B)  ||
    if(!inRange(voxelCoord.xy, newRange_C)) {
        discard;
    }

    voxel vox;
    getVoxel( voxelCoord,vox,2);

    // SUN LIGHT SOURCES
    if (voxelCoord.z >= heightLimit_C- 2.) {
        vox.light.s = 15.;
    } else  {
        //vox.light.s=0.; //correct but initial value is better oon surface
        vox.light.s = lightDefault(voxelCoord.z);
    }

    // TORCH LIGHT SOURCES
    if(vox.id==12.) vox.light.t=max(2.,vox.light.t);
    else if(vox.id==6.) vox.light.t=15.;
    if(length( load(_pos).xyz + vec3(0,0,3.)- voxelCoord.xyz) <2.) vox.light.t=max( 12.,vox.light.t);



    //LIGHT DIFFUSE
    voxel temp;
    float air=0.;
    //int border=0;


    //NEIGHBOURS 2=ABOVE 5=BELOW, 0-1-3-4= SIDES
    float iE=0.;

    float g=MAX_GROUND;

    voxel next[6];
    for(int j=0;j<=1;j++){
        for(int i=0;i<3;i++){
            vec3 n= vec3(i==0?1.:0. ,i==1?1.:0.,i==2?1.:0.) * vec3((j==0?1.:-1.));

            if(voxelCoord.z >= heightLimit_C +SURFACE_C-1.) break;
            if( voxelCoord.z <SURFACE_C +1.) break;
            voxel temp;
            getVoxel(voxelCoord + n,temp,2);//- vec3(0.,0.,SURFACE_C));

            next[i+3*j]= temp;

            if(voxelCoord.z> heightLimit_C +SURFACE_C) vox.light.s=15.;
            else lightDiffusion(vox,temp,n);

            //ELECTRICITY DIFFUSION
            if(vox.id==17.){
                if(temp.id==8.) iE=10.;
                if(temp.id==17. && temp.life>1.) iE=max(iE,temp.life-1.);
            }


            if(temp.id==0.) air += pow(2., float(j*3+i));

            //LEAFS:
            if(temp.id==11.  && temp.life>0. &&vox.id==0.) {vox.id=11.;  vox.life=temp.life-1.; }

        }
    }


    vec3 pos = load(_pos).xyz;

    //ELECTRICITIY
    if(vox.id==17.){
        vox.life=max(iE,vox.life-1.);
        //if(iE>0.) vox.light.t=15.; else vox.light.t=0.;
    }

    if(sdBox(pos-voxelCoord -vec3(0.,0.,1.),vec3(.5,.5,.5))<=.01 &&vox.id==3.) vox.id=2.;


    //ABOVE
    if(next[2].id==0.  &&  vox.id==2.) {if(hash13(voxelCoord +iTime ) >.95 && hash(iTime)>.99) vox.id=3.;vox.life=0.;}
    if(next[2].id==0.  &&  vox.id==3.) {if(hash13(voxelCoord +iTime+30.) >.95 && hash(iTime +30.)>.99) vox.life=clamp(vox.life+1.,0.,3.);}
    if(next[2].id==3.  &&  vox.id==3.) {vox.id=2.;}
    if(next[2].id==12. && vox.id==0.) {vox.id=12.;}
    if(next[2].value==3 && (vox.id==0.|| vox.id==12.)) {vox.id=next[2].id;}

    //BELOW
    if(next[5].id==10.  && next[5].life>0. && vox.id==0.) {vox.id=10.;  vox.life=next[5].life-1.; vox.ground=0.;}
    if(next[5].id==10.  && next[5].life<1.) {vox.id=11.;  vox.life=TREE_SIZE;}
    if((next[5].id!=3.|| next[5].shape!=0)  &&  vox.id==0.) {vox.life=0.;}
    if((next[5].id!=0.|| next[5].id==12.)  &&  vox.value==3) {vox.id=0.; vox.value=0;vox.life=0.;}
    if(next[5].id==3.  &&  vox.id==0.) {vox.life=1.;}

    #ifdef TREE_DETAIL
    if(vox.id==11.) vox.shape=8;
    if(vox.id==10.) {vox.shape=9;};
    #endif


    // FIREFLIES
    //if(vox.id==26.){vox.id=0.;  vox.light.t=15.;}
    if(vox.id==26.){if(vox.light.t>1.) vox.light.t--; else vox.id=0.;vox.light.s=15.; }

    if(voxelCoord.z<35. || abs(load(_time).r-750.)<250.)
    if( air>=62. && (voxelCoord.z < heightLimit_C +SURFACE_C - 1.)){
        if(vox.id==0.  && hash13(voxelCoord +vec3(iTime))>0.9999  ) {vox.id=26.;  vox.light.t=15.;}

    }

    #ifdef STRUCTURES
    vec3 oldOffset = floor(vec3(load(_old+_pos).xy, 0.));
    structures( voxelCoord,   vox,  oldOffset,  iFrame,  iTime);
    #endif

    fragColor = encodeVoxel(vox);
    #endif
}
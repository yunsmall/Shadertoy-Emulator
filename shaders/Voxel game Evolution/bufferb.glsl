void  lightDiffusion(inout voxel vox,in voxel temp ,vec3 rPos){
    if(vox.id != 6. && vox.id != 26. ){
        vox.light.s =  max( vox.light.s  ,  	temp.light.s  -(rPos.z==1.?0.:1.) - (vox.id==0.?0.: vox.id==11.?5.:15.));
        vox.light.t =  max( vox.light.t,   temp.light.t - (vox.id==0.|| vox.id==12.?1.:vox.id==11.? 5.:15.));

    }
}

//VOXEL MEMORY 1 - NEAR BLOCKS
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    #ifdef EXCLUDE_CACHE
    discard;
    #else

    vec2 textelCoord = floor(fragCoord);
    vec3 offset = floor(vec3(load(_pos).xy, 0.));
    vec3 voxelCoord = texToVoxCoord(textelCoord, offset,BUFFER_B);

    vec4 newRange= calcLoadRange_B(offset.xy,iChannelResolution[1].xy,0.);

    if(!inRange(voxelCoord.xy, newRange)) {discard;}

    vec4 pick = load(_pick);

    voxel vox ;
    getVoxel( voxelCoord,vox,1);

    if (voxelCoord == pick.xyz || vox.value==2 )  {
        if(vox.value==0)vox.value=1;

        if (pick.a == 1. &&  vox.id != 16. && load(_pickTimer).r > 1.)
        {vox.value=1;
         vox.id = 0.;
         vox.shape=0;
         vox.light.t=0.;
         vox.life=0.;
         vox.ground=0.;
        }
        else if (pick.a == 2.)
        {
            vox.id = getInventory(load(_selectedInventory).r);
            if(vox.id==10.) vox.life=3.;
            else if (vox.id==12.)vox.life=64.;
            else vox.life=0.;
            vox.value=1;
            vox.shape=0;
        }
        else if (pick.a == 3. && vox.id != 10. && vox.id != 11. && vox.id != 12.)
        { if(vox.shape<7) vox.shape++; else vox.shape=0;}
        else  if (pick.a == 4. && vox.id != 10. && vox.id != 11. && vox.id != 12.)
        {if(vox.rotation<3.) vox.rotation++; else vox.rotation=0.;}
        else if (pick.a == 5. && vox.id != 10. && vox.id != 11. && vox.id != 12.)
        { if(vox.rotation<12.) vox.rotation+=4.; else vox.rotation= mod(vox.rotation , 4.);}
    }

    if(voxelCoord == pick.xyz  &&  pick.a == 6. )
    {vox.value= 2 ;}

    if(voxelCoord == pick.xyz  &&  pick.a == 7. )
    {
        if(vox.value==2) vox.value=1;
        else vox.value=2;
    }
    if(load(_pickTimer).r >1. && pick.a == 6. && vox.value==2)
    {vox.value= 1 ;}

    // SUN LIGHT SOURCES

    if (voxelCoord.z >= heightLimit_B - 2.) {
        vox.light.s = 15.;
    } else  {
        //vox.light.s=0.; //correct but initial value is better oon surface
        vox.light.s = lightDefault(voxelCoord.z);
    }

    // TORCH LIGHT SOURCES
    if(vox.id==12.) vox.light.t=max(2.,vox.light.t);
    else if(vox.id==6.) vox.light.t=15.;
    else vox.light.t=clamp(vox.light.t- (hash(iTime)>.5?1.:0.),0.,15.);

    if(length( load(_pos).xyz + vec3(0,0,3.)- voxelCoord.xyz) <2.) vox.light.t=max( 12.,vox.light.t);



    voxel temp;
    float air=0.;
    //int border=0;

    //NEIGHBOURS 2=ABOVE 5=BELOW, 0-1-3-4= SIDES
    float iE=0.;

    float g=MAX_GROUND;
    //vox.surface=0.;
    voxel next[9];
    for(int j=0;j<=2;j++){
        for(int i=0;i<3;i++){
            vec3 n= vec3(i==0?1.:0. ,i==1?1.:0.,i==2?1.:0.) * vec3((j==0?1.:-1.));
            #ifdef WATER_FLOW
            // lateral voxels, random direction
            if(j==2) {
                int k= int(hash(iTime)*4.);// iFrame%4;
                n = vec3(   (1- k/2) * (-1 +(k%2)*2), (k/2)* (-1 +(k%2)*2)  ,1-i);;
            }
            #endif
            voxel temp;
            getVoxel(voxelCoord + n ,temp,1 );
            next[i+3*j]= temp;

            if(vox.id==0. && temp.id!=0.) vox.surface=1.;
            if(vox.id!=0. && temp.id==0. ) vox.surface=1.;

            if(j!=2){
                if(voxelCoord.z> 80.) {vox.light.s=15.;vox.light.t=0.;}
                else  lightDiffusion(vox,temp,n);

                //ELECTRICITY DIFFUSION
                if(vox.id==17.){
                    if(temp.id==8.) iE=10.;
                    if(temp.id==17. && temp.life>1.) iE=max(iE,temp.life-1.);
                }
                //GROUND DISTANCE
                if(vox.id!=0. && vox.id!=12. &&vox.id!=26.){
                    if(voxelCoord.z <=1.) g=1.;
                    if(temp.id!=0. && temp.id!=12. &&vox.id!=26. && temp.ground>0. )  g=min(g, temp.ground+(i+3*j==5?0.:vox.id==13.?10.:1.));
                }

                if(temp.id==0.) air += pow(2., float(j*3+i));

                //LEAFS:
                if(temp.id==11.  && temp.life>0. &&vox.id==0.) {vox.id=11.;  vox.life=temp.life-1.; }
            }
        }
    }

    vec3 pos = load(_pos).xyz;

    //ELECTRICITIY
    if(vox.id==17.){
        vox.life=max(iE,vox.life-1.);
        //if(iE>0.) vox.light.t=15.; else vox.light.t=0.;
    }

    //GROUND CONNECTION: blocks not connected to the ground or sand with 4+ horizontal steps
    if(vox.id!=0. && vox.id!=12. &&vox.id!=26.){
        vox.ground=clamp(min(vox.ground+2.,g),0.,MAX_GROUND);

        //FALLING BLOCK
        #ifdef FALLING_SAND
        if(vox.ground>=MAX_GROUND
        && length(pos.xy-voxelCoord.xy)<load(_loadDistLimit).r -5.
        &&  (next[5].id==0.|| next[5].id==12.)) vox.value=3;
        #endif
    }

    if(sdBox(pos-voxelCoord -vec3(0.,0.,1.),vec3(.5,.5,.5))<=.01 &&vox.id==3.) vox.id=2.;

    //ABOVE
    if(next[2].id==0.  &&  vox.id==2.) {if(hash13(voxelCoord +iTime ) >.95 && hash(iTime)>.99) vox.id=3.;vox.life=0.;}
    if(next[2].id==0.  &&  vox.id==3.) {if(hash13(voxelCoord +iTime+30.) >.95 && hash(iTime +30.)>.99) vox.life=clamp(vox.life+1.,0.,3.);}
    if(next[2].id==3.  &&  vox.id==3.) {vox.id=2.;}
    if(next[2].value==3 && (vox.id==0.|| vox.id==12.)) {vox.id=next[2].id;}

    //BELOW
    if(next[5].id==10.  && next[5].life>0. && vox.id==0.) {vox.id=10.;  vox.life=next[5].life-1.; vox.ground=0.;}
    if(next[5].id==10.  && next[5].life<1.) {vox.id=11.;  vox.life=TREE_SIZE;}
    if((next[5].id!=3.|| next[5].shape!=0)  &&  vox.id==0.) {vox.life=0.;}
    if((next[5].id!=0.|| next[5].id==12.)  &&  vox.value==3) {vox.id=0.; vox.value=0;vox.life=0.;}

    #ifdef WATER_FLOW
    if(load(_flow).r>0.5) {
    if(vox.id==0.) vox.life=0.;
    if(vox.id==12. || vox.id==0.){

        float w= vox.id==12.?vox.life:0.;
        float w_new=w;

        float w_U  = next[2].id==12.?next[2].life:0.;//(next[2].id==0.? 0.:-1.);
        float w_D  = next[5].id==12.?next[5].life:(next[5].id==0.? 0.:-1.);
        float w_LU = next[6].id==12.?next[6].life:(next[6].id==0.? 0.:-1.);
        float w_L  = next[7].id==12.?next[7].life:(next[7].id==0.? 0.:-1.);
        float w_LD = next[8].id==12.?next[8].life:(next[8].id==0.? 0.:-1.);


        float OW=.0;
        float FL=.9; // lateral flow
        //TRANSITIONS
        //porting from https://www.shadertoy.com/view/WdjBDV


        //RULE 1 OUT
        if( w>0. && w_D < WATER_FLOW && w_D>-1.) { w_new =max(0.,w +w_D -WATER_FLOW   ); }
        //RULE 1 IN
        if( w_U>0. && w<WATER_FLOW ) {w_new=min(WATER_FLOW, w + w_U);}


        // RULE2_OUT
        if(w>0. && (w_LD>= WATER_FLOW*OW || w_D<0.) && (w_L < w -2. ) && w_L>=0. && w_LU <1. )
        {w_new= w -floor(w-w_L)*FL;}

        //RULE2 IN
        if( ( w_L >0. ) && (w_LD>=WATER_FLOW*(1.-OW*2.) || w_LD<0.) && (w<w_L-2.) && (w_U <1.))
        {w_new  =  w + floor((w_L-w)*FL );}


        //INFINITE SOURCE
        if(next[7].id==15. || next[5].id==15. || next[2].id==15. ){ w_new  =  WATER_FLOW; }


        if(w_new >0. && vox.value==0) {vox.id=12.; vox.life= clamp(w_new,0.,WATER_FLOW);}
        if(w_new <.1 &&  vox.value==0){vox.id=0.;vox.life= 0.;}
        if( vox.value==1) {vox.value=0;}
        #ifdef SUBVOXEL
        //surface water is half block
        if( next[2].id!=12. && vox.id==12.){
            if(vox.life < WATER_FLOW*.3) vox.shape=2;
            else vox.shape=3;

        }
        else  vox.shape=0;

        #endif

    }
}
    #endif
    if(next[5].id==3.  &&  vox.id==0.) {vox.life=1.;}

    #ifdef TREE_DETAIL
    if(vox.id==11.) vox.shape=8;
    if(vox.id==10.) {vox.shape=9;};
    #endif

    #ifdef FIREFLIES
    //if(vox.id==26.){vox.id=0.;  vox.light.t=15.;}
    if(vox.id==26.){if(vox.light.t>1.) vox.light.t--; else vox.id=0.;vox.light.s=15.; }

    if(voxelCoord.z<35. || abs(load(_time).r-750.)<250.)
    if( air>=62. && (voxelCoord.z < heightLimit_B - 1.)){
        if(vox.id==0.  && hash13(voxelCoord +vec3(iTime))>0.9999  ) {vox.id=26.;  vox.light.t=15.;}
    }
    #endif

    #if STRUCTURES>0
    vec3 oldOffset = floor(vec3(load(_old+_pos).xy, 0.));
    structures( voxelCoord,   vox,  oldOffset,  iFrame,  iTime);
    #endif

    fragColor = encodeVoxel(vox);
    #endif
}
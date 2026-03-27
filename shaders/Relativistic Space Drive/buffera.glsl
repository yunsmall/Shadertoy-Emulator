// For details on how the keyboard input works, see iq's tutorial: https://www.shadertoy.com/view/lsXGzf

// Numbers are based on JavaScript key codes: https://keycode.info/
const int KEY_LEFT  = 37;
const int KEY_UP    = 38;
const int KEY_RIGHT = 39;
const int KEY_DOWN  = 40;

const int KEY_A  = 65;
const int KEY_W  = 87;
const int KEY_D =68;
const int KEY_S  = 83;




vec2 m= vec2(.5);

vec3 handleKeyboard() {
    if(iMouse.z!=0.){
        m = (iMouse.xy-.5)/iResolution.xy;
    }
    // texelFetch(iChannel1, ivec2(KEY, 0), 0).x will return a value of one if key is pressed, zero if not pressed
    vec3 left = texelFetch(iChannel1, ivec2(KEY_LEFT, 0), 0).x * vec3(0, 0,1)
    +texelFetch(iChannel1, ivec2(KEY_A, 0), 0).x * vec3(0, 0,1);
    vec3 up = texelFetch(iChannel1, ivec2(KEY_UP,0), 0).x * vec3(1, 0,0)
    +texelFetch(iChannel1, ivec2(KEY_W,0), 0).x * vec3(1, 0,0);
    vec3 right = texelFetch(iChannel1, ivec2(KEY_D, 0), 0).x * vec3(0, 0,-1)
    + texelFetch(iChannel1, ivec2(KEY_RIGHT, 0), 0).x * vec3(0, 0,-1);
    vec3 down = texelFetch(iChannel1, ivec2(KEY_S, 0), 0).x * vec3(-1, 0,0)
    +texelFetch(iChannel1, ivec2(KEY_DOWN, 0), 0).x * vec3(-1, 0,0);

    vec3 acceleration=(left + up + right + down) ;

    // steer with mouse
    // acceleration.xy*=rot((m.y-.5)*PI);
    acceleration.xz*=rot(-(m.x-.5)*2.*PI);

    return acceleration*c;
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    if(fragCoord.x>7.|| fragCoord.y>7. ) discard;
    //coordinates of the boost are in rockect coordinates
    vec3  boost=vec3(0,0,0);
    vec3 orientation=vec3(1,0,0);

    boost = handleKeyboard();
    orientation = texelFetch( iChannel0, ivec2(6, 6), 0).rgb;


    //orientation is for the rockets alignment
    orientation +=(boost-orientation)*.01;
    if(ivec2(fragCoord)==ivec2(6,6))
    fragColor= vec4(orientation,0);

    //this will transform coordinates from rockets frame to stationary
    mat4 TransformMat = mat4(1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1);


    if(boost==vec3(0)){ //if no keys are pressed we just copy from the previous frame

                        fragColor= texelFetch( iChannel0, ivec2(fragCoord), 0);
                        if(ivec2(fragCoord)==ivec2(5,5)){
                            fragColor= vec4(texelFetch( iChannel0, ivec2(5, 5), 0).rgb,0);
                        }else if(ivec2(fragCoord)==ivec2(1,0)){
                            fragColor= texelFetch( iChannel0, ivec2(1, 0), 0)
                            +dt*texelFetch( iChannel0, ivec2(0, 0), 0);
                        }
                        if(iFrame<10){
                            if(ivec2(fragCoord)==ivec2(0,0)){
                                vec4 fourvel = TransformMat*vec4(0,0,0,1);
                                fragColor= fourvel;
                            }
                            else if(ivec2(fragCoord)==ivec2(1,0)){
                                fragColor= vec4(0);
                            }
                            else if(ivec2(fragCoord)==ivec2(5,5)){
                                fragColor= vec4(boost,0);
                            }else if(ivec2(fragCoord)==ivec2(6,6)){
                                fragColor= vec4(orientation,0);
                            }
                            for(int j=1; j<=4; j++)
                            if(ivec2(fragCoord)==ivec2(0,j))
                            fragColor=TransformMat[j-1];
                        }
    }else{
        boost/= texelFetch( iChannel0, ivec2(0, 0), 0).w; //to scale boost according to speed
        //next the boost transform
        mat4 NextBoost= Lorentz(-boost*dt);//mat4(1)+LorentzGenerator(-boost*dt); //

        if(iFrame>10){
            for(int j=1; j<=4; j++){
                TransformMat[j-1]=texelFetch( iChannel0, ivec2(0, j), 0);
            }
        }

        //how to transform to stationary coords
        TransformMat*=NextBoost;
        vec4 fourvel =TransformMat*vec4(0,0,0,1);

        if(ivec2(fragCoord)==ivec2(0,0)){
            fragColor= fourvel;
        }else if(ivec2(fragCoord)==ivec2(1,0)){
            vec4 fourPos=texelFetch( iChannel0, ivec2(1, 0), 0);
            fourPos+=dt*fourvel;
            fragColor= fourPos;
        }
        else if(ivec2(fragCoord)==ivec2(5,5)){
            fragColor= vec4(boost,1);
        }else{
            //StoreMatrix:
            for(int j=1; j<=4; j++)
            if(ivec2(fragCoord)==ivec2(0,j))
            fragColor=TransformMat[j-1];
        }
    }
}
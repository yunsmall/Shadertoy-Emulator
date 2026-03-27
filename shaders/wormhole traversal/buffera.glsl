// Buffer A holds camera/view state and manages movement/panning controls.

#define keyboardTexture(coord) texture(iChannel1, (coord) / vec2(256.0, 3.0))



#define KEY_A 65
#define KEY_Q 81
#define KEY_D 68
#define KEY_E 69
#define KEY_N 78
#define KEY_R 82
#define KEY_S 83
#define KEY_W 87


bool isKeyDown(int key)
{
    return keyboardTexture((vec2(0.5) + vec2(float(key), 0.0))).x > 0.5;
}

bool isKeyPressed(int key)
{
    return keyboardTexture((vec2(0.5) + vec2(float(key), 1.0))).x > 0.5;
}




void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    deserializeState(iChannel0, iResolution.xy);

    //if(!KeyboardInput(iChannel1))
    //state.timer+=min(1./5., iMouse.z<0.5?iTimeDelta*(10.*tc+1.-tb*4.):0.);

    if (!state.init)
    {
        initState();
    }


    vec2 uv = (fragCoord / iResolution.xy) * 2. - 1.;

    if (fragCoord.x < STATE_SIZE && fragCoord.y < 1.0) // state
    {
        float delta = iTimeDelta;

        if(iMouse.w > 0.5) { // clicked
                             state.cam_ctl = true;
        } else if(iMouse.z > 0.5) { // dragged
                                    vec2 imouse_delta = iMouse.xy - state.prev_iMouse.xy;

                                    float yaw_ang = imouse_delta.x * -0.003;
                                    float pit_ang = imouse_delta.y * 0.003;

                                    mat4x4 yaw = mat4x4(vec4(cos(yaw_ang),  sin(yaw_ang), 0., 0.),
                                    vec4(-sin(yaw_ang), cos(yaw_ang), 0., 0.),
                                    vec4(0.,            0.,           1., 0.),
                                    vec4(0,             0.,           0., 1.));


                                    mat4x4 pitch = mat4x4(vec4(cos(pit_ang),  0,  sin(pit_ang), 0.),
                                    vec4(0.,            1., 0.,           0.),
                                    vec4(-sin(pit_ang), 0., cos(pit_ang), 0.),
                                    vec4(0,             0., 0.,           1.));

                                    state.camera = state.camera * yaw * pitch;

        }

        if(state.cam_ctl) {
            // manual camera control
            bool move_left = isKeyDown(KEY_A);
            bool move_right = isKeyDown(KEY_D);


            bool move_fwd = isKeyDown(KEY_W);
            bool move_back = isKeyDown(KEY_S);

            bool move_up = isKeyDown(KEY_E);
            bool move_down = isKeyDown(KEY_Q);

            vec4 move = vec4(float(move_fwd) - float(move_back), float(move_left) - float(move_right), float(move_up) - float(move_down), 0.);

            state.camera[3] += state.camera * move * delta;

        } else {
            // automatically fly through wormhole
            state.camera[3] += delta * state.camera[0] * sin(iTime * 0.6);
        }



        state.camera = space_orthonorm_gs(state.camera);
        state.prev_iMouse = iMouse;

        fragColor = serializeState(int(floor(fragCoord.x)));
    }
    else
    {
        fragColor = vec4(0.);
    }
}

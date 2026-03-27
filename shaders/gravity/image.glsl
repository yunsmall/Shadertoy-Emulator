PointData points[POINT_COUNT];

#define VIEW_SCALER 100.

vec2 get_point_in_uv(PointData point){
    // 鼠标位置控制视图中心
    vec2 view_center = vec2(0.0);
    if(iMouse.z > 0.0){  // 鼠标按下
                         view_center = (iMouse.xy / iResolution.xy - 0.5) * 40.0;  // 映射到 [-20, 20] 范围
    }

    // 将世界坐标映射到 UV [0, 1] 范围
    float scale = 0.05;
    vec2 result = (point.position - view_center) * scale + vec2(0.5);
    return result;
}

vec4 draw_point(vec2 uv,int index){
    return vec4(0.);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    fragColor = vec4(0.0);

    vec2 uv = fragCoord/iResolution.xy;

    for(int i=0;i<POINT_COUNT;i++){
        points[i]=extract_point(iChannel0,i);
    }

    for(int current_index=0;current_index<POINT_COUNT;current_index++){
        vec2 point_pos_in_uv = get_point_in_uv(points[current_index]);

        // 直接在像素空间计算距离，避免宽高比问题
        vec2 pixel_uv = uv * iResolution.xy;
        vec2 pixel_point = point_pos_in_uv * iResolution.xy;

        if(length(pixel_point - pixel_uv) < 5.0){  // 5像素半径
                                                   fragColor=vec4(1,0,0,1);
        }
    }
}
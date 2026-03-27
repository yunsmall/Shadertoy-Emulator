#define POINT_DATA_CHANNEL iChannel0

// 模拟步长因子（越小模拟越慢）
#define TIME_SCALE 0.01

PointData points[POINT_COUNT];

// 简单的伪随机函数
float random(float seed){
    return fract(sin(seed * 12.9898) * 43758.5453);
}

vec4 init(in vec2 fragCoord,out bool isDiscard){
    int index;
    ivec2 col_row_index;
    int pixel_offset;
    get_row_col_index_and_pixel_offset(fragCoord,index,col_row_index,pixel_offset);
    if(col_row_index.x==-1)
    return vec4(0.);

    // 圆形随机分布
    float angle = random(float(index)) * 6.28318;
    float radius = random(float(index) + 100.0) * 3.0;
    float init_x = cos(angle) * radius;
    float init_y = sin(angle) * radius;
    vec2 init_pos = vec2(init_x, init_y);
    vec2 init_vel = vec2(0.0);  // 零初始速度

    if(pixel_offset==0){
        return vec4(init_pos, init_pos)/STORE_SCALER;  // position, last_position
    }
    else if(pixel_offset==1){
        return vec4(init_vel, 0.0, -1.0)/STORE_SCALER;  // velocity, acceleration
    }
    else if(pixel_offset==2){
        return vec4(1.,0.,0.,1.)/STORE_SCALER;  // mass (w = 1 避免 alpha=0 问题)
    }
    return vec4(0.);
}

vec2 calc_acceleration(int current_index){
    vec2 acc = vec2(0.0);
    float G = 10.0;  // 万有引力常数

    for(int i = 0; i < POINT_COUNT; i++){
        if(i == current_index)
        continue;

        vec2 delta = points[i].position - points[current_index].position;
        float dist = length(delta);

        if(dist > 0.1){  // 避免除以零
                         float force = G * points[i].mass / (dist * dist);
                         acc += normalize(delta) * force;
        }
    }

    return acc;
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    //初始化数据
    if(iFrame==0){
        bool isDiscard=false;
        vec4 ret_color=init(fragCoord,isDiscard);
        if(!isDiscard){
            fragColor=ret_color;
        }else{
            discard;
        }
        return;
    }


    int index;
    ivec2 row_and_col_index;
    int pixel_offset;
    get_row_col_index_and_pixel_offset(fragCoord,index,row_and_col_index,pixel_offset);
    if(row_and_col_index.x==-1)
    discard;

    if(index>=POINT_COUNT){
        discard;
    }

    //当前像素位于数据中

    for(int i=0;i<POINT_COUNT;i++){
        points[i]=extract_point(iChannel0,i);
    }

    //获取当前像素所在的结构体数据
    PointData current_point=points[index];

    //根据当前位于的像素位置更新数据
    if(pixel_offset==0){
        vec4 output_vec;
        output_vec.xy=current_point.position+current_point.velocity*iTimeDelta*TIME_SCALE;
        output_vec.zw=current_point.position;
        fragColor= output_vec/STORE_SCALER;
    }
    else if(pixel_offset==1){
        vec4 output_vec;
        output_vec.xy=current_point.velocity+current_point.acceleration*iTimeDelta*TIME_SCALE;
        output_vec.zw=calc_acceleration(index);
        fragColor= output_vec/STORE_SCALER;
    }
    else{
        vec4 output_vec;
        output_vec.x=current_point.mass;
        fragColor= output_vec/STORE_SCALER;
    }

}
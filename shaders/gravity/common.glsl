struct PointData{
    vec2 position;
    vec2 last_position;

    vec2 velocity;
    vec2 acceleration;

    float mass;
};
#define PI 3.141592653589793238462643502884197169

#define sizeof_point_data_in_pixel 3

#define POINT_PER_ROW 10

#define STORE_SCALER 1000.

#define POINT_COUNT 100

//从三个像素里提取数据
PointData extract_point_in_pixel(vec4 pixel0,vec4 pixel1,vec4 pixel2){
    PointData point;

    vec4 pixel0_scalered=pixel0*STORE_SCALER;
    vec4 pixel1_scalered=pixel1*STORE_SCALER;
    vec4 pixel2_scalered=pixel2*STORE_SCALER;

    point.position=pixel0_scalered.xy;
    point.last_position=pixel0_scalered.zw;
    point.velocity=pixel1_scalered.xy;
    point.acceleration=pixel1_scalered.zw;
    point.mass=pixel2_scalered.x;
    return point;
}
//从数据位于的位置提取数据起点的uv坐标，不检查是否合法
vec2 get_start_uv_from_col_row_index(ivec2 row_and_col_index){

    return vec2(row_and_col_index.x*sizeof_point_data_in_pixel,row_and_col_index.y);
}
//从当前uv获取对应的起点坐标，不检查是否合法
vec2 get_start_uv(vec2 current_uv){
    int row_point_index=int(current_uv.x)/sizeof_point_data_in_pixel;
    int col_point_index=int(current_uv.y);

    return get_start_uv_from_col_row_index(ivec2(row_point_index,col_point_index));
}
//从当前uv获取当前数据的索引，获取行列坐标，获取当前uv坐标所在像素在一组数据中的索引，如果uv非法会返回-1
void get_row_col_index_and_pixel_offset(in vec2 uv,out int index,out ivec2 row_and_col_index, out int pixel_offset){
    int row_point_index=int(uv.x)/sizeof_point_data_in_pixel;
    int col_point_index=int(uv.y);

    if(row_point_index>=POINT_PER_ROW){
        index=-1;
        row_and_col_index = ivec2(-1,-1);
        return;
    }


    int current_pixel_index=int(uv.x)%sizeof_point_data_in_pixel;
    if(current_pixel_index>=sizeof_point_data_in_pixel){
        index=-1;
        row_and_col_index = ivec2(-1,-1);
        return;
    }
    index=row_point_index+col_point_index*POINT_PER_ROW;
    row_and_col_index = ivec2(row_point_index, col_point_index);
    pixel_offset=current_pixel_index;
    return;
}

// 从像素坐标提取数据（coord是像素坐标，如ivec2(0,0), ivec2(3,0)等）
PointData extract_point_at_coord(sampler2D channel, ivec2 coord){
    vec4 pixel0 = texelFetch(channel, coord, 0);
    vec4 pixel1 = texelFetch(channel, coord + ivec2(1, 0), 0);
    vec4 pixel2 = texelFetch(channel, coord + ivec2(2, 0), 0);
    return extract_point_in_pixel(pixel0, pixel1, pixel2);
}

PointData extract_point(sampler2D channel, int index){
    int target_row = index / POINT_PER_ROW;
    int target_start_col = (index % POINT_PER_ROW) * sizeof_point_data_in_pixel;

    return extract_point_at_coord(channel, ivec2(target_start_col, target_row));
}

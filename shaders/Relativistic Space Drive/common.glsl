const float frames = 70.;

//t is the proper time in rockets moving frame (counted in frames so
//fps would not affect the physics)
#define dt iTimeDelta

#define PI 3.14159265359

const float c=1.;//Not sure if everything works if you change c.

float gamma(float b){

    return pow(1.-b*b,-.5);
}

//for infinitesimal lorentz transforms it is faster to use this generator:
mat4 LorentzGenerator(vec3 e){
    float cc=c*c;

    return mat4(0 ,0  , 0 , -e.x/cc ,
    0,   0,  0 ,  -e.y/cc ,
    0, 0,   0,   -e.z/cc  ,
    -e.x, -e.y  , -e.z ,0);
}

mat4 Lorentz(vec3 v){
    float beta= length(v)/c;
    float gamma = gamma(beta);

    float v2=dot(v,v);

    return mat4(1.+(gamma-1.)*v.x*v.x/v2, (gamma-1.)*v.x*v.y/v2, (gamma-1.)*v.x*v.z/v2, -gamma*v.x/c,
    (gamma-1.)*v.y*v.x/v2, 1.+(gamma-1.)*v.y*v.y/v2, (gamma-1.)*v.y*v.z/v2, -gamma*v.y/c,
    (gamma-1.)*v.z*v.x/v2, (gamma-1.)*v.z*v.y/v2, 1.+(gamma-1.)*v.z*v.z/v2, -gamma*v.z/c ,
    -gamma*v.x/c, -gamma*v.y/c, -gamma*v.z/c,   gamma);
}

mat2 rot(float a){
    return mat2(cos(a), -sin(a),sin(a),cos(a));
}




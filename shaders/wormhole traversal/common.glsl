// ------------------ WORMHOLE GEOMETRY ------------------
// wormhole parameters
// major radius = radius of wormhole + curvature at lip, minor radius = radius of curvature at wormhole lip
const float r_maj = 1.4;
// actual radius of hole is major - minor
const float r_min = 0.6;
// 3d space on each side is offset by r_min in the w axis

// potential optimization: pass r_maj/r_min squared and compare it to p.xy . p.xy
float wormhole_sdf(vec4 p, float r_maj, float r_min) {
    if (length(p.xyz) > r_maj) {
        // outside wormhole
        return abs(p.w) - r_min;
    } else {
        vec4 rim_point = vec4(normalize(p.xyz / r_maj) * r_maj, 0.);
        return length(rim_point - p) - r_min;
    }
}

// 4d SDF of the wormhole. The 0-level hypersurface is the space the camera is embedded within.
float space(vec4 p) {
    return wormhole_sdf(p, r_maj, r_min);
    //return p.w - r_min; // flat space, no wormhole
}

// project v1 onto v2
vec4 proj(vec4 v1, vec4 v2) {
    return v2 * dot(v1, v2);
}


// normalizes a ray into the space.
// "Normalization" here means moving it to the nearest point within the 3d hypersurface
// and constraining its direction to be normal to the hypersurface.
// TODO: Using analytical normals it should be possible to do this in arbitrary geometries
void space_normalize(inout vec4 p, inout vec4 dir) {
    vec4 space_normal;
    if (length(p.xyz) > r_maj) {
        // outside wormhole
        if(p.w > 0.) space_normal = vec4(vec3(0.), 1.); // top space
        else space_normal = vec4(vec3(0.), -1.); // bottom space
    } else {
        // wormhole space
        vec4 rim_point = vec4(normalize(p.xyz / r_maj) * r_maj,  0.);
        space_normal = normalize(p - rim_point);
    }
    float space_d = space(p);

    // snap point back to the space

    p -= space_d * space_normal;
    // constrain direction to surface of space
    // subtract the projection of the direction onto the space's normal vector
    // this is a single step of the graham-schmidt process
    dir = normalize(dir - proj(dir, space_normal));
}

// Graham-Schmidt orthonormalization. https://en.wikipedia.org/wiki/Gram%E2%80%93Schmidt_process
// Resulting matrix is orthogonal to the space normal and orthonormalized, but maintains its orientation as closely as possible.
// TODO: Using analytical normals it should be possible to do this in arbitrary geometries
mat4x4 space_orthonorm_gs( mat4x4 pos_basis) {
    vec4 x_basis = pos_basis[0];
    vec4 y_basis = pos_basis[1];
    vec4 z_basis = pos_basis[2];

    vec4 p = pos_basis[3];

    vec4 space_normal;
    if (length(p.xyz) > r_maj) {
        // outside wormhole
        if(p.w > 0.) space_normal = vec4(vec3(0.), 1.); // top space
        else space_normal = vec4(vec3(0.), -1.); // bottom space
    } else {
        // wormhole space
        vec4 rim_point = vec4(normalize(p.xyz / r_maj) * r_maj,  0.);
        space_normal = normalize(p - rim_point);
    }
    float space_d = space(p);

    // snap point back to the space
    p -= space_d * space_normal;

    // do the Graham Schmidt thing
    x_basis = normalize(x_basis - proj(x_basis, space_normal));
    y_basis = normalize(y_basis - proj(y_basis, space_normal) - proj(y_basis, x_basis));
    z_basis = normalize(z_basis - proj(z_basis, space_normal) - proj(z_basis, x_basis) - proj(z_basis, y_basis));

    return mat4x4(x_basis, y_basis, z_basis, p);
}

// constrains length of raymarches based on space curvature.
// this limits numerical errors as rays pass through the curved interior of the wormhole.
float space_sdf(vec4 p) {
    return max(0.1 * r_min, length(p.xyz) - r_maj);
}


// ------------------ STATE MANAGEMENT ------------------


struct State
{
    bool init;          // 0
    mat4x4 camera;      // 1..4
    bool cam_ctl;       // 5
    vec4 prev_iMouse;   // 6

} state;

#define STATE_SIZE 7.

void initState() {
    state.init = true;

    // pointing along X axis, 3d position (-2, 0.5, 0.), in positive universe (w=r_min)
    state.camera = mat4x4(vec4(1., 0.,0.,0.),
    vec4(0.,1.,0.,0.),
    vec4(0.,0.,1.,0.),
    vec4(-2.,0.5,0.,r_min));
    state.cam_ctl = false;
    state.prev_iMouse = vec4(0.);
}

vec4 serializeState(int idx){
    if (idx == 0) return vec4(state.init?1.:0., vec3(0.));
    if (idx == 1) return state.camera[0];
    if (idx == 2) return state.camera[1];
    if (idx == 3) return state.camera[2];
    if (idx == 4) return state.camera[3];
    if (idx == 5) return vec4(state.cam_ctl?1.:0., vec3(0.));
    if (idx == 6) return state.prev_iMouse;
    return vec4(0);
}

#define inputTextureState(i) texture(iChannel0, vec2(float(i) + 0.5, 0.5) / iResolution.xy)
void deserializeState(sampler2D iChannel0, vec2 iResolution){
    state.init = inputTextureState(0).x > 0.;
    state.camera[0] = inputTextureState(1);
    state.camera[1] = inputTextureState(2);
    state.camera[2] = inputTextureState(3);
    state.camera[3] = inputTextureState(4);
    state.cam_ctl = inputTextureState(5).x > 0.;
    state.prev_iMouse = inputTextureState(6);
}

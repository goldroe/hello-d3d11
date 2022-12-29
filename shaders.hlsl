struct Directional_Light {
    float4 ambient;
    float4 diffuse;
    float4 specular;
    
    float3 direction;
    float pad;
};

struct Point_Light {
    float4 ambient;
    float4 diffuse;
    float4 specular;
    
    float3 position;
    float range;
    
    float3 att;
    float pad;
};

struct Spot_Light {
    float4 ambient;
    float4 diffuse;
    float4 specular;
    
    float3 position;
    float range;
    
    float3 direction;
    float spot;
    
    float3 att;
    float pad;
};

struct Material {
    float4 ambient;
    float4 diffuse;
    float4 specular;
};

cbuffer CB_Per_Frame : register(b0) {
    Directional_Light dir_light;
    Point_Light point_light;
    Spot_Light spot_light;
    float3 eye_posw;
    float pad;
};

cbuffer CB_Per_Obj : register(b1) {
    float4x4 world;
    float4x4 world_inv_transpose;
    float4x4 wvp;
    Material material;
};

struct PS_IN {
    float3 posw : POSITION;
    float3 normalw : NORMAL;
    float4 posh : SV_POSITION;
};

void compute_spot_light(
    Material mat, Spot_Light light, float3 pos, float3 normal, float3 to_eye,
    out float4 ambient, out float4 diffuse, out float4 specular) {
    
    ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
    diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
    specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 light_vec = light.position - pos;

    float d = length(light_vec);

    if (d > light.range)
        return;

    light_vec /= d;
    
    ambient = light.ambient * mat.ambient;

    float diffuse_factor = dot(light_vec, normal);

    if (diffuse_factor > 0.0f) {
        float3 v = reflect(-light_vec, normal);
        float spec_factor = pow(max(dot(v, to_eye), 0.0f), mat.specular.w);
        diffuse = diffuse_factor * mat.diffuse * light.diffuse;
        specular = spec_factor * mat.specular * light.specular;
    }

    float att = 1.0f / dot(light.att, float3(1.0f, d, d * d));
    diffuse *= att;
    specular *= att;

    float spot = pow(max(dot(-light_vec, light.direction), 0.0f), light.spot);
    ambient *= spot;
    diffuse *= spot;
    specular *= spot;
}

void compute_point_light(
    Material mat, Point_Light light, float3 pos, float3 normal, float3 to_eye,
    out float4 ambient, out float4 diffuse, out float4 specular) {
    
    ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
    diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
    specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 light_vec = light.position - pos;

    float d = length(light_vec);

    if (d > light.range)
        return;

    light_vec /= d;
    
    ambient = light.ambient * mat.ambient;

    float diffuse_factor = dot(light_vec, normal);

    if (diffuse_factor > 0.0f) {
        float3 v = reflect(-light_vec, normal);
        float spec_factor = pow(max(dot(v, to_eye), 0.0f), mat.specular.w);
        diffuse = diffuse_factor * mat.diffuse * light.diffuse;
        specular = spec_factor * mat.specular * light.specular;
    }

    float att = 1.0f / dot(light.att, float3(1.0f, d, d * d));
    diffuse *= att;
    specular *= att;
}

void compute_directional_light(
    Material mat, Directional_Light light, float3 normal, float3 to_eye,
    out float4 ambient, out float4 diffuse, out float4 specular) {
    
    ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
    diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
    specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 light_vec = -light.direction;

    ambient = light.ambient * mat.ambient;

    float diffuse_factor = dot(light_vec, normal);

    if (diffuse_factor > 0.0f) {
        float3 v = reflect(-light_vec, normal);
        float spec_factor = pow(max(dot(v, to_eye), 0.0f), mat.specular.w);
        diffuse = diffuse_factor * mat.diffuse * light.diffuse;
        specular = spec_factor * mat.specular * light.specular;
    }
}

PS_IN vs_main(float3 positionl : POSITION, float3 normall : NORMAL) {
    PS_IN vout;
    vout.posw = mul(world, float4(positionl, 1.0f)).xyz;
    vout.normalw = mul(normall, (float3x3)world_inv_transpose);

    vout.posh = mul(wvp, float4(positionl, 1.0));

    return vout;
}

float4 ps_main(PS_IN pin) : SV_TARGET {
    pin.normalw = normalize(pin.normalw);

    float3 to_eyew = normalize(eye_posw - pin.posw);

    float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

    float4 A, D, S;
    compute_directional_light(material, dir_light, pin.normalw, to_eyew, A, D, S);
    ambient += A;
    diffuse += D;
    specular += S;

    compute_point_light(material, point_light, pin.posw, pin.normalw, to_eyew, A, D, S);
    ambient += A;
    diffuse += D;
    specular += S;

    compute_spot_light(material, spot_light, pin.posw, pin.normalw, to_eyew, A, D, S);
    ambient += A;
    diffuse += D;
    specular += S;
    
    float4 lit_color;
    lit_color = ambient + diffuse + specular;
    return lit_color;
}

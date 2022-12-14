
cbuffer Constant_Buffer : register(b0) {
    float4x4 wvp;
};

struct PS_IN {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

PS_IN VS(float3 position : POSITION, float3 color : COLOR) {
    PS_IN vout;
    vout.position = mul(wvp, float4(position, 1.0));
    vout.color = color;
    return vout;
}

float4 PS(PS_IN pin) : SV_TARGET {
    return float4(pin.color, 1.0);
}

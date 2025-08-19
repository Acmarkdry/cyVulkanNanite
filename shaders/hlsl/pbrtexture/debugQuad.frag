Texture2D tex:register(t0);
SamplerState texSampler: register(s0);

float4 main(float4 position: SV_Position, float uv:TEXCOORD0):SV_Target
{
    float3 color = tex.SampleLevel(texSampler, uv, 3).rbg;
    return float4(color, 1.0f);
}

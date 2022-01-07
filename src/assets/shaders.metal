#include <metal_stdlib>

using namespace metal;
vertex float4 v_quad(
    const device packed_float3* vertex_array [[ buffer(0) ]],
    constant float4x4 &P [[buffer(1)]],
    constant float4x4 &M [[buffer(2)]],
    unsigned int vid [[ vertex_id ]])
{
    return P * M * float4(vertex_array[vid], 1.0);
}
fragment half4 f_quad()
{
    return half4(0, 0, 0, 1);
}

vertex float4 v_post(uint idx [[vertex_id]])
{
    float2 pos[] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };
    return float4(pos[idx].xy, 0, 1);
}
fragment half4 f_post(
    float4 in [[ position ]],
    texture2d<half> albedo [[ texture(0) ]]
)
{
    constexpr sampler Sampler(coord::pixel, filter::nearest);
    return half4(albedo.sample(Sampler, in.xy).xyz, 1);
}

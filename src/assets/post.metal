#include <metal_stdlib>

using namespace metal;
vertex float4 v_main(uint idx [[vertex_id]])
{
    float2 pos[] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };
    return float4(pos[idx].xy, 0, 1);
}
fragment half4 f_main(
    float4 in [[ position ]],
    texture2d<half> albedo [[ texture(0) ]]
)
{
    constexpr sampler Sampler(coord::pixel, filter::nearest);
    return half4(albedo.sample(Sampler, in.xy).xyz, 1);
}

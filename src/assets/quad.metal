#include <metal_stdlib>

using namespace metal;
vertex float4 v_main(
    const device packed_float3* vertex_array [[ buffer(0) ]],
    constant float4x4 &P [[buffer(1)]],
    constant float4x4 &M [[buffer(2)]],
    unsigned int vid [[ vertex_id ]])
{
    return P * M * float4(vertex_array[vid], 1.0);
}
fragment half4 f_main()
{
    return half4(0, 0, 0, 1);
}

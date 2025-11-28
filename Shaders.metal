#include <metal_stdlib>
using namespace metal;

struct SimArgs {
    float diffA;
    float diffB;
    float feed;
    float kill;
};

// Simulation

kernel void sim_main(
    texture2d<float, access::read> inputTexture [[texture(0)]],
    texture2d<float, access::write> outputTexture [[texture(1)]],
    uint2 gid [[thread_position_in_grid]],
    constant SimArgs &args [[buffer(3)]]
) {
    float4 a = inputTexture.read(gid);
    float4 b = inputTexture.read(gid);
    outputTexture.write(a, gid);
}

// Visualization

struct VertexOutPost {
    float4 position [[position]];
    float2 uv;
};

// Vertexless -- Generate full screen tri based on vert id
vertex VertexOutPost full_screen_tri(uint vertexID [[vertex_id]]) {
    VertexOutPost out;
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    out.position = float4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    out.uv = float2(pos.x, 1.0 - pos.y);
    return out;
}

fragment float4 sim_visualizer(
        VertexOutPost in [[stage_in]],
        texture2d<float> simResult [[texture(1)]]) {
    constexpr sampler s(address::clamp_to_edge, filter::linear);
    float4 a = simResult.sample(s, in.uv);
    return float4(a.r, a.g, a.b, a.a) * 10.0f; // multiply to make it more visible. Temporarily use all channels while debugging
}

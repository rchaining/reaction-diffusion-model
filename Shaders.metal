#include <metal_stdlib>
using namespace metal;

// This matches the "Vertex" struct in our C++ code exactly.
struct VertexIn {
    float4 position;
    float4 normal;
    float4 color;
};

struct Uniforms {
    float4x4 rotationMatrix;
};

// Passed from Vertex shader to Fragment
struct VertexOut {
    float4 position [[position]]; // Tag with position for the GPU (Why is this necessary?)
    float3 normal;
    float4 color;
};

vertex VertexOut vertex_main(device const VertexIn* vertices [[buffer(0)]], // Read array from Buffer 0
                             constant Uniforms &uniforms   [[buffer(1)]], // Read matrix from Buffer 1
                             uint vertexId                 [[vertex_id]]) // Current index
{
    VertexOut out;
    float4 pos = vertices[vertexId].position;
    out.position = uniforms.rotationMatrix * pos;
    out.normal = (uniforms.rotationMatrix * vertices[vertexId].normal).xyz; // Rotate normal as well
    out.color = vertices[vertexId].color;
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
    float3 normal = normalize(in.normal);
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float lightIntensity = saturate(dot(normal, lightDir));
    // Smoothstep the lighting to help push the shadows a bit for visibility.
    lightIntensity = smoothstep(0.0, 1.0, lightIntensity);
    float3 finalColor = in.color.rgb * (lightIntensity + 0.1);

    return float4(finalColor, 1.0);
}

// POST PROCESSING

struct VertexOutPost {
    float4 position [[position]];
    float2 uv;
};

// "Vertexless" shader: Generate a full-screen tri based on the vertex id 
vertex VertexOutPost post_vertex_main(uint vertexID [[vertex_id]]) {
    VertexOutPost out;
    // Magic math to generate a full-screen triangle from 3 points
    // (Gemini's description, not mine) =/
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    out.position = float4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    out.uv = float2(pos.x, 1.0 - pos.y); // Flip Y for Metal
    return out;
}

// Post-process fragment shader
// Reads color and depth textures from Pass 1
fragment float4 post_fragment_main(
        VertexOutPost in [[stage_in]],
        texture2d<float> colorTexture [[texture(0)]],
        texture2d<float> depthTexture [[texture(1)]]) {
    // Texture sampler
    constexpr sampler s(address::clamp_to_edge, filter::linear);

    float4 originalColor = colorTexture.sample(s, in.uv);
    float depth = depthTexture.sample(s, in.uv).r; // 0 = near, 1.0 = far

    // Edge detection logic
    // Note: Normally we use screen resolution uniform for pixel size.
    // For now we approximate pixel offset as 1/1000 = 0.001
    float offset = 0.001;
    float dLeft  = depthTexture.sample(s, in.uv + float2(-offset, 0)).r;
    float dRight = depthTexture.sample(s, in.uv + float2( offset, 0)).r;
    float dUp    = depthTexture.sample(s, in.uv + float2(0, -offset)).r;
    float dDown  = depthTexture.sample(s, in.uv + float2(0,  offset)).r;

    // Simple Laplacian filter
    float depthDiff = fabs(depth - dLeft) + fabs(depth - dRight) + 
                      fabs(depth - dUp)   + fabs(depth - dDown);
    
    // Threshold: If difference is high, make it white.
    const float EDGE_SENSITIVITY = 0.05;
    // float edge = step(EDGE_SENSITIVITY, depthDiff); // 0.01 is sensitivity
    float edge = smoothstep(0.0, EDGE_SENSITIVITY, depthDiff);
    edge = edge * edge;
    // If difference is high, we're at the bottom of a bowl remember.

    return originalColor + float4(edge, edge, edge, 1.0);
}
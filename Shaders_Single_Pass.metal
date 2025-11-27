#include <metal_stdlib>
using namespace metal;

// 1. Define the incoming data structure
// This matches the "Vertex" struct in our C++ code exactly.
struct VertexIn {
    float4 position;
    float4 color;
};

// 2. Define the data passing from Vertex -> Fragment
struct VertexOut {
    float4 position [[position]]; // Tag with position for the GPU (Why is this necessary?)
    float4 color;
};

struct Uniforms {
    float4x4 rotationMatrix;
};

vertex VertexOut vertex_main(device const VertexIn* vertices [[buffer(0)]], // Read array from Buffer 0
                             constant Uniforms &uniforms   [[buffer(1)]], // Read matrix from Buffer 1
                             uint vertexId                 [[vertex_id]]) // Current index
{
    VertexOut out;
    
    // Grab the vertex from the array using the index
    float4 pos = vertices[vertexId].position;
    
    // Apply rotation
    out.position = uniforms.rotationMatrix * pos;
    
    // Pass the color through to the fragment shader
    out.color = vertices[vertexId].color;
    
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
    // 1. Calculate the Normal on the fly
    // We take the derivative (slope) of the position relative to the screen X and Y.
    // The Cross Product of these two slopes gives us the vector pointing "out" of the triangle.
    float3 dpdx = dfdx(in.position.xyz);
    float3 dpdy = dfdy(in.position.xyz);
    float3 normal = normalize(cross(dpdx, dpdy));

    // 2. Define a simple Light Direction (coming from top-right)
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));

    // 3. Calculate "Dot Product" (Diffuse Lighting)
    // If the normal points AT the light, this number is 1.0. If away, it's 0.0.
    // We use absolute value (fabs) so the back of the triangle lights up too 
    // (since we don't have a back-face culling enabled yet).
    float lightIntensity = saturate(dot(normal, lightDir));
    lightIntensity = smoothstep(0.0, 1.0, lightIntensity);
    
    // Add a little "Ambient" light (0.1) so shadows aren't pitch black
    // float3 finalColor = in.color.rgb * (lightIntensity + 0.1);
    float3 finalColor = in.color.rgb * lightIntensity;

    return float4(finalColor, 1.0);
}
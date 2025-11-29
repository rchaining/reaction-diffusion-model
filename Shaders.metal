#include <metal_stdlib>
using namespace metal;

struct SimArgs {
    float frequency;
    float scale;
    float diffA;
    float diffB;
    float feed;
    float kill;
    float timeStep;
};

// Would like this to be configurable somehow. Doing so may require templating this file to generate from a config.
#define GROUP_WIDTH 32
#define GROUP_HEIGHT 32


// Hash function to generate pseudo-random gradients
float2 hash(float2 p) {
    p = float2(dot(p, float2(127.1, 311.7)),
               dot(p, float2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

// 2D Perlin (Gradient) Noise function
float perlin_noise(float2 p) {
    float2 i = floor(p);
    float2 f = fract(p);

    // Quintic interpolation curve (smoother than cubic)
    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    // Mix dot products of gradients
    return mix(mix(dot(hash(i + float2(0.0, 0.0)), f - float2(0.0, 0.0)),
                   dot(hash(i + float2(1.0, 0.0)), f - float2(1.0, 0.0)), u.x),
               mix(dot(hash(i + float2(0.0, 1.0)), f - float2(0.0, 1.0)),
                   dot(hash(i + float2(1.0, 1.0)), f - float2(1.0, 1.0)), u.x), u.y);
}


// Simulation
kernel void init_simulation(texture2d<float, access::write> tex [[texture(0)]],
                            uint2 gid [[thread_position_in_grid]],
                            constant SimArgs &args [[buffer(1)]]) {
    if (gid.x >= tex.get_width() || gid.y >= tex.get_height()) {
        return;
    }

    float2 uv = float2(gid) / float2(tex.get_width(), tex.get_height());
    float frequency = args.frequency;
    float scale = args.scale;
    float n = perlin_noise(uv * frequency);
    n = n * scale + 0.5;
    float4 color = float4(1.0, n, 0.0, 1.0);

    tex.write(color, gid);
}

kernel void sim_main(
    texture2d<float, access::read> inputTexture [[texture(0)]],
    texture2d<float, access::write> outputTexture [[texture(1)]],
    uint2 gid [[thread_position_in_grid]],
    uint2 tid [[thread_position_in_threadgroup]],
    constant SimArgs &args [[buffer(3)]]
) {
    // Ensure not out of bounds
    if (gid.x >= inputTexture.get_width() || gid.y >= inputTexture.get_height()) {
        return;
    } 
    
    // Optimization: load neighbors into memory in parallel, reducing total reads from VRAM
    threadgroup float4 sharedMemory[GROUP_WIDTH + 2][GROUP_HEIGHT + 2];
    uint2 localPos = tid + uint2(1, 1);
    sharedMemory[localPos.x][localPos.y] = inputTexture.read(gid);
    // Load the skirt around the edge of the grid
    // Would be good to wrap to edges.
    // I think we underflow when gid.x or gid.y == 0. That should only be the bottom or the top of the grid though.
    if (tid.y == 0) {
        sharedMemory[localPos.x][0] = inputTexture.read(gid - uint2(0, 1));
    }
    if (tid.x == 0) {
        sharedMemory[0][localPos.y] = inputTexture.read(gid - uint2(1, 0));
    }
    if (tid.y == GROUP_HEIGHT - 1) {
        sharedMemory[localPos.x][GROUP_HEIGHT + 1] = inputTexture.read(gid + uint2(0, 1));
    }
    if (tid.x == GROUP_WIDTH - 1) {
        sharedMemory[GROUP_WIDTH + 1][localPos.y] = inputTexture.read(gid + uint2(1, 0));
    }

    // Sync -- wait until everything finished wiritng to shared memory
    threadgroup_barrier(mem_flags::mem_threadgroup);

    float4 center = sharedMemory[localPos.x][localPos.y];
    float4 up     = sharedMemory[localPos.x][localPos.y - 1];
    float4 down   = sharedMemory[localPos.x][localPos.y + 1];
    float4 left   = sharedMemory[localPos.x - 1][localPos.y];
    float4 right  = sharedMemory[localPos.x + 1][localPos.y];

    float a = center.r;
    float b = center.g;
    float n = args.timeStep;

    // Corresponds to laplacian weights of
    // [ 0 -1 0
    //   -1 4 -1
    //   0 -1 0]
    float4 laplacian =  (center * 4.0) - up - down - left - right;

    // D_A, D_b: Diffusion rates for A, B
    // F: Feed rate of A
    // K: Kill rate of B
    // L_A, L_B: Laplacian of A, B

    // Grey-Scott Model
    // A_(t+n) = A(t) + (n * (D_A * L_A(t) - A * B^2 + F * (1 - A)))
    // B_(t+n) = B(t) + (n * (D_B * L_B(t) + A * B^2 - (F + K) * B))

    float reaction = a * b * b; // a * b^2
    float deltaA = (args.diffA * laplacian.r - reaction + args.feed * (1.0 - a));
    float deltaB = (args.diffB * laplacian.g + reaction - (args.feed + args.kill) * b);
    float aNew = a + n * deltaA;
    float bNew = b + n * deltaB;
    aNew = clamp(aNew, 0.0, 1.0);
    bNew = clamp(bNew, 0.0, 1.0);

    outputTexture.write(float4(aNew, bNew, 0, 0), gid);
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

// VIZUALIZER
// (Via gemini for the color mapping)

// A cosine-based palette function. 
// Inputs: t = value (0.0 to 1.0)
// Returns: RGB color
float3 get_palette(float t) {
    // These vector coefficients define the gradient "theme"
    // Theme: "Oceanic Bioluminescence" (Blue/Purple/Cyan)
    float3 a = float3(0.5, 0.5, 0.5);
    float3 b = float3(0.5, 0.5, 0.5);
    float3 c = float3(1.0, 1.0, 1.0);
    float3 d = float3(0.30, 0.20, 0.20); 

    return a + b * cos(6.28318 * (c * t + d));
}

fragment float4 sim_visualizer(
        VertexOutPost in [[stage_in]],
        texture2d<float> simResult [[texture(0)]]
) {
    constexpr sampler s(mag_filter::nearest, min_filter::nearest, address::clamp_to_edge);
    float4 data = simResult.sample(s, in.uv);

    // Read both chemicals
    // A = Feed/Fuel (Starts at 1.0, gets eaten)
    // B = Reaction/Coral (Starts at 0.0, grows)
    float a = clamp(data.r, 0.0, 1.0);
    float b = clamp(data.g, 0.0, 1.0);

    // --- Layer 1: Chemical A (Background) ---
    // We map A to a deep purple/blue. 
    // As A gets depleted (eaten by B), this background will darken, 
    // showing you exactly where the "fuel" is running low.
    float3 colorA = float3(0.2, 0.0, 0.4) * a;

    // --- Layer 2: Chemical B (Foreground) ---
    // Use the cosine palette for the pattern
    float3 paletteB = get_palette(b);
    
    // CRITICAL: Mask the palette so it fades to transparent where B is low.
    // Without this, the palette's "zero" color (grey) would hide our A layer.
    float bMask = smoothstep(0.01, 0.25, b); 
    
    // Combine:
    // Start with A, then blend B on top based on its density (bMask)
    float3 finalColor = mix(colorA, paletteB, bMask);

    return float4(finalColor, 1.0);
}
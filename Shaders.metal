#include <metal_stdlib>
using namespace metal;

struct SimArgs {
    float diffA;
    float diffB;
    float feed;
    float kill;
    float timeStep;
};

// Would like this to be configurable somehow. Doing so may require templating this file to generate from a config.
#define GROUP_WIDTH 16
#define GROUP_HEIGHT 16
    

// Simulation
kernel void init_simulation(texture2d<float, access::write> tex [[texture(0)]],
                            uint2 gid [[thread_position_in_grid]]) {
    if (gid.x >= tex.get_width() || gid.y >= tex.get_height()) {
        return;
    }

    float4 color = float4(1.0, 0.0, 0.0, 1.0); 

    uint w = tex.get_width();
    uint h = tex.get_height();
    
    if (gid.x > w / 2 - 10 && gid.x < w / 2 + 10 &&
        gid.y > h / 2 - 10 && gid.y < h / 2 + 10) {
        color.y = 1.0;
    }

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
    float da = args.diffA;
    float db = args.diffB;
    float feed = args.feed;
    float kill = args.kill;
    float n = args.timeStep;

    // Corresponds to laplacian weights of
    // [ 0 -1 0
    //   -1 4 -1
    //   0 -1 0]
    float4 laplacianValue = (center * 4.0) - up - down - left - right;

    // D_A, D_b: Diffusion rates for A, B
    // F: Feed rate of A
    // K: Kill rate of B
    // L_A, L_B: Laplacian of A, B

    // Grey-Scott Model
    // A_(t+n) = n * (A(t) + D_A x L_A(t) - A x B^2 + F x (1 - A))
    // B_(t+n) = n * (B(t) + D_B x L_B(t) + A x B^2 - (F + K) x B)

    float aNew = n * (a + da * laplacianValue.r - a * b * b + feed * (1 - a));
    float bNew = n * (b + db * laplacianValue.g + a * b * b - (feed + kill) * b);

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

fragment float4 sim_visualizer(
        VertexOutPost in [[stage_in]],
        texture2d<float> simResult [[texture(0)]]) {
    constexpr sampler s(address::clamp_to_edge, filter::linear);
    float4 a = simResult.sample(s, in.uv);
    return float4(a.r, a.g, a.b, a.a) * 100.0f; // multiply to make it more visible. Temporarily use all channels while debugging
}

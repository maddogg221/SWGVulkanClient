// Trivial pass-through shader for wireframe boxes/grid/walls/minimap circles:
// transform by a precomputed world*view*proj matrix, output a flat per-draw
// color. No lighting/textures - the entire "wireframe only" visual style
// lives in drawing an explicit line-list mesh (see VulkanRenderer.cpp's
// kBoxVertices/kBoxIndices etc.), not in any rasterizer fill mode.
//
// Ported from the D3D11 crude visualizer's kShaderSource (same HLSL, same
// math) - only the per-draw constant delivery mechanism changed, from a
// D3D11 constant buffer (Map/Unmap per draw) to a Vulkan push constant
// block (worldViewProj + color = 80 bytes, comfortably under the
// spec-guaranteed minimum 128-byte push constant budget).
struct PerDrawConstants {
    float4x4 worldViewProj;
    float4 color;
};
[[vk::push_constant]] PerDrawConstants pc;

struct VSInput {
    float3 position : POSITION;
};

struct PSInput {
    float4 position : SV_POSITION;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), pc.worldViewProj);
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return pc.color;
}

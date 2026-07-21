// Solid-fill mesh shader - real geometry (parsed by assets::StaticMesh),
// filled triangles instead of wireframe lines. Real per-submesh textures
// (Phase 19) are sampled here and multiplied into the existing flat `color`
// - a submesh with no real resolved texture binds a shared 1x1 opaque-white
// fallback instead (see MeshHandle::textureDescriptorSet's own comment), so
// this shader has no separate "untextured" code path: sampling white is
// numerically identical to the old flat-color-only behavior. The other
// concession beyond a flat color: a fixed-direction half-lambert shade using
// the mesh's own real per-vertex normals - free once normals are already
// uploaded, and a genuine depth cue the wireframe boxes never had. Lighting
// itself is unchanged this pass - texturing was the scope, not relighting.
//
// Ported from the D3D11 crude visualizer's kMeshShaderSource unchanged, but
// delivered via a uniform buffer descriptor (set 0, binding 0) instead of a
// push constant: worldViewProj + world + color = 144 bytes, which exceeds
// the Vulkan spec's guaranteed-minimum 128-byte push constant budget (unlike
// Wireframe.hlsl/Label.hlsl, which fit comfortably and use push constants).
// The texture (set 1) is a SEPARATE descriptor set from the UBO (set 0)
// rather than an extra binding on the same set, since it changes every
// drawMesh() call (per-submesh) while the UBO set itself only changes once
// per frame-in-flight (per-draw data delivered via its own dynamic offset
// instead) - see VulkanRenderer.cpp's drawMesh() for the two separate
// vkCmdBindDescriptorSets calls this implies.
cbuffer PerDrawConstants : register(b0, space0) {
    float4x4 worldViewProj;
    float4x4 world;
    float4 color;
};

Texture2D meshTexture : register(t0, space1);
SamplerState meshSampler : register(s0, space1);

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldNormal : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    float3 lightDir = normalize(float3(0.4f, 1.0f, -0.3f));
    float shade = 0.5f + 0.5f * saturate(dot(normalize(input.worldNormal), lightDir));
    float4 texel = meshTexture.Sample(meshSampler, input.uv);
    return float4(texel.rgb * color.rgb * shade, texel.a * color.a);
}

// Billboarded, textured label quads (object name labels). Quad corners are
// computed fully in world space on the CPU side (see VulkanRenderer.cpp's
// drawLabel()), so this shader only needs viewProj, no separate per-draw
// world matrix - delivered as a push constant (64 bytes, well under the
// 128-byte guaranteed minimum). The label texture itself is bound as a
// combined image sampler descriptor (set 0, binding 0) - one descriptor set
// per distinct label texture, allocated once in createTextTexture() and
// reused every frame after, mirroring the D3D11 pass' one-SRV-per-texture
// caching.
struct LabelConstants {
    float4x4 viewProj;
};
[[vk::push_constant]] LabelConstants pc;

Texture2D labelTexture : register(t0, space0);
SamplerState labelSampler : register(s0, space0);

struct VSInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), pc.viewProj);
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return labelTexture.Sample(labelSampler, input.uv);
}

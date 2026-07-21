// Terrain chunk shader - vertex-colored (no textures this pass, matching
// the terrain plan's own "prove the pipeline first, materials later" cut,
// same as Mesh.hlsl's own precedent). Terrain vertex positions are baked
// in absolute world space (terrain::TerrainMesh::buildChunkMesh()), so
// unlike Mesh.hlsl there's no per-draw model matrix - just view*projection,
// small enough (64 bytes) to fit Wireframe.hlsl's push-constant pattern
// rather than Mesh.hlsl's dynamic-UBO indirection (that indirection exists
// for many instances sharing one mesh; terrain chunks are each unique,
// drawn once).
struct PerDrawConstants {
    float4x4 viewProj;
};
[[vk::push_constant]] PerDrawConstants pc;

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldNormal : TEXCOORD0;
    float3 color : TEXCOORD1;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), pc.viewProj);
    output.worldNormal = input.normal; // already world-space, no model matrix to transform by
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    // Same half-lambert shading as Mesh.hlsl, using the real per-vertex
    // normal (central-difference from the height field) instead of a flat
    // color - terrain is the first geometry this renderer draws with
    // dynamically-derived (not baked-in-the-asset) normals.
    float3 lightDir = normalize(float3(0.4f, 1.0f, -0.3f));
    float shade = 0.5f + 0.5f * saturate(dot(normalize(input.worldNormal), lightDir));
    return float4(input.color * shade, 1.0f);
}

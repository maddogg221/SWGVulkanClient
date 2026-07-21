#pragma once

#include <string>
#include <vector>

namespace assets {

// Reads a client ".sht" (shader/material) file - the format
// StaticMeshSubmesh::shaderFilename / SkeletalMeshSubmesh::shaderFilename
// point at. Real structure confirmed via a real, working third-party
// reference implementation (nostyleguy/io_scene_swg_msh's swg_types.py,
// SWGShader class - a Blender add-on that already generates real materials
// from real .sht files), cross-checked against real bytes extracted from
// this project's own client archives before trusting it (same "reference
// implementation plus real-data confirmation" methodology already used for
// `.msh`/`.pob`).
//
// Real root FORM tag is one of "SSHT" (the common case - a plain static
// shader), "CSHD" (a customizable/dyeable shader - wraps an inner FORM SSHT
// plus a palette-recoloring block this project doesn't need), or "SWTS"
// (rare, unsupported - not enough real samples found to justify chasing).
// Either way, once inside the real per-version FORM (e.g. "0000"/"0001"),
// texture references live in FORM TXMS > repeated FORM "TXM " (one per real
// texture slot), each holding a DATA chunk (an 11-byte field starting with a
// 4-byte ASCII slot tag stored BYTE-REVERSED on disk - confirmed against
// real bytes, e.g. a real "MAIN" slot's DATA chunk literally starts with
// "NIAM" - "MAIN" for the primary diffuse texture, "SPEC"/"NRML"/"CNRM"/
// "ENVM"/"EMIS"/"DETA"/"HUEB" for others this project doesn't use yet,
// followed by 7 more bytes not read here) and a NAME chunk
// (the real texture's archive path, e.g. "texture/foo_diffuse.dds"). A real
// per-vertex-color material block (FORM MATS) and several other real,
// empty-for-this-project's-purposes FORMs (TCSS/TFNS/ARVS/SRVS) are present
// but deliberately skipped - out of scope alongside every material property
// besides the primary diffuse texture (see the project plan's own explicit
// scope note).
struct ShaderTemplateData {
    std::string mainTextureFilename; // "MAIN" slot - the primary diffuse texture; empty if absent
};

class ShaderTemplate {
public:
    // Throws std::runtime_error only for genuinely malformed input (not a
    // FORM-rooted file, or an unrecognized root tag). A real shader with no
    // MAIN texture slot at all is not an error - callers see an empty
    // `mainTextureFilename` and fall back to flat-color rendering, same
    // per-part fallback convention used throughout this project.
    static ShaderTemplateData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets

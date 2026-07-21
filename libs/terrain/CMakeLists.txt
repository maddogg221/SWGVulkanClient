# The classic procedural terrain system (Phase 12, see PHASE_11_STATUS.md for
# the research this is built from). Deliberately zero rendering dependency,
# same principle as libs/assets - TerrainGenerator's height/color/shader
# queries are pure data, independently testable without Vulkan; libs/renderer
# only ever consumes the plain ChunkHeightData/TerrainMeshData structs this
# library hands back.
add_library(terrain STATIC
    src/TrnFile.cpp
    src/MultiFractal.cpp
    src/GeneratorItem.cpp
    src/LayerItem.cpp
    src/Boundary.cpp
    src/Filter.cpp
    src/Affector.cpp
    src/Layer.cpp
    src/Family.cpp
    src/TerrainGenerator.cpp
    src/ProceduralTerrainSource.cpp
    src/TerrainMesh.cpp
    src/TerrainChunkManager.cpp
)
add_library(terrain::terrain ALIAS terrain)

target_include_directories(terrain PUBLIC include)
# assets::assets: .trn is the same IFF container format .iff/.apt/.msh
# already use - reuses assets::IffReader/IffChunk directly, no second
# container parser. soe::soe: soe::PacketBuffer as the chunk byte reader,
# same reuse as libs/assets itself.
target_link_libraries(terrain PUBLIC assets::assets soe::soe)

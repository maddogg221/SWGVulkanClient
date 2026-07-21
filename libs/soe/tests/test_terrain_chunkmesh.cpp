// Verification for Phase 6: ProceduralTerrainSource, buildChunkMesh(), and
// TerrainChunkManager - pure data, no Vulkan.
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/TreArchive.h"
#include "terrain/ProceduralTerrainSource.h"
#include "terrain/TerrainChunkManager.h"
#include "terrain/TerrainMesh.h"

using namespace terrain;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
} // namespace

TEST_CASE("ProceduralTerrainSource::generateChunk - real naboo chunk near map center produces "
          "real height variation"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto source = ProceduralTerrainSource::parse(bytes, &archive);

    ChunkHeightData chunk = source.generateChunk(ChunkCoord{0, 0});
    CHECK(chunk.verticesPerSide == ProceduralTerrainSource::kTilesPerChunk + 1);
    CHECK(chunk.heights.size() == static_cast<size_t>(chunk.verticesPerSide) * chunk.verticesPerSide);
    CHECK(chunk.colors.size() == chunk.heights.size());

    float minHeight = chunk.heights[0];
    float maxHeight = chunk.heights[0];
    for (float h : chunk.heights) {
        minHeight = std::min(minHeight, h);
        maxHeight = std::max(maxHeight, h);
    }
    // Not asserting a specific range (no oracle for this exact chunk) -
    // just confirming the chunk isn't perfectly flat, i.e. real generator
    // variation is actually reaching the chunk data.
    CHECK(maxHeight > minHeight);
}

TEST_CASE("buildChunkMesh - real naboo chunk produces a well-formed grid mesh"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto source = ProceduralTerrainSource::parse(bytes, &archive);

    ChunkHeightData chunk = source.generateChunk(ChunkCoord{0, 0});
    TerrainMeshData mesh = buildChunkMesh(chunk);

    uint32_t n = chunk.verticesPerSide;
    CHECK(mesh.vertices.size() == static_cast<size_t>(n) * n);
    CHECK(mesh.indices.size() == static_cast<size_t>(n - 1) * (n - 1) * 6);

    for (uint32_t idx : mesh.indices) {
        CHECK(idx < mesh.vertices.size());
    }

    // Every normal should be a real unit vector, generally pointing
    // upward (positive Y) for open, non-vertical-cliff terrain.
    int upwardCount = 0;
    for (const auto& v : mesh.vertices) {
        float len = std::sqrt(v.normal.x * v.normal.x + v.normal.y * v.normal.y +
                               v.normal.z * v.normal.z);
        CHECK(len == doctest::Approx(1.0f).epsilon(0.01));
        if (v.normal.y > 0.0f) {
            ++upwardCount;
        }
    }
    CHECK(upwardCount > static_cast<int>(mesh.vertices.size()) / 2);
}

TEST_CASE("TerrainChunkManager - loads chunks around a center point and evicts far ones"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto source =
        std::make_shared<ProceduralTerrainSource>(ProceduralTerrainSource::parse(bytes, &archive));

    TerrainChunkManager manager(source, /*radiusInChunks=*/1);
    manager.update(0.0f, 0.0f);

    // radius 1 -> a 3x3 window of chunks.
    CHECK(manager.loadedChunks().size() == 9);

    float chunkWidth = source->chunkWidthInMeters();
    manager.update(chunkWidth * 100.0f, chunkWidth * 100.0f);
    CHECK(manager.loadedChunks().size() == 9);
    for (const auto& [coord, data] : manager.loadedChunks()) {
        (void)data;
        CHECK(coord.chunkX >= 99);
    }
}

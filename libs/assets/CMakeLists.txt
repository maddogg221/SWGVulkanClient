add_library(assets STATIC
    src/IffReader.cpp
    src/TreArchive.cpp
    src/SharedObjectTemplate.cpp
    src/AppearanceTemplate.cpp
    src/LodFile.cpp
    src/StaticMesh.cpp
    src/TgaImage.cpp
    src/Skeleton.cpp
    src/SkeletalMeshLod.cpp
    src/SkeletalMesh.cpp
    src/SkeletalAppearance.cpp
    src/BuildingLayout.cpp
    src/ShaderTemplate.cpp
    src/DdsImage.cpp
)
add_library(assets::assets ALIAS assets)

target_include_directories(assets PUBLIC include)
# Absolute path to libs/assets/data/ (the bundled static-template candidate
# list, mechanically extracted from Core3's object/static/**/*.lua scripts -
# see static_template_candidates.txt's own header comment). A dev-machine
# absolute path, not a redistributable-installer-safe one, matching this
# project's existing "single developer, same machine" scope (e.g. the
# visualizer's own --client-path default is likewise a hardcoded absolute
# path already).
target_compile_definitions(assets PUBLIC SWG_ASSETS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
# swg::zlib: TreArchive decompresses its record/filename tables (zlib, same
# as this project already depends on for SOE packet compression - see
# libs/soe/src/Compression.cpp). soe::soe: only for soe/MessageHash.h
# (header-only) and soe/PacketBuffer.h, reused here as the chunk byte reader
# rather than inventing a second one.
target_link_libraries(assets PUBLIC swg::zlib soe::soe)

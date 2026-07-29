add_library(worldmodel STATIC
    src/ObjectStore.cpp
    src/SingletonRegistry.cpp
    src/PortalVisibility.cpp
)
add_library(worldmodel::worldmodel ALIAS worldmodel)

target_include_directories(worldmodel PUBLIC include)
# assets::assets - PortalVisibility.cpp's real BuildingCell/PortalShape
# types (from a parsed .pob file), same real data
# tools/dummyclient's own building-rendering path already resolves. Pure
# CPU/data dependency - assets itself doesn't pull in Vulkan/renderer, so
# this doesn't make worldmodel Windows-only or GPU-dependent.
target_link_libraries(worldmodel PUBLIC swgproto::swgproto assets::assets)

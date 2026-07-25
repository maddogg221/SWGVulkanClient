# Windows-only, same reasoning as libs/renderer: needs DirectXMath (Windows
# SDK) for bone matrix math. Pure CPU computation - no Vulkan/rendering
# dependency itself, just the math types the renderer already uses so
# tools/dummyclient can hand it computed matrices directly.
if(NOT WIN32)
    message(FATAL_ERROR "libs/animation needs DirectXMath (Windows SDK) and is Windows-only for now.")
endif()

add_library(animation STATIC
    src/SkeletalPose.cpp
)
add_library(animation::animation ALIAS animation)

target_include_directories(animation PUBLIC include)
target_link_libraries(animation PUBLIC assets::assets)

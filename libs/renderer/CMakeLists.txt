# Win32/Vulkan - production rendering engine, DECIDED 2026-07-18 (see
# DISCOVERY.txt's "DIRECTION DISCUSSION") after the throwaway D3D11 crude
# visualizer plus the real-mesh-asset pipeline (libs/assets) gave the project
# its first real rendering experience. Windows-only today (Win32 windowing in
# Window.cpp) even though Vulkan itself is cross-platform - matches this
# project's current Windows-only scope everywhere else, not a Vulkan
# limitation.
if(NOT WIN32)
    message(FATAL_ERROR "libs/renderer is Vulkan-based (Win32 windowing) and Windows-only for now.")
endif()

add_library(renderer STATIC
    src/Window.cpp
    src/VulkanRenderer.cpp
    src/Camera.cpp
)
add_library(renderer::renderer ALIAS renderer)

target_include_directories(renderer PUBLIC include)
target_compile_definitions(renderer PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(renderer PUBLIC Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator gdi32 assets::assets terrain::terrain)

# Shaders stay HLSL (kept from the D3D11 pass almost unchanged) but compile
# to SPIR-V via DXC (bundled with the Vulkan SDK) instead of D3DCompile at
# build time rather than runtime, so a shader typo is a build failure, not a
# runtime exception in the middle of a live visualizer session.
find_program(DXC_EXECUTABLE dxc HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)

function(swg_add_hlsl_shader TARGET_NAME SHADER_FILE PROFILE ENTRY)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    set(SPIRV_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}_${ENTRY}.spv")
    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND ${DXC_EXECUTABLE} -spirv -T ${PROFILE} -E ${ENTRY} -Fo ${SPIRV_OUTPUT} ${SHADER_FILE}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling ${SHADER_FILE} (${ENTRY}) to SPIR-V"
        VERBATIM
    )
    target_sources(${TARGET_NAME} PRIVATE ${SPIRV_OUTPUT})
endfunction()

swg_add_hlsl_shader(renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Wireframe.hlsl" vs_6_0 VSMain)
swg_add_hlsl_shader(renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Wireframe.hlsl" ps_6_0 PSMain)
swg_add_hlsl_shader(renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Label.hlsl" vs_6_0 VSMain)
swg_add_hlsl_shader(renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Label.hlsl" ps_6_0 PSMain)
swg_add_hlsl_shader(renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Mesh.hlsl" vs_6_0 VSMain)
swg_add_hlsl_shader(renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Mesh.hlsl" ps_6_0 PSMain)
swg_add_hlsl_shader(renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Terrain.hlsl" vs_6_0 VSMain)
swg_add_hlsl_shader(renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Terrain.hlsl" ps_6_0 PSMain)

target_compile_definitions(renderer PRIVATE SWG_SHADER_SPV_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders")

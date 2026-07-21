cmake_minimum_required(VERSION 3.21)

project(SWGClientNew LANGUAGES CXX)

# Pin the MSVC v143 (Visual Studio 2022) toolset. This project is developed across
# multiple machines and must stay openable/buildable in VS2022 on all of them, even
# though newer toolsets (e.g. VS2026's) may also be installed locally. Configure with
# the "windows-vs2022" CMakePresets.json preset, which passes -T v143 explicitly.
if(MSVC)
    if(NOT DEFINED MSVC_TOOLSET_VERSION OR NOT MSVC_TOOLSET_VERSION EQUAL 143)
        message(FATAL_ERROR
            "This project pins the MSVC v143 (Visual Studio 2022) toolset for consistency "
            "across machines/IDEs. Detected toolset: ${MSVC_TOOLSET_VERSION}. "
            "Configure with: cmake --preset windows-vs2022 "
            "(or pass -T v143 explicitly if invoking the Visual Studio generator directly).")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SWG_BUILD_TESTS "Build unit tests" ON)

include(FetchContent)

# Some FetchContent'd third-party projects (doctest 2.4.11, older zlib tags)
# declare a `cmake_minimum_required` floor below 3.5, which recent CMake
# versions (4.0+) refuse to evaluate policies for. This only relaxes policy
# compatibility for those subbuilds, not our own project.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(SWG_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)
endif()

# zlib: prefer a system install, fall back to building from source.
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    add_library(swg_zlib INTERFACE)
    target_link_libraries(swg_zlib INTERFACE ZLIB::ZLIB)
else()
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.1
    )
    FetchContent_MakeAvailable(zlib)

    add_library(swg_zlib INTERFACE)
    target_link_libraries(swg_zlib INTERFACE zlibstatic)
    # zlib.h lives in the fetched source dir; zconf.h is generated into the binary dir.
    target_include_directories(swg_zlib INTERFACE "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")
endif()
add_library(swg::zlib ALIAS swg_zlib)

# Asio (standalone, no Boost) - async I/O for SoeSession's UDP socket handling.
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
)
FetchContent_MakeAvailable(asio)

add_library(swg_asio INTERFACE)
target_include_directories(swg_asio INTERFACE "${asio_SOURCE_DIR}/asio/include")
target_compile_definitions(swg_asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
if(WIN32)
    target_compile_definitions(swg_asio INTERFACE _WIN32_WINNT=0x0A00)
    target_link_libraries(swg_asio INTERFACE ws2_32 mswsock)
endif()
add_library(swg::asio ALIAS swg_asio)

add_subdirectory(libs/soe)
add_subdirectory(libs/swgproto)
add_subdirectory(libs/clientcommon)
add_subdirectory(libs/worldmodel)
add_subdirectory(libs/assets)
add_subdirectory(libs/terrain)
if(WIN32)
    # Vulkan is the decided production rendering engine (2026-07-18, see
    # DISCOVERY.txt's "DIRECTION DISCUSSION"), replacing the throwaway D3D11
    # crude-visualizer backend. Unlike D3D11 (ships with the Windows SDK
    # already required by this project), the Vulkan SDK is a separate,
    # required local install (https://vulkan.lunarg.com/sdk/home#windows) -
    # find_package fails loudly with a clear message if it's missing rather
    # than silently skipping the renderer.
    find_package(Vulkan REQUIRED)

    # Vulkan Memory Allocator (VMA) - single-purpose header-only library that
    # replaces Vulkan's fully-manual memory-type-selection/allocation/binding
    # dance for every buffer/image this renderer creates (box/grid/wall/
    # circle vertex buffers, per-mesh vertex/index buffers, label textures,
    # the depth buffer). FetchContent'd like doctest/zlib/asio above rather
    # than requiring a second separate SDK install.
    FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.1.0
    )
    FetchContent_MakeAvailable(vma)

    add_subdirectory(libs/renderer)
endif()
add_subdirectory(tools/dummyclient)
add_subdirectory(tools/pcapdecoder)

# Bridges worldmodel::CreatureObject (real, already-decoded object state)
# to assets::AnimationSelectionContext/a real .lat state name. Needs neither
# libs/animation (the pose-MATH library, a separate concern) nor Vulkan/
# rendering, so unlike animation/renderer this is NOT Windows-gated - see
# the root CMakeLists.txt's own placement comment.
add_library(creatureanim STATIC
    src/CreatureState.cpp
    src/AnimationContext.cpp
    src/AnimationStateSelection.cpp
)
add_library(creatureanim::creatureanim ALIAS creatureanim)

target_include_directories(creatureanim PUBLIC include)
target_link_libraries(creatureanim PUBLIC worldmodel::worldmodel assets::assets)

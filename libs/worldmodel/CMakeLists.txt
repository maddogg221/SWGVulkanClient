add_library(worldmodel STATIC
    src/ObjectStore.cpp
    src/SingletonRegistry.cpp
)
add_library(worldmodel::worldmodel ALIAS worldmodel)

target_include_directories(worldmodel PUBLIC include)
target_link_libraries(worldmodel PUBLIC swgproto::swgproto)

add_library(clientcommon STATIC
    src/HexDump.cpp
    src/ObjControllerHandlers.cpp
)
add_library(clientcommon::clientcommon ALIAS clientcommon)

target_include_directories(clientcommon PUBLIC include)
target_link_libraries(clientcommon PUBLIC soe::soe swgproto::swgproto)

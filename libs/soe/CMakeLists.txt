add_library(soe STATIC
    src/PacketBuffer.cpp
    src/Crc32.cpp
    src/XorCipher.cpp
    src/Compression.cpp
    src/SoeSession.cpp
    src/MessageDispatcher.cpp
    src/FragmentReassembler.cpp
)
add_library(soe::soe ALIAS soe)

target_include_directories(soe PUBLIC include)
target_link_libraries(soe PUBLIC swg::zlib swg::asio)

if(SWG_BUILD_TESTS)
    add_subdirectory(tests)
endif()

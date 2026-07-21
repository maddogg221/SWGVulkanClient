add_executable(pcapdecoder main.cpp)
target_link_libraries(pcapdecoder PRIVATE soe::soe swgproto::swgproto clientcommon::clientcommon)

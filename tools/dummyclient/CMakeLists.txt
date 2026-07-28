add_executable(dummyclient main.cpp Visualizer.cpp PngWriter.cpp
    AnimationDebugControls.cpp ScreenshotCapture.cpp RestPoseAutoTest.cpp
    AnimationDiagnosticLogging.cpp)
target_link_libraries(dummyclient PRIVATE soe::soe swgproto::swgproto clientcommon::clientcommon worldmodel::worldmodel)
if(WIN32)
    target_link_libraries(dummyclient PRIVATE renderer::renderer assets::assets terrain::terrain animation::animation creatureanim::creatureanim)
endif()

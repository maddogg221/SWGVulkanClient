#pragma once

// Windows/Vulkan only - see Visualizer.cpp's own top-of-file comment. This
// header is safe to include unconditionally; runVisualizer() itself only
// exists when _WIN32 is defined, matching main.cpp's existing #ifdef guard
// around its one call site.
#ifdef _WIN32

#include <string>

namespace soe {
class SoeSession;
class MessageDispatcher;
}
namespace swgproto {
class ObjControllerDispatcher;
}
namespace worldmodel {
class ObjectStore;
}

// The crude wireframe/real-mesh visualizer (Windows/Vulkan only - see
// libs/renderer). Extracted out of main.cpp 2026-07-18 (was ~640 lines
// embedded in a 2200+-line file) - this is application-level glue specific
// to dummyclient, not general-purpose enough for libs/renderer or
// libs/assets themselves, so it stays in tools/dummyclient rather than
// becoming a new library.
void runVisualizer(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher, bool& failed,
                    swgproto::ObjControllerDispatcher& objControllerDispatcher,
                    const worldmodel::ObjectStore& objectStore, const std::string& clientPath,
                    const std::string& terrainName);

#endif

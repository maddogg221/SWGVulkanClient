# How to Build and Run

## Before you start

This project does **not** include, and will never include, any of Star Wars Galaxies'
own copyrighted game content (models, textures, sounds, terrain data, etc.). To see
real 3D rendering (rather than placeholder wireframe boxes), you need:

- **A legitimate copy of the original SWG client's data files**, installed locally on
  your own machine. This project only *reads* files from an install you already have -
  it never downloads, bundles, or redistributes any of it.
- **Your own Core3 server** (or access to someone else's) to connect to. This project
  doesn't run or provide a server - see [SWGEmu](https://www.swgemu.com/) and
  [Core3](https://github.com/swgemu/Core3) to set one up yourself.

If you don't have either of those, the code still builds and the test suite still
runs (real-client-dependent tests are automatically skipped, not failed, when no
client install is found), but you won't get a live visual result.

## Prerequisites

- **Windows** (the renderer and visualizer are Windows-only; the protocol/asset-parsing
  libraries themselves are portable C++20, but this project hasn't been built/tested
  on any other OS).
- **Visual Studio 2022**, with the MSVC v143 toolset installed.
- **CMake 3.21 or newer.**
- **The Vulkan SDK** ([vulkan.lunarg.com](https://vulkan.lunarg.com/sdk/home#windows)) -
  the one dependency you need to install yourself; required only for the rendering/
  visualizer pieces (`libs/renderer`, `tools/dummyclient`'s `--visualize` mode).

Everything else - zlib, standalone Asio, doctest, and Vulkan Memory Allocator - is
fetched automatically by CMake (`FetchContent`) the first time you configure the
project. No manual dependency installation needed beyond the Vulkan SDK.

**Clone to a short path.** This project's build tree nests deeply (fetched
dependencies, generated object files, etc.), and Windows' classic `MAX_PATH` (260
character) limit is real - cloning somewhere like `C:\dev\swgcn` will save you a real
headache versus a long, deeply-nested path (e.g. somewhere under a long
`Documents\Projects\...` tree, or a temp/download folder several directories deep).
If you hit `MSB6003`/"could not be run"/"DirectoryNotFoundException" errors during
the build, this is almost certainly why - move the whole tree somewhere shorter and
reconfigure.

## Building

From the repository root, in a Visual Studio Developer environment (or any shell
with `cmake`/`msbuild` on `PATH`):

```
cmake --preset windows-vs2022
cmake --build build/vs2022
```

The first configure step will take a few minutes while dependencies are fetched and
built. Subsequent builds are fast.

## Running the test suite

```
ctest --preset windows-vs2022
```

Or run the test binary directly for more detailed output:

```
build\vs2022\libs\soe\tests\Debug\soe_tests.exe
```

Most tests run with no external dependencies at all. A real subset (asset-format
parsers) is gated on a real client install being present at the default path
(`C:\Program Files (x86)\StarWarsGalaxies`) - those tests are automatically **skipped**,
not failed, if that path doesn't exist on your machine.

## Running `dummyclient`

Headless mode (login, decode, and print world state - no rendering):

```
build\vs2022\tools\dummyclient\Debug\dummyclient.exe --host <server> --port <port> --username <user> --password <pass>
```

Live 3D visualizer mode (Windows/Vulkan only):

```
build\vs2022\tools\dummyclient\Debug\dummyclient.exe --host <server> --port <port> --username <user> --password <pass> --visualize
```

If your SWG client install isn't at the default path above, pass it explicitly:

```
--client-path "D:\Games\StarWarsGalaxies"
```

`dummyclient` performs the full login → character-select → zone-in sequence, then
either prints a live structured decode of every object/message it observes
(headless mode) or opens a real-time 3D view of the world (`--visualize`). WASD moves
your character once you've zoned in; hold the right mouse button and scroll to orbit/
zoom the camera.

See `tools/dummyclient/main.cpp`'s own CLI argument parsing for the full set of
options (character creation, command sending, and a few other diagnostic modes).

## Something not working?

This is real, in-progress software with known rough edges - see the README's "In
progress / deliberately deferred" section for what's genuinely not built yet before
assuming something is broken. If you've confirmed it's a real bug, see
[`COMMUNITY.md`](COMMUNITY.md) for how (and how not) to reach out about it.

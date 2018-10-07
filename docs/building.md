# Building ShaderSim

## Requirements

- a C99 compatible C-compiler. Tested with:
  - GCC 7.3 on Linux
  - Visual Studio 2015 and 2017 on Windows
- CMake (version 3.1 or newer)
- (optional) Emscripten to build the browser front-end

## Building the CLI front-end

ShaderSim uses the CMake build system, all you need is the traditional CMake workflow.
On Linux it would be something like this:

```bash
git clone --recursive https://github.com/JohanSmet/shader_sim.git
cd shader_sim
mkdir build
cmake ..
make
```

Adapt for your platform/compiler/ide.

## Building the browser front-end

Requires a working Emscripten installation.

```bash
git clone --recursive https://github.com/JohanSmet/shader_sim.git
cd shader_sim/webui
mkdir wasm
emcmake cmake ..
emmake make
```

This builds the WebAssembly and glue-files in a location that is easily accessible from the `webui` JavaScript application.
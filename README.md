# ShaderSim

ShaderSim is an instruction-level simulator for GPU 'shaders'. The primary intention is to help debug/validate a shader and to visualize and help understand the execution of the shader. More attention has been given to the clarity and maintainability of the code, run-time performance comes secondary in this respect.

At the moment only (a subset of) of SPIR-V has been implemented. Support for other, compiled, shader formats (HLSL, Metal) might be added in the future.

## Why ?

The short answer is that it seemed like a fun way to spend some free time.

A slightly longer answer would be it's a combination of events. I was playing around with using SPIR-V shaders in OpenGL, and having some trouble figuring out what was going on in the shaders. At the same time I was kinda watching [bitwise](https://github.com/pervognsen/bitwise) building computer systems from scratch. So I thought it would be fun to try and write a SPIR-V simulator. I decided to use this opportunity to brush op on C.

It's definitely not ground breaking and might not be useful to many people. But I had a lot of fun and learned quite a bit while doing this.

## Structure

The core of ShaderSim is implemented as a C99 library, with minimal external dependencies.

There's a command-line front-end that can be used to verify expected outputs from given inputs. The parameters of the execution are specified through a JSON file.

A browser-based front-end can be used to step through the shader instruction by instruction and inspect the state of the internal registers and the outputs. This front-end runs entirely on the client-side of the browser, no communication with an external server is required once the program is loaded. You can test it out now by following [this link](https://johansmet.github.io/shader_sim/).

## License

This project is licensed under the 3-clause BSD License. See [LICENSE](LICENSE) for the full text.

## Acknowledgements

The browser front-end would not have been possible without the excellent [Emscripten](http://emscripten.org) project.

The unit tests were written using the [µnit](https://nemequ.github.io/munit) framework.

## More information

- [Building](docs/building.md)
- [Usage](docs/usage.md)
This is a Quake II game DLL that loads "game.wasm" compiled WebAssembly data. It can also load AoT-compiled wasm, using the wamrc from https://github.com/bytecodealliance/wasm-micro-runtime

You'll need to set up a build of WAMR to be able to compile this.

# Compiling Game WASMs
To compile a Quake II game DLL source repo into the .wasm format, you will need, at the very least, clang (I believe llvm 11.0 is minimum requirement here) and `wasi-sysroot` from  https://github.com/WebAssembly/wasi-sdk/releases (+ the lib tarball, as that will be required for linking the builtins).

Then, it is as simple as running the following command:

`clang -mbulk-memory --target=wasm32-wasi --sysroot=wasi-sysroot -Wl,--no-entry -I./src -o "./game.wasm" src/*.c wasm.c`

where `src` is the folder that contains the mod source you're trying to compile. You must also either copy `wasm.c` and `api.h` from this repo (in the game subfolder) to your game source, or include it in the command line like I do for mine.

This will do a 'debug' build of the source. Including `-Os` will enable optimizations.

The interpreter is pretty fast; in my tests, it is much faster than QuakeC's interpreter.

AoT is also supported. If "game.aot" is found in the folder, it will attempt to load that instead. To compile an AoT file, you'll need `wamrc.exe` from WASM Micro Runtime.
`wamrc --enable-bulk-memory --target=i386 -o game.aot game.wasm`

The AoT-compiled code is much, much faster. I've yet to get JIT stable enough to test, but it bloats the DLL size too much for it to be something to seriously consider at the moment.

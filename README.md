# A Compiler for the Oberon-0 Programming Language

## About This Project

This project is a compiler for the [Oberon-0](#about-oberon-0) programming language. The compiler was initially created by [Max Galetskiy](https://github.com/max-galetskiy) and me as part of the **"Compiler Construction"** course at the [University of Konstanz](https://www.uni-konstanz.de/en/). This course was taught by **Prof. Dr. Michael Grossniklaus**, who also developed a more comprehensive [Oberon compiler](https://github.com/zaskar9/oberon-lang). Max is also extending the project in this [oberon0c](https://github.com/max-galetskiy/oberon0c) repository.

Our compiler was originally built as a command-line application (CLI) and uses LLVM for code generation. In this repository, I am extending the compiler by adding a **WebAssembly (WASM)** backend, enabling it to run directly in the browser.

## About Oberon-0

Oberon-0 is a simplified subset of the [Oberon](https://oberon.org/en) programming language, designed by [Niklaus Wirth](https://people.inf.ethz.ch/wirth/) as a successor to Pascal and Modula-2. It is a strongly typed, imperative language that emphasizes modularity and simplicity. This project aims to provide a fully functional Oberon-0 compiler for various execution environments.

## Compilation Targets

### CLI Version

The CLI version of the compiler transforms Oberon-0 source code into LLVM Intermediate Representation (LLVM IR), which can be further compiled into executable binaries.

#### Dependencies

To build the CLI version, the following dependencies are required:
- [Boost](https://www.boost.org/) (for general utility functions)
- [CMake](https://cmake.org/) (for project configuration and build management)
- [LLVM](https://llvm.org/) (for intermediate representation handling)

#### Build Instructions

```bash
mkdir build && cd build
cmake ..
make
```

This will produce the `oberon0c` executable.

### WebAssembly Version

The WebAssembly version of the compiler enables execution within a browser or other WASM runtimes.

#### Dependencies

To build the WASM version, the following dependencies are required:
- [Boost](https://www.boost.org/) (use installation below)
- [Make](https://www.gnu.org/software/make/)
- [Emscripten](https://emscripten.org/) (for compiling to WASM)

#### Install Boost

```bash
wget -O boost.tar.xz https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-b2-nodocs.tar.xz
tar xf boost.tar.xz 
mv boost-1.86.0 ./boost
rm boost.tar.xz
```

Then, export the environment variable:

```bash
export BOOST_ROOT=./boost
```

#### Install Emscripten
```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

#### Compile to WebAssembly

```bash
mkdir build && cd build
emcmake cmake ..
make
```

#### Compile with jonah

```bash
curl -L -o jonah https://github.com/elias-2001-de/jonah/releases/download/v0.1.8/jonah
chmod +x jonah
./jonah project jonah_editor.toml dist
```

This will produce the `wasm_lib.wasm` and `wasm_lib.js` files needed to run the compiled Oberon-0 programs in a browser or a WASM runtime.

### Running the WebAssembly Compiler

To test the compiled WebAssembly module, open `index.html` in a browser, or run:

```bash
python3 -m http.server
```

then go to [localhost:8000](http://localhost:8000/)
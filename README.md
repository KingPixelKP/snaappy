# snaappy

A C++ snapshot testing library

This project was generated in `hybrid` mode.

## Features

* `CMakePresets.json` with presets for:
  * `Debug`
  * `RelWithDebInfo`
  * Address/Undefined Sanitizers (`ASan`/`UBSan`)
  * `clang-tidy`
* Reusable `libs/` layout for project libraries
* Per-package `example/` and `benchmark/` discovery for optional example and benchmark binaries
* Reusable `package()` and `workspace()` CMake helpers for component and workspace discovery
* Nix flake that pins the toolchain, every library and the CMake helpers in `flake.lock`
* Sandboxed `nix build` (shared) and `nix build .#static` (fully static, musl) packages
* GoogleTest integration for unit testing
* Optional `clang-tidy` and `cppcheck` support
* `format` build target when `clang-format` is available
* Extra `tsan` and `coverage` CMake presets for concurrency debugging and coverage runs

* Optional Doxygen documentation support


* Nix-based GitHub Actions and GitLab CI templates


## Requirements

Only the [Nix](https://nixos.org/download/) package manager, on any Linux distribution, macOS, or
WSL2 (NixOS is not required). Flakes must be enabled:

```text
# ~/.config/nix/nix.conf
experimental-features = nix-command flakes
```

The [Determinate Nix installer](https://determinate.systems/nix-installer/) enables flakes by default.

Everything else (compiler, CMake, Ninja, libraries and the
[Cookie-Cmake](https://github.com/KingPixelKP/Cookie-Cmake) helpers) comes from the flake and is
pinned by hash in `flake.lock`.

> [!IMPORTANT]
> Flakes only see files tracked by git. `git add` new files before running `nix build` or
> `nix develop`, or Nix will not find them.

## Nix Builds

```bash
nix build              # shared build, runs ctest in the sandbox; output in ./result
nix build .#static     # fully static (musl) build for release
nix flake check        # builds and tests the packages, checks C/C++ formatting
nix fmt                # format tracked Nix and C/C++ files in place
nix flake update       # move every pinned input (nixpkgs, Cookie-Cmake) forward
```

`.#static` exists only on Linux, because `pkgsStatic` (musl) does not support macOS.

## Development

The [`Makefile`](./Makefile) wraps the presets and flake outputs, and works from any shell. Outside
the dev shell, each command runs through `nix develop` for you:

```bash
make            # list targets
make build      # configure (first time) and build the debug preset
make test       # build and run the tests
make test FILTER=Vec              # only tests whose name matches
make test PRESET=asan
make run        # build and run the main executable
make run ARGS="--help"            # pass arguments
make run EXAMPLE=vector_example   # run an example (or BIN=<name> for bin/ executables)
make bench      # release build with benchmarks, runs them all (BENCH=<name> for one)
make coverage   # tests with coverage, HTML report + summary (gcovr)
make add DEP=spdlog               # add a library, like `cargo add`
make format     # nix fmt
make check      # nix flake check, same as CI
make static     # fully static release build into ./result
```

To run CMake directly, enter the dev shell. It provides the build dependencies plus `clang-tools`,
`gdb`, `gcovr` and Google Benchmark, and exports `COOKIE_CMAKE_DIR` for CMake:

```bash
make shell      # or: nix develop --command $SHELL
cmake --preset debug
```

Running `cmake` outside the dev shell fails because CMake cannot find Cookie-Cmake or the libraries.
Launch your editor from the dev shell so CMake Tools and clangd see the same environment.

### direnv

With [direnv](https://direnv.net/) (ideally with [nix-direnv](https://github.com/nix-community/nix-direnv)
for caching), the included `.envrc` loads the dev shell whenever you `cd` into the project:

```bash
direnv allow
```

direnv only runs if its hook is in your shell config. See
[hook setup](https://direnv.net/docs/hook.html): for example, `direnv hook fish | source` in
`~/.config/fish/config.fish`, or `eval "$(direnv hook zsh)"` in `~/.zshrc`. Without the hook,
`cd` loads nothing and CMake uses the system compiler.

VSCode prompts to install the recommended extensions from
[`.vscode/extensions.json`](./.vscode/extensions.json). The direnv extension loads the same
environment into the editor, so clangd and CMake Tools work without launching from a shell.

On first entry it also runs `make configure` (the debug preset), so `compile_commands.json` is linked into the
project root for clangd. Editors with a direnv extension pick up the same environment.

The default project uses [`fmt`](https://github.com/fmtlib/fmt), plus GoogleTest and Google Benchmark
when tests and benchmarks are enabled. Examples and benchmarks are discovered from each package's
`example/` and `benchmark/` directories.

## Adding Libraries

Libraries come from [nixpkgs](https://search.nixos.org/packages) and are listed once, in
[`deps.json`](./deps.json). Nix installs every entry (see [`package.nix`](./package.nix)) and
Cookie-Cmake's `dependencies()` runs `find_package` for it:

```json
{
  "fmt": { "package": "fmt" },
  "gtest": { "package": "GTest", "kind": "test" },
  "gbenchmark": { "package": "benchmark", "kind": "bench" }
}
```

The key is the nixpkgs attribute, `package` is the `find_package` name, and `kind` is `build`
(default), `test` (only with tests) or `bench` (only with benchmarks, and in the dev shell).

Packages split into components (Qt, Boost) list the ones you need, which become
`find_package(... COMPONENTS ...)`:

```json
"qt6.qtbase": { "components": ["Core", "Widgets"], "package": "Qt6" }
```

An optional `version` guards against `nix flake update` moving a library somewhere you did not
expect. It takes a minimum (`"11.1"`) or a range (`"11...<13"`), and configuring fails when the
locked nixpkgs version does not satisfy it:

```json
"fmt": { "package": "fmt", "version": "11...<13" }
```

It checks the version; it does not choose one. Every library comes from the single nixpkgs
revision in `flake.lock`, so there is one version per library, as with a `Cargo.lock`.

`make add` fills in the entry for you. It builds the package from the locked nixpkgs, finds its
CMake package name and lists the targets it exports:

```bash
make add DEP=spdlog
make add DEP=catch2_3 KIND=test
make add DEP=qt6.qtbase PACKAGE=Qt6 COMPONENTS="Core Widgets"
make add DEP=pugixml VERSION="1.14...<2"
```

Then link a target from `package(...)`, for example `DEPS spdlog::spdlog`.

Common libraries:

| nixpkgs attribute | `find_package` name | Target |
| --- | --- | --- |
| `fmt` | `fmt` | `fmt::fmt` |
| `gtest` | `GTest` | `GTest::gtest_main` |
| `gbenchmark` | `benchmark` | `benchmark::benchmark` |
| `spdlog` | `spdlog` | `spdlog::spdlog` |
| `abseil-cpp` | `absl` | `absl::<component>` |
| `cli11` | `CLI11` | `CLI11::CLI11` |
| `entt` | `EnTT` | `EnTT::EnTT` |
| `nlohmann_json` | `nlohmann_json` | `nlohmann_json::nlohmann_json` |
| `pugixml` | `pugixml` | `pugixml::pugixml` |
| `raylib` | `raylib` | `raylib` |
| `yaml-cpp` | `yaml-cpp` | `yaml-cpp::yaml-cpp` |

Library versions follow the nixpkgs revision in `flake.lock`. To pin a specific version, override
the package in [`flake.nix`](./flake.nix) with `overrideAttrs` and a `fetchFromGitHub` source.

## Project Layout

To add a new library package, create a new folder under [`libs/`](./libs/) with a minimal
[`CMakeLists.txt`](./libs/math/CMakeLists.txt) that calls `package(...)`, then add `src/`,
`include/`, `test/`, `example/`, or `benchmark/` directories as needed.

If a package needs runtime assets, add a sibling `data/` directory under `bin/`, `test/`,
`example/`, or `benchmark/`. The `package()` helper copies those directories into the matching
build output folder so the executables can load assets with stable relative paths.

Build:

```bash
cmake --build --preset debug
```

Run the test suite:

```bash
ctest --preset debug
```

Additional presets:

```bash
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan

cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage

cmake --build --preset coverage --target coverage-report
```

When `gcovr` is available, the `coverage-report` target writes HTML and XML
reports under `.build/coverage/coverage/`. `make coverage` does all of the above in one step, starts
from fresh counters, and prints the line and branch coverage summary.


## Documentation

Build the Doxygen docs with:

```bash
make docs       # or: cmake --build --preset debug --target snaappy_docs
```

The HTML output lands in `.build/debug/docs/html/index.html`.



## Install And Package Export

Install the project to a local prefix with:

```bash
cmake --preset release
cmake --build --preset release
cmake --install .build/release --prefix ./install
```


Consumers can then use:

```cmake
find_package(snaappy CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE snaappy::math)
```




## Safety Checks

Every target builds with strict warnings as errors (`-Wall -Wextra -Wpedantic -Werror`, plus
conversion, shadowing, old-style-cast and similar checks from Cookie-Cmake). Debug builds, and the
`asan`, `tsan` and `coverage` presets built on them, also enable libstdc++ assertions: an
out-of-range `operator[]` or `front()` on an empty container aborts with a message instead of
reading past the end, much like a Rust panic.

## Sanitizers

Configure and build with AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

## Static Analysis

Configure and build with `clang-tidy` enabled:

```bash
cmake --preset clang-tidy
cmake --build --preset clang-tidy
```

## Renaming the Project

If you change the project name in the `project(...)` declaration, update any project-specific cache variables in [`CMakePresets.json`](./CMakePresets.json) and [`package.nix`](./package.nix) (for example, `CPPTEMPLATE_SANITIZERS` and `CPPTEMPLATE_ENABLE_CLANG_TIDY`).

A quick way to do this is:

```bash
sed -i 's/cpptemplate/myproject/g; s/CPPTEMPLATE/MYPROJECT/g' CMakeLists.txt CMakePresets.json package.nix
```

> [!NOTE]
> Replace `cpptemplate` and `CPPTEMPLATE` with your project's lowercase and uppercase names, respectively.

## Authors

* KingPixelKP


## License

See [`LICENSE`](./LICENSE).


## Acknowledgements

This project was generated from the **CppCookieCutter**.

The template repository contains updates, additional generators, and documentation:

* <https://github.com/KingPixelKP/CppCookieCutter.git>

The CMake function directory used to provide versioned CMake helpers is:

* <https://github.com/KingPixelKP/Cookie-Cmake.git>
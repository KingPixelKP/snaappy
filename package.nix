{
  lib,
  stdenv,
  pkgs,
  cmake,
  ninja,
  cookie-cmake,
}:

let
  # deps.json is the single dependency manifest: its keys are nixpkgs attribute
  # paths (e.g. "fmt", "qt6.qtbase"), and Cookie-Cmake's dependencies() runs
  # find_package() for the same entries.
  deps = lib.importJSON ./deps.json;
  depsOfKind =
    kinds:
    map (name: lib.getAttrFromPath (lib.splitString "." name) pkgs) (
      builtins.filter (name: builtins.elem (deps.${name}.kind or "build") kinds) (builtins.attrNames deps)
    );
in
stdenv.mkDerivation {
  pname = "snaappy";
  version = "0.1.0";

  src = lib.cleanSource ./.;

  nativeBuildInputs = [
    cmake
    ninja
  ]; # build-time tools
  buildInputs = depsOfKind [
    "build"
    "test"
  ]; # libraries to link against

  cmakeFlags = [
    "-DCOOKIE_CMAKE_DIR=${cookie-cmake}"
    "-DSNAAPPY_BUILD_DOCS=OFF"
    "-DSNAAPPY_BUILD_BENCHMARKS=OFF"
    "-DSNAAPPY_INSTALL=ON" # populate $out
  ];

  doCheck = true; # runs ctest during `nix build`

  # Benchmark libraries are only needed in the dev shell.
  passthru.benchInputs = depsOfKind [ "bench" ];
}

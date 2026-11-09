{
  description = "snaappy: C++23 + CMake project with a shared dev build and a static release build";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # CMake helpers providing package(), workspace() and the tooling targets.
    cookie-cmake = {
      url = "github:KingPixelKP/Cookie-Cmake";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      cookie-cmake,
    }:
    let
      lib = nixpkgs.lib;
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];
      forAllSystems = f: lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # C/C++ sources checked by `nix flake check` and rewritten by `nix fmt`.
      cxxGlob = [
        "*.c"
        "*.cc"
        "*.cpp"
        "*.cxx"
        "*.h"
        "*.hh"
        "*.hpp"
        "*.hxx"
        "*.ipp"
      ];
    in
    {
      packages = forAllSystems (
        pkgs:
        {
          default = pkgs.callPackage ./package.nix { inherit cookie-cmake; }; # shared, for devs
        }
        // lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
          # fully static (musl), for release; pkgsStatic only works on Linux
          static = pkgs.pkgsStatic.callPackage ./package.nix { inherit cookie-cmake; };
        }
      );

      # `nix fmt`: format tracked Nix and C/C++ files in place.
      formatter = forAllSystems (
        pkgs:
        pkgs.writeShellApplication {
          name = "format";
          runtimeInputs = [
            pkgs.git
            pkgs.nixfmt
            pkgs.clang-tools
          ];
          text = ''
            git ls-files -z -- '*.nix' | xargs -0 -r nixfmt
            git ls-files -z -- ${lib.escapeShellArgs cxxGlob} | xargs -0 -r clang-format -i
          '';
        }
      );

      # `nix flake check`: build and test every package, and check C/C++ formatting.
      checks = forAllSystems (
        pkgs:
        self.packages.${pkgs.stdenv.hostPlatform.system}
        // {
          format = pkgs.runCommand "check-format" { nativeBuildInputs = [ pkgs.clang-tools ]; } ''
            cd ${self}
            find . \( ${lib.concatMapStringsSep " -o " (g: "-name '${g}'") cxxGlob} \) -print0 \
              | xargs -0 -r clang-format --dry-run -Werror
            touch $out
          '';
        }
      );

      devShells = forAllSystems (
        pkgs:
        let
          package = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ package ]; # same deps as the build

            # dev-only extras, plus the "bench" entries from deps.json
            packages = [
              pkgs.clang-tools
              pkgs.gdb
              pkgs.gcovr
              pkgs.doxygen
              # Cookie-Cmake links with lld when it finds one; provide Nix's
              # wrapped lld so a host /usr/bin/ld.lld isn't used, which would
              # skip the store-path RPATHs (gtest, fmt, libstdc++).
              pkgs.llvmPackages.bintools
            ]
            ++ package.benchInputs;

            COOKIE_CMAKE_DIR = "${cookie-cmake}";
          };
        }
      );
    };
}

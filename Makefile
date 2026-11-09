# Thin wrapper over the CMake presets and flake outputs, so every command works
# from any shell without direnv. Build logic lives in CMakePresets.json and flake.nix.
#
#   make test PRESET=asan FILTER=Vec
#   make run EXAMPLE=vector_example ARGS="--flag"

PRESET ?= debug
BUILD_DIR := .build/$(PRESET)
BIN ?= snaappy
KIND ?= build

# Inside `nix develop` or direnv run tools directly; otherwise enter the dev shell per command.
ifdef IN_NIX_SHELL
NIX :=
else
NIX := nix develop --command
endif

# Runs every executable in $(1) (a directory pattern such as */bench/*) from its
# own directory, so its data/ folder resolves. $(2) restricts it to one name.
define run_executables
	@exes=$$(find $(BUILD_DIR) -type f -perm -u+x -path '$(1)$(if $(2),$(2),*)' | sort); \
	if [ -z "$$exes" ]; then echo "No executable matching $(1)$(2) in $(BUILD_DIR)" >&2; exit 1; fi; \
	for exe in $$exes; do (cd "$$(dirname "$$exe")" && "./$$(basename "$$exe")" $(ARGS)) || exit 1; done
endef

.PHONY: help configure build test run bench coverage docs add format check static shell clean
.DEFAULT_GOAL := help

help: ## Show this help
	@grep -E '^[a-z]+:.*## ' $(MAKEFILE_LIST) | awk -F ':.*## ' '{ printf "  %-10s %s\n", $$1, $$2 }'

# Configure only when the build directory is missing; Ninja re-runs CMake when CMakeLists change.
$(BUILD_DIR)/CMakeCache.txt:
	$(NIX) cmake --preset $(PRESET)

configure: $(BUILD_DIR)/CMakeCache.txt ## Configure PRESET (default: debug)

build: configure ## Build PRESET
	$(NIX) cmake --build --preset $(PRESET)

test: build ## Run the tests; FILTER=<regex> selects tests by name
	$(NIX) ctest --preset $(PRESET) $(if $(FILTER),--tests-regex '$(FILTER)')

run: build ## Run the main executable, BIN=<name> or EXAMPLE=<name>; ARGS="..." passes arguments
	$(call run_executables,$(if $(EXAMPLE),*/example/,*/bin/),$(or $(EXAMPLE),$(BIN)))

bench: ## Build the bench preset and run all benchmarks, or BENCH=<name>
	@$(MAKE) --no-print-directory build PRESET=bench
	$(MAKE) --no-print-directory run-benchmarks PRESET=bench

.PHONY: run-benchmarks
run-benchmarks:
	$(call run_executables,*/benchmark/,$(BENCH))

coverage: ## Run the tests with coverage; HTML report in .build/coverage/coverage/
	@$(MAKE) --no-print-directory build PRESET=coverage
	@# Counters accumulate across runs; start from zero so the report matches this run.
	find .build/coverage -name '*.gcda' -delete
	$(NIX) cmake --build --preset coverage --target coverage-report
	@echo "Report: .build/coverage/coverage/index.html"

docs: configure ## Build the Doxygen docs into .build/PRESET/docs/html
	$(NIX) cmake --build --preset $(PRESET) --target snaappy_docs
	@echo "Docs: $(BUILD_DIR)/docs/html/index.html"

add: ## Add a nixpkgs library to deps.json: DEP=<attr> [KIND=build|test|bench] [PACKAGE=<name>] [VERSION=<range>] [COMPONENTS="A B"]
	$(NIX) cmake -DDEP='$(DEP)' -DKIND='$(KIND)' -DPACKAGE='$(PACKAGE)' -DVERSION='$(VERSION)' -DCOMPONENTS='$(COMPONENTS)' -P cmake/add-dependency.cmake
	@echo "Reload the dev shell (exit and re-enter, or let direnv reload) to use it."

format: ## Format Nix and C/C++ files
	nix fmt

check: ## Sandboxed build, tests and format check (same as CI)
	nix flake check -L

static: ## Fully static release build into ./result (Linux only)
	nix build -L .#static

shell: ## Open the dev shell in your current shell
	nix develop --command "$${SHELL:-bash}"

clean: ## Remove build directories and nix result links
	rm -rf .build result result-*

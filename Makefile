BUILD_DIR  := build
BUILD_TYPE ?= Release
JOBS       ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
ARGS       ?=

CMAKE       := cmake
CTEST       := ctest
CLANG_FORMAT := clang-format

TUIG_BIN  := $(BUILD_DIR)/tuig
CHESS_BIN := $(BUILD_DIR)/chesscli/chesscli
DAMA_BIN  := $(BUILD_DIR)/damacli/damacli

.PHONY: all configure build release debug run chess dama test clean distclean fresh submodules update-submodules compile-commands format install-hooks help

all: build ## Build tuig (default)

configure: ## Run cmake configure (BUILD_TYPE=Release|Debug)
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: ## Build all targets in parallel
	@if [ ! -f "$(BUILD_DIR)/CMakeCache.txt" ]; then $(MAKE) configure; fi
	$(CMAKE) --build "$(BUILD_DIR)" -j $(JOBS)

release: ## Configure Release and build
	$(MAKE) configure BUILD_TYPE=Release
	$(MAKE) build

debug: ## Configure Debug (with sanitizers) and build
	$(MAKE) configure BUILD_TYPE=Debug
	$(MAKE) build

run: build ## Build and run ./build/tuig
	"./$(TUIG_BIN)"

chess: build ## Run chesscli (ARGS="...")
	"./$(CHESS_BIN)" $(ARGS)

dama: build ## Run damacli (ARGS="...")
	"./$(DAMA_BIN)" $(ARGS)

test: ## Configure with tests on, build, and run ctest
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCHESSCLI_BUILD_TESTS=ON -DDAMACLI_BUILD_TESTS=ON
	$(CMAKE) --build "$(BUILD_DIR)" -j $(JOBS)
	cd "$(BUILD_DIR)" && $(CTEST) --output-on-failure -j $(JOBS)

clean: ## Clean build artifacts (preserves CMake cache)
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then $(CMAKE) --build "$(BUILD_DIR)" --target clean; fi

distclean: ## Remove the entire build directory
	rm -rf "$(BUILD_DIR)"

fresh: ## Nuke build dir and do a clean Release build
	$(MAKE) distclean
	$(MAKE) release

submodules: ## Init and update git submodules recursively
	git submodule update --init --recursive

update-submodules: ## Pull latest default-branch tip for each submodule
	git submodule foreach --recursive 'branch=$$(git remote show origin | sed -n "s/.*HEAD branch: //p"); git checkout $$branch && git pull --ff-only origin $$branch'
	@echo ""
	@echo "Submodule pointers updated. Review and commit with:"
	@echo "  git add chesscli damacli && git commit -m 'bump submodules'"

compile-commands: ## Symlink build/compile_commands.json to repo root
	@if [ ! -f "$(BUILD_DIR)/compile_commands.json" ]; then $(MAKE) configure; fi
	ln -sf "$(BUILD_DIR)/compile_commands.json" compile_commands.json

install-hooks: ## Wire .githooks/ into this git repo (pre-commit format check)
	git config core.hooksPath .githooks
	@echo "pre-commit hook installed (.githooks/pre-commit)"

format: ## Run clang-format on src/ (no-op if clang-format missing)
	@if command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		files=$$(ls src/*.cc src/*.h 2>/dev/null); \
		if [ -n "$$files" ]; then $(CLANG_FORMAT) -i $$files; fi; \
	else \
		echo "clang-format not found; skipping"; \
	fi

help: ## Show this help
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / {printf "  \033[36m%-20s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# build.sh — Unified build system for design-patterns project
#
# Usage: ./build.sh [command] [options]
#
# Commands: build, run, clean, help
# Languages: --lang c | cpp | rust | all
# Modes: --debug | --release (default: release)
# Compilers: --compiler gcc | clang (C/C++ only, default: gcc)
# Targets: --target <name> (run a specific binary)
# ============================================================

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# --- Defaults ---
COMMAND=""
LANG="all"
MODE="release"
COMPILER="gcc"
TARGET=""
VERBOSE=0

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# --- C/C++ binary names ---
C_BINS=(c_strategy c_visitor c_command)
CPP_BINS=(strategy_classic strategy_modern visitor_classic visitor_modern command_classic command_modern)
RUST_BINS=(strategy visitor command)

# ============================================================
# Helper functions
# ============================================================

log()   { echo -e "${GREEN}[build]${NC} $*"; }
warn()  { echo -e "${YELLOW}[warn]${NC}  $*"; }
err()   { echo -e "${RED}[error]${NC} $*" >&2; }
header() { echo -e "\n${BOLD}${CYAN}=== $* ===${NC}\n"; }

usage() {
    cat <<'EOF'

  ┌─────────────────────────────────────────────────────────┐
  │        Design Patterns — Unified Build Script           │
  └─────────────────────────────────────────────────────────┘

  USAGE
      ./build.sh <command> [options]

  COMMANDS
      build       Build the project (default if omitted)
      run         Build and run all binaries for selected language(s)
      clean       Remove build artifacts
      help        Show this message

  OPTIONS
      --lang <lang>           Language to build/run
                                c       Pure C only
                                cpp     C++ only (classic + modern)
                                rust    Rust only
                                all     Everything (default)

      --debug                 Build with debug symbols, no optimization
      --release               Build optimized (default)

      --compiler <compiler>   C/C++ compiler toolchain (ignored for Rust)
                                gcc     Use gcc/g++ (default)
                                clang   Use clang/clang++

      --target <name>         Run a specific binary instead of all
                              Examples: strategy_classic, c_visitor,
                                        strategy (Rust)

      --verbose               Show full compiler commands

  EXAMPLES
      ./build.sh build
      ./build.sh build --lang cpp --compiler clang --debug
      ./build.sh run --lang rust
      ./build.sh run --lang c --compiler gcc --debug
      ./build.sh run --target strategy_modern
      ./build.sh run --target c_command --debug
      ./build.sh clean
      ./build.sh clean --lang rust

  AVAILABLE TARGETS
      C:      c_strategy  c_visitor  c_command
      C++:    strategy_classic  strategy_modern
              visitor_classic   visitor_modern
              command_classic   command_modern
      Rust:   strategy  visitor  command

EOF
}

# ============================================================
# Argument parsing
# ============================================================

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            build|run|clean|help)
                COMMAND="$1"
                shift
                ;;
            --lang)
                LANG="${2:?'--lang requires a value: c, cpp, rust, all'}"
                if [[ ! "$LANG" =~ ^(c|cpp|rust|all)$ ]]; then
                    err "Invalid language: $LANG (expected: c, cpp, rust, all)"
                    exit 1
                fi
                shift 2
                ;;
            --debug)
                MODE="debug"
                shift
                ;;
            --release)
                MODE="release"
                shift
                ;;
            --compiler)
                COMPILER="${2:?'--compiler requires a value: gcc, clang'}"
                if [[ ! "$COMPILER" =~ ^(gcc|clang)$ ]]; then
                    err "Invalid compiler: $COMPILER (expected: gcc, clang)"
                    exit 1
                fi
                shift 2
                ;;
            --target)
                TARGET="${2:?'--target requires a binary name'}"
                shift 2
                ;;
            --verbose)
                VERBOSE=1
                shift
                ;;
            -h|--help)
                COMMAND="help"
                shift
                ;;
            *)
                err "Unknown argument: $1"
                usage
                exit 1
                ;;
        esac
    done

    # Default command
    if [[ -z "$COMMAND" ]]; then
        COMMAND="build"
    fi
}

# ============================================================
# Prerequisite checks
# ============================================================

check_cmake() {
    if ! command -v cmake &>/dev/null; then
        err "cmake not found. Install it with:"
        err "  Ubuntu/Debian: sudo apt install cmake"
        err "  macOS:         brew install cmake"
        err "  Arch:          sudo pacman -S cmake"
        exit 1
    fi
}

check_c_compiler() {
    if [[ "$COMPILER" == "gcc" ]]; then
        if ! command -v gcc &>/dev/null; then
            err "gcc not found. Install it or use --compiler clang"
            exit 1
        fi
    elif [[ "$COMPILER" == "clang" ]]; then
        if ! command -v clang &>/dev/null; then
            err "clang not found. Install it or use --compiler gcc"
            exit 1
        fi
    fi
}

check_rust() {
    if ! command -v cargo &>/dev/null; then
        err "cargo not found. Install Rust from https://rustup.rs"
        exit 1
    fi
}

needs_c_cpp() {
    [[ "$LANG" == "c" || "$LANG" == "cpp" || "$LANG" == "all" ]]
}

needs_rust() {
    [[ "$LANG" == "rust" || "$LANG" == "all" ]]
}

# ============================================================
# Build functions
# ============================================================

cmake_build_type() {
    if [[ "$MODE" == "debug" ]]; then
        echo "Debug"
    else
        echo "Release"
    fi
}

get_build_dir() {
    echo "${BUILD_DIR}/${COMPILER}-${MODE}"
}

build_c_cpp() {
    local build_type
    build_type="$(cmake_build_type)"
    local bdir
    bdir="$(get_build_dir)"

    header "Building C/C++ [${COMPILER}, ${MODE}]"

    check_cmake
    check_c_compiler

    mkdir -p "$bdir"

    local cmake_args=(
        -S "$PROJECT_ROOT"
        -B "$bdir"
        -DCMAKE_BUILD_TYPE="$build_type"
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    )

    if [[ "$COMPILER" == "gcc" ]]; then
        cmake_args+=(-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++)
    elif [[ "$COMPILER" == "clang" ]]; then
        cmake_args+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
    fi

    log "Configuring with CMake..."
    cmake "${cmake_args[@]}" 2>&1 | { [[ $VERBOSE -eq 1 ]] && cat || tail -3; }

    # Build only the targets we need
    local targets=()
    case "$LANG" in
        c)   targets=("${C_BINS[@]}") ;;
        cpp) targets=("${CPP_BINS[@]}") ;;
        all) targets=("${C_BINS[@]}" "${CPP_BINS[@]}") ;;
    esac

    local make_args=(--no-print-directory -C "$bdir" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)")
    [[ $VERBOSE -eq 1 ]] && make_args+=(VERBOSE=1)
    make_args+=("${targets[@]}")

    log "Compiling ${#targets[@]} targets..."
    make "${make_args[@]}" 2>&1

    # Symlink compile_commands.json to project root for LSP
    if [[ -f "$bdir/compile_commands.json" ]]; then
        ln -sf "$bdir/compile_commands.json" "$PROJECT_ROOT/compile_commands.json" 2>/dev/null || true
    fi

    log "C/C++ build complete: $bdir"
}

build_rust() {
    header "Building Rust [${MODE}]"

    check_rust

    local cargo_args=(build)
    [[ "$MODE" == "release" ]] && cargo_args+=(--release)

    log "Compiling with Cargo..."
    (cd "$PROJECT_ROOT/rust" && cargo "${cargo_args[@]}" 2>&1)

    log "Rust build complete"
}

# ============================================================
# Run functions
# ============================================================

run_binary() {
    local name="$1"
    local path="$2"

    if [[ ! -x "$path" ]]; then
        warn "Binary not found: $path (did you build first?)"
        return 1
    fi

    echo -e "${BOLD}────────────────────────────────────────${NC}"
    echo -e "${BOLD}  Running: ${CYAN}${name}${NC} ${YELLOW}[${MODE}]${NC}"
    echo -e "${BOLD}────────────────────────────────────────${NC}"
    echo
    "$path"
    echo
}

run_c_cpp() {
    local bdir
    bdir="$(get_build_dir)"

    local targets=()
    case "$LANG" in
        c)   targets=("${C_BINS[@]}") ;;
        cpp) targets=("${CPP_BINS[@]}") ;;
        all) targets=("${C_BINS[@]}" "${CPP_BINS[@]}") ;;
    esac

    for bin in "${targets[@]}"; do
        run_binary "$bin" "$bdir/$bin"
    done
}

run_rust() {
    local rust_target_dir="$PROJECT_ROOT/rust/target"
    local rust_mode_dir

    if [[ "$MODE" == "release" ]]; then
        rust_mode_dir="$rust_target_dir/release"
    else
        rust_mode_dir="$rust_target_dir/debug"
    fi

    for bin in "${RUST_BINS[@]}"; do
        run_binary "rust/$bin" "$rust_mode_dir/$bin"
    done
}

run_specific_target() {
    local target="$TARGET"
    local found=0

    # Check C/C++ targets
    local bdir
    bdir="$(get_build_dir)"
    for bin in "${C_BINS[@]}" "${CPP_BINS[@]}"; do
        if [[ "$bin" == "$target" ]]; then
            run_binary "$bin" "$bdir/$bin"
            found=1
            break
        fi
    done

    # Check Rust targets
    if [[ $found -eq 0 ]]; then
        local rust_mode_dir
        if [[ "$MODE" == "release" ]]; then
            rust_mode_dir="$PROJECT_ROOT/rust/target/release"
        else
            rust_mode_dir="$PROJECT_ROOT/rust/target/debug"
        fi

        for bin in "${RUST_BINS[@]}"; do
            if [[ "$bin" == "$target" ]]; then
                run_binary "rust/$bin" "$rust_mode_dir/$bin"
                found=1
                break
            fi
        done
    fi

    if [[ $found -eq 0 ]]; then
        err "Unknown target: $target"
        err "Available targets:"
        err "  C:    ${C_BINS[*]}"
        err "  C++:  ${CPP_BINS[*]}"
        err "  Rust: ${RUST_BINS[*]}"
        exit 1
    fi
}

# ============================================================
# Clean functions
# ============================================================

clean_c_cpp() {
    if [[ -d "$BUILD_DIR" ]]; then
        log "Removing $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi
    if [[ -f "$PROJECT_ROOT/compile_commands.json" ]]; then
        rm -f "$PROJECT_ROOT/compile_commands.json"
    fi
    log "C/C++ clean complete"
}

clean_rust() {
    if [[ -d "$PROJECT_ROOT/rust/target" ]]; then
        log "Running cargo clean..."
        (cd "$PROJECT_ROOT/rust" && cargo clean 2>&1)
    fi
    log "Rust clean complete"
}

# ============================================================
# Main dispatch
# ============================================================

main() {
    parse_args "$@"

    case "$COMMAND" in
        help)
            usage
            exit 0
            ;;

        build)
            if needs_c_cpp; then
                build_c_cpp
            fi
            if needs_rust; then
                build_rust
            fi
            echo
            log "Build complete! Use './build.sh run' to execute."
            ;;

        run)
            # Build first, then run
            if [[ -n "$TARGET" ]]; then
                # Build everything needed, then run the specific target
                # Determine what language the target belongs to
                for bin in "${C_BINS[@]}"; do
                    [[ "$bin" == "$TARGET" ]] && LANG="c" && break
                done
                for bin in "${CPP_BINS[@]}"; do
                    [[ "$bin" == "$TARGET" ]] && LANG="cpp" && break
                done
                for bin in "${RUST_BINS[@]}"; do
                    [[ "$bin" == "$TARGET" ]] && LANG="rust" && break
                done

                if needs_c_cpp; then build_c_cpp; fi
                if needs_rust; then build_rust; fi

                echo
                run_specific_target
            else
                if needs_c_cpp; then
                    build_c_cpp
                    echo
                    run_c_cpp
                fi
                if needs_rust; then
                    build_rust
                    echo
                    run_rust
                fi
            fi
            ;;

        clean)
            header "Cleaning"
            if needs_c_cpp; then clean_c_cpp; fi
            if needs_rust; then clean_rust; fi
            log "All clean."
            ;;

        *)
            err "Unknown command: $COMMAND"
            usage
            exit 1
            ;;
    esac
}

main "$@"

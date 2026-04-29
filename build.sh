#!/bin/bash
# build.sh — Build linphone-desktop from source on Ubuntu 24.04
# Usage: ./build.sh [clean|deps|qt|submodules|all]
#   (no args) — incremental build
#   clean     — removes build directory and rebuilds from scratch
#   deps      — install system dependencies only
#   qt        — install Qt via aqtinstall only
#   submodules — init submodules only
#   all       — full setup from scratch (deps + Qt + submodules + clean build)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
QT_VERSION="6.10.0"
QT_DIR="$HOME/Qt/$QT_VERSION/gcc_64"
BUILD_TYPE="RelWithDebInfo"
PARALLEL_JOBS=$(nproc)

# ── 1. System dependencies ──────────────────────────────────────────────
install_deps() {
  echo "=== Installing build + runtime dependencies ==="
  sudo apt-get update
  # Build dependencies (-dev packages)
  sudo apt-get install -y \
    build-essential cmake git pkg-config \
    ninja-build nasm yasm doxygen meson autoconf libtool-bin \
    python3-pystache python3-pip python3-venv \
    libgl1-mesa-dev libglu1-mesa-dev libegl1-mesa-dev \
    libx11-dev libxkbcommon-dev libxkbcommon-x11-dev libudev-dev \
    libv4l-dev libasound2-dev libpulse-dev libglew-dev libxinerama-dev \
    libxrandr-dev libxcursor-dev libxi-dev libxext-dev libxfixes-dev \
    libwayland-dev libdbus-1-dev
  # Runtime dependencies (shared libraries needed at execution time)
  sudo apt-get install -y \
    libasound2t64 libpulse0 libv4l-0 \
    libgl1 libglew2.2 libglu1-mesa libegl1 \
    libx11-6 libxkbcommon0 libxkbcommon-x11-0 \
    libxrandr2 libxcursor1 libxi6 libxext6 libxfixes3 libxinerama1 \
    libudev1 libdbus-1-3 libwayland-client0 \
    libhidapi-hidraw0 libfreetype6 libfontconfig1 \
    libssl3t64 libglib2.0-0t64 libsqlite3-0
}

# ── 2. Qt (via aqtinstall) ──────────────────────────────────────────────
install_qt() {
  if [ -d "$QT_DIR" ]; then
    echo "=== Qt $QT_VERSION already installed at $QT_DIR ==="
    return
  fi
  echo "=== Installing Qt $QT_VERSION via aqtinstall ==="
  pip3 install --user aqtinstall
  "$HOME/.local/bin/aqt" install-qt linux desktop "$QT_VERSION" gcc_64 \
    --outputdir "$HOME/Qt" \
    -m qtshadertools qtwebsockets qtnetworkauth
}

# ── 3. Submodules ───────────────────────────────────────────────────────
init_submodules() {
  echo "=== Initializing submodules ==="
  if [ -x "$SCRIPT_DIR/init-submodules.sh" ]; then
    "$SCRIPT_DIR/init-submodules.sh"
  else
    git submodule update --init --recursive --force
  fi
}

# ── 4. Build ────────────────────────────────────────────────────────────
build() {
  if [ "${1:-}" = "clean" ] && [ -d "$BUILD_DIR" ]; then
    echo "=== Removing existing build directory ==="
    rm -rf "$BUILD_DIR"
  fi

  mkdir -p "$BUILD_DIR"
  cd "$BUILD_DIR"

  export Qt6_DIR="$QT_DIR/lib/cmake/Qt6"
  export PATH="$QT_DIR/bin:$PATH"

  echo "=== CMake configure ==="
  cmake "$SCRIPT_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_BUILD_PARALLEL_LEVEL="$PARALLEL_JOBS"

  echo "=== Building with $PARALLEL_JOBS parallel jobs ==="
  cmake --build . --parallel "$PARALLEL_JOBS" --config "$BUILD_TYPE"

  echo "=== Installing ==="
  cmake --install .

  echo ""
  echo "=== Build complete ==="
  echo "Binary: $BUILD_DIR/OUTPUT/bin/callforge"
  echo ""
  echo "Run with:"
  echo "  ./start-linphone.sh"
}

# ── Main ────────────────────────────────────────────────────────────────
case "${1:-}" in
  deps)
    install_deps
    ;;
  qt)
    install_qt
    ;;
  submodules)
    init_submodules
    ;;
  clean)
    build clean
    ;;
  all)
    install_deps
    install_qt
    init_submodules
    build clean
    ;;
  *)
    build "${1:-}"
    ;;
esac

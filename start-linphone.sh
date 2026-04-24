#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BINARY="$SCRIPT_DIR/build/OUTPUT/bin/linphone"
QT_LIB="$HOME/Qt/6.10.0/gcc_64/lib"
SDK_LIB="$SCRIPT_DIR/build/OUTPUT/lib"
ERRORS=0

check() {
    if ! "$@" > /dev/null 2>&1; then
        echo "MISSING: $1"
        ERRORS=$((ERRORS + 1))
        return 1
    fi
    return 0
}

echo "Checking dependencies..."

if [ ! -f "$BINARY" ]; then
    echo "ERROR: Linphone binary not found at $BINARY"
    echo "       Run the build first. See CLAUDE.md for instructions."
    exit 1
fi

if [ ! -d "$QT_LIB" ]; then
    echo "ERROR: Qt6 not found at $QT_LIB"
    echo "       Install Qt6 6.10.0: pip3 install aqtinstall && aqt install-qt linux desktop 6.10.0 linux_gcc_64 -m qtnetworkauth qtshadertools qtquick3d -O ~/Qt"
    exit 1
fi

if [ ! -d "$SDK_LIB" ]; then
    echo "ERROR: SDK libraries not found at $SDK_LIB"
    echo "       Run: cd build && cmake --install ."
    exit 1
fi

check pkg-config --exists alsa          || echo "       Fix: sudo apt install libasound2-dev"
check pkg-config --exists libpulse      || echo "       Fix: sudo apt install libpulse-dev"
check pkg-config --exists libv4l2       || echo "       Fix: sudo apt install libv4l-dev"
check pkg-config --exists gl            || echo "       Fix: sudo apt install libgl1-mesa-dev"
check pkg-config --exists glew          || echo "       Fix: sudo apt install libglew-dev"
check pkg-config --exists libudev       || echo "       Fix: sudo apt install libudev-dev"
check pkg-config --exists x11           || echo "       Fix: sudo apt install libx11-dev"
check pkg-config --exists xkbcommon     || echo "       Fix: sudo apt install libxkbcommon-dev"

if [ $ERRORS -gt 0 ]; then
    echo ""
    echo "ERROR: $ERRORS missing runtime dependencies. Install them and try again."
    exit 1
fi

echo "All checks passed. Starting Linphone..."
export Qt6_DIR="$HOME/Qt/6.10.0/gcc_64/lib/cmake/Qt6"
export PATH="$HOME/Qt/6.10.0/gcc_64/bin:$PATH"
export LD_LIBRARY_PATH="$SDK_LIB:$QT_LIB:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HOME/Qt/6.10.0/gcc_64/plugins"
exec "$BINARY" "$@"

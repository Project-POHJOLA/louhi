#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OSGEARTH_DIR="$REPO_ROOT/deps/osgearth"
BUILD_DIR="$OSGEARTH_DIR/build"
OSGEARTH_VERSION="osgearth-3.8"
JOBS=${JOBS:-$(nproc)}

echo "==> osgEarth build script"
echo "    Source:  $OSGEARTH_DIR"
echo "    Build:   $BUILD_DIR"
echo "    Version: $OSGEARTH_VERSION"
echo "    Jobs:    $JOBS"

# Check if already installed
if pkg-config --modversion osgEarth 2>/dev/null; then
    echo "==> osgEarth is already installed. Skipping."
    exit 0
fi

# Clone if not present
if [ ! -d "$OSGEARTH_DIR" ]; then
    echo "==> Cloning osgEarth $OSGEARTH_VERSION..."
    git clone --branch "$OSGEARTH_VERSION" --depth 1 \
        https://github.com/pelicanmapping/osgearth.git "$OSGEARTH_DIR"
fi

cd "$OSGEARTH_DIR"

echo "==> Configuring osgEarth..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DOSGEARTH_BUILD_TOOLS=OFF \
    -DOSGEARTH_BUILD_EXAMPLES=OFF \
    -DOSGEARTH_BUILD_IMGUI_NODEKIT=OFF \
    -DOSGEARTH_BUILD_TESTS=OFF \
    -DOSGEARTH_ENABLE_FASTDXT=OFF \
    -DOSGEARTH_BUILD_DOCS=OFF

echo "==> Building osgEarth..."
make -j"$JOBS"

echo "==> Installing osgEarth..."
sudo make install
sudo ldconfig

echo "==> osgEarth $OSGEARTH_VERSION installed successfully."

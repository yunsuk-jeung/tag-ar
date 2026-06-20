#!/bin/bash

# Build and install third-party dependencies for omni_slam (macOS)
# Usage: ./build_dependencies_macos.sh [debug|release]

set -e  # Exit on error

# Target OS for the install layout (this is the macOS build script)
TARGET_OS="mac"

# Get build type (default: release)
BUILD_TYPE=${1:-release}

# Convert to proper CMake build type
case "$BUILD_TYPE" in
    debug)
        CMAKE_BUILD_TYPE="Debug"
        ;;
    release)
        CMAKE_BUILD_TYPE="Release"
        ;;
    *)
        echo "Invalid build type: $BUILD_TYPE"
        echo "Usage: $0 [debug|release]"
        exit 1
        ;;
esac

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
THIRD_PARTY_DIR="$PROJECT_ROOT/third_party"
# Install per-OS: libs/{mac,ubuntu,ios}/<buildtype>
INSTALL_PREFIX="$PROJECT_ROOT/libs/$TARGET_OS/$BUILD_TYPE"
BUILD_DIR="$PROJECT_ROOT/build/deps-$TARGET_OS-$BUILD_TYPE"
PREFIX_PATH="$CMAKE_PREFIX_PATH"
if [ -n "$PREFIX_PATH" ]; then
    PREFIX_PATH="$PREFIX_PATH;$INSTALL_PREFIX"
else
    PREFIX_PATH="$INSTALL_PREFIX"
fi

echo "=================================================="
echo "Building third-party dependencies (macOS)"
echo "  Target OS:  $TARGET_OS"
echo "  Build Type: $CMAKE_BUILD_TYPE"
echo "  Install To: $INSTALL_PREFIX"
echo "  Cmake prefix: $INSTALL_PREFIX"
echo "  Build Dir:  $BUILD_DIR"
echo "=================================================="

# Create source, build and install directories
mkdir -p "$THIRD_PARTY_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$INSTALL_PREFIX"

# Number of parallel jobs
if command -v sysctl >/dev/null 2>&1; then
    NPROC=$(sysctl -n hw.ncpu)
elif command -v nproc >/dev/null 2>&1; then
    NPROC=$(nproc)
else
    NPROC=1
fi

# Function to download a dependency pinned to an exact ref (tag or commit SHA)
# Usage: fetch_source <name> <git-url> <ref> <dest-dir>
# Versions are pinned to match the omni_slam repo submodules.
fetch_source() {
    local name=$1
    local url=$2
    local ref=$3
    local dest=$4

    if [ -d "$dest/.git" ]; then
        echo "[$name] source already present at $dest (skipping clone)"
        return
    fi

    echo ""
    echo "Fetching $name ($ref)..."
    echo "----------------------------------------"
    # Fetch the exact ref (works for both tags and commit SHAs on GitHub/GitLab),
    # then pull in submodules (needed by Sophus/OpenCV).
    git init "$dest"
    git -C "$dest" remote add origin "$url"
    git -C "$dest" fetch --depth 1 origin "$ref"
    git -C "$dest" checkout --detach FETCH_HEAD
    git -C "$dest" submodule update --init --recursive --depth 1
}

# Download all sources (versions pinned to omni_slam)
echo ""
echo "Downloading third-party sources into $THIRD_PARTY_DIR ..."
fetch_source "eigen"         "https://gitlab.com/libeigen/eigen.git"        "3.4.1"                                      "$THIRD_PARTY_DIR/eigen"
fetch_source "sophus"        "https://github.com/strasdat/Sophus.git"       "1.24.6"                                     "$THIRD_PARTY_DIR/sophus"
fetch_source "nlohmann_json" "https://github.com/nlohmann/json.git"         "v3.12.0"                                    "$THIRD_PARTY_DIR/json"
fetch_source "spdlog"        "https://github.com/gabime/spdlog.git"         "33375433e096d59b1e4dd9d46cac9d58a5528ccb"   "$THIRD_PARTY_DIR/spdlog"
fetch_source "googletest"    "https://github.com/google/googletest.git"     "ff6133ab49b364a883a55ba75c39e520fea6245b"   "$THIRD_PARTY_DIR/googletest"
fetch_source "opencv"        "https://github.com/opencv/opencv.git"         "4.13.0"                                     "$THIRD_PARTY_DIR/opencv"

# Function to build and install a library
build_library() {
    local name=$1
    local source_dir=$2
    local cmake_args=$3

    echo ""
    echo "Building $name..."
    echo "----------------------------------------"

    local lib_build_dir="$BUILD_DIR/$name"
    mkdir -p "$lib_build_dir"
    cd "$lib_build_dir"

    cmake "$source_dir" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DCMAKE_PREFIX_PATH="$PREFIX_PATH" \
        $cmake_args

    cmake --build . -j$NPROC
    cmake --install .

    echo "$name installed successfully!"
}

# 1. Eigen
build_library "eigen" "$THIRD_PARTY_DIR/eigen" \
    "-DEIGEN_BUILD_DOC=OFF \
     -DBUILD_TESTING=OFF \
     -DEIGEN_BUILD_PKGCONFIG=OFF"

# 2. Sophus (depends on Eigen)
build_library "sophus" "$THIRD_PARTY_DIR/sophus" \
    "-DBUILD_SOPHUS_TESTS=OFF \
     -DBUILD_SOPHUS_EXAMPLES=OFF"

# 3. nlohmann_json
build_library "nlohmann_json" "$THIRD_PARTY_DIR/json" \
    "-DJSON_BuildTests=OFF \
     -DJSON_Install=ON"

# 4. spdlog
build_library "spdlog" "$THIRD_PARTY_DIR/spdlog" \
    "-DSPDLOG_BUILD_EXAMPLE=OFF \
     -DSPDLOG_USE_STD_FORMAT=ON \
     -DSPDLOG_BUILD_SHARED=ON \
     -DSPDLOG_BUILD_TESTS=OFF"

# 5. GoogleTest
build_library "googletest" "$THIRD_PARTY_DIR/googletest" \
    "-DBUILD_GMOCK=OFF \
     -DINSTALL_GTEST=ON"

# 6. OpenCV
build_library "opencv" "$THIRD_PARTY_DIR/opencv" \
    "-DBUILD_TESTS=OFF \
     -DBUILD_PERF_TESTS=OFF \
     -DBUILD_EXAMPLES=OFF \
     -DBUILD_opencv_apps=OFF \
     -DBUILD_DOCS=OFF \
     -DWITH_GTK=OFF \
     -DWITH_QT=OFF \
     -DWITH_TBB=OFF \
     -DWITH_CUDA=OFF \
     -DWITH_PROTOBUF=OFF \
     -DBUILD_PROTOBUF=OFF \
     -DBUILD_opencv_dnn=OFF \
     -DBUILD_opencv_gapi=OFF \
     -DBUILD_opencv_world=ON"

echo ""
echo "=================================================="
echo "All dependencies built and installed successfully!"
echo "Install location: $INSTALL_PREFIX"
echo "=================================================="

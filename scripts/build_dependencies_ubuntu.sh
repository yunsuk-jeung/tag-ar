#!/bin/bash

# Build and install third-party dependencies for tag-ar (Ubuntu/Linux)
# Usage: ./build_dependencies_ubuntu.sh [debug|release]
#
# Installs the system toolchain + GLFW/GLEW runtime deps via apt, then builds
# the same source dependencies as the macOS script (Eigen, Sophus,
# nlohmann_json, spdlog, GoogleTest, OpenCV, GLFW).
#
# Env:
#   SKIP_APT=1   skip the apt-get system package step (already installed)

set -e  # Exit on error

# Target OS for the install layout (this is the Ubuntu build script)
TARGET_OS="ubuntu"

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
echo "Building third-party dependencies (Ubuntu)"
echo "  Target OS:  $TARGET_OS"
echo "  Build Type: $CMAKE_BUILD_TYPE"
echo "  Install To: $INSTALL_PREFIX"
echo "  Build Dir:  $BUILD_DIR"
echo "=================================================="

# Install system packages: build toolchain plus the system libraries the
# from-source deps link against on Linux. Unlike macOS there is no
# OpenGL.framework, so the desktop viewer uses <GL/glew.h> (GLEW, built from
# source below) and GLFW is built against the X11 development packages.
#   libgl1-mesa-dev  - OpenGL/GLX headers+libs (needed by GLEW and OpenGL::GL)
#   libx11-dev ...   - X11 dev headers GLFW links against
if [ "${SKIP_APT:-0}" != "1" ]; then
    SUDO=""
    if [ "$(id -u)" != "0" ]; then
        SUDO="sudo"
    fi

    echo ""
    echo "Installing system packages via apt-get..."
    echo "----------------------------------------"
    $SUDO apt-get update
    $SUDO apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        git \
        curl \
        pkg-config \
        libgl1-mesa-dev \
        libx11-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libxext-dev \
        libavcodec-dev \
        libavformat-dev \
        libswscale-dev \
        libavutil-dev
else
    echo ""
    echo "SKIP_APT=1 set; skipping system package installation."
fi

# Create source, build and install directories
mkdir -p "$THIRD_PARTY_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$INSTALL_PREFIX"

# Number of parallel jobs
if command -v nproc >/dev/null 2>&1; then
    NPROC=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    NPROC=$(sysctl -n hw.ncpu)
else
    NPROC=1
fi

# Function to download a dependency pinned to an exact ref (tag or commit SHA)
# Usage: fetch_source <name> <git-url> <ref> <dest-dir>
# Versions are pinned to match the macOS/iOS scripts.
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

# Function to download + extract a release tarball (top-level dir stripped).
# Used for GLEW: its git tree ships no generated src/glew.c, but the release
# tarball does, so it can be built straight from CMake.
# Usage: fetch_tarball <name> <tarball-url> <dest-dir>
fetch_tarball() {
    local name=$1
    local url=$2
    local dest=$3

    if [ -d "$dest" ]; then
        echo "[$name] source already present at $dest (skipping download)"
        return
    fi

    echo ""
    echo "Fetching $name ($url)..."
    echo "----------------------------------------"
    local tmp="$dest.download"
    rm -rf "$tmp"
    mkdir -p "$tmp"
    curl -fsSL "$url" -o "$tmp/src.tgz"
    tar xzf "$tmp/src.tgz" -C "$tmp" --strip-components=1
    rm -f "$tmp/src.tgz"
    mv "$tmp" "$dest"
}

# Download all sources (versions pinned to match macOS/iOS scripts)
echo ""
echo "Downloading third-party sources into $THIRD_PARTY_DIR ..."
fetch_source "eigen"         "https://gitlab.com/libeigen/eigen.git"        "3.4.1"                                      "$THIRD_PARTY_DIR/eigen"
fetch_source "sophus"        "https://github.com/strasdat/Sophus.git"       "1.24.6"                                     "$THIRD_PARTY_DIR/sophus"
fetch_source "nlohmann_json" "https://github.com/nlohmann/json.git"         "v3.12.0"                                    "$THIRD_PARTY_DIR/json"
fetch_source "spdlog"        "https://github.com/gabime/spdlog.git"         "33375433e096d59b1e4dd9d46cac9d58a5528ccb"   "$THIRD_PARTY_DIR/spdlog"
fetch_source "googletest"    "https://github.com/google/googletest.git"     "ff6133ab49b364a883a55ba75c39e520fea6245b"   "$THIRD_PARTY_DIR/googletest"
fetch_source "opencv"        "https://github.com/opencv/opencv.git"         "4.13.0"                                     "$THIRD_PARTY_DIR/opencv"
fetch_source "glfw"          "https://github.com/glfw/glfw.git"             "3.4"                                        "$THIRD_PARTY_DIR/glfw"
fetch_tarball "glew"         "https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz"          "$THIRD_PARTY_DIR/glew"

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
     -DBUILD_opencv_world=ON \
     -DWITH_FFMPEG=ON"

# 7. GLFW (window + OpenGL context; static lib)
# On Linux GLFW links against X11 (installed above) and there is no
# OpenGL.framework, so the desktop viewer pulls the core API through GLEW
# (libglew-dev, also installed above). Wayland is disabled to keep a single
# X11 path matching the macOS build.
build_library "glfw" "$THIRD_PARTY_DIR/glfw" \
    "-DGLFW_BUILD_EXAMPLES=OFF \
     -DGLFW_BUILD_TESTS=OFF \
     -DGLFW_BUILD_DOCS=OFF \
     -DGLFW_BUILD_WAYLAND=OFF"

# 8. GLEW (GL loader for the Linux viewer). The CMake project lives in the
# build/cmake subdirectory of the GLEW tree. BUILD_UTILS=OFF skips the
# glewinfo/visualinfo tools (which would pull in GLU). find_package(GLEW) in
# the root CMakeLists picks this up from the install prefix.
build_library "glew" "$THIRD_PARTY_DIR/glew/build/cmake" \
    "-DBUILD_UTILS=OFF"

echo ""
echo "=================================================="
echo "All dependencies built and installed successfully!"
echo "Install location: $INSTALL_PREFIX"
echo "=================================================="

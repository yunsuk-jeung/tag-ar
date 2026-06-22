#!/bin/bash

# Build and install third-party dependencies for tag-ar (iOS).
# Cross-compiles only what the on-device core needs: Eigen, Sophus,
# nlohmann_json, spdlog, OpenCV. Desktop-only deps (GLFW, GoogleTest) are
# skipped. Libraries are static (iOS does not load arbitrary dylibs).
#
# Usage: ./build_dependencies_ios.sh [debug|release]
# Env:
#   IOS_PLATFORM=OS|SIMULATOR   target device or simulator (default OS)
#   IOS_DEPLOYMENT_TARGET=15.0  minimum iOS version

set -e  # Exit on error

# Target OS for the install layout (this is the iOS build script)
TARGET_OS="ios"

# Get build type (default: release)
BUILD_TYPE=${1:-release}
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

# iOS target: device (arm64) by default; IOS_PLATFORM=SIMULATOR for the sim.
IOS_PLATFORM=${IOS_PLATFORM:-OS}
# >= 16.3: the core uses std::format / std::to_chars, which libc++ marks as
# introduced in iOS 16.3 (matches the desktop SPDLOG_USE_STD_FORMAT build).
DEPLOYMENT_TARGET=${IOS_DEPLOYMENT_TARGET:-16.4}
if [ "$IOS_PLATFORM" = "SIMULATOR" ]; then
    OSX_SYSROOT="iphonesimulator"
    OSX_ARCHS="arm64"  # Apple-silicon simulator (add x86_64 for Intel hosts)
else
    OSX_SYSROOT="iphoneos"
    OSX_ARCHS="arm64"
fi

# The iOS SDK lives inside a full Xcode, not the CommandLineTools. If the
# active developer dir lacks it, point DEVELOPER_DIR at Xcode.app (no sudo).
if ! xcrun --sdk "$OSX_SYSROOT" --show-sdk-path >/dev/null 2>&1; then
    for xc in /Applications/Xcode*.app; do
        if [ -d "$xc/Contents/Developer" ] && \
           DEVELOPER_DIR="$xc/Contents/Developer" \
               xcrun --sdk "$OSX_SYSROOT" --show-sdk-path >/dev/null 2>&1; then
            export DEVELOPER_DIR="$xc/Contents/Developer"
            break
        fi
    done
fi
if ! xcrun --sdk "$OSX_SYSROOT" --show-sdk-path >/dev/null 2>&1; then
    echo "Error: '$OSX_SYSROOT' SDK not found."
    echo "Install a full Xcode (CommandLineTools alone has no iOS SDK)."
    exit 1
fi

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
THIRD_PARTY_DIR="$PROJECT_ROOT/third_party"
# Install per-OS: libs/{mac,ubuntu,ios}/<buildtype>
INSTALL_PREFIX="$PROJECT_ROOT/libs/$TARGET_OS/$BUILD_TYPE"
BUILD_DIR="$PROJECT_ROOT/build/deps-$TARGET_OS-$BUILD_TYPE-$IOS_PLATFORM"
PREFIX_PATH="$CMAKE_PREFIX_PATH"
if [ -n "$PREFIX_PATH" ]; then
    PREFIX_PATH="$PREFIX_PATH;$INSTALL_PREFIX"
else
    PREFIX_PATH="$INSTALL_PREFIX"
fi

# Shared iOS cross-compile flags applied to every dependency.
# CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY lets CMake probe the toolchain
# without trying to run iOS binaries on the host.
IOS_CMAKE_ARGS="-DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_SYSTEM_PROCESSOR=arm64 \
    -DCMAKE_OSX_ARCHITECTURES=$OSX_ARCHS \
    -DCMAKE_OSX_SYSROOT=$OSX_SYSROOT \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=$DEPLOYMENT_TARGET \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_FIND_ROOT_PATH=$INSTALL_PREFIX \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH"

echo "=================================================="
echo "Building third-party dependencies (iOS)"
echo "  Target OS:   $TARGET_OS ($IOS_PLATFORM, $OSX_SYSROOT)"
echo "  Archs:       $OSX_ARCHS  (min iOS $DEPLOYMENT_TARGET)"
echo "  Build Type:  $CMAKE_BUILD_TYPE"
echo "  Install To:  $INSTALL_PREFIX"
echo "  Build Dir:   $BUILD_DIR"
echo "=================================================="

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

# Function to download a dependency pinned to an exact ref (tag or commit SHA).
# Sources are shared with the macOS script and pinned to the same versions.
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
    git init "$dest"
    git -C "$dest" remote add origin "$url"
    git -C "$dest" fetch --depth 1 origin "$ref"
    git -C "$dest" checkout --detach FETCH_HEAD
    git -C "$dest" submodule update --init --recursive --depth 1
}

# Download core sources (no GLFW / GoogleTest: desktop-only).
echo ""
echo "Downloading third-party sources into $THIRD_PARTY_DIR ..."
fetch_source "eigen"         "https://gitlab.com/libeigen/eigen.git"        "3.4.1"                                      "$THIRD_PARTY_DIR/eigen"
fetch_source "sophus"        "https://github.com/strasdat/Sophus.git"       "1.24.6"                                     "$THIRD_PARTY_DIR/sophus"
fetch_source "nlohmann_json" "https://github.com/nlohmann/json.git"         "v3.12.0"                                    "$THIRD_PARTY_DIR/json"
fetch_source "spdlog"        "https://github.com/gabime/spdlog.git"         "33375433e096d59b1e4dd9d46cac9d58a5528ccb"   "$THIRD_PARTY_DIR/spdlog"
fetch_source "opencv"        "https://github.com/opencv/opencv.git"         "4.13.0"                                     "$THIRD_PARTY_DIR/opencv"

# Function to build and install a library with the iOS toolchain flags.
build_library() {
    local name=$1
    local source_dir=$2
    local cmake_args=$3

    echo ""
    echo "Building $name (iOS)..."
    echo "----------------------------------------"

    local lib_build_dir="$BUILD_DIR/$name"
    mkdir -p "$lib_build_dir"
    cd "$lib_build_dir"

    cmake "$source_dir" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DCMAKE_PREFIX_PATH="$PREFIX_PATH" \
        $IOS_CMAKE_ARGS \
        $cmake_args

    cmake --build . -j$NPROC
    cmake --install .

    echo "$name installed successfully!"
}

# 1. Eigen (header-only; install exports the CMake config)
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

# 4. spdlog (static on iOS)
build_library "spdlog" "$THIRD_PARTY_DIR/spdlog" \
    "-DSPDLOG_BUILD_EXAMPLE=OFF \
     -DSPDLOG_USE_STD_FORMAT=ON \
     -DSPDLOG_BUILD_SHARED=OFF \
     -DSPDLOG_BUILD_TESTS=OFF"

# 5. OpenCV (static; FFmpeg/GTK/Qt unavailable on iOS, video uses AVFoundation).
#    This is the heaviest, most fragile cross-compile. If the plain CMake path
#    fails, OpenCV's official platforms/ios/build_framework.py is the fallback.
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
     -DWITH_FFMPEG=OFF \
     -DWITH_PROTOBUF=OFF \
     -DBUILD_PROTOBUF=OFF \
     -DBUILD_opencv_dnn=OFF \
     -DBUILD_opencv_gapi=OFF \
     -DBUILD_opencv_python_bindings_generator=OFF \
     -DBUILD_opencv_java_bindings_generator=OFF \
     -DENABLE_NEON=ON \
     -DBUILD_opencv_world=ON"

echo ""
echo "=================================================="
echo "All iOS dependencies built and installed successfully!"
echo "Install location: $INSTALL_PREFIX"
echo "=================================================="

# TagAR

## Multi-Tag Augmented Reality using AprilTag and ARKit

TagAR is a marker-based augmented reality application that supports simultaneous detection and tracking of multiple AprilTags. 

Each detected tag can be associated with a unique virtual object, enabling multi-object AR experiences on both desktop and mobile platforms.

## Features
- Simultaneous detection of multiple AprilTags
- Independent pose estimation for each tag
- Per-tag 3D object augmentation
- Real-time camera pose visualization
- Cross-platform C++ core library
- ARKit-based iOS application
- Desktop visualization and debugging tools

## Architecture

The project is organized into a platform-independent C++ core and platform-specific applications.

- Core Library
  - AprilTag detection
  - Pose estimation
  - Multi-tag tracking
  - Coordinate transformations
- Desktop Application
  - Camera/video input
  - Visualization and debugging
  - Rapid prototyping
- iOS Application
  - ARKit integration
  - Real-time AR rendering
  - Mobile deployment

## Install Dependencies

Third-party libraries are downloaded and built from source by the platform
build script. Sources are pinned to the same versions used in the `omni_slam`
repository.

### macOS

```bash
# Build type: debug | release (default: release)
./scripts/build_dependencies_macos.sh release
```

The script will:

1. Download each dependency into `third_party/<name>` (shallow git clone at a
   pinned tag/commit, submodules included). Existing sources are reused.
2. Build and install them into `libs/<os>/<buildtype>`.

### Layout

```
third_party/        # dependency sources (OS-independent)
  eigen/ sophus/ json/ spdlog/ googletest/ opencv/
libs/               # built + installed artifacts, separated per OS
  mac/
    release/  debug/
  ubuntu/
  ios/
build/              # intermediate CMake build trees (deps-<os>-<buildtype>)
```

### Dependencies (pinned to omni_slam)

| Library       | Version / Commit |
| ------------- | ---------------- |
| Eigen         | 3.4.1            |
| Sophus        | 1.24.6           |
| nlohmann_json | v3.12.0          |
| spdlog        | `33375433`       |
| GoogleTest    | `ff6133ab`       |
| OpenCV        | 4.13.0           |

> OpenCV is built without the `dnn`/`gapi` modules and protobuf to avoid
> conflicts with a system protobuf.


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

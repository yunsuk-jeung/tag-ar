# TagAR

2D 마커 인식 기반 AR 과제 프로젝트입니다. AprilTag를 인식해 각 태그의 6DoF pose를 추정하고, 태그 위치에 3D 오브젝트를 증강합니다. 공통 C++ tracker core를 두고 desktop viewer와 iOS ARKit 앱에서 같은 추적 로직을 사용합니다.

## 구현 범위

- AprilTag 36h11 기반 다중 태그 검출
- `solvePnPGeneric` + `SOLVEPNP_IPPE_SQUARE` 기반 태그 pose 추정
- 깊이 데이터가 있는 recorded dataset에 대한 depth 기반 pose refinement
- 검출이 잠깐 끊긴 태그를 위한 pyramidal Lucas-Kanade optical flow fallback
- One Euro Filter 기반 pose smoothing
- ARKit camera pose와 태그 pose를 같은 world 좌표계에서 시각화
- 태그별 texture cube 증강
- Desktop recorded dataset replay viewer
- iOS ARKit live camera 앱
- `config/tracker.json` 기반 runtime tracker 설정
- iOS 앱에서 tag size 입력 후 tracker 재초기화

## 프로젝트 구조

```text
cpp/
  include/tagar/        # platform-independent public headers
  src/                  # tracker, frame, file reader, config loader
  tests/                # C++ unit tests

apps/desktop/           # OpenGL/GLFW desktop replay viewer
apps/ios/TagARKit/      # Objective-C++ bridge + C++ core wrapper
apps/ios/tag_ar_app/    # ARKit + Metal iOS sample app

config/tracker.json     # tracker runtime config
datasets/               # desktop replay용 recorded datasets
assets/                 # tag texture assets
scripts/                # third-party dependency build scripts
```

## 핵심 코드 위치

- Tracker main loop: `cpp/src/tracker.cpp`
- Frame conversion / pyramid cache: `cpp/src/frame.cpp`
- Tracker config defaults: `cpp/include/tagar/tracker_config.hpp`
- Config JSON loader: `cpp/src/tracker_config.cpp`
- Desktop entrypoint: `apps/desktop/main.cpp`
- iOS bridge: `apps/ios/TagARKit/TagARKit/TagARKitWrapper.cpp`
- iOS app controller: `apps/ios/tag_ar_app/tag_ar_app/ViewController.swift`
- iOS Metal renderer: `apps/ios/tag_ar_app/tag_ar_app/Renderer.swift`

## Dependency

Third-party dependency는 platform별 script로 source build 후 `libs/<os>/<buildtype>`에 설치합니다.

### macOS

```bash
./scripts/build_dependencies_macos.sh release
```

### Ubuntu

```bash
./scripts/build_dependencies_ubuntu.sh release
```

### iOS

```bash
./scripts/build_dependencies_ios.sh release
```

iOS dependency build는 full Xcode가 필요합니다. Command Line Tools만 선택된 상태에서는 iOS SDK를 찾지 못할 수 있습니다.

## Desktop Build / Run

```bash
cmake --preset release
cmake --build --preset release --target tagar_ar_sample
```

실행:

```bash
./build/apps/desktop/tagar_ar_sample
```

기본 dataset은 `datasets/09`입니다. 다른 dataset을 사용하려면 첫 번째 인자로 경로를 넘깁니다.

```bash
./build/apps/desktop/tagar_ar_sample datasets/01
```

## Dataset

Desktop replay에 사용한 recorded dataset은 repository의 `datasets/` 디렉터리 또는 아래 Google Drive 폴더에서 확인할 수 있습니다.

- [TagAR recorded datasets](https://drive.google.com/drive/folders/1QVz6w2GrT7RH60xMwjhhKRJ-sOmz1JpY?usp=sharing)

각 dataset 디렉터리는 같은 basename의 `.mp4`, `.json`, 선택적 `.depth` 파일로 구성됩니다. 예를 들어 `datasets/09`는 RGB 영상, ARKit metadata, depth blob을 함께 사용합니다.

## Demo

썸네일을 클릭하면 YouTube에서 영상을 볼 수 있습니다.

| Desktop demo | iPhone demo |
| --- | --- |
| [![Desktop demo video](https://img.youtube.com/vi/jRqBUkxuKtc/hqdefault.jpg)](https://youtu.be/jRqBUkxuKtc) | [![iPhone demo video](https://img.youtube.com/vi/tFITOZecvIE/hqdefault.jpg)](https://youtu.be/tFITOZecvIE) |

## Test

자동화 테스트는 최소 범위만 포함되어 있습니다.

```bash
ctest --test-dir build --output-on-failure
```

현재 test는 recorded dataset reader의 metadata parsing, frame decode, timestamp/intrinsics parsing을 확인하는 smoke test 수준입니다. Tracker pose estimation, optical flow, depth refinement에 대한 synthetic test는 남은 개선 항목입니다.

## iOS Build / Run

1. iOS dependency를 먼저 빌드합니다.

```bash
./scripts/build_dependencies_ios.sh release
```

2. Xcode에서 앱 project를 엽니다.

```text
apps/ios/tag_ar_app/tag_ar_app.xcodeproj
```

3. `tag_ar_app` scheme을 iPhone device에서 실행합니다.

iOS 앱은 bundle에 포함된 `tracker.json`을 읽어 tracker를 초기화합니다. 화면의 `Tag size (m)` 입력값을 바꾸고 `Reset`을 누르면 JSON 설정은 유지한 채 `tag_size_m`만 입력값으로 override해 tracker와 renderer cube scale을 다시 설정합니다.

## Tracker Config

Tracker 동작은 `config/tracker.json`에서 조정합니다. Desktop viewer는 `apps/desktop/main.cpp`에서 이 파일을 읽고, iOS 앱은 bundle에 포함된 `tracker.json`을 읽습니다.

```json
{
  "tag_size_m": 0.085,
  "tag_discard_time_threshold": 0.08e9,
  "target_width": 640,
  "flow_enabled": true,
  "flow_fb_threshold_px": 0.5,
  "depth_refine_enabled": true,
  "max_depth_samples": 256,
  "refine_sigma_px": 1.0,
  "refine_sigma_z": 0.4,
  "refine_huber_m": 0.8,
  "refine_max_iters": 5,
  "filter_enabled": true,
  "filter_translation": true,
  "filter_rotation": true,
  "pos_min_cutoff": 1.0,
  "pos_beta": 100,
  "rot_min_cutoff": 1.0,
  "rot_beta": 0.5,
  "d_cutoff": 3.0
}
```

| Key | 의미 | 현재 값 |
| --- | --- | --- |
| `tag_size_m` | 태그 한 변의 실제 길이(m). PnP pose scale을 결정합니다. iOS 앱에서는 UI 입력값으로 이 값만 runtime override합니다. | `0.085` |
| `tag_discard_time_threshold` | 마지막 관측 이후 tag를 유지하는 시간(ns). 검출이 잠깐 끊겨도 바로 사라지지 않게 합니다. | `0.08e9` |
| `target_width` | detection/tracking용 grayscale frame resize width. 작을수록 빠르지만 corner precision은 낮아질 수 있습니다. | `640` |
| `flow_enabled` | 현재 frame에서 검출되지 않은 기존 tag를 optical flow로 추적할지 여부입니다. | `true` |
| `flow_fb_threshold_px` | forward-backward optical flow consistency threshold(px). 작을수록 엄격하게 reject합니다. | `0.5` |
| `depth_refine_enabled` | depth frame이 있을 때 tag plane의 depth sample로 pose를 refine할지 여부입니다. | `true` |
| `max_depth_samples` | tag quad 내부에서 사용할 최대 depth sample 수입니다. | `256` |
| `refine_sigma_px` | corner reprojection residual의 pixel noise scale입니다. 작을수록 image corner를 더 강하게 믿습니다. | `1.0` |
| `refine_sigma_z` | depth residual의 meter 단위 noise scale입니다. 클수록 depth 영향이 약해집니다. | `0.4` |
| `refine_huber_m` | depth residual robust threshold(m). 큰 depth outlier가 pose를 끌고 가지 않도록 제한합니다. | `0.8` |
| `refine_max_iters` | Gauss-Newton depth refinement 최대 반복 횟수입니다. | `5` |
| `filter_enabled` | tag pose에 One Euro Filter를 적용할지 여부입니다. | `true` |
| `filter_translation` | translation component smoothing 여부입니다. | `true` |
| `filter_rotation` | rotation component smoothing 여부입니다. | `true` |
| `pos_min_cutoff` | translation filter의 기본 cutoff입니다. 낮을수록 정지 상태 jitter가 줄고 반응은 느려집니다. | `1.0` |
| `pos_beta` | translation 속도에 따라 cutoff를 키우는 계수입니다. 높을수록 움직일 때 lag가 줄어듭니다. | `100` |
| `rot_min_cutoff` | rotation filter의 기본 cutoff입니다. | `1.0` |
| `rot_beta` | rotation 속도에 따른 adaptive cutoff 계수입니다. | `0.5` |
| `d_cutoff` | 속도 추정값에 대한 low-pass cutoff입니다. 높을수록 움직임 시작에 더 빠르게 반응합니다. | `3.0` |

튜닝 기준은 다음과 같습니다.

- detection 속도가 부족하면 `target_width`를 낮춥니다.
- tag가 순간적으로 사라지는 문제가 있으면 `flow_enabled`를 켜고 `tag_discard_time_threshold`를 늘립니다.
- AR 오브젝트가 너무 늦게 따라오면 `pos_min_cutoff`, `pos_beta`, `d_cutoff`를 올립니다.
- 정지 상태 jitter가 크면 `pos_min_cutoff`, `rot_min_cutoff`를 낮춥니다.
- depth refine이 reprojection을 과하게 흔들면 `refine_sigma_z`, `refine_huber_m`을 키워 depth 영향을 약하게 합니다.

`TrackerConfig`의 C++ 기본값도 현재 JSON 값과 맞춰두었습니다. 따라서 JSON 파일을 읽지 못하는 경우에도 같은 기본 동작을 사용합니다.

## 알고리즘 흐름

1. 입력 frame을 grayscale로 변환하고 필요 시 `target_width`로 downscale
2. AprilTag detector로 tag corner와 id 검출
3. 현재 frame에서 검출되지 않은 기존 tag는 optical flow로 corner 추적 시도
4. 각 tag에 대해 IPPE square PnP 후보 pose 계산
5. depth가 있으면 depth score로 PnP ambiguity를 선택
6. depth sample과 corner reprojection residual을 함께 사용해 Gauss-Newton refinement 수행
7. tag pose를 world 좌표계로 변환
8. One Euro Filter로 pose smoothing
9. desktop/iOS renderer에서 camera pose와 tag cube를 시각화

## Desktop Viewer

Desktop viewer는 recorded dataset을 replay하면서 다음을 시각화합니다.

- world origin axis
- ARKit camera frustum / camera axis
- tag pose에 놓인 textured cube
- grayscale reprojection inset
- depth point cloud

## iOS App

iOS 앱은 ARKit live camera frame을 C++ tracker로 넘기고, Metal renderer가 tracker 결과를 받아 tag 위치에 cube를 그립니다.

- ARKit camera transform은 OpenCV camera convention에 맞게 Y/Z basis를 flip합니다.
- tag texture cube는 unit cube mesh를 사용하고, 입력된 tag size로 model matrix를 scale합니다.
- `Look at me` switch가 켜져 있으면 cube의 textured face가 camera를 바라보도록 billboard orientation을 적용합니다.

## 제출 / 시연 포인트

시연 영상에서는 다음 장면이 보이면 구현 범위가 명확합니다.

- 여러 AprilTag를 동시에 인식
- 태그별 cube가 안정적으로 증강
- 카메라 이동 시 cube pose가 world에 고정
- 태그가 잠깐 가려져도 optical flow fallback으로 짧게 유지

## 남은 개선 가능 부분

- Tracker 로직에 대한 synthetic unit test 추가
- depth refinement 결과가 reprojection error를 과도하게 악화시키는 경우 reject하는 acceptance criterion 추가
- iOS에서 config parameter를 UI로 조정하는 debug panel 추가
- feature tracking과 detection result를 결합하는 더 견고한 multi-frame tracker 확장 (키포인트 검출기 활용)

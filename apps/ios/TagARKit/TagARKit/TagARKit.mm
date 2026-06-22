//
//  TagARKit.mm
//  TagARKit
//
//  Created by Yunsuk Jeung on 22/6/2026.
//

#import "TagARKit.h"

#include <cstring>

#include "TagARKitWrapper.h"

#pragma mark - TagARTagPose

@interface TagARTagPose ()
- (instancetype)initWithTagId:(int)tagId transform:(simd_float4x4)transform;
@end

@implementation TagARTagPose {
  int _tagId;
  simd_float4x4 _transform;
}

- (instancetype)initWithTagId:(int)tagId transform:(simd_float4x4)transform {
  if (self = [super init]) {
    _tagId = tagId;
    _transform = transform;
  }
  return self;
}

- (int)tagId {
  return _tagId;
}

- (simd_float4x4)transform {
  return _transform;
}

@end

#pragma mark - TagARTracker

@implementation TagARTracker {
  tagar::Wrapper _wrapper;
}

+ (void)setLogDirectory:(NSString *)directory {
  tagar::Wrapper::InitLogging(directory.UTF8String);
}

- (instancetype)initWithConfigPath:(NSString *)configPath {
  if (self = [super init]) {
    _wrapper.InitWithConfigFile(configPath.UTF8String);
  }
  return self;
}

- (void)start {
  _wrapper.Start();
}

- (void)stop {
  _wrapper.Stop();
}

- (void)submitFrameRGB:(const uint8_t *)rgb
                 width:(int)width
                height:(int)height
             timestamp:(int64_t)timestampNs
                  pose:(simd_float4x4)pose
            intrinsics:(simd_float4)intrinsics {
  // simd_float4x4 is column-major, matching the wrapper's pose layout.
  float poseArray[16];
  std::memcpy(poseArray, &pose, sizeof(float) * 16);

  const float intrinsicsArray[4] = {intrinsics.x, intrinsics.y, intrinsics.z,
                                     intrinsics.w};

  _wrapper.SubmitFrameRGB(timestampNs, rgb, width, height, poseArray,
                          intrinsicsArray);
}

- (NSArray<TagARTagPose *> *)latestTags {
  tagar::WrapperResult result;
  if (!_wrapper.GetLatestResult(result)) {
    return @[];
  }

  NSMutableArray<TagARTagPose *> *tags =
      [NSMutableArray arrayWithCapacity:result.tags.size()];
  for (const tagar::WrapperTagPose &tag : result.tags) {
    simd_float4x4 transform;
    std::memcpy(&transform, tag.T_w_t, sizeof(float) * 16);
    [tags addObject:[[TagARTagPose alloc] initWithTagId:tag.id
                                              transform:transform]];
  }
  return tags;
}

+ (void)ping {
  tagar::Wrapper::Ping();
}

@end

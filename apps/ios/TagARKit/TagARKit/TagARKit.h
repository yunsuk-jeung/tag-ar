//
//  TagARKit.h
//  TagARKit
//
//  Created by Yunsuk Jeung on 22/6/2026.
//

#import <Foundation/Foundation.h>
#import <simd/simd.h>

//! Project version number for TagARKit.
FOUNDATION_EXPORT double TagARKitVersionNumber;

//! Project version string for TagARKit.
FOUNDATION_EXPORT const unsigned char TagARKitVersionString[];

NS_ASSUME_NONNULL_BEGIN

// One tracked tag's pose, exposed to Swift.
@interface TagARTagPose : NSObject
@property(nonatomic, readonly) int tagId;
@property(nonatomic, readonly) simd_float4x4 transform;  // tag-to-world
@end

@interface TagARTracker : NSObject

+ (void)setLogDirectory:(NSString *)directory;

- (instancetype)initWithConfigPath:(NSString *)configPath NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)start;
- (void)stop;

// Submit an RGB frame (3 bytes/pixel, row-major). pose is camera-to-world,
// intrinsics is [fx, fy, cx, cy].
- (void)submitFrameRGB:(const uint8_t *)rgb
                 width:(int)width
                height:(int)height
             timestamp:(int64_t)timestampNs
                  pose:(simd_float4x4)pose
            intrinsics:(simd_float4)intrinsics;

// Latest tracked tags (empty if none yet).
- (NSArray<TagARTagPose *> *)latestTags;

// Bridge smoke test.
+ (void)ping;

@end

NS_ASSUME_NONNULL_END

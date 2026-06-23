//
//  TagARKit.h
//  TagARKit
//
//  Created by Yunsuk Jeung on 22/6/2026.
//

#import <CoreVideo/CoreVideo.h>
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

// Create a tracker from tracker.json, overriding only tag_size_m.
- (instancetype)initWithConfigPath:(NSString *)configPath
                           tagSize:(float)tagSizeMeters NS_DESIGNATED_INITIALIZER;

// Create a tracker with an explicit tag size in meters (for runtime reset).
- (instancetype)initWithTagSize:(float)tagSizeMeters NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)start;
- (void)stop;

- (void)submitFrame:(CVPixelBufferRef)pixelBuffer
          timestamp:(int64_t)timestampNs
               pose:(simd_float4x4)pose
         intrinsics:(simd_float4)intrinsics;

// Latest tracked tags (empty if none yet).
- (NSArray<TagARTagPose *> *)latestTags;

// Bridge smoke test.
+ (void)ping;

@end

NS_ASSUME_NONNULL_END

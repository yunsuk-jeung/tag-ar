//
//  TagARKit.h
//  TagARKit
//
//  Created by Yunsuk Jeung on 22/6/2026.
//

#import <Foundation/Foundation.h>

//! Project version number for TagARKit.
FOUNDATION_EXPORT double TagARKitVersionNumber;

//! Project version string for TagARKit.
FOUNDATION_EXPORT const unsigned char TagARKitVersionString[];

NS_ASSUME_NONNULL_BEGIN

@interface TagARTracker : NSObject
// Bridge smoke test: logs from the Objective-C++ side.
+ (void)ping;
@end

NS_ASSUME_NONNULL_END

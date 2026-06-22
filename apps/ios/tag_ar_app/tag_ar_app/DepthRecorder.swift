//
//  DepthRecorder.swift
//  tag_ar_app
//
//  Created by Yunsuk Jeung on 23/6/2026.
//

import CoreVideo
import Foundation

/// Streams per-frame LiDAR depth (Float32 meters) to a flat binary blob: each
/// frame is width*height tightly packed floats, one after another. Frame i
/// aligns 1:1 with MetadataRecorder frame i (both appended together per frame).
final class DepthRecorder {

    private(set) var isRecording = false
    private(set) var width = 0
    private(set) var height = 0

    private var handle: FileHandle?

    /// Opens the output blob. Depth resolution is discovered on the first append.
    func start(url: URL) {
        guard !isRecording else { return }
        width = 0
        height = 0
        FileManager.default.createFile(atPath: url.path, contents: nil)
        handle = try? FileHandle(forWritingTo: url)
        isRecording = (handle != nil)
        if handle == nil {
            print("DepthRecorder: failed to open \(url.path)")
        }
    }

    /// Appends one depth frame. `depthMap` must be a DepthFloat32 pixel buffer
    /// (ARFrame.sceneDepth.depthMap).
    func append(depthMap: CVPixelBuffer) {
        guard isRecording, let handle = handle else { return }

        CVPixelBufferLockBaseAddress(depthMap, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(depthMap, .readOnly) }

        let w = CVPixelBufferGetWidth(depthMap)
        let h = CVPixelBufferGetHeight(depthMap)
        if width == 0 {
            width = w
            height = h
        }
        guard w == width, h == height,
              let base = CVPixelBufferGetBaseAddress(depthMap) else {
            return
        }

        let bytesPerRow = CVPixelBufferGetBytesPerRow(depthMap)
        let rowBytes = w * MemoryLayout<Float32>.size

        // Copy row by row into a tightly packed buffer (stride may exceed w*4).
        var packed = Data(count: rowBytes * h)
        packed.withUnsafeMutableBytes { dst in
            guard let dstBase = dst.baseAddress else { return }
            for row in 0..<h {
                memcpy(dstBase.advanced(by: row * rowBytes),
                       base.advanced(by: row * bytesPerRow),
                       rowBytes)
            }
        }
        handle.write(packed)
    }

    /// Closes the blob and returns the recorded depth resolution.
    @discardableResult
    func stop() -> (width: Int, height: Int)? {
        guard isRecording else { return nil }
        isRecording = false
        try? handle?.close()
        handle = nil
        return width > 0 ? (width, height) : nil
    }
}

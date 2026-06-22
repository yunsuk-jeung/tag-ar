//
//  MetadataRecorder.swift
//  tag_ar_app
//
//  Created by Yunsuk Jeung on 20/6/2026.
//

import ARKit
import CoreMedia
import simd

/// Records per-frame camera intrinsics and 6DoF pose alongside the video,
/// then writes them to a JSON file in the app's Documents directory.
final class MetadataRecorder {

    /// One captured frame's camera metadata.
    private struct FrameMetadata: Encodable {
        /// CMTime value (numerator); seconds = t / timescale. Matches the video frame PTS.
        let timestamp: Int64
        /// 4x4 camera-to-world pose (6DoF), column-major, 16 floats.
        let transform: [Float]
        /// Pinhole intrinsics in pixels: [fx, fy, cx, cy].
        let intrinsics: [Float]
    }

    /// Top-level recording document.
    private struct Recording: Encodable {
        /// Image resolution the intrinsics are referenced to (pixels).
        let imageWidth: Int
        let imageHeight: Int
        let depthWidth: Int
        let depthHeight: Int
        /// CMTime timescale (denominator) shared with the video; seconds = t / timescale.
        let timescale: Int32
        let frames: [FrameMetadata]
    }

    private(set) var isRecording = false
    private var frames: [FrameMetadata] = []
    private var imageWidth = 0
    private var imageHeight = 0
    private var depthWidth = 0
    private var depthHeight = 0
    private var timescale: Int32 = 0
    private var outputURL: URL?

    /// Begins collecting metadata. `imageResolution` is `ARCamera.imageResolution`.
    func start(url: URL, imageResolution: CGSize) {
        guard !isRecording else { return }
        frames.removeAll(keepingCapacity: true)
        imageWidth = Int(imageResolution.width)
        imageHeight = Int(imageResolution.height)
        outputURL = url
        isRecording = true
    }

    func setDepthResolution(width: Int, height: Int) {
        depthWidth = width
        depthHeight = height
    }

    /// Appends the camera intrinsics and pose for a single frame.
    /// `timestamp` is the same CMTime used for the matching video frame's PTS.
    func append(camera: ARCamera, timestamp: CMTime) {
        guard isRecording else { return }
        timescale = timestamp.timescale
        let k = camera.intrinsics
        frames.append(FrameMetadata(
            timestamp: timestamp.value,
            transform: Self.flatten(camera.transform),
            intrinsics: [k.columns.0.x, k.columns.1.y, k.columns.2.x, k.columns.2.y]
        ))
    }

    /// Writes the collected metadata to JSON and returns the saved file URL.
    @discardableResult
    func stop() -> URL? {
        guard isRecording, let url = outputURL else { return nil }
        isRecording = false

        let recording = Recording(
            imageWidth: imageWidth,
            imageHeight: imageHeight,
            depthWidth: depthWidth,
            depthHeight: depthHeight,
            timescale: timescale,
            frames: frames
        )
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted]

        do {
            let data = try encoder.encode(recording)
            try data.write(to: url, options: .atomic)
            reset()
            return url
        } catch {
            print("MetadataRecorder: failed to write JSON: \(error)")
            reset()
            return nil
        }
    }

    private func reset() {
        frames.removeAll(keepingCapacity: false)
        imageWidth = 0
        imageHeight = 0
        depthWidth = 0
        depthHeight = 0
        timescale = 0
        outputURL = nil
    }

    /// Flattens a 4x4 matrix into a column-major float array.
    private static func flatten(_ m: simd_float4x4) -> [Float] {
        [m.columns.0, m.columns.1, m.columns.2, m.columns.3]
            .flatMap { [$0.x, $0.y, $0.z, $0.w] }
    }
}

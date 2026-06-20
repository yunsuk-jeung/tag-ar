//
//  VideoRecorder.swift
//  tag_ar_app
//
//  Created by Yunsuk Jeung on 20/6/2026.
//

import AVFoundation
import CoreVideo

/// Writes ARKit captured frames to an H.264 .mp4 file in the app's Documents directory.
final class VideoRecorder {

    private var assetWriter: AVAssetWriter?
    private var videoInput: AVAssetWriterInput?
    private var pixelBufferAdaptor: AVAssetWriterInputPixelBufferAdaptor?

    private var hasStartedSession = false
    private(set) var isRecording = false
    private(set) var outputURL: URL?

    /// Configures the asset writer for the given frame dimensions and begins a recording.
    /// `width`/`height` come from the ARKit captured pixel buffer (sensor orientation, landscape).
    func start(url: URL, width: Int, height: Int) {
        guard !isRecording else { return }

        try? FileManager.default.removeItem(at: url)

        guard let writer = try? AVAssetWriter(outputURL: url, fileType: .mp4) else {
            print("VideoRecorder: failed to create asset writer")
            return
        }

        let settings: [String: Any] = [
            AVVideoCodecKey: AVVideoCodecType.h264,
            AVVideoWidthKey: width,
            AVVideoHeightKey: height,
        ]
        let input = AVAssetWriterInput(mediaType: .video, outputSettings: settings)
        input.expectsMediaDataInRealTime = true

        // Match ARKit's captured image format (YCbCr 4:2:0 bi-planar, full range).
        let sourceAttributes: [String: Any] = [
            kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
            kCVPixelBufferWidthKey as String: width,
            kCVPixelBufferHeightKey as String: height,
        ]
        let adaptor = AVAssetWriterInputPixelBufferAdaptor(
            assetWriterInput: input,
            sourcePixelBufferAttributes: sourceAttributes
        )

        guard writer.canAdd(input) else {
            print("VideoRecorder: cannot add video input")
            return
        }
        writer.add(input)

        guard writer.startWriting() else {
            print("VideoRecorder: failed to start writing: \(String(describing: writer.error))")
            return
        }

        assetWriter = writer
        videoInput = input
        pixelBufferAdaptor = adaptor
        outputURL = url
        hasStartedSession = false
        isRecording = true
    }

    /// Appends a single captured frame. `timestamp` is the ARFrame timestamp in seconds.
    func append(pixelBuffer: CVPixelBuffer, timestamp: TimeInterval) {
        guard isRecording,
              let input = videoInput,
              let adaptor = pixelBufferAdaptor else { return }

        let time = CMTime(seconds: timestamp, preferredTimescale: 1_000_000)

        if !hasStartedSession {
            assetWriter?.startSession(atSourceTime: time)
            hasStartedSession = true
        }

        guard input.isReadyForMoreMediaData else { return }
        adaptor.append(pixelBuffer, withPresentationTime: time)
    }

    /// Finishes the recording and reports the saved file URL on the main queue.
    func stop(completion: @escaping (URL?) -> Void) {
        guard isRecording, let writer = assetWriter, let input = videoInput else {
            completion(nil)
            return
        }

        isRecording = false
        let url = outputURL
        input.markAsFinished()
        writer.finishWriting {
            DispatchQueue.main.async {
                if writer.status == .completed {
                    completion(url)
                } else {
                    print("VideoRecorder: finish failed: \(String(describing: writer.error))")
                    completion(nil)
                }
            }
            self.reset()
        }
    }

    private func reset() {
        assetWriter = nil
        videoInput = nil
        pixelBufferAdaptor = nil
        hasStartedSession = false
        outputURL = nil
    }
}

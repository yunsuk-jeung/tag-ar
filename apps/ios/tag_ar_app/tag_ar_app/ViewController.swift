//
//  ViewController.swift
//  tag_ar_app
//
//  Created by Yunsuk Jeung on 20/6/2026.
//

import UIKit
import Metal
import MetalKit
import ARKit

extension MTKView : RenderDestinationProvider {
}

class ViewController: UIViewController, MTKViewDelegate, ARSessionDelegate {
    
    var session: ARSession!
    var renderer: Renderer!

    // Recording state, button, and writers
    private var isRecording = false
    private let recordButton = UIButton(type: .system)
    private let videoRecorder = VideoRecorder()
    private let metadataRecorder = MetadataRecorder()

    override func viewDidLoad() {
        super.viewDidLoad()

        // Set the view's delegate
        session = ARSession()
        session.delegate = self
        
        // Set the view to use the default device
        if let view = self.view as? MTKView {
            view.device = MTLCreateSystemDefaultDevice()
            view.backgroundColor = UIColor.clear
            view.delegate = self
            
            guard view.device != nil else {
                print("Metal is not supported on this device")
                return
            }
            
            // Configure the renderer to draw to the view
            renderer = Renderer(session: session, metalDevice: view.device!, renderDestination: view)
            
            renderer.drawRectResized(size: view.bounds.size)
        }
        
        setupRecordButton()

        let tapGesture = UITapGestureRecognizer(target: self, action: #selector(ViewController.handleTap(gestureRecognize:)))
        view.addGestureRecognizer(tapGesture)
    }

    private func setupRecordButton() {
        recordButton.translatesAutoresizingMaskIntoConstraints = false
        recordButton.setTitle("Record", for: .normal)
        recordButton.titleLabel?.font = .boldSystemFont(ofSize: 18)
        recordButton.setTitleColor(.white, for: .normal)
        recordButton.backgroundColor = .systemRed
        recordButton.layer.cornerRadius = 28
        recordButton.addTarget(self, action: #selector(toggleRecording), for: .touchUpInside)

        view.addSubview(recordButton)

        // Set size and position in code (avoids the greyed-out width/height in Storyboard)
        NSLayoutConstraint.activate([
            recordButton.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            recordButton.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -32),
            recordButton.widthAnchor.constraint(equalToConstant: 160),
            recordButton.heightAnchor.constraint(equalToConstant: 56),
        ])
    }

    @objc private func toggleRecording() {
        isRecording.toggle()
        if isRecording {
            recordButton.setTitle("Stop", for: .normal)
            recordButton.backgroundColor = .systemGray
            startRecording()
        } else {
            recordButton.setTitle("Record", for: .normal)
            recordButton.backgroundColor = .systemRed
            stopRecording()
        }
    }

    private func startRecording() {
        // Use the current captured frame to determine the video dimensions.
        guard let frame = session.currentFrame else {
            print("startRecording: no current frame")
            return
        }
        let pixelBuffer = frame.capturedImage
        let width = CVPixelBufferGetWidth(pixelBuffer)
        let height = CVPixelBufferGetHeight(pixelBuffer)

        // Pair the video and metadata files under a shared base name.
        let base = "recording-\(Int(Date().timeIntervalSince1970))"
        let documents = FileManager.default
            .urls(for: .documentDirectory, in: .userDomainMask)[0]
        let videoURL = documents.appendingPathComponent("\(base).mp4")
        let metadataURL = documents.appendingPathComponent("\(base).json")

        videoRecorder.start(url: videoURL, width: width, height: height)
        metadataRecorder.start(url: metadataURL, imageResolution: frame.camera.imageResolution)
    }

    private func stopRecording() {
        let metadataURL = metadataRecorder.stop()
        videoRecorder.stop { url in
            if let url = url {
                print("Video saved: \(url.path)")
            } else {
                print("Video recording failed")
            }
            if let metadataURL = metadataURL {
                print("Metadata saved: \(metadataURL.path)")
            }
        }
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        // Create a session configuration
        let configuration = ARWorldTrackingConfiguration()

        // Run the view's session
        session.run(configuration)
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        
        // Pause the view's session
        session.pause()
    }
    
    @objc
    func handleTap(gestureRecognize: UITapGestureRecognizer) {
        // Create anchor using the camera's current position
        if let currentFrame = session.currentFrame {
            
            // Create a transform with a translation of 0.2 meters in front of the camera
            var translation = matrix_identity_float4x4
            translation.columns.3.z = -0.2
            let transform = simd_mul(currentFrame.camera.transform, translation)
            
            // Add a new anchor to the session
            let anchor = ARAnchor(transform: transform)
            session.add(anchor: anchor)
        }
    }
    
    // MARK: - MTKViewDelegate
    
    // Called whenever view changes orientation or layout is changed
    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
        renderer.drawRectResized(size: size)
    }
    
    // Called whenever the view needs to render
    func draw(in view: MTKView) {
        renderer.update()
    }
    
    // MARK: - ARSessionDelegate

    // Called every time the session updates with a new captured frame.
    func session(_ session: ARSession, didUpdate frame: ARFrame) {
        guard isRecording else { return }
        videoRecorder.append(pixelBuffer: frame.capturedImage, timestamp: frame.timestamp)
        metadataRecorder.append(camera: frame.camera, timestamp: frame.timestamp)
    }

    func session(_ session: ARSession, didFailWithError error: Error) {
        // Present an error message to the user
        
    }
    
    func sessionWasInterrupted(_ session: ARSession) {
        // Inform the user that the session has been interrupted, for example, by presenting an overlay
        
    }
    
    func sessionInterruptionEnded(_ session: ARSession) {
        // Reset tracking and/or remove existing anchors if consistent tracking is required
        
    }
}

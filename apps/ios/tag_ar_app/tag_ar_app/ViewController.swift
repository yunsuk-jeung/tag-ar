//
//  ViewController.swift
//  tag_ar_app
//
//  Created by Yunsuk Jeung on 20/6/2026.
//

import ARKit
import Metal
import MetalKit
import UIKit
import TagARKit

extension MTKView: RenderDestinationProvider {
}

class ViewController: UIViewController, MTKViewDelegate, ARSessionDelegate {

  var session: ARSession!
  var renderer: Renderer!

  // Recording state, button, and writers
  private var isRecording = false
  private let recordButton = UIButton(type: .system)
  private let lookAtSwitch = UISwitch()
  private let tagSizeField = UITextField()
  private let resetButton = UIButton(type: .system)
  private let defaultTagSize: Float = 0.085
  private let videoRecorder = VideoRecorder()
  private let metadataRecorder = MetadataRecorder()
  private let depthRecorder = DepthRecorder()

  // Tag tracker bridged from the C++ core.
  private var tagTracker: TagARTracker?

  override func viewDidLoad() {
    super.viewDidLoad()

    setupTagTracker()

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
      renderer.tagCubeSize = defaultTagSize

      renderer.drawRectResized(size: view.bounds.size)
    }

    setupRecordButton()
    setupLookAtControl()
    setupTagSizeControls()

    let tapGesture = UITapGestureRecognizer(
      target: self, action: #selector(ViewController.handleTap(gestureRecognize:)))
    view.addGestureRecognizer(tapGesture)
  }

  private func setupTagTracker() {
    let logDir = FileManager.default
      .urls(for: .cachesDirectory, in: .userDomainMask)[0].path
    TagARTracker.setLogDirectory(logDir)

    guard let configPath = trackerConfigPath() else { return }
    tagTracker = TagARTracker(configPath: configPath)
    tagTracker?.start()
  }

  private func trackerConfigPath() -> String? {
    guard let configPath = Bundle.main.path(forResource: "tracker", ofType: "json") else {
      print("[TagARKit] ERROR: tracker.json not found in app bundle")
      return nil
    }
    return configPath
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

    // Bottom-right corner.
    NSLayoutConstraint.activate([
      recordButton.trailingAnchor.constraint(
        equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -20),
      recordButton.bottomAnchor.constraint(
        equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -32),
      recordButton.widthAnchor.constraint(equalToConstant: 120),
      recordButton.heightAnchor.constraint(equalToConstant: 56),
    ])
  }

  private func setupLookAtControl() {
    let label = UILabel()
    label.text = "Look at me"
    label.textColor = .white
    label.font = .boldSystemFont(ofSize: 16)

    lookAtSwitch.isOn = true
    lookAtSwitch.addTarget(self, action: #selector(toggleLookAt), for: .valueChanged)

    let stack = UIStackView(arrangedSubviews: [label, lookAtSwitch])
    stack.axis = .horizontal
    stack.spacing = 8
    stack.alignment = .center
    stack.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(stack)

    NSLayoutConstraint.activate([
      stack.trailingAnchor.constraint(
        equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -20),
      stack.topAnchor.constraint(
        equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 16),
    ])
  }

  @objc private func toggleLookAt() {
    renderer?.faceCamera = lookAtSwitch.isOn
  }

  // Tag-size input + Reset button: rebuilds the tracker with the entered scale.
  private func setupTagSizeControls() {
    let label = UILabel()
    label.text = "Tag size (m)"
    label.textColor = .white
    label.font = .boldSystemFont(ofSize: 14)

    tagSizeField.text = String(defaultTagSize)
    tagSizeField.keyboardType = .decimalPad
    tagSizeField.borderStyle = .roundedRect
    tagSizeField.backgroundColor = .white
    tagSizeField.textColor = .black
    tagSizeField.widthAnchor.constraint(equalToConstant: 90).isActive = true

    resetButton.setTitle("Reset", for: .normal)
    resetButton.titleLabel?.font = .boldSystemFont(ofSize: 16)
    resetButton.setTitleColor(.white, for: .normal)
    resetButton.backgroundColor = .systemBlue
    resetButton.layer.cornerRadius = 8
    resetButton.addTarget(self, action: #selector(resetTracker), for: .touchUpInside)
    resetButton.widthAnchor.constraint(equalToConstant: 72).isActive = true
    resetButton.heightAnchor.constraint(equalToConstant: 36).isActive = true

    let stack = UIStackView(arrangedSubviews: [label, tagSizeField, resetButton])
    stack.axis = .horizontal
    stack.spacing = 8
    stack.alignment = .center
    stack.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(stack)

    NSLayoutConstraint.activate([
      stack.leadingAnchor.constraint(
        equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 20),
      stack.topAnchor.constraint(
        equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 16),
    ])
  }

  // Tear down the tracker and recreate it with the entered tag size.
  @objc private func resetTracker() {
    view.endEditing(true)

    let parsed = Float(tagSizeField.text ?? "") ?? defaultTagSize
    let tagSize = parsed > 0 ? parsed : defaultTagSize
    tagSizeField.text = String(tagSize)

    // Destroy the old tracker first: stop() joins its worker thread, then
    // releasing the last reference runs the C++ Tracker destructor (which also
    // stops/joins). nil-first guarantees only one tracker exists at a time.
    tagTracker?.stop()
    tagTracker = nil

    guard let configPath = trackerConfigPath() else { return }
    tagTracker = TagARTracker(configPath: configPath, tagSize: tagSize)
    tagTracker?.start()
    renderer?.tagCubeSize = tagSize
    renderer?.tagInstances = []  // clear stale cubes until re-detected
    print("[TagARKit] tracker reset, tag size = \(tagSize) m")
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
    let depthURL = documents.appendingPathComponent("\(base).depth")

    videoRecorder.start(url: videoURL, width: width, height: height)
    metadataRecorder.start(url: metadataURL, imageResolution: frame.camera.imageResolution)
    depthRecorder.start(url: depthURL)
  }

  private func stopRecording() {
    if let dims = depthRecorder.stop() {
      metadataRecorder.setDepthResolution(width: dims.width, height: dims.height)
    }
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

    if ARWorldTrackingConfiguration.supportsFrameSemantics(.sceneDepth) {
      configuration.frameSemantics.insert(.sceneDepth)
    } else {
      print("[TagARKit] sceneDepth not supported on this device")
    }

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
    // Dismiss the tag-size keyboard if it's up (decimal pad has no return key).
    view.endEditing(true)

    // Anchor-on-tap (debug cube) disabled; tags are augmented instead.
    // if let currentFrame = session.currentFrame {
    //   var translation = matrix_identity_float4x4
    //   translation.columns.3.z = -0.2
    //   let transform = simd_mul(currentFrame.camera.transform, translation)
    //   let anchor = ARAnchor(transform: transform)
    //   session.add(anchor: anchor)
    // }
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

  private func feedTracker(_ frame: ARFrame) {
    guard let tracker = tagTracker else { return }

    let k = frame.camera.intrinsics
    let intrinsics = simd_float4(k.columns.0.x, k.columns.1.y,
                                 k.columns.2.x, k.columns.2.y)

    let timestampNs = Int64(frame.timestamp * 1_000_000_000)
    tracker.submitFrame(frame.capturedImage,
                        timestamp: timestampNs,
                        pose: frame.camera.transform,
                        intrinsics: intrinsics)

    renderer?.tagInstances = tracker.latestTags().map {
      Renderer.TagInstance(id: Int32($0.tagId), transform: $0.transform)
    }
  }

  // MARK: - ARSessionDelegate

  // Called every time the session updates with a new captured frame.
  func session(_ session: ARSession, didUpdate frame: ARFrame) {
    feedTracker(frame)

    guard isRecording else { return }
    // Build the presentation time once so video and metadata share the exact same CMTime.
    let timestamp = CMTime(seconds: frame.timestamp, preferredTimescale: 1_000_000_000)
    videoRecorder.append(pixelBuffer: frame.capturedImage, timestamp: timestamp)

    if let depthMap = frame.sceneDepth?.depthMap {
      metadataRecorder.append(camera: frame.camera, timestamp: timestamp)
      depthRecorder.append(depthMap: depthMap)
    }
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

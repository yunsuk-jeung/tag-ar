#include "tagar/file_reader.hpp"

#include <string>

#include <gtest/gtest.h>

namespace {
const std::string kDatasetDir = std::string(TAGAR_TEST_DATA_DIR) + "/01";
}  // namespace

TEST(FileReaderTest, InitLoadsMetadata) {
  tagar::FileReader reader;
  ASSERT_TRUE(reader.Init(kDatasetDir));

  EXPECT_GT(reader.GetFrameCount(), 0u);
  EXPECT_EQ(reader.GetDatasetName(), "01");
  EXPECT_LT(reader.GetStartTimestampNs(), reader.GetEndTimestampNs());
}

TEST(FileReaderTest, InitFailsOnMissingDir) {
  tagar::FileReader reader;
  EXPECT_FALSE(reader.Init(std::string(TAGAR_TEST_DATA_DIR) + "/does_not_exist"));
}

TEST(FileReaderTest, FramesHavePoseIntrinsicsAndMonotonicTime) {
  tagar::FileReader reader;
  ASSERT_TRUE(reader.Init(kDatasetDir));

  int64_t prev_t = -1;
  size_t visited = 0;
  while (reader.HasNextFrame()) {
    tagar::FrameBuffer f = reader.GetNextFrame();

    // Timestamps must be strictly increasing.
    EXPECT_GT(f.t_ns, prev_t);
    prev_t = f.t_ns;

    // Intrinsics come from metadata and must be sane.
    EXPECT_GT(f.intrinsics[0], 0.0f);  // fx
    EXPECT_GT(f.intrinsics[1], 0.0f);  // fy

    // Pose is a 4x4 matrix; the homogeneous bottom row should be [0,0,0,1]
    // (column-major: indices 3, 7, 11, 15).
    EXPECT_FLOAT_EQ(f.pose[15], 1.0f);

    // Every yielded frame is backed by a decoded video frame.
    EXPECT_FALSE(f.image_buffer.buffer.empty());

    ++visited;
  }
  EXPECT_EQ(visited, reader.GetFrameCount());
}

TEST(FileReaderTest, DecodesVideoFrames) {
  tagar::FileReader reader;
  ASSERT_TRUE(reader.Init(kDatasetDir));

  size_t decoded = 0;
  while (reader.HasNextFrame()) {
    tagar::FrameBuffer f = reader.GetNextFrame();
    ASSERT_FALSE(f.image_buffer.buffer.empty());
    EXPECT_EQ(f.image_buffer.format, tagar::ColorFormat::kRGB);
    EXPECT_GT(f.image_buffer.width, 0);
    EXPECT_GT(f.image_buffer.height, 0);
    EXPECT_EQ(f.image_buffer.buffer.size(),
              static_cast<size_t>(f.image_buffer.width) * f.image_buffer.height * 3);
    ++decoded;
  }
  // Every video frame is decoded and yielded.
  EXPECT_EQ(decoded, reader.GetFrameCount());
}

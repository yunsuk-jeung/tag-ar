add_executable(
  file_reader_test
  file_reader_test.cpp
)

target_link_libraries(
  file_reader_test
  PRIVATE
  TagAR::Core
  GTest::gtest_main
)

# Absolute path to the committed test recordings (datasets/<name>).
target_compile_definitions(
  file_reader_test
  PRIVATE
  TAGAR_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/datasets"
)

include(GoogleTest)
gtest_discover_tests(file_reader_test)

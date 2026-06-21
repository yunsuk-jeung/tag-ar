
#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace tagar {

class Logger {
 public:
  // Default location for log files. On desktop this is project-root/logs (when
  // built with tagar_SOURCE_DIR) or cwd-relative "logs" otherwise. On sandboxed
  // platforms (e.g. iOS) the host app must pass a writable directory to Init().
  static std::filesystem::path DefaultLogDir() {
#ifdef tagar_SOURCE_DIR
    return std::filesystem::path(tagar_SOURCE_DIR) / "logs";
#else
    return std::filesystem::path("logs");
#endif
  }

  // Initialize logging. Pass log_dir to write files to a platform-appropriate
  // writable location (required on iOS: e.g. NSDocumentDirectory).
  static void Init(bool enable_file_logging,
                   const std::filesystem::path& log_dir) {
    if (logger_) {
      return;
    }

    if (enable_file_logging) {
      // Create sinks for both console and file output. If the directory cannot
      // be created (e.g. sandbox/permission issues), fall back to console-only
      // so logging never aborts the application.
      auto console_sink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

      std::string log_file;
      std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink;
      try {
        std::filesystem::create_directories(log_dir);

        // Generate filename with current timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "tagar_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S")
           << ".log";
        log_file = (log_dir / ss.str()).string();

        file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
      } catch (const std::exception& e) {
        file_sink.reset();
      }

      std::vector<spdlog::sink_ptr> sinks{console_sink};
      if (file_sink) {
        sinks.push_back(file_sink);
      }
      logger_ =
          std::make_shared<spdlog::logger>("tagar", sinks.begin(), sinks.end());
      logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

      if (file_sink) {
        logger_->info("Logging to file: {}", log_file);
      } else {
        logger_->warn("File logging disabled (could not open log dir: {})",
                      log_dir.string());
      }
    } else {
      logger_ = spdlog::default_logger();
    }
    spdlog::set_default_logger(logger_);
  }

  static void Init(bool enable_file_logging = true) {
    Init(enable_file_logging, DefaultLogDir());
  }

  template <typename... Args>
  static void Debug(const char* fmt, Args&&... args) {
    Init();
    logger_->debug(fmt, args...);
  }

  template <typename... Args>
  static void Info(const char* fmt, Args&&... args) {
    Init();
    logger_->info(fmt, args...);
  }

  template <typename... Args>
  static void Warn(const char* fmt, Args&&... args) {
    Init();
    logger_->warn(fmt, args...);
  }

  template <typename T, typename... Args>
  static void Error(const char* file, int line, const T& fmt, Args&&... args) {
    Init();
    const std::string_view fmt_view(fmt);
    if constexpr (sizeof...(args) == 0) {
      logger_->error("[{}:{}] {}", file, line, fmt_view);
    } else {
      auto arg_tuple =
          std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...);
      auto format_args = std::apply(
          [](auto&... unpacked) { return std::make_format_args(unpacked...); },
          arg_tuple);
      logger_->error("[{}:{}] {}", file, line,
                     std::vformat(fmt_view, format_args));
    }
  }

  static inline const char* extractFileName(const char* path) {
    if (!path) {
      return "";
    }
    const char* last_slash = path;
    for (const char* p = path; *p != '\0'; ++p) {
      if (*p == '/' || *p == '\\') {
        last_slash = p + 1;
      }
    }
    return last_slash;
  }

 private:
  static inline std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace tagar

#define LogD(fmt, ...) tagar::Logger::Debug(fmt, ##__VA_ARGS__);
#define LogI(fmt, ...) tagar::Logger::Info(fmt, ##__VA_ARGS__);
#define LogW(fmt, ...) tagar::Logger::Warn(fmt, ##__VA_ARGS__);
#define LogE(fmt, ...)                                                     \
  tagar::Logger::Error(tagar::Logger::extractFileName(__FILE__), __LINE__, \
                       fmt, ##__VA_ARGS__);
#define DEBUG_POINT() LogE("THIS Line is for debugging");

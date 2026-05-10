// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "spdlog_logger.h"

#include <spdlog/async.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

#include <filesystem>
#include <vector>

namespace fl {

namespace {

spdlog::level::level_enum ToSpdlogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::Verbose:
      return spdlog::level::trace;
    case LogLevel::Debug:
      return spdlog::level::debug;
    case LogLevel::Information:
      return spdlog::level::info;
    case LogLevel::Warning:
      return spdlog::level::warn;
    case LogLevel::Error:
      return spdlog::level::err;
    case LogLevel::Fatal:
      return spdlog::level::critical;
    default:
      return spdlog::level::info;
  }
}

}  // namespace

SpdlogLogger::SpdlogLogger(LogLevel min_level, const std::string& logs_dir) {
  // Initialize the async thread pool (8K queue, 1 background thread).
  // init_thread_pool is idempotent — safe to call multiple times.
  spdlog::init_thread_pool(8192, 1);

  std::vector<spdlog::sink_ptr> sinks;

#ifdef __ANDROID__
  // On Android, route logs to logcat (stderr is not visible).
  auto console_sink = std::make_shared<spdlog::sinks::android_sink_mt>("foundry_local");
  console_sink->set_pattern("%v");
#else
  // Colored stderr sink for desktop platforms.
  auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  console_sink->set_pattern("[%l] %v");
#endif
  sinks.push_back(console_sink);

  // Daily rotating file sink — added when logs_dir is specified
  if (!logs_dir.empty()) {
    std::filesystem::create_directories(logs_dir);

    auto log_path = (std::filesystem::path(logs_dir) / "foundry_local.log").string();
    // Rotate daily at midnight, keep unlimited history (let the caller manage disk)
    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_path, 0, 0);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    sinks.push_back(file_sink);
  }

  logger_ = std::make_shared<spdlog::async_logger>("foundry_local",
                                                   sinks.begin(), sinks.end(),
                                                   spdlog::thread_pool(),
                                                   spdlog::async_overflow_policy::block);

  logger_->set_level(ToSpdlogLevel(min_level));
  logger_->flush_on(spdlog::level::warn);

  spdlog::register_logger(logger_);
}

SpdlogLogger::~SpdlogLogger() {
  if (logger_) {
    logger_->flush();
    spdlog::drop(logger_->name());
  }
}

void SpdlogLogger::Log(LogLevel level, std::string_view message) {
  logger_->log(ToSpdlogLevel(level), "{}", message);
}

}  // namespace fl

#pragma once

#include <clap/ext/log.h>
#include <clasp-gui/webview.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

namespace clasp {

// Centralized logging system for thunder.clap
// Logs to: stderr, file, and WebView console (when available)
class Logger {
public:
  static Logger &instance();

  // Initialize logging system
  void init(const std::filesystem::path &logDir = "");

  // Set WebView for console forwarding (optional)
  void setWebView(clasp_gui::WebView *webview);

  // Log with CLAP severity levels
  void log(clap_log_severity severity, const char *msg);
  void log(clap_log_severity severity, const std::string &msg);

  // Convenience methods
  void debug(const std::string &msg) { log(CLAP_LOG_DEBUG, msg); }
  void info(const std::string &msg) { log(CLAP_LOG_INFO, msg); }
  void warning(const std::string &msg) { log(CLAP_LOG_WARNING, msg); }
  void error(const std::string &msg) { log(CLAP_LOG_ERROR, msg); }
  void fatal(const std::string &msg) { log(CLAP_LOG_FATAL, msg); }

  // Flush all outputs
  void flush();

  // Close log file
  void close();

private:
  Logger() = default;
  ~Logger();

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  std::mutex mutex_;
  std::ofstream logFile_;
  clasp_gui::WebView *webview_ = nullptr;
  bool initialized_ = false;

  // Get severity string for formatting
  const char *severityToString(clap_log_severity severity);
  const char *severityToConsoleMethod(clap_log_severity severity);

  // Format log message with timestamp
  std::string formatMessage(clap_log_severity severity, const std::string &msg);
};

// Convenience macros for logging
#define CLASP_LOG_DEBUG(msg) ::clasp::Logger::instance().debug(msg)
#define CLASP_LOG_INFO(msg) ::clasp::Logger::instance().info(msg)
#define CLASP_LOG_WARNING(msg) ::clasp::Logger::instance().warning(msg)
#define CLASP_LOG_ERROR(msg) ::clasp::Logger::instance().error(msg)
#define CLASP_LOG_FATAL(msg) ::clasp::Logger::instance().fatal(msg)

} // namespace clasp

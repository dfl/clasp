#include "clasp/logger.h"
#include <chrono>
#include <clasp-gui/webview.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace clasp {

Logger &Logger::instance() {
  static Logger instance;
  return instance;
}

Logger::~Logger() { close(); }

void Logger::init(const std::filesystem::path &logDir) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (initialized_)
    return;

  // Determine log directory
  std::filesystem::path logPath;
  if (!logDir.empty()) {
    logPath = logDir;
  } else {
    // Default: ~/.clasp/logs/
#ifdef _WIN32
    char *appData = nullptr;
    size_t len = 0;
    _dupenv_s(&appData, &len, "LOCALAPPDATA");
    if (appData) {
      logPath = std::filesystem::path(appData) / "clasp" / "logs";
      free(appData);
    }
#else
    const char *home = getenv("HOME");
    if (home) {
      logPath = std::filesystem::path(home) / ".clasp" / "logs";
    }
#endif
  }

  // Create log directory if it doesn't exist
  if (!logPath.empty()) {
    try {
      std::filesystem::create_directories(logPath);

      // Create log file with timestamp
      auto now = std::chrono::system_clock::now();
      auto time_t = std::chrono::system_clock::to_time_t(now);
      std::tm tm;
#ifdef _WIN32
      localtime_s(&tm, &time_t);
#else
      localtime_r(&time_t, &tm);
#endif

      std::ostringstream filename;
      filename << "thunder_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".log";

      std::filesystem::path logFilePath = logPath / filename.str();
      logFile_.open(logFilePath, std::ios::out | std::ios::app);

      if (logFile_.is_open()) {
        std::cerr << "[thunder] Logging to: " << logFilePath << std::endl;
      }
    } catch (const std::exception &e) {
      std::cerr << "[thunder] Failed to create log directory: " << e.what()
                << std::endl;
    }
  }

  initialized_ = true;
}

void Logger::setWebView(clasp_gui::WebView *webview) {
  std::lock_guard<std::mutex> lock(mutex_);
  webview_ = webview;
}

const char *Logger::severityToString(clap_log_severity severity) {
  switch (severity) {
  case CLAP_LOG_DEBUG:
    return "DEBUG";
  case CLAP_LOG_INFO:
    return "INFO";
  case CLAP_LOG_WARNING:
    return "WARNING";
  case CLAP_LOG_ERROR:
    return "ERROR";
  case CLAP_LOG_FATAL:
    return "FATAL";
  case CLAP_LOG_HOST_MISBEHAVING:
    return "HOST_MISBEHAVING";
  case CLAP_LOG_PLUGIN_MISBEHAVING:
    return "PLUGIN_MISBEHAVING";
  default:
    return "UNKNOWN";
  }
}

const char *Logger::severityToConsoleMethod(clap_log_severity severity) {
  switch (severity) {
  case CLAP_LOG_DEBUG:
    return "debug";
  case CLAP_LOG_INFO:
    return "info";
  case CLAP_LOG_WARNING:
    return "warn";
  case CLAP_LOG_ERROR:
  case CLAP_LOG_FATAL:
  case CLAP_LOG_HOST_MISBEHAVING:
  case CLAP_LOG_PLUGIN_MISBEHAVING:
    return "error";
  default:
    return "log";
  }
}

std::string Logger::formatMessage(clap_log_severity severity,
                                  const std::string &msg) {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::tm tm;
#ifdef _WIN32
  localtime_s(&tm, &time_t);
#else
  localtime_r(&time_t, &tm);
#endif

  std::ostringstream formatted;
  formatted << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "."
            << std::setfill('0') << std::setw(3) << ms.count() << "] ["
            << severityToString(severity) << "] " << msg;

  return formatted.str();
}

void Logger::log(clap_log_severity severity, const char *msg) {
  if (msg == nullptr)
    return;
  log(severity, std::string(msg));
}

void Logger::log(clap_log_severity severity, const std::string &msg) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!initialized_) {
    init();
  }

  std::string formatted = formatMessage(severity, msg);

  // 1. Log to stderr
  std::cerr << formatted << std::endl;

  // 2. Log to file
  if (logFile_.is_open()) {
    logFile_ << formatted << std::endl;
  }

  // 3. Forward to WebView console (if available)
  if (webview_) {
    // Escape quotes in message
    std::string escapedMsg = msg;
    size_t pos = 0;
    while ((pos = escapedMsg.find('"', pos)) != std::string::npos) {
      escapedMsg.replace(pos, 1, "\\\"");
      pos += 2;
    }
    // Also escape newlines
    pos = 0;
    while ((pos = escapedMsg.find('\n', pos)) != std::string::npos) {
      escapedMsg.replace(pos, 1, "\\n");
      pos += 2;
    }

    std::ostringstream js;
    js << "if (console && console." << severityToConsoleMethod(severity)
       << ") { "
       << "console." << severityToConsoleMethod(severity) << "('[thunder] "
       << escapedMsg << "'); }";

    webview_->evaluateScript(js.str());
  }
}

void Logger::flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (logFile_.is_open()) {
    logFile_.flush();
  }
  std::cerr.flush();
}

void Logger::close() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (logFile_.is_open()) {
    logFile_.close();
  }
}

} // namespace clasp

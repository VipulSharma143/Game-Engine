#include "engine/core/Logger.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace ge {
namespace {

std::atomic<LogLevel> g_level{LogLevel::Info};
std::mutex g_mutex;
LogSink g_sink = nullptr;
void* g_user = nullptr;

void default_sink(LogLevel level, const char* message, void*) {
    std::fprintf(stderr, "[%s] %s\n", Logger::level_name(level), message);
}

}  // namespace

void Logger::set_level(LogLevel level) noexcept { g_level.store(level); }
LogLevel Logger::level() noexcept { return g_level.load(); }

void Logger::set_sink(LogSink sink, void* user) noexcept {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sink = sink;
    g_user = user;
}

void Logger::log(LogLevel level, const char* fmt, ...) {
    if (level < g_level.load() || g_level.load() == LogLevel::Off) return;
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof buffer, fmt, args);
    va_end(args);
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_sink != nullptr) {
        g_sink(level, buffer, g_user);
    } else {
        default_sink(level, buffer, nullptr);
    }
}

const char* Logger::level_name(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warn: return "warn";
        case LogLevel::Error: return "error";
        case LogLevel::Off: return "off";
    }
    return "?";
}

bool Logger::parse_level(const char* name, LogLevel& out) noexcept {
    static constexpr LogLevel all[] = {LogLevel::Trace, LogLevel::Debug, LogLevel::Info,
                                       LogLevel::Warn,  LogLevel::Error, LogLevel::Off};
    for (LogLevel candidate : all) {
        if (std::strcmp(name, level_name(candidate)) == 0) {
            out = candidate;
            return true;
        }
    }
    return false;
}

}  // namespace ge

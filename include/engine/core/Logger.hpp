#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define GE_PRINTF_FORMAT(fmt_index, first_arg) __attribute__((format(printf, fmt_index, first_arg)))
#else
#define GE_PRINTF_FORMAT(fmt_index, first_arg)
#endif

namespace ge {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Off };

using LogSink = void (*)(LogLevel level, const char* message, void* user);

// Process-wide logger. The only global mutable state in the engine; it is
// thread-safe and formats into a fixed stack buffer (no heap allocation).
class Logger {
public:
    static void set_level(LogLevel level) noexcept;
    static LogLevel level() noexcept;
    // Pass nullptr to restore the default stderr sink.
    static void set_sink(LogSink sink, void* user = nullptr) noexcept;
    static void log(LogLevel level, const char* fmt, ...) GE_PRINTF_FORMAT(2, 3);
    static const char* level_name(LogLevel level) noexcept;
    // Returns false if the name is not recognised.
    static bool parse_level(const char* name, LogLevel& out) noexcept;
};

}  // namespace ge

#define GE_LOG_ERROR(...) ::ge::Logger::log(::ge::LogLevel::Error, __VA_ARGS__)
#define GE_LOG_WARN(...) ::ge::Logger::log(::ge::LogLevel::Warn, __VA_ARGS__)
#define GE_LOG_INFO(...) ::ge::Logger::log(::ge::LogLevel::Info, __VA_ARGS__)

// Debug and trace logging compile away in Release (NDEBUG) builds.
#if defined(NDEBUG)
#define GE_LOG_DEBUG(...) ((void)0)
#define GE_LOG_TRACE(...) ((void)0)
#else
#define GE_LOG_DEBUG(...) ::ge::Logger::log(::ge::LogLevel::Debug, __VA_ARGS__)
#define GE_LOG_TRACE(...) ::ge::Logger::log(::ge::LogLevel::Trace, __VA_ARGS__)
#endif

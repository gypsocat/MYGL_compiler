#pragma once
#if __cplusplus < 202002L
#include <cstdint>
#include <exception>
#include <sstream>
#include <string>
#include <string_view>

struct SourceLocation {
    using cstring = const char*;
    using count_t = uint32_t;

    constexpr cstring file_name()     const noexcept { return _filename; }
    constexpr cstring function_name() const noexcept { return _func_name; }
    constexpr count_t column()        const noexcept { return _column; }
    constexpr count_t line()          const noexcept { return _line; }

/** INACCESSABLE */
    cstring _filename;
    cstring _func_name;
    count_t _line, _column;
}; // struct SourceLocation


#define CURRENT_SRCLOC   SourceLocation{__FILE__, "", __LINE__, 0}
#define CURRENT_SRCLOC_F SourceLocation{__FILE__, __func__, __LINE__, 0}
#define CURRENT_SRCLOC_EX(col) SourceLocation{__FILE__, __func__, __LINE__, col}

namespace MTB {

enum class ErrorLevel {
    NORMAL = 0,
    INFO, DEBUG, WARNING, CRITICAL, FATAL
}; // enum class ErrorLevel

class Exception: public std::exception {
public:
    using osb_t = std::ostringstream;
public:
    Exception(ErrorLevel level,
              std::string_view msg,
              SourceLocation location = CURRENT_SRCLOC) noexcept
        : level(level), msg(msg), location(location) {
    }
    Exception(ErrorLevel level,
              std::string &&msg,
              SourceLocation location = CURRENT_SRCLOC) noexcept
        : level(level), msg(std::move(msg)), location(location) {
    }

    const char *what() const noexcept override { return msg.c_str(); }
public:
    SourceLocation location;
    ErrorLevel  level;
    std::string msg;
}; // class Exception

class NullException: public Exception {
public:
    NullException(std::string_view pointer_name,
                  std::string info = {},
                  SourceLocation location = CURRENT_SRCLOC)
        : Exception(ErrorLevel::CRITICAL,
            (std::ostringstream("NullException occured in ") << pointer_name).str(),
            location),
            pointer_name(pointer_name),
            info(std::move(info)) {
        if (!this->info.empty())
            msg += "(tips: " + info + ")";
    }
public:
    std::string pointer_name;
    std::string info;
}; // class NullException

/** @class EmptySetException
 * @brief 空集异常, 用于自定义容器类中 */
class EmptySetException: public Exception {
public:
    EmptySetException(std::string_view set_name, std::string_view detail)
        : Exception(ErrorLevel::CRITICAL,
                    (osb_t("EmptySetException occured at ")
                          << set_name << ": " << detail).str(),
                    CURRENT_SRCLOC),
            set_name(set_name), detail(detail) {}
public:
    std::string_view set_name;
    std::string_view detail;
}; // class EmptySetException
} // namespace MTB

#endif
#ifndef __MTB_EXCEPTION_H__
#define __MTB_EXCEPTION_H__

#if __cplusplus >= 202002L

#include "mtb-base.hxx"
#include <sstream>
#include <iostream>
#include <exception>
#include <format>
#include <source_location>
#include <string>
#include <string_view>

#define CURRENT_SRCLOC         std::source_location::current()
#define CURRENT_SRCLOC_F       std::source_location::current()
#define CURRENT_SRCLOC_EX(col) std::source_location::current()

#if (__cplusplus >= 202302L) && (__MTB_DEBUG_EXP__ != 0)
#   include <stacktrace>
#endif
#if defined(__GNUC__) && defined (__GLIBC__)
#   include <execinfo.h>
#   include <unistd.h>
#endif

namespace MTB {
    using SourceLocation = std::source_location;
    static std::string
    source_location_stringfy(SourceLocation const& src_loc)
    {
        std::ostringstream osb("{");
        osb << "Filename: \"" << src_loc.file_name() << "\", ";
        osb << "Line: "     << src_loc.line() << ", ";
        osb << "Function: `"  << src_loc.function_name() << "`}";
        return osb.str();
    }

    [[noreturn]]
    void crash(bool pauses, SourceLocation &&srcloc, std::string const& reason);

    [[noreturn]]
    void crash_with_stacktrace(bool pauses, SourceLocation srcloc, std::string const& reason);

#if (__cplusplus >= 202302L) && (__MTB_DEBUG_EXP__ != 0)
    static always_inline int print_stacktrace()
    {
        /* 使用 C++23 的栈回溯, 其中 GCC 需要开启选项 -lstdc++exp */
        fputs("============== [简要栈回溯: 添加 '-ggdb' 选项以查看详细信息] ==============\n", stderr);
        auto cur = std::stacktrace::current();
        std::cerr << cur << std::endl;
        return cur.size();
    }
#elif defined(__GNUC__) && defined (__GLIBC__) && (__MTB_DEBUG__ != 0)
    static always_inline int print_stacktrace()
    {
        fputs("============== [简要栈回溯: 添加 '-rdynamic' 选项以查看函数名] ==============\n", stderr);
        void *buffer[64];
        
        int size = backtrace(buffer, 32);
        backtrace_symbols_fd(buffer, size, STDERR_FILENO);
        return size;
    }
#elif __MTB_DEBUG__ != 0
    static always_inline int print_stacktrace() {
        fputs("========== [使用的标准库不是 glibc, 栈回溯不可用] ==========\n", stderr);
        return 0;
    }
#else
    static always_inline int print_stacktrace() {
        fputs("========== [请设置 __MTB_DEBUG__ 宏为 1 以启用栈回溯] ==========\n", stderr);
        return 0;
    }
#endif
}

namespace MTB {
    enum class ErrorLevel {
        NORMAL = 0,
        INFO, DEBUG, WARNING, CRITICAL, FATAL
    }; // enum class ErrorLevel

    class Exception: public std::exception {
        void external_action() {
            print_stacktrace();
        }
    public:
        Exception(ErrorLevel level,
                  std::string_view msg,
                  SourceLocation location = CURRENT_SRCLOC) noexcept
            : level(level), msg(msg), location(location) {
            external_action();
        }
        Exception(ErrorLevel level,
                  std::string &&msg,
                  SourceLocation location = CURRENT_SRCLOC) noexcept
            : level(level), msg(std::move(msg)), location(location) {
            external_action();
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
                std::format("NullException occured in {}", pointer_name),
                location),
              pointer_name(pointer_name),
              info(std::move(info)) {
            if (!this->info.empty())
                msg += std::format("(tips: {})", this->info);
        }
    public:
        std::string pointer_name;
        std::string info;
    }; // class NullException

    /** @class EmptySetException
     * @brief 空集异常, 用于自定义容器类中 */
    class EmptySetException: public Exception {
    public:
        EmptySetException(std::string_view set_name, std::string_view detail,
                          SourceLocation srcloc = CURRENT_SRCLOC_F)
            : Exception(ErrorLevel::CRITICAL,
                        std::format("EmptySetException occured at {}({}): {}",
                                set_name, source_location_stringfy(srcloc), detail),
                        srcloc),
              set_name(set_name), detail(detail) {}
    public:
        std::string_view set_name;
        std::string_view detail;
    }; // class EmptySetException
} // namespace MTB
#else
#include "fallback/mtb-exception.fallback.hxx"
#endif

#endif

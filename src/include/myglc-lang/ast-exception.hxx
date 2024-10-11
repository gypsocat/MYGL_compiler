/** @file ast-exception.hxx
 * @brief AST树的异常类汇总 */
#pragma once

#include <string_view>
#include <string>
#include <map>
#include <exception>
#include <source_location>
#include <cstdint>
#include <format>
#include "ast-scope.hxx"
#include "base.hxx"
#include "ast-node-decl.hxx"

namespace MYGL::Ast {
    /** @enum ErrorLevel
     * @brief 错误等级，有5挡. */
    enum class ErrorLevel: int32_t {
        NORMAL,  // 没有错误
        DEBUG,   // 调试时才会报告的错误，建议不要使用抛出异常的方式返回该错误
        WARNING, // 警告，也就是一般不影响运行的错误，可以抛异常
        CRITICAL,// 严重但不立即致命的错误，建议抛异常并当场解决
        FATAL	 // 致命错误，建议抛异常并终止处理。
    }; // enum class ErrorLevel

    static std::string_view ErrorLevelGetString(ErrorLevel level) noexcept {
        static const std::string_view ErrorLevelString[] = {
            "NORMAL", "DEBUG", "WARNING", "CRITICAL", "FATAL"
        };
        if (level < ErrorLevel::NORMAL || level > ErrorLevel::FATAL)
            return "FATAL";
        return ErrorLevelString[static_cast<int32_t>(level)];
    }

    static ErrorLevel ErrorLevelFromString(std::string_view const &err_value) noexcept
    {
        static std::map<std::string_view, ErrorLevel> err_level_map = {
            {"NORMAL",   ErrorLevel::NORMAL},
            {"DEBUG",    ErrorLevel::DEBUG},
            {"WARNING",  ErrorLevel::WARNING},
            {"CRITICAL", ErrorLevel::CRITICAL},
            {"FATAL",    ErrorLevel::FATAL}
        };
        if (err_level_map.contains(err_value))
            return err_level_map[err_value];
        return ErrorLevel::FATAL;
    }

    /** @brief 全局默认抛出的异常等级，可在运行时修改 */
    extern ErrorLevel DefaultErrLevel;
    /** @brief 不可以容忍继续运行的最低异常等级，只能在编译期确定 */
    constexpr ErrorLevel GlobalErrLimit = ErrorLevel::FATAL;

    /* end enum ErrorLevel */

    /** @class NullException
     * @brief 倘若遇到不该出现的NULL则抛出该类异常. */
    class NullException: public std::exception {
    public:
        NullException(Node *instance = nullptr,
                      ErrorLevel err_level = DefaultErrLevel,
                      std::string_view msg = "Encountered NULL Exception",
                      std::source_location location = std::source_location::current())
            :instance(instance), error_level(err_level), location(location),
             msg(std::format("[Ast::NullException at (line:{},col:{}) in {}:{} level {}]: {}",
                    location.line(), location.column(),
                    location.file_name(), location.function_name(),
                    ErrorLevelGetString(err_level),
                    msg)) {}
        virtual ~NullException() = default;

        cstring what() const noexcept override {
            return msg.c_str();
        }
    public:
        ErrorLevel  error_level;
        std::string msg;
        Node       *instance;
        std::source_location location;
    }; // class NullException

    /** @class UndefinedException
     * @brief 碰到符号/函数等未定义时会抛出这类异常. */
    class UndefinedException: public std::exception {
    public:
        UndefinedException(Identifier *instance = nullptr,
                           ErrorLevel err_level = DefaultErrLevel,
                           std::string_view msg = "Encountered Undefined Exception",
                           std::source_location location = std::source_location::current())
            :instance(instance), error_level(err_level), location(location),
             msg(std::format("[Ast::UndefinedException at (line:{},col:{}) in {}:{} level {}]: {}",
                    location.line(), location.column(),
                    location.file_name(), location.function_name(),
                    ErrorLevelGetString(err_level),
                    msg)) {}
        virtual ~UndefinedException() = default;

        cstring what() const noexcept override {
            return msg.c_str();
        }
        Scope *get_scope();
    public:
        ErrorLevel  error_level;
        std::string msg;
        Identifier *instance;
        std::source_location location;
    };
} // namespace MYGL::Ast

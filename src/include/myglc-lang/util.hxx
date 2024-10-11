#pragma once

#include "base.hxx"
#include "ast-node-decl.hxx"
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace MYGL::Ast {
    /** @struct SourceLocation
     * @brief 标识源代码中的某一位置 
     * 
     * @fn to_string()
     * @fn get_content(int, int)
     * @fn operator[](int)
     * @fn operator*()
     * @fn advance(uint32_t)
     * @fn advance_and_count(uint32_t) */
    struct SourceLocation {
        std::string *owner;
        size_t    location;
        int line, col;   // 所处行列。

        inline char *actual() const {
            return owner->data() + location;
        }
        inline std::string to_string() const {
            return std::format("(line {}, col {})", line, col);
        }
        /** @brief 获取以该位置为中心，向左backwd个字符、向右fwd个字符的字串 */
        std::string_view get_content(int backwd = 0, int fwd = 10) const;
        /** @brief 获取以自己为基址的第index个字符. */
        const char &operator[](int index) const { return actual()[index]; }
        /** @brief 把自己当成const char*指针. */
        char *operator*() const { return actual(); }

        /** @brief 如果源码没有结束的话就向后移动nchars个字符，并返回自己。 */
        SourceLocation &advance(uint32_t nchars = 1);
        /** @brief 如果源码没有结束的话就向后移动nchars个字符，并返回走了多少步。 */
        int advance_and_count(uint32_t nchars = 1);
    }; // struct SourceLocation

    /** @struct SourceRange
     * @brief 在代码中标明一段范围
     *
     * @fn to_string()
     * @fn get_content() */
    struct SourceRange {
        cstring file_name;    // 所处文件的名称。没有对文件名的所有权.
        SourceLocation begin; // 起始位置
        SourceLocation end;   // 终止位置

        /** @brief 把位置信息格式化成字符串 */
        inline std::string to_string() const
        {
            std::string ret;
            // if (file_name == nullptr)
            //     ret = "<anonymous>";
            // else
            //     ret = file_name;
            ret.append(":").append(begin.to_string());
            ret.append("-").append(end.to_string());
            return ret;
        }

        /** @brief 输出这段范围内的源码 */
        std::string_view get_content() const {
            return {begin.actual(), end.actual()};
        }
        operator std::string_view() {
            return get_content();
        }
        static SourceRange merge_unsafe(SourceRange const &s1, SourceRange const &s2) {
            return SourceRange {
                s1.file_name,
                s1.begin, s2.end
            };
        }
    }; // struct SourceRange
} // namespace MYGL::Ast

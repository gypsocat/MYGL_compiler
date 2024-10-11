#pragma once
#include <alloca.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>
#include "mtb-base.hxx"

namespace MTB::Compatibility {
    
    using osb_t = std::ostringstream;

    template<typename Arg0>
    static size_t fmt_fill_osb(osb_t &stream, Arg0 const& arg) {
        stream << arg; return 1;
    }
    template<typename Arg0, typename...Args>
    static size_t fmt_fill_osb(osb_t &stream, Arg0 arg0, Args...args) {
        stream << arg0;
        return 1 + fmt_fill_osb(stream, args...);
    }
    template<typename...Args>
    static std::string osb_fmt(Args...args)
    {
        osb_t osb;
        fmt_fill_osb(osb, args...);
        return osb.str();
    }

    static std::string osb_fmt(const char *str) {
        return std::string(str);
    }
    static std::string osb_fmt(std::string_view const& str) {
        return std::string(str);
    }
    static std::string osb_fmt(std::string const& str) {
        return std::string(str);
    }
    static std::string osb_fmt(std::string &&str) {
        return std::string(std::move(str));
    }
    static inline std::string osb_fmt() { return std::string(); }

    std::string vrt_fmt(char const* fmt, va_list ap);
    MTB_PRINTF_FORMAT(1, 2)
    static inline std::string rt_fmt(char const* fmt, ...)
    {
        va_list ap; va_start(ap, fmt);
        std::string ret = vrt_fmt(fmt, ap);
        va_end(ap);
        return ret;
    }
} // namespace MTB::Compatibility

/* std::string      format "%s" */
#define MTB_STR_FMT(string_value)        (string_value).c_str()
/* std::string_view format "%*s" */
#define MTB_TWINE_FMT(string_view_value) (string_view_value).length(), (string_view_value).begin()
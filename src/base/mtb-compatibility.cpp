#include "base/mtb-compatibility.hxx"
#include <cstdio>
#include <string>

namespace MTB::Compatibility {

std::string vrt_fmt(char const* fmt, va_list ap)
{
    constexpr const unsigned INLINE_BUFLEN = 256;
    size_t size = vsnprintf(nullptr, 0, fmt, ap);

    if (size == 0) {
        return {};
    } else if (size + 8 <= INLINE_BUFLEN) {
#ifdef __GNUC__
        auto buffer = (char*)alloca(size);
#else
        char buffer[INLINE_BUFLEN];
#endif
        (size_t&)(*buffer) = 0;
        vsnprintf(buffer, size, fmt, ap);
        return std::string{buffer, size};
    } else {
        char *buffer = new char[size + 8];
        (size_t&)(*buffer) = 0;
        vsnprintf(buffer, size, fmt, ap);
        std::string ret{buffer, size};
        delete[] buffer;
        return ret;
    }
}

}
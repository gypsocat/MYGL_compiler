#include "myglc-lang/util.hxx"
#include <cstdint>
#include <cstring>
#include <string_view>

namespace MYGL {
    bool is_newline(char ch) {
        return (ch == '\n' || ch == '\r');
    }
} // namespace MYGL

namespace MYGL::Ast {
    std::string_view
    SourceLocation::get_content(int backwd, int fwd) const {
        size_t last_len = std::min(owner->length() - location, (size_t)fwd);
        return {actual() - backwd, last_len};
    }

    SourceLocation &SourceLocation::advance(uint32_t nchars)
    {
        advance_and_count(nchars);
        return *this;
    }
    always_inline int SourceLocation::advance_and_count(uint32_t nchars)
    {
        int ret = 0;
        while (ret < nchars && location < owner->length()) {
            if (is_newline(*actual()))
                line++, col = 0;
            else
                col++;
            location++;
        }
        return ret;
    }
} // namespace MYGL::Ast

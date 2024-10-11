#include "base/mtb-reflist.hxx"
#include <string>

namespace MTB::RefListImpl {
    std::string w5i4e9_make_msg(void *list, void *item, uint8_t code,
                                          std::string const& msg, SourceLocation srcloc)
    {
        using ErrorCode = GenericWrongItemException::ErrorCode;
        using namespace Compatibility;
        using namespace std::string_view_literals;
        std::string ret;
        char const* error_reason;

        constexpr size_t uptr_fmt_size     = sizeof(uintptr_t) * 2;
        constexpr size_t err_code_fmt_size = sizeof("Item in Wrong List");
        size_t msg_length  = uptr_fmt_size * 2 + err_code_fmt_size;
        msg_length += msg.length() + 8;

        switch (code) {
        case ErrorCode::UNEXIST:
            error_reason = "Item Unexisted";
            break;
        case ErrorCode::WRONG_LIST:
            error_reason = "Item in Wrong List";
            break;
        default:
            error_reason = "<undefined>";
        }
        msg_length += 64;

        size_t back_buff_len;
        char *back_buff = new char[msg_length]; {
            if (msg.empty()) {
                back_buff_len = snprintf(back_buff, msg_length,
                        "(with list:%p, item:%p) because of %s",
                        list, item, error_reason);
            } else {
                back_buff_len = snprintf(back_buff, msg_length,
                        "(with list:%p, item:%p) because of %s: %s",
                        list, item, error_reason, msg.c_str());
            }
            ret = osb_fmt("WrongItemException at {"sv,
                source_location_stringfy(srcloc), "} ",
                std::string_view{back_buff, back_buff_len});
        } delete[] back_buff;

        return ret;
    }
} // namespace MTB::RefListImpl
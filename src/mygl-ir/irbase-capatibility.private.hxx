#include "mygl-ir/ir-instruction-base.hxx"
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#define MYGL_ADD_BBLOCK_PROPERTY(property) \
    MYGL_ADD_TYPED_USEE_PROPERTY_FINAL(BasicBlock, BASIC_BLOCK, property)
#define capatibility _311c3e251718f9a4_

#if __cplusplus >= 202002L
#define MTB_LIKELY_CASE(cond)   [[likely]]   case cond
#define MTB_UNLIKELY_CASE(cond) [[unlikely]] case cond
#define MTB_LIKELY_DEFAULT      [[likely]]   default
#define MTB_UNLIKELY_DEFAULT    [[unlikely]] default
#else
#define MTB_LIKELY_CASE(cond)   case cond
#define MTB_UNLIKELY_CASE(cond) case cond
#define MTB_LIKELY_DEFAULT      default
#define MTB_UNLIKELY_DEFAULT    default
#endif

namespace MYGL::IR {

namespace capatibility {

template<typename SetT, typename...Args>
bool contains(SetT const &self, Args...args)
{
#if __cplusplus >= 202002L
    return self.contains(args...);
#else
    return self.find(args...) != self.end();
#endif
}

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
static std::string osb_fmt(std::string_view str) {
    return std::string(str);
}
static std::string osb_fmt(std::string const& str) {
    return std::string(str);
}
static std::string osb_fmt(std::string &&str) {
    return std::string(std::move(str));
}

} // namespace capatibility

using OpCodeT = Instruction::OpCode;
}

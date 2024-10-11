#include "base/mtb-compatibility.hxx"
#include "mygl-ir/irbase-type.hxx"
#include "mygl-ir/irbase-type-context.hxx"
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>

size_t global_machine_word_size = sizeof(void*);

namespace MYGL::IRBase {

size_t hash_combine(size_t hash1, size_t hash2)
{
    std::hash<size_t> hasher;
    return hasher(hash1 + (~hash2 << 1));
}
template<typename... Args>
size_t hash_combine(size_t hash1, Args...args)
{
    std::hash<size_t> hasher;
    size_t hash2 = hash_combine(args...);
    return hasher(hash1 + (~hash2 << 1));
}

/** 2008年, ISO规定一个字节有8位. 所以可以直接硬编码8进去. */
inline size_t bit_to_byte(size_t bit) {
    return (bit >> 3) + ((bit & 0b0111) != 0);
}
inline size_t fill_to(size_t size, size_t align)
{
    if (align == 0 || align == 1)
        return size;
    size_t ret = size / align;
    size_t mod = size % align;
    return (ret + (mod != 0)) * align;
}
inline size_t get_next_power_of_two(size_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    if constexpr (sizeof(size_t) >= 8)
        x |= x >> 32;
    if constexpr (sizeof(size_t) >= 16)
        x |= x >> 64;
    return x + 1;
}

/** @enum CastMode */
bool can_implicit_cast(CastMode cast_mode)
{
    size_t mode = size_t(cast_mode);
    size_t mode_implicit = size_t(CastMode::CAST_IMPLICIT);
    return (mode & mode_implicit) != 0;
}
bool can_static_cast(CastMode cast_mode)
{
    size_t mode = size_t(cast_mode);
    size_t mode_static = size_t(CastMode::STATIC_CAST);
    return (mode & mode_static) != 0;
}
bool can_reinterpret_cast(CastMode cast_mode)
{
    size_t mode = size_t(cast_mode);
    size_t mode_bits = size_t(CastMode::REINTERPRET_CAST);
    return (mode & mode_bits) != 0;
}
/** end enum CastMode */

inline size_t
type_context_get_machine_word_size(const TypeContext &ctx) {
    return ctx.get_machine_word_size();
}

/** @class Type */
Type::Type(TypeTID type_id, Type *base_type, TypeContext *ctx) noexcept
    : _type_id(type_id), _base_type(base_type), _type_context(ctx) {
    if (ctx != nullptr)
        _machine_word_size = ctx->get_machine_word_size();
}
Type::~Type() = default;

inline uint8_t Type::get_machine_word_size() const
{
    if (_type_context == nullptr)
        return _machine_word_size;
    return _type_context->get_machine_word_size();
}
inline void Type::set_machine_word_size(uint8_t mwsize)
{
    if (_type_context == nullptr)
        _machine_word_size = mwsize;
}

size_t Type::getAlignedSize() const
{
    size_t instance_size = get_instance_size();
    size_t p2size = fill_to_power_of_two(instance_size);
    size_t f1size = fill_to(instance_size, get_machine_word_size());
    return std::min(p2size, f1size);
}

bool Type::fully_equals(const Type *that) const
{
    return (this == that) ||
           ((that != nullptr) &&
            equals(that)      &&
            (is_constant() == that->is_constant()));
}
/** end class Type */

/** @class VoidType */
VoidType::VoidType() noexcept
    : Type(TypeTID::VOID, nullptr) {}

static VoidType void_ty;
VoidType *VoidType::voidty = &void_ty;
/** end class VoidType */

/** @class ValueType */
ValueType::ValueType(TypeTID type_id)
    : Type(type_id, VoidType::voidty) {}
ValueType::ValueType(TypeTID type_id, TypeContext &ctx)
    : Type(type_id, VoidType::voidty, &ctx) {
    if (ctx.hasType(this))
        return;
    std::ignore = ctx.getOrRegisterType(this);
}

size_t ValueType::get_instance_size() const {
    return bit_to_byte(get_binary_bits());
}
size_t ValueType::get_instance_align() const
{
    size_t aligned_size = fill_to_power_of_two(get_instance_size());
    return std::min(
        aligned_size,
        _type_context->get_machine_word_size());
}
/** end class ValueType */

/** @class IntType */
IntType::IntType(size_t binary_bits,
                 bool   is_unsigned,
                 bool   is_constant)
    : ValueType(TypeTID::INT_TYPE),
      _binary_bits(binary_bits) {
    this->_is_signed   = !is_unsigned;
    this->_is_constant = is_constant;
}
IntType::IntType(size_t binary_bits,
                 TypeContext   &ctx,
                 bool   is_unsigned,
                 bool   is_constant)
    : ValueType(TypeTID::INT_TYPE, ctx),
      _binary_bits(binary_bits) {
    this->_is_signed   = !is_unsigned;
    this->_is_constant = is_constant;
}

std::string IntType::toString() const
{
    char   buffer[16];
    size_t len = snprintf(buffer, 16,
                    _is_signed? "i%hhd": "u%hhd",
                    _binary_bits);
    return {buffer, len};
}
bool IntType::weakly_equals(const Type *that) const
{
    if (this == that) return true;
    if (that == nullptr ||
        that->get_type_id() != TypeTID::INT_TYPE)
        return false;
    auto ithat = static_cast<const IntType*>(that);
    return _binary_bits == ithat->_binary_bits;
}
bool IntType::equals(const Type *that) const
{
    return IntType::weakly_equals(that) &&
           is_signed() == that->is_signed();
}
bool IntType::fully_equals(const Type *that) const
{
    return IntType::equals(that) &&
           is_constant() == that->is_constant();
}
CastMode IntType::canCastTo(Type *that) const
{
    if (weakly_equals(that))
        return CastMode::CAST_KEEP_THIS;
    switch (that->get_type_id()) {
    case TypeTID::INT_TYPE:
    case TypeTID::FLOAT_TYPE:
        return CastMode(size_t(CastMode::STATIC_CAST) |
                        size_t(CastMode::CAST_IMPLICIT));
    case TypeTID::POINTER_TYPE:
    case TypeTID::LABEL_TYPE:
        return CastMode::REINTERPRET_CAST;
    default:
        return CastMode::CAST_NONE;
    }
}
CastMode IntType::canImplicitAccept(Type *that) const
{
    if (weakly_equals(that))
        return CastMode::CAST_KEEP_THIS;
    switch (that->get_type_id()) {
    case TypeTID::INT_TYPE:
    case TypeTID::FLOAT_TYPE:
        return CastMode(size_t(CastMode::STATIC_CAST) |
                        size_t(CastMode::CAST_IMPLICIT));
    default:
        return CastMode::CAST_NONE;
    }
}
/** static class IntType */
static IntType _i0(0),
               _i1(1),
               _i8(8),
              _i32(32),
              _i64(64);

IntType *IntType::i0(&_i0),
        *IntType::i1(&_i1),
        *IntType::i8(&_i8),
        *IntType::i32(&_i32),
        *IntType::i64(&_i64);

/** end class IntType */

/** @class FloatType */
FloatType::FloatType(size_t index, size_t tail,
                     size_t base_num, std::string_view name)
    : ValueType(TypeTID::FLOAT_TYPE),
      _index_nbits(index),
      _tail_nbits(tail),
      _base_number(base_num) {
    _initName(name);
}
FloatType::FloatType(TypeContext &ctx,
                     size_t index, size_t tail, size_t base_num,
                     std::string_view name)
    : ValueType(TypeTID::FLOAT_TYPE, ctx),
      _index_nbits(index),
      _tail_nbits(tail),
      _base_number(base_num) {
    _initName(name);
}

std::string FloatType::toString() const
{
    if (get_name().empty()) {
        char   buffer[16] = {'\0'};
        size_t len = snprintf(buffer, 16, "f%ld", get_binary_bits());
        return {buffer, len};
    }
    return _name;
}

size_t FloatType::get_hash() const
{
    size_t tid = size_t(TypeTID::FLOAT_TYPE);
    tid = std::hash<size_t>()(tid);
    size_t tmp1 = std::hash<int>()(_index_nbits);
    size_t tmp2 = std::hash<int>()(_tail_nbits);
    size_t tmp3 = std::hash<int>()(_base_number);
    return std::hash<size_t>()(tid + tmp1 + tmp2 + tmp3);
}

bool FloatType::weakly_equals(const Type *that) const
{
    if (this == that)
        return true;
    if (that == nullptr ||
        that->get_type_id() != TypeTID::FLOAT_TYPE)
        return false;
    auto *ft = static_cast<const FloatType*>(that);
    return (get_index_nbits() == ft->get_index_nbits() &&
            get_tail_nbits()  == ft->get_tail_nbits() &&
            get_base_number() == ft->get_base_number());
}
bool FloatType::equals(const Type *that) const
{
    auto ft = static_cast<const FloatType*>(that);
    return (weakly_equals(that) &&
            get_name() == ft->get_name());
}
bool FloatType::fully_equals(const Type *that) const
{
    return (equals(that) &&
            is_constant() == that->is_constant());
}

CastMode FloatType::canCastTo(Type *that) const
{
    if (weakly_equals(that))
        return CastMode::CAST_KEEP_THIS;
    switch (get_type_id()) {
    case TypeTID::INT_TYPE:
    case TypeTID::FLOAT_TYPE:
        return CastMode{
                size_t(CastMode::STATIC_CAST) |
                size_t(CastMode::CAST_IMPLICIT)
        };
    default:
        return CastMode::CAST_NONE;
    }
}
CastMode FloatType::canImplicitAccept(Type *that) const
{
    return canCastTo(that);
}
/** private class FloatType */
void FloatType::_initName(std::string_view name)
{
    if (!name.empty())
        _name = name;
}
/** static class FloatType */
FloatType _ieee_f32(8, 23, 2, "float");
FloatType _ieee_f64(11, 52, 2, "double");
InlineInit<FloatType> _nvidia_f16(8, 7, 2, "bf16");

FloatType *FloatType::ieee_f32(&_ieee_f32);
FloatType *FloatType::ieee_f64(&_ieee_f64);
FloatType *nvidia_f16(_nvidia_f16);
/** end class FloatType */

/** @class ArrayType */
ArrayType::ArrayType(Type *element_type, size_t nelems)
    : Type(TypeTID::ARRAY_TYPE, element_type),
      ITypeIndexable(this),
      _length(nelems){}

size_t ArrayType::get_instance_size() const
{
    size_t elem_aligned_size = get_element_type()->getAlignedSize();
    size_t array_size = elem_aligned_size * _length;
    return array_size;
}
size_t ArrayType::get_instance_align() const
{
    size_t size   = fill_to_power_of_two(get_instance_size());
    size_t mwsize = get_machine_word_size();
    return std::min(size, mwsize);
}
size_t ArrayType::get_hash() const
{
    std::hash<size_t> hasher;
    size_t elem_type = get_element_type()->get_hash();
    size_t type_id = hasher(size_t(_type_id));
    return hash_combine(elem_type, type_id, hasher(_length));
}

bool ArrayType::weakly_equals(const Type *that) const
{
    if (this == that)
        return true;
    if (_type_id != that->get_type_id())
        return false;
    auto athat = static_cast<const ArrayType*>(that);
    return get_element_type()->weakly_equals(athat->get_element_type());
}
bool ArrayType::equals(const Type *that) const
{
    if (this == that)
        return true;
    if (_type_id != that->get_type_id())
        return false;
    auto athat = static_cast<const ArrayType*>(that);
    return get_element_type()->equals(athat->get_element_type());
}
bool ArrayType::fully_equals(const Type *that) const
{
    if (this == that)
        return true;
    if (_type_id != that->get_type_id())
        return false;
    auto athat = static_cast<const ArrayType*>(that);
    return get_element_type()->fully_equals(athat->get_element_type());
}
CastMode ArrayType::canCastTo(Type *that) const
{
    if (weakly_equals(that))
        return CastMode::CAST_KEEP_THIS;
    return CastMode::CAST_NONE;
}
CastMode ArrayType::canImplicitAccept(Type *that) const
{
    if (weakly_equals(that))
        return CastMode::CAST_KEEP_THIS;
    return CastMode::CAST_NONE;
}
std::string ArrayType::toString() const
{
    using namespace MTB::Compatibility;
    std::string elemty = get_element_type()->toString();
    return osb_fmt("[",   get_length(),
                   " x ", std::move(elemty),
                   "]");
}
size_t ArrayType::indexGetStartOffset(size_t index) const
{
    size_t elem_size = get_element_type()->getAlignedSize();
    return elem_size * index;
}
size_t ArrayType::indexGetEndOffset(size_t index) const
{
    Type  *elem_type = get_element_type();
    size_t elem_size = elem_type->getAlignedSize();
    return elem_size * index + elem_type->get_instance_size();
}
/** end class ArrayType */

/** @class FunctionType */
FunctionType::FunctionType(Type *return_type, TypeListT const &type_list)
    : Type(TypeTID::FUNCTION_TYPE, return_type),
      _param_list(type_list){}
FunctionType::FunctionType(Type *return_type, TypeListT &&type_list)
    : Type(TypeTID::FUNCTION_TYPE, return_type),
      _param_list(std::move(type_list)){}

size_t FunctionType::get_hash() const
{
    std::hash<size_t> hasher;
    size_t ret = hash_combine(
        hasher(size_t(TypeTID::FUNCTION_TYPE)),
        get_return_type()->get_hash()
    );
    for (Type *i : get_param_list())
        ret = hash_combine(ret, i->get_hash());
    return ret;
}
bool FunctionType::weakly_equals(const Type *that) const
{
    if (this == that)
        return true;
    if (that->get_type_id() != TypeTID::FUNCTION_TYPE)
        return false;
    auto *fthat = static_cast<const FunctionType*>(that);
    if (!(fthat->get_return_type()->weakly_equals(get_return_type())) ||
        (fthat->get_param_list().size() != get_param_list().size()))
        return false;
    bool ret = true;
    auto this_it = get_param_list().begin();
    auto that_it = fthat->get_param_list().begin();
    while (this_it != get_param_list().end()) {
        ret &= ((*this_it)->weakly_equals(*that_it));
        this_it++, that_it++;
    }
    return ret;
}
bool FunctionType::equals(const Type *that) const
{
    if (this == that)
        return true;
    if (that->get_type_id() != TypeTID::FUNCTION_TYPE)
        return false;
    auto *fthat = static_cast<const FunctionType*>(that);
    if (!(fthat->get_return_type()->equals(get_return_type())) ||
        (fthat->get_param_list().size() != get_param_list().size()))
        return false;
    bool ret = true;
    auto this_it = get_param_list().begin();
    auto that_it = fthat->get_param_list().begin();
    while (this_it != get_param_list().end()) {
        ret &= ((*this_it)->equals(*that_it));
        this_it++, that_it++;
    }
    return ret;
}
bool FunctionType::fully_equals(const Type *that) const
{
    if (this == that)
        return true;
    if (that->get_type_id() != TypeTID::FUNCTION_TYPE)
        return false;
    auto *fthat = static_cast<const FunctionType*>(that);
    if (!(fthat->get_return_type()->fully_equals(get_return_type())) ||
        (fthat->get_param_list().size() != get_param_list().size()))
        return false;
    bool ret = true;
    auto this_it = get_param_list().begin();
    auto that_it = fthat->get_param_list().begin();
    while (this_it != get_param_list().end()) {
        ret &= ((*this_it)->fully_equals(*that_it));
        this_it++, that_it++;
    }
    return ret;
}
CastMode FunctionType::canCastTo(Type *that) const
{
    if (weakly_equals(that))
        return CastMode::CAST_KEEP_THIS;
    return CastMode::CAST_NONE;
}
CastMode FunctionType::canImplicitAccept(Type *that) const
{
    return canCastTo(that);
}

std::string FunctionType::toString() const
{
    using namespace std::string_view_literals;
    std::stringstream ret;
    ret << get_return_type()->toString()
        << "(";
    for (size_t cnt = 0; Type *i: _param_list) {
        ret << ((cnt == 0)? "": ", ")
            << i->toString();
    }
    ret << ')';
    return ret.str();
}
/** end class FunctionType */

/** @class PointerType */
PointerType::PointerType(Type  *target_type,
                         bool   is_constant,
                         size_t machine_word_size)
    : Type(TypeTID::POINTER_TYPE, target_type),
      ITypeIndexable(this) {
    set_machine_word_size(machine_word_size);
    _is_constant = is_constant;
}
PointerType::PointerType(TypeContext &ctx,
                         Type  *target_type,
                         bool   is_constant)
    : Type(TypeTID::POINTER_TYPE, target_type, &ctx),
      ITypeIndexable(this) {
    _is_constant = is_constant;
}

size_t PointerType::get_hash() const
{
    std::hash<size_t> hasher;
    size_t type_id = hasher(size_t(get_type_id()));
    size_t target  = get_target_type()->get_hash();
    return hash_combine(type_id, target, size_t(_is_constant));
}

bool PointerType::weakly_equals(const Type *that) const
{
    auto pthat = equal_prefix(that);
    if (pthat == nullptr)
        return false;
    return get_target_type()->weakly_equals(pthat->get_target_type());
}
bool PointerType::equals(const Type *that) const
{
    auto pthat = equal_prefix(that);
    if (pthat == nullptr)
        return false;
    return get_target_type()->equals(pthat->get_target_type());
}
bool PointerType::fully_equals(const Type *that) const
{
    auto pthat = equal_prefix(that);
    if (pthat == nullptr)
        return false;
    return get_target_type()->fully_equals(pthat->get_target_type());
}
inline const PointerType*
PointerType::equal_prefix(const Type *that) const
{
    if (that == nullptr)
        return nullptr;
    bool ret = (this == that);
    ret &= (get_type_id() == that->get_type_id());
    return ret ? static_cast<const PointerType*>(that) : nullptr; 
}

CastMode PointerType::canCastTo(Type *that) const
{
    if (weakly_equals(that))
        return CastMode::CAST_KEEP_THIS;
    if (that->is_pointer_type())
        return CastMode::REINTERPRET_CAST;
    if (that->is_integer_type())
        return CastMode::STATIC_CAST;
    return CastMode::CAST_NONE;
}
CastMode PointerType::canImplicitAccept(Type *that) const
{
    if (weakly_equals(that))
        return CastMode::CAST_KEEP_THIS;
    return CastMode::CAST_NONE;
}
std::string PointerType::toString() const {
    return get_target_type()->toString() + '*';
}
/** end class PointerType */

/** @class LabelType */
owned<LabelType> LabelType::labelty = new LabelType(global_machine_word_size);
/** end class LabelType */


/* 内部类的实现 */

#define irbase_type_initializer _509864f21d81c6c4_type_
#define irbase_type_initializer_instance _49671bedefb00795_
class irbase_type_initializer {
public:
    explicit irbase_type_initializer()
    {
        void_ty.setInlineInit();
        _i0.setInlineInit();
        _i1.setInlineInit();
        _i8.setInlineInit();
        _i32.setInlineInit();
        _i64.setInlineInit();
        /* float */
        _ieee_f32.setInlineInit();
        _ieee_f64.setInlineInit();
    }
    ~irbase_type_initializer() = default;
public:
}; // class irbase_type_initializer
irbase_type_initializer irbase_type_initializer_instance;

} // namespace MYGL::IRBase
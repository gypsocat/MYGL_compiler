#ifndef __MYGL_IRBASE_TYPE_H__
#define __MYGL_IRBASE_TYPE_H__

#include "base/mtb-exception.hxx"
#include "base/mtb-object.hxx"
#include "base/mtb-getter-setter.hxx"
#include "base/mtb-compatibility.hxx"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#if __cplusplus >= 202002L
#include <source_location>
#endif

#define MYGL_HAS_STRUCT_TYPE false

extern size_t global_machine_word_size;

namespace MYGL::IRBase {

using namespace MTB;

size_t fill_to(size_t size, size_t align);
size_t bit_to_byte(size_t bits);
size_t get_next_power_of_two(size_t x);

static size_t fill_to_power_of_two(size_t x)
{
    size_t ret = get_next_power_of_two(x);
    if (ret == (x << 1))
        return x;
    return ret;
}

/** @enum CastMode */
enum class CastMode: uint64_t {
    CAST_NONE        = 0x0, // 不能转换到目标类型.
    CAST_IMPLICIT    = 0x1, // 允许隐式转换到目标类型
    STATIC_CAST      = 0x2, // 以保证数值相等或者相近的方式做转换
    REINTERPRET_CAST = 0x4, // 以内存重解释的方式做转换
    VALUE_ALL_CAST   = 0x6,
    CAST_KEEP_THIS   = 0xFFFFFFFFFFFFFFFF // 不需要转换, 目标和我一样
}; // enum class CastMode

bool can_implicit_cast(CastMode cast_mode);
bool can_static_cast(CastMode cast_mode);
bool can_reinterpret_cast(CastMode cast_mode);
/* end enum CastMode */

class TypeContext;
extern size_t type_context_get_machine_word_size(TypeContext const &ctx);

/** @enum TypeTID */
enum class TypeTID: uint64_t {
    BASE,
    VOID,           // void类型
    VALUE_TYPE, INT_TYPE, FLOAT_TYPE, // 数值类型
    COLLECTION_TYPE, ARRAY_TYPE,      // 集合值类型
    FUNCTION_TYPE,  // 可调用类型
    POINTER_TYPE,   // 指针类型
    INSTRUCTION_TYPE, LABEL_TYPE,     // 指令类型、标签类型
    TYPE_ALIAS      // 类型别名
}; // enum class TypeTID

/** @class Type
 * @brief 类型系统的基础类。虽然是为IR系统准备的，但是在实践过程中发现有很多功能都是为前端准备的。 */
imcomplete class Type: public MTB::Object {
public:
    ~Type() override;

    /** @property base_type{get;construct;}
     * 这个类基于哪个类型。基础类型基于void类型, void类型的base_type定为null. */
    DEFINE_GETSET(Type*, base_type)
    /** @property type_id{get;construct;}*/
    DEFINE_GETTER(type_id)
    /** @property type_context{get;set;} */
    DEFINE_GETSET(TypeContext*, type_context)
    /** @property machine_word_size{get;set;} */
    DECLARE_GETSET(uint8_t, machine_word_size)

    /* 实例的访问形式,表示与其他类型实例的交互 */
    /** 实例是否为常量 */
    DEFINE_VIRTUAL_BOOL_GETTER(is_constant)
    /** 实例是否可调用 */
    DEFINE_VIRTUAL_BOOL_GETTER(is_callable)
    /** 实例是否可被放进索引表达式 */
    DEFINE_VIRTUAL_BOOL_GETTER(is_indexable)
    /** 实例的内容是否可读取 */
    DEFINE_VIRTUAL_BOOL_GETTER(is_readable)
    DEFINE_DEFAULT_FALSE_GETTER(is_signed)

    /* Type对象类型的快速判断 */
    DECLARE_ABSTRACT_BOOL_GETTER(is_defined_type)
    DECLARE_ABSTRACT_BOOL_GETTER(is_void_type)
    DECLARE_ABSTRACT_BOOL_GETTER(is_value_type)
    DECLARE_ABSTRACT_BOOL_GETTER(is_integer_type)
    DECLARE_ABSTRACT_BOOL_GETTER(is_float_type)
    DECLARE_ABSTRACT_BOOL_GETTER(is_array_type)
    DECLARE_ABSTRACT_BOOL_GETTER(is_function_type)
    DEFINE_DEFAULT_FALSE_GETTER(is_pointer_type)
    DECLARE_ABSTRACT_GETTER(size_t, hash)

    /* 类型实例的内存布局 */
    DEFINE_VIRTUAL_GETSET(size_t, instance_size)
    DEFINE_VIRTUAL_GETSET(size_t, instance_align)
    abstract size_t get_binary_bits() const { return get_instance_size() * 8; }

    size_t   getAlignedSize() const;
    size_t   getAlignedStartOffset(size_t start_offset) const;
    size_t   getAlignedEndOffset(size_t start_offset) const;
    abstract bool weakly_equals(const Type *that) const = 0;
    abstract bool equals(const Type *that) const = 0;
    virtual  bool fully_equals(const Type *that) const;

    abstract CastMode canCastTo(Type *that) const = 0;
    abstract CastMode canImplicitAccept(Type *that) const = 0;

    [[nodiscard("Type.toString() -> 'type name'")]]
    abstract std::string toString() const = 0;
protected:
    TypeTID  _type_id;
    Type    *_base_type;
    TypeContext *_type_context;
    size_t   _instance_size, _instance_align;
    bool     _is_constant,   _is_callable;
    bool     _is_indexable,  _is_readable;
    uint8_t  _machine_word_size;

    explicit Type(TypeTID type_id, Type *base_type = nullptr) noexcept
        : _type_id(type_id), _base_type(base_type),
          _machine_word_size(global_machine_word_size),
          _type_context(nullptr) {}
    explicit Type(TypeTID type_id, Type *base_type, TypeContext *ctx) noexcept;
}; // class Type

/** @interface ITypeIndexable extends Type
 * @brief 可以索引取指针的类型. 由于 C++ 类继承方式的限制,
 *        继承 Type 的部分是手动实现的. */
interface ITypeIndexable {
public:
    operator Type &     ()       { return *_base; }
    operator Type const&() const { return *_base; }

    /** @fn indexGetElemType[index] -> Type*
     * @brief 根据参数 `index` 所示的下标对类型为类型为自己的
     *        `Value` 实例取索引, 得到的元素类型 */
    abstract Type *indexGetElemType(int index) const = 0;
protected:
    ITypeIndexable(Type *base): _base(base) {}
private:
    Type *_base;
}; // interface ITypeIndexable

/** @class VoidType */
class VoidType final: public Type {
public:
    explicit VoidType() noexcept;

    bool is_constant()  const final { return false; }
    bool is_callable()  const final { return false; }
    bool is_indexable() const final { return false; }
    bool is_readable()  const final { return false; }

    bool is_defined_type()  const final { return false; }
    bool is_void_type()     const final { return true;  }
    bool is_value_type()    const final { return false; }
    bool is_integer_type()  const final { return false; }
    bool is_float_type()    const final { return false; }
    bool is_array_type()    const final { return false; }
    bool is_function_type() const final { return false; }

    size_t get_instance_size()  const final { return 0; }
    size_t get_instance_align() const final { return 0; }

    size_t get_hash() const final { return 0; }
    bool weakly_equals(const Type *that) const final { return this == that; }
    bool equals(const Type *that)        const final { return this == that; }
    bool fully_equals(const Type *that)  const final { return false; }

    CastMode canCastTo(Type *that) const final { return CastMode::CAST_NONE; }
    CastMode canImplicitAccept(Type *that) const final { return CastMode::CAST_NONE; }
    std::string toString() const final { return "void"; }
public:
    static VoidType *voidty; // Void类型原则上是单例，这是实例指针
}; // class VoidType final

/** @class ValueType
 * @brief 标量类型，表示一个可以比较大小、可以运算、可以取哈希的基础类型 */
imcomplete class ValueType: public Type {
public:
    bool is_callable()  const final { return false; }
    bool is_indexable() const final { return false; }
    bool is_readable()  const final { return true;  }

    bool is_defined_type()  const final { return true; }
    bool is_void_type()     const final { return false; }
    bool is_value_type()    const final { return true;  }
    bool is_array_type()    const final { return false; }
    bool is_function_type() const final { return false; }

    size_t get_instance_size()  const final;
    size_t get_instance_align() const final;
protected:
    explicit ValueType(TypeTID type_id);
    explicit ValueType(TypeTID type_id, TypeContext &ctx);
}; // class ValueType

/** @class IntType
 * @brief 整数类型, 可以设定任意位数 */
class IntType final: public ValueType {
public:
    static IntType *i0;        // 恒为0的类型
    static IntType *i1,  *i8;  // bool类型, char类型
    static IntType *i32, *i64; // int类型, int64类型
public:
    explicit IntType(size_t binary_bits,
                     bool   is_unsigned = false,
                     bool   is_constant = false);
    explicit IntType(size_t binary_bits,
                     TypeContext   &ctx,
                     bool   is_unsigned = false,
                     bool   is_constant = false);

    bool   is_integer_type() const final { return true;  }
    bool   is_float_type()   const final { return false; }
    bool   is_signed()       const final { return _is_signed; }
    size_t get_binary_bits() const final { return _binary_bits; }

    size_t get_hash() const final {
        return std::hash<size_t>()(int(get_type_id()) * _binary_bits);
    }

    /** 弱相等规则: 为该实例，或类型相等、位数相等 */
    bool weakly_equals(const Type *that) const final;
    /** 相等规则: 弱相等且符号相同 */
    bool equals(const Type *that)        const final;
    /** 强相等规则: 相等且常值属性相同 */
    bool fully_equals(const Type *that)  const final;
    CastMode canCastTo(Type *that)       const final;
    CastMode canImplicitAccept(Type *that) const final;
    std::string toString()               const final;

    bool is_bool_type() const { return _binary_bits == 1; }
private:
    uint8_t _binary_bits;
    bool8_t _is_signed;
}; // class IntType final

/** @class FloatType
 * 浮点类型，可以调整浮点的进位制、指数位、小数位.都是有符号的 */
class FloatType final: public ValueType {
public:
    static FloatType *ieee_f32, *ieee_f64; // float与double类型
public:
    explicit FloatType(size_t index, size_t tail,
                       size_t base_num = 2, std::string_view name = {});

    explicit FloatType(TypeContext &ctx,
                       size_t     index,
                       size_t      tail,
                       size_t  base_num = 2,
                       std::string_view name = {});

    DEFINE_TYPED_GETTER(int, index_nbits)
    DEFINE_TYPED_GETTER(int, tail_nbits)
    DEFINE_TYPED_GETTER(int, base_number)
    DEFINE_TYPED_GETTER(std::string_view, name)

    bool   is_integer_type() const final { return false; }
    bool   is_float_type()   const final { return true;  }
    bool   is_signed()       const final { return true;  }
    size_t get_binary_bits() const final {
        return _index_nbits + _tail_nbits + 1;
    }
    /** 弱相等规则: 都是浮点，且指数位数、尾数位数、进制相同 */
    bool weakly_equals(const Type *that) const final;
    /** 相等规则: 弱相等且名称相同 */
    bool equals(const Type *that)        const final;
    /** 强相等规则: 相等且常值属性相同 */
    bool fully_equals(const Type *that)  const final;
    /** 哈希函数: 玄学 */
    size_t get_hash()                    const final;

    CastMode canCastTo(Type *that)         const final;
    CastMode canImplicitAccept(Type *that) const final;
    std::string toString() const final;
private:
    uint8_t _index_nbits, _tail_nbits;
    uint8_t _base_number;
    std::string    _name;

    void _initName(std::string_view name);
}; // class FloatType

/** @class ArrayType
 * @brief 一维数组类型。多维数组类型请使用数组套数组来模拟。 */
class ArrayType final: public Type,
                       public ITypeIndexable {
public:
    explicit ArrayType(Type *element_type, size_t nelems);
    explicit ArrayType(TypeContext &ctx, Type *element_type, size_t nelems)
        : ArrayType(element_type, nelems) {
        set_type_context(&ctx);
    }

    bool is_constant()  const final { return get_element_type()->is_constant(); }
    bool is_callable()  const final { return false; }
    bool is_indexable() const final { return true;  }
    bool is_readable()  const final { return true;  }

    bool is_defined_type()  const final { return true;  }
    bool is_void_type()     const final { return false; }
    bool is_value_type()    const final { return false; }
    bool is_integer_type()  const final { return false; }
    bool is_float_type()    const final { return false; }
    bool is_array_type()    const final { return true;  }
    bool is_function_type() const final { return false; }

    size_t get_instance_size()  const final;
    size_t get_instance_align() const final;

    size_t get_hash() const final;
    bool weakly_equals(const Type *that) const final;
    bool equals(const Type *that)        const final;
    bool fully_equals(const Type *that)  const final;

    CastMode canCastTo(Type *that) const final;
    CastMode canImplicitAccept(Type *that) const final;
    std::string toString() const final;

    DEFINE_GETSET_WRAPPER(Type*, element_type, base_type)

    /** @property length{get;set;}
     *  @brief 数组元素个数. */
    size_t get_length() const { return _length; }
    /** @fn set_length(size_t value)
     * @see property:length
     * @see get_length() */
    void set_length(size_t length) { _length = length; }

    size_t indexGetStartOffset(size_t index) const;
    size_t indexGetEndOffset(size_t index) const;
    /** @fn indexGetElemType(int index)
     * @brief 根据参数 `index` 所示的下标对类型为类型为自己的
     *        `Value` 实例取索引, 得到类型 `element_type`.
     * @see ITypeIndexable::indexGetElemType(int index) */
    Type  *indexGetElemType(int index) const final {
        return get_element_type();
    }
private:
    size_t _length; // 数组元素个数
}; // class ArrayType final

/** @class FunctionType */
class FunctionType final: public Type {
public:
    using TypeListT = std::list<Type*>;
public:
    explicit FunctionType(Type *return_type, TypeListT const &type_list);
    explicit FunctionType(Type *return_type, TypeListT &&type_list);

    bool is_constant()  const final { return true;  }
    bool is_callable()  const final { return true;  }
    bool is_indexable() const final { return false; }
    bool is_readable()  const final { return false; }

    bool is_defined_type()  const final { return true;  }
    bool is_void_type()     const final { return false; }
    bool is_value_type()    const final { return false; }
    bool is_integer_type()  const final { return false; }
    bool is_float_type()    const final { return false; }
    bool is_array_type()    const final { return false; }
    bool is_function_type() const final { return true;  }

    size_t get_instance_size()  const final { return 0; }
    size_t get_instance_align() const final { return 0; }

    size_t get_hash() const final;
    bool weakly_equals(const Type *that) const final;
    bool equals(const Type *that)        const final;
    bool fully_equals(const Type *that)  const final;

    CastMode canCastTo(Type *that) const final;
    CastMode canImplicitAccept(Type *that) const final;
    std::string toString() const final;

    DEFINE_GETTER_WRAPPER(Type*, return_type, base_type)
    DEFINE_TYPED_GETTER(TypeListT const &, param_list)

    /** @property param_nmemb{get;} */
    size_t get_param_nmemb() const {
        return get_param_list().size();
    }
private:
    TypeListT _param_list;
}; // class FunctionType final

class PointerType: public Type,
                   public ITypeIndexable {
public:
    explicit PointerType(Type  *target_type,
                         bool   is_constant = false,
                         size_t machine_word_size = global_machine_word_size);
    explicit PointerType(TypeContext &ctx,
                         Type  *target_type,
                         bool   is_constant = false);

    bool is_callable()  const final {
        return get_target_type()->is_function_type();
    }
    bool is_indexable() const final { return true; }
    bool is_readable()  const final { return true; }

    bool is_defined_type()  const final { return true;  }
    bool is_void_type()     const final { return false; }
    bool is_value_type()    const final { return false; }
    bool is_integer_type()  const final { return false; }
    bool is_float_type()    const final { return false; }
    bool is_array_type()    const final { return false; }
    bool is_function_type() const final { return false; }
    bool is_pointer_type()  const final { return true;  }

    size_t get_instance_size()  const final { return get_machine_word_size(); }
    size_t get_instance_align() const final { return get_machine_word_size(); }

    size_t get_hash() const final;
    bool weakly_equals(const Type *that) const final;
    bool equals(const Type *that)        const final;
    bool fully_equals(const Type *that)  const final;

    CastMode canCastTo(Type *that) const final;
    CastMode canImplicitAccept(Type *that) const final;
    std::string toString() const final;

    /** @property target_type = base_type */
    DEFINE_GETTER_WRAPPER(Type*, target_type, base_type)

    /** @fn indexGetElemType(int index)
     * @brief 根据参数 `index` 所示的下标对类型为类型为自己的
     *        `Value` 实例取索引, 得到类型 `element_type`.
     * @see ITypeIndexable::indexGetElemType(int index) */
    Type *indexGetElemType(int index) const final {
        return get_target_type();
    }
private:
    const PointerType *equal_prefix(const Type *that) const;
}; // class PointerType final

class LabelType final: public PointerType {
public:
    static owned<LabelType> labelty;
public:
    explicit LabelType(size_t machine_word_size = 8):
        PointerType(VoidType::voidty,
                    false, machine_word_size) {}
    explicit LabelType(TypeContext &ctx)
        : PointerType(ctx, VoidType::voidty, false){}
}; // class LabelType final

struct TypeHasher {
    size_t operator()(owned<Type> const &type) const {
        return type->get_hash();
    }
    size_t operator()(const Type *type) const {
        return type->get_hash();
    }
}; // struct TypeHasher
struct TypeFullEqual {
    bool operator()(owned<Type> const &lhs, owned<Type> const &rhs) const {
        return lhs->fully_equals(rhs);
        owned<Type> i{new FloatType(8, 23)};
    }
    bool operator()(const Type *lhs, const Type *rhs) const {
        return lhs->fully_equals(rhs);
    }
}; // struct TypeFullEqual

using TypePtrT = owned<Type>;
using TypeSet  = std::unordered_set<TypePtrT, TypeHasher, TypeFullEqual>;
using TypeMap  = std::unordered_map<std::string_view, TypePtrT>;

class TypeMismatchException: public Exception {
public:
    TypeMismatchException(Type *current, std::string &&mismatch_reason,
                          SourceLocation location = CURRENT_SRCLOC)
        : Exception {
            ErrorLevel::CRITICAL,
            MTB::Compatibility::osb_fmt("TypeMismatchException at",
                source_location_stringfy(location), ": ",
                std::move(mismatch_reason)
            ), location
        }, current_type(current) {
    }
    Type *current_type;
}; // abstract class TypeMismatchException

} // namespace MYGL::IRBase

#endif

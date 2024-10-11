#ifndef __MYGL_IR_CONSTANT_H__
#define __MYGL_IR_CONSTANT_H__

#include "base/mtb-exception.hxx"
#include "base/mtb-getter-setter.hxx"
#include "base/mtb-object.hxx"
#include "base/mtb-apint.hxx"
#include "irbase-type.hxx"
#include "irbase-use-def.hxx"
#include "ir-basic-value.hxx"
#include <cstdint>
#include <string>
#include <utility>

namespace MYGL::IR {
using namespace IRBase;

class Module;

enum CompareResult: uint8_t {
    FALSE = 0b0000,
    LT = 0b0001,    // 小于less than
    GT = 0b0010,    // 大于greater than
    EQ = 0b0100,    // 等于equals
    LE = LT | EQ,   // 小于或等于
    GE = GT | EQ,   // 大于或等于
    NE = LT | GT,   // 不等于
    TRUE = 0b0111
}; // enum class CompareResult

std::string_view CompareResultToString(CompareResult result);

/** @class Constant
 * 存储不变量或者全局量的值。有时Constant会引用其他值，所以Constant是User. */
imcomplete class Constant: public User {
public:
    enum class ConstantKind: uint8_t {
        NONE = 0x0,     INTEGER = 0x1,  FLOAT = 0x2,
        COL_NONE = 0x0, STRUCT  = 0x4,  ARRAY = 0x8,
        DATA = 0x0,     FUNCTION= 0x10, POINTER = 0x20,
        INTERNAL = 0x0, GLOBAL  = 0x40
    }; // enum class ConstantKind
public:
    static owned<Constant> CreateZeroOrUndefined(Type *type);
public:
    OVERRIDE_GET_TRUE(is_readable)

    /* 常量类型标识 */
    ConstantKind get_const_kind() const;
    DECLARE_ABSTRACT_BOOL_GETTER(is_raw_data)
    DECLARE_ABSTRACT_BOOL_GETTER(is_integer)
    DECLARE_ABSTRACT_BOOL_GETTER(is_float)
    DECLARE_ABSTRACT_BOOL_GETTER(is_array)
    DECLARE_ABSTRACT_BOOL_GETTER(is_pointer)
    DECLARE_ABSTRACT_BOOL_GETTER(is_global)
    DECLARE_ABSTRACT_BOOL_GETTER(is_function)
    DECLARE_ABSTRACT_BOOL_GETTER(is_global_variable)
    DECLARE_ABSTRACT_BOOL_GETTER(target_is_mutable)
    DECLARE_ABSTRACT_BOOL_GETTER(is_zero)

    virtual void reset() = 0;
    virtual bool equals(const Constant *that) const {
        return this == that;
    }
protected:
    explicit Constant(ValueTID type_id, Type *value_type);
}; // imcomplete class Constant

interface IConstantZero {
    Constant *iconst_zero_self;
public:
    explicit IConstantZero(Constant *self): iconst_zero_self(self) {}

    operator Constant &() { return *iconst_zero_self; }
    operator Constant const&() const { return *iconst_zero_self; }

    abstract owned<Constant> make_nonzero_instance() const = 0;
}; // interface ConstantZero

imcomplete class ConstantData: public Constant {
public:
    /** @struct Less
     * @brief 用于比较的接口。这个类是为STL容器准备的，一般来说像std::set这样的容器会给
     *        一个参数用来表示采用什么比较关系。如果它要求这样的泛型参数，你就要把这个结
     *        构体作为参数填进去。 */
    struct Less {
        bool operator()(const ConstantData *lhs, const ConstantData *rhs) {
            CompareResult result = lhs->compare(owned(const_cast<ConstantData*>(rhs)));
            if (result == CompareResult::NE)
                return false;
            return (size_t(result) & size_t(CompareResult::LT)) != 0;
        }
    }; // struct Less
public:
    /** @fn CreateZero
     * @throws NullException
     * @throws TypeMismatchException */
    static owned<ConstantData> CreateZero(Type *value_type) noexcept(false);
public:
    OVERRIDE_GET_TRUE (is_readable)
    OVERRIDE_GET_FALSE(is_writable)
    OVERRIDE_GET_TRUE (is_raw_data)
    OVERRIDE_GET_FALSE(is_array)
    OVERRIDE_GET_FALSE(is_pointer)
    OVERRIDE_GET_FALSE(is_global)
    OVERRIDE_GET_FALSE(is_function)
    OVERRIDE_GET_FALSE(is_global_variable)
    OVERRIDE_GET_FALSE(target_is_mutable)
    OVERRIDE_GET_FALSE(uniquely_referenced)
    size_t replaceAllUsee(Value *from, Value *to) final {
        (void)from; (void)to;
        return 0;
    }

    abstract owned<ConstantData> castToClosest(Type *target_type) const = 0;
    abstract owned<ConstantData> add(const owned<ConstantData> rhs) const = 0;
    abstract owned<ConstantData> sub(const owned<ConstantData> rhs) const = 0;
    abstract owned<ConstantData> mul(const owned<ConstantData> rhs) const = 0;
    abstract owned<ConstantData> sdiv(const owned<ConstantData> rhs) const = 0;
    virtual  owned<ConstantData> udiv(const owned<ConstantData> rhs) const {
        return sdiv(std::move(rhs));
    }
    owned<ConstantData> div(const owned<ConstantData> rhs) const { return sdiv(rhs); }
    abstract owned<ConstantData> smod(const owned<ConstantData> rhs) const = 0;
    abstract owned<ConstantData> umod(const owned<ConstantData> rhs) const {
        return smod(std::move(rhs));
    }
    abstract owned<ConstantData> mod(const owned<ConstantData> rhs) const {
        return smod(std::move(rhs));
    }
    abstract owned<ConstantData> neg() const = 0;
    abstract CompareResult compare(const owned<ConstantData> rhs) const = 0;
    abstract owned<ConstantData> copy() const = 0;

    /** @property apint_value{get;set;} abstract */
    abstract APInt get_apint_value() const = 0;
    abstract void set_apint_value(APInt v) = 0;

    DECLARE_ABSTRACT_GETSET(int64_t, int_value)
    DECLARE_ABSTRACT_GETSET(uint64_t, uint_value)
    DECLARE_ABSTRACT_GETSET(double, float_value)

    void reset() final { set_uint_value(0); }
    bool equals(const Constant *rhs) const override;
protected:
    explicit ConstantData(ValueTID type_id, Type *value_type);
}; // imcomplete class ConstantData

/** @class IntConst
 * @brief 整数常量 */
class IntConst final: public ConstantData {
public:
    static owned<IntConst> BooleanTrue, BooleanFalse;
public:
    explicit IntConst(IntType *value_type, int64_t value);
    explicit IntConst(IntType *value_type, APInt value, bool as_signed = true);
    ~IntConst() final;

    OVERRIDE_GET_TRUE(is_integer)
    OVERRIDE_GET_FALSE(is_float)
    bool is_zero() const override { return _value == 0; }

    APInt get_apint_value() const final { return _value; }
    void set_apint_value(APInt v) final { set_value(v); }

    uint64_t get_uint_value() const final { return _value.get_unsigned_value(); }
    void set_uint_value(uint64_t value) final { _value.set_value(value); }

    int64_t  get_int_value() const final { return _value.get_signed_value(); }
    void set_int_value(int64_t value)   final { _value.set_value(value); }

    double get_float_value() const final { return _value.get_signed_value(); }
    void set_float_value(double value)  final { _value = value; }

    DEFINE_GETSET(APInt, value)

    owned<ConstantData> castToClosest(Type *target_type) const final;
    owned<ConstantData> add(const owned<ConstantData> rhs) const final;
    owned<ConstantData> sub(const owned<ConstantData> rhs) const final;
    owned<ConstantData> mul(const owned<ConstantData> rhs) const final;
    owned<ConstantData> sdiv(const owned<ConstantData> rhs) const final;
    owned<ConstantData> udiv(const owned<ConstantData> rhs) const final;
    owned<ConstantData> smod(const owned<ConstantData> rhs) const final;
    owned<ConstantData> umod(const owned<ConstantData> rhs) const final;
    owned<ConstantData> neg() const final {
        return new IntConst(
            static_cast<IntType*>(_value_type), -_value
        );
    }
    owned<ConstantData> copy() const final {
        return new IntConst(static_cast<IntType*>(get_value_type()), _value);
    }

    void accept(IValueVisitor &visitor) final;
    CompareResult compare(const owned<ConstantData> rhs) const final;
private:
    APInt _value;
}; // class IntConst final

class FloatConst final: public ConstantData {
public:
    static owned<FloatConst> Create(FloatType *value_type, double value = 0);
public:
    explicit FloatConst(FloatType *value_type, double value) noexcept;
    ~FloatConst() final;

    OVERRIDE_GET_FALSE(is_integer)
    OVERRIDE_GET_TRUE (is_float)
    bool is_zero() const override { return _value == 0.0; }

    string_view get_name()    const final { return _name; }
    void set_name(string_view name) final { _name = name; }

    APInt get_apint_value() const final {
        return APInt{
            uint8_t(((FloatType*)get_value_type())->get_binary_bits()),
            int64_t(_value)
        };
    }
    void set_apint_value(APInt v) final { set_value(v.get_signed_value()); }

    uint64_t get_uint_value() const final { return _value; }
    void set_uint_value(uint64_t value) final { set_value(value); }

    int64_t  get_int_value()  const final { return _value; }
    void set_int_value(int64_t value)   final { set_value(value); }

    double get_float_value()  const final { return _value; }
    void set_float_value(double value)  final { set_value(value); }

    /** @property value{get;set;} 浮点类存储的真值 */
    DEFINE_GETSET(double, value)

    owned<ConstantData> castToClosest(Type *target_type) const final;
    owned<ConstantData> add(const owned<ConstantData> rhs) const final;
    owned<ConstantData> sub(const owned<ConstantData> rhs) const final;
    owned<ConstantData> mul(const owned<ConstantData> rhs) const final;
    owned<ConstantData> sdiv(const owned<ConstantData> rhs) const final;
    owned<ConstantData> smod(const owned<ConstantData> rhs) const final;
    owned<ConstantData> neg() const final {
        return new FloatConst(static_cast<FloatType*>(_value_type), -_value);
    }
    owned<ConstantData> copy() const final {
        return new FloatConst(static_cast<FloatType*>(get_value_type()), _value);
    }

    void accept(IValueVisitor &visitor) final;
    CompareResult compare(const owned<ConstantData> rhs) const final;
    bool equals(const Constant *another) const override;
private:
    double _value;
}; // class FloatConst final

/** @class ZeroDataConst
 *  @brief 类型无关的零值.
 *
 * <等待补充说明> */
class ZeroDataConst final: public ConstantData,
                           public IConstantZero {
public:
    /** 全局共用的0值 */
    static owned<ZeroDataConst> zero;
public:
    explicit ZeroDataConst(TypeContext &ctx);

    OVERRIDE_GET_FALSE(is_integer)
    OVERRIDE_GET_FALSE(is_float)
    OVERRIDE_GET_TRUE(is_zero)

    string_view get_name() const final { return "0"; }

    APInt get_apint_value() const final {
        return APInt{32, 0};
    }

    uint64_t get_uint_value()  const final { return 0; }
    int64_t  get_int_value()   const final { return 0; }
    double   get_float_value() const final { return 0.0; }

    owned<ConstantData> castToClosest(Type *target_type)   const final;
    owned<ConstantData> add(const owned<ConstantData> rhs) const final { return const_cast<ConstantData*>(rhs.get()); }
    owned<ConstantData> sub(const owned<ConstantData> rhs) const final { return rhs->neg(); }
    owned<ConstantData> mul(const owned<ConstantData> rhs) const final { return this; }

    owned<ConstantData> sdiv(const owned<ConstantData> rhs) const final { return this; }
    owned<ConstantData> smod(const owned<ConstantData> rhs) const final { return this; }
    owned<ConstantData> neg()  const final { return this; }
    owned<ConstantData> copy() const final {
        return new ZeroDataConst(*_value_type->get_type_context());
    }
    CompareResult compare(const owned<ConstantData> rhs) const final;
    void accept(IValueVisitor &visitor) final;
    owned<Constant> make_nonzero_instance() const final;

private: /** 这些set函数对于0值没有任何作用，所以不使用。 */
    void set_name(string_view name) final{}
    void set_uint_value(uint64_t value) final {}
    void set_int_value(int64_t value) final {}
    void set_float_value(double value) final {}
    void set_apint_value(APInt v) final {}
}; // class ZeroDataConst final
using ZeroConst = ZeroDataConst;

/** @class UndefinedConst
 * @brief 未定义值的占位符。只负责保存类型信息，不负责保存值。
 *
 * 在这一版实现中, UndefinedConst的父类是Constant而不是ConstantData, 
 * 原因是Undefined根本不含有ConstantData的数学运算等等特性。 */
class UndefinedConst: public Constant {
public:
    static owned<UndefinedConst> undefined;
public:
    explicit UndefinedConst(Type *value_type = VoidType::voidty)
        : Constant(ValueTID::UNDEFINED, value_type){}

    OVERRIDE_GET_FALSE(is_readable)
    OVERRIDE_GET_FALSE(is_writable)
    OVERRIDE_GET_FALSE(is_raw_data)
    OVERRIDE_GET_FALSE(is_integer)
    OVERRIDE_GET_FALSE(is_float)
    OVERRIDE_GET_FALSE(is_array)
    OVERRIDE_GET_FALSE(is_pointer)
    OVERRIDE_GET_FALSE(is_global)
    OVERRIDE_GET_FALSE(is_function)
    OVERRIDE_GET_FALSE(is_global_variable)
    OVERRIDE_GET_FALSE(target_is_mutable)
    OVERRIDE_GET_FALSE(is_zero)
    OVERRIDE_GET_FALSE(uniquely_referenced)
    size_t replaceAllUsee(Value *from, Value *to) final {
        (void)from; (void)to; return 0;
    }

    void reset() final {}
    void accept(IValueVisitor &visitor) override;
}; // class UndefinedConst final

/** @class PoisonConst
 *  @brief 有害的常量. 适用于数组下标越界、野指针取索引等有害的未定义行为. */
class PoisonConst final: public UndefinedConst {
public:
    explicit PoisonConst(Type *value_type = VoidType::voidty)
        : UndefinedConst(value_type) {
        _type_id = ValueTID::POISON;
    }
    OVERRIDE_GET_TRUE(is_poisonous)
    void accept(IValueVisitor &visitor) final;
}; // class PoisonConst final

/** @class ConstantExpr
 *  @brief 常量表达式，包括数组、指针。倘若要求结构体，还可以进一步扩展结构体.
 *
 * @warning 所有的常量表达式对象都是不可变的。以数组为例，要修改一个数组的
 *          index下的某个元素, 你唯一能做的是创建一份新的拷贝，然后删了原来
 *          那个数组。 */
incomplete class ConstantExpr: public Constant {
public:
    using ValueReadFunc      = bool(*)(Value*);
    using ValueReadClosure   = std::function<bool(Value*)>;
    using ValueModifyClosure = std::function<bool(Value *from, Value *&out_to)>;
public:
    OVERRIDE_GET_FALSE(is_raw_data)
    OVERRIDE_GET_FALSE(is_integer)
    OVERRIDE_GET_FALSE(is_float)
    OVERRIDE_GET_FALSE(is_global)
    OVERRIDE_GET_FALSE(is_function)
    OVERRIDE_GET_FALSE(is_global_variable)
    OVERRIDE_GET_FALSE(target_is_mutable)
    OVERRIDE_GET_FALSE(uniquely_referenced)
public: /* ==== [ConstantExpr as Content Container] ==== */
    /** @property content_nmemb{get;} */
    uint32_t get_content_nmemb() const;

    /** @property content_type */
    Type *get_content_type() const {
        return contentTypeAt(0);
    }
    Type *contentTypeAt(size_t index = 0) const;

    /** @fn contentAt */
    abstract owned<Value> getContent(size_t index) = 0;
    abstract bool         setContent(size_t index, owned<Value> content) = 0;
    template<typename ValueT>
    MTB_REQUIRE((std::is_base_of_v<Value, ValueT> &&
                !std::same_as<Value, ValueT>))
    owned<ValueT> contentAt(size_t index) {
        return dynamic_cast<ValueT*>(getContent(index).get());
    }

    abstract void traverseContent(ValueReadFunc    fn)  = 0;
    abstract void traverseContent(ValueReadClosure fn)  = 0;
    abstract void traverseModify(ValueModifyClosure fn) = 0;
protected:
    explicit ConstantExpr(ValueTID type_id, Type *value_type);
}; // class ConstantExpr

/** @class ArrarExpr
 * @brief 聚合数组表达式, 用于表示一系列数组. 该表达式具有CoW(Copy on Write)
 *        特性, 对于空数组来说，只有访问元素时才会为数组分配空间. */
class ArrayExpr final: public ConstantExpr {
public:
    static owned<ArrayExpr> CreateEmpty(ArrayType *array_type) {
        return new ArrayExpr(array_type);
    }
    static owned<ArrayExpr> CreateEmpty(Type *prob_array_type)
    {
        MTB_UNLIKELY_IF(!prob_array_type->is_array_type()) {
            throw TypeMismatchException {
                prob_array_type,
                "Target type should be array type",
                CURRENT_SRCLOC_F
            };
        }
        return new ArrayExpr(static_cast<ArrayType*>(prob_array_type));
    }
    static owned<ArrayExpr> CreateFromValueArray(ValueArrayT const& value_list, Type *content_type = nullptr);

    explicit ArrayExpr(ArrayType *array_type);
    explicit ArrayExpr(ValueArrayT &&value_list);
public:
    ArrayType *get_array_type() const {
        return static_cast<ArrayType*>(_value_type);
    }
    Type *get_element_type() const {
        return get_array_type()->get_element_type();
    }

    /** @property element_list{get;access;}
     *  @brief 元素列表. 空列表在访问是会被展开. */
    ValueArrayT const &get_element_list() const;
    ValueArrayT &      element_list();

    /** @property element_list{unsafe raw get; unsafe raw access;}
     *  @brief 没有经过处理的元素列表, 空列表不会被展开.
     *  @warning 由于该数组是CoW的，你需要单独处理列表为空的情况. */
    ValueArrayT const &unsafe_get_raw_element_list() const {
        return _element_list;
    }
    ValueArrayT &      unsafe_raw_element_list() {
        return _element_list;
    }

    /** @fn contentAt(size_t index)
     *  @brief 数组取索引. 倘若遇到被0填充的子数组/子结构体，就展开。 */
    owned<Value> getContent(size_t index) final;
    bool setContent(size_t index, owned<Value> content) final;

    void traverseContent(ValueReadFunc fn) final;
    void traverseContent(ValueReadClosure fn) final;
    void traverseModify(ValueModifyClosure fn) final;

    OVERRIDE_GET_TRUE(is_array)
    OVERRIDE_GET_FALSE(is_pointer)
    bool is_zero() const final;

    void accept(IValueVisitor &visitor) final;
    void reset() final;
private:
    ValueArrayT _element_list;
}; // class ArrayExpr final
using Array = ArrayExpr;

/** @class Definition
 * @brief 全局声明/定义值, 包括函数、全局变量. 
 * 
 * 在实际的可执行文件中,函数与全局变量都是通过指针传递的, 所以这些值在IR中**也都是指针**.
 * 如果想要访问它们的实际类型, Definition提供了target_type这个只读属性供各位使用. */
imcomplete class Definition: public Constant {
public:
    OVERRIDE_GET_TRUE(is_defined)
    OVERRIDE_GET_FALSE(is_raw_data)
    OVERRIDE_GET_FALSE(is_integer)
    OVERRIDE_GET_FALSE(is_float)
    OVERRIDE_GET_FALSE(is_array)
    OVERRIDE_GET_TRUE(is_pointer)
    OVERRIDE_GET_TRUE(is_global)

    /** @property parent{get;set;} */
    Module *get_parent() const        { return  _parent; }
    /** @property parent{get;set;} */
    void    set_parent(Module *value) { _parent = value; }

    /** @property target_type{get;} abstract */
    DECLARE_ABSTRACT_GETTER(Type*, target_type)

    /** @property value_ptr_type{get;} */
    PointerType *get_value_ptr_type() const {
        return static_cast<PointerType*>(_value_type);
    }

    /** @property is_extern{get;} abstract bool */
    abstract bool is_extern() const = 0;
protected:
    explicit Definition(ValueTID type_id, PointerType *value_type, Module *parent);
    ~Definition() override;

    Module *_parent;
}; // imcomplete class Definition

/** @class GlobalVariable
 * @brief 全局变量声明或定义. */
class GlobalVariable final: public Definition {
public:
    using RefT = owned<GlobalVariable>;

    /** @fn CreateExternRaw(gvar_type)
     * @brief 创建一个 Value 类型为 gvar_type 的 */
    MTB_NONNULL(1, 2)
    static RefT CreateExternRaw(Module *parent, PointerType *gvar_type, bool target_mutable = true);

    /** @fn CreateExtern(element_type) */
    MTB_NONNULL(1, 2)
    static RefT CreateExtern(Module *parent, Type *element_type, bool target_mutable = true);

    /** @fn CreateDefaultRaw(gvar_type)
     * @brief 直接创建一个值类型为 gvar_type、值为 0 的全局变量
     * @param gvar_type      返回的 Value 的类型
     * @param target_mutable 是创建为变量还是常量. `false` 表示创建一个常量. */
    MTB_NONNULL(1, 2)
    static RefT CreateDefaultRaw(Module *parent, PointerType *gvar_type, bool target_mutable = true);

    /** @fn CreateDefault(element_type)
     * @brief 创建一个内含值类型为 element_type、值为 0 的全局变量
     * @param gvar_type      返回的 Value 的类型
     * @param target_mutable 是创建为变量还是常量. `false` 表示创建一个常量. */
    MTB_NONNULL(1, 2)
    static RefT CreateDefault(Module *parent, Type *element_type, bool target_mutable = true);

    /** @fn CreateWithValue(init_value) */
    MTB_NONNULL(1)
    static RefT CreateWithValue(Module *parent, owned<Constant> init_value,
                                bool target_mutable = true);
public:
    /** 创建一个全局变量声明, Value 类型为 gvar_type */
    MTB_NONNULL(2, 3)
    explicit GlobalVariable(Module          *parent, PointerType  *gvar_type,
                            owned<Constant> init_value, bool  target_mutable) noexcept;
    ~GlobalVariable() override;

    /* overrides 来自父类的方法与属性 */
    OVERRIDE_GET_FALSE(is_function)
    OVERRIDE_GET_TRUE(is_global_variable)
    bool is_zero() const final
    {
        if (_target == nullptr)
            return false;
        return _target->is_zero();
    }
    /** @property target_is_mutable{override get;set;} */
    bool target_is_mutable() const final { return _target_mutable; }
    /** @property target_is_mutable{override get;set;} */
    void set_target_is_mutable(bool value) {
        _target_mutable = value;
    }

    /** @property is_extern{get;} final */
    bool is_extern() const final { return is_declaration(); }

    void accept(IValueVisitor &visitor) final;
    void reset() final { _target.reset(); }
    Type *get_target_type() const final;

    /** @property target{get;set;} */
    Constant *get_target() const { return _target; }
    /// target.set: target会被注册到Use列表里.
    /// 允许把 target 直接设置为 null 来改变是否为 extern
    void set_target(owned<Constant> target);

    /** @property align{get;set;} */
    size_t get_align() const { return _align; }
    void set_align(size_t value) { _align = value; }

    /** @property is_declaration{get;} */
    bool is_declaration() const noexcept {
        return _target == nullptr ||
               !_target->is_defined();
    }

private:
    /** 当为 null 或者 undefined 时就表示是 extern 语句 */
    owned<Constant> _target;
    size_t  _align;
    bool    _target_mutable;
}; // class GlobalVariable final

} // namespace MYGL::IR

#endif

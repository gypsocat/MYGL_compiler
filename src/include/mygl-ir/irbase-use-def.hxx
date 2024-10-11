#ifndef __MYGL_IR_BASE_USE_DEF_H__
#define __MYGL_IR_BASE_USE_DEF_H__

#include "base/mtb-exception.hxx"
#include "base/mtb-getter-setter.hxx"
#include "base/mtb-object.hxx"
#include "base/mtb-reflist.hxx"
#include "irbase-type.hxx"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
namespace MYGL::IRBase {

using namespace MTB;

class Value;
class User;
class Use;
interface IValueVisitor;

/** @enum ValueTID
 * @brief Value的RTTI枚举标识, 你可以叫它"运行时类型".
 *
 * 这个枚举是C++ RTTI之外的的补充枚举, 主要用于觉得typeid表达式非常繁琐的代码里.
 * 与RTTI一样, 每个Value类都有一个独一无二的ValueTID, 同一个类的每个Value实例
 * 都共享一个ValueTID. 你可以通过判断ValueTID来简单快速地判断两个Value实例的类
 * 是否相同, 也可以在ValueTID的枚举下面补充一些方法来辅助判断Value类的性质.
 * 
 * 每个Value类都可以通过读写属性`id`来查看Value的RTTI枚举标识. */
enum class ValueTID: uint64_t {
    VALUE, USER, ARGUMENT, BASIC_BLOCK,
    MUTABLE, CONSTANT, INSTRUCTION, 
    CONSTANT_DATA, INT_CONST, FLOAT_CONST, ZERO_CONST,
    UNDEFINED, POISON,
    CONST_EXPR, ARRAY,
    DEFINITION, FUNCTION, GLOBAL_VARIABLE,
    PHI_SSA, JUMP_BASE, JUMP_SSA, BRANCH_SSA, SWITCH_SSA,
    BINARY_SELECT_SSA, MOVE_INST, ALLOCA_SSA,
    UNARY_SSA, CAST_SSA, LOAD_SSA, UNARY_OP_SSA,
    BINARY_SSA, CALL_SSA,
    GET_ELEM_PTR_SSA, EXTARCT_ELEM_SSA, INSERT_ELEM_SSA,
    RETURN_SSA, STORE_SSA,
    MEMORY_INTRIN, MEMMOVE_SSA, MEMSET_SSA,
    COMPARE_SSA,
    UNREACHEBLE_SSA,
}; // enum class ValueTID

/** @class Value
 * @brief MYGL-IR的值类型, 是use-def关系的被拥有者.
 *
 * 下面的写法是你可能比较陌生，但是在Value的子类中经常存在的:
 * - 属性: 通常表现为带有 `GETTER/SETTER/GETSET` 字样的宏，展开后会生成与宏的
 *   名称相符的 `get_xxx()` 或者 `set_xxx()` 方法。在MYGL中，你应当把这些宏当
 *   成一种称为“属性”的全新语法. 与 `C#/Vala` 这种自带“属性”语法的语言类似，属性
 *   有如下特征:
 *   - 每个属性都有一个独一无二的名称. 名称为 `foo` 的属性, `get` 方法叫
 *     `get_foo()`, `set` 方法叫 `set_foo()`. 属性 `foo` 有时会对应一个成员
 *     变量 `_foo`, 这时这个带有下划线的 `_foo` 变量是属性 `foo` 的一部分, 属
 *     性 `foo` 是对成员变量 `_foo` 的包装。
 *   - 属性与成员变量等价，不管属性是否有对应的成员变量本体。
 *   - 属性可以是虚属性，可以是抽象属性。虚属性和一般属性在使用方式上没有差异, 其
 *     差异在内部实现的语义上 -- 虚属性的内部实现可以被子类更改. 抽象属性不允许对
 *     应成员变量.
 *   - 在 MYGL 中约定：除非特殊情况，一个继承自 `Object` 的类不允许出现公有成员
 *     变量, 但是允许出现公有属性。也就是说, 一旦涉及变量的公有暴露，你就需要使用
 *     属性把它封装起来。
 *   - 在 C++ 版本的 MYGL 中约定: 一旦出现属性宏，就要用 C# 的格式在注释里重写
 *     一遍。 */
imcomplete class Value: public Object {
public:
    using string_view = std::string_view;
    using UseListT    = std::list<Use*>;
    using UValueListT = std::list<Value*>;
    using ValueListT  = std::list<owned<Value>>;
    using ValueArrayT = std::vector<owned<Value>>;

    template<size_t length>
    using ValueFixedArrayT = std::array<owned<Value>, length>;

    /** @class ValueTypeUnmatchException
     * @brief 类型不匹配时抛出该异常.
     *
     * @param expected 函数/对象期望的类型
     * @param real     实际接收到的类型 */
    class ValueTypeUnmatchException: public MTB::Exception {
    public:
        ValueTypeUnmatchException(Type *expected, Type *real,
                                  std::string_view info   = "",
                                  SourceLocation location = CURRENT_SRCLOC);
    public:
        Type *expected, *real;
    }; // class TypeUnmatchException

    class InvalidAccessException;
public:
    ~Value() override;

    /** @property value_type{get;construct;} */
    DEFINE_TYPED_GETTER(Type*, value_type)

    /** @property type_id{get;construct;} */
    DEFINE_TYPED_GETTER(ValueTID, type_id)

    /** @property id{get;set;} */
    DEFINE_GETSET(uint32_t, id)

    /** @property is_defined{get;default=true;} virtual */
    DEFINE_DEFAULT_TRUE_GETTER(is_defined)

    /** @property is_poisonous{get;default=false} virtual */
    DEFINE_DEFAULT_FALSE_GETTER(is_poisonous)

    /** @property is_readable{get;} virtual = true */
    DEFINE_DEFAULT_TRUE_GETTER(is_readable)

    /** @property is_writable{get;set;} virtual */
    DEFINE_VIRTUAL_BOOL_GETSET(is_writable)

    /** @property uniquely_referenced{get;default=true} virtual
     * @brief 引用唯一性，即任取两个对该Value实例的不同引用, 应该把它看成一个值
     *        还是两个值.
     *
     * 像Function, Instruction这样的应当看成一个值, 该属性应当为true;
     * 对于IntConst这样的应当看成不同的值(并且一般不可变),该属性为false.
     *
     * 这个属性是在写writer时添加的. 引用唯一的值会以id形式出现在操作数里,
     * 实例可复用的值会以值字符串的形式出现在操作数里. */
    virtual bool uniquely_referenced() const { return true; }

    /** @property shares_representation = !uniquely_referenced
     * @brief 实例可复用性, uniquely_referenced的反义属性. */
    bool shares_representation() const { return !uniquely_referenced(); }

    /** @property name{get;set;access;} virtual
     * @warning MYGL不会验证两个Value的name是不是唯一的, 但是要保证任意两个Value
     *          的ID是唯一的。 */
    virtual string_view get_name() const { return _name; }
    virtual void set_name(string_view value) { _name = value; }
    virtual std::string &name() { return _name; }

    /** @property name_or_id{get;} virtual
     * @brief 名称不为空时返回名称, 否则返回id. */
    virtual std::string get_name_or_id() const;
    virtual std::string &initNameAsId();

    /** @property list_as_usee{get;access;}
     * @brief 作为操作数的链表, 记录有哪些 `Use` 关系使用了自己 */
    UseListT const &get_list_as_usee() const { return _list_as_usee; }
    /** @fn list_as_usee()
     * @see property:list_as_usee */
    UseListT &      list_as_usee()           { return _list_as_usee; }

    /* Value不能自己动, 需要一个访问器来帮它完成成员访问操作 */
    virtual void accept(IValueVisitor &visitor) = 0;

    void addUseAsUsee(Use *use) {
        _list_as_usee.push_back(use);
    }
    /** @fn removeUseAsUsee(Use)
     * @brief 移除一个使用自己当操作数的 `Use`. 由于每个 `Use`
     *        都是互斥的, 当一条指令有不止一个操作数为自己时, 因
     *        为所处的 `Use` 不同, 自己只会被删除一次. */
    void removeUseAsUsee(Use *use) {
        _list_as_usee.remove(use);
    }
protected:
    ValueTID _type_id;      // 与RTTI类似，表示Value实例的类的ID
    Type    *_value_type;   // 每个Value都有一个类型, 这个类型被注册在TypeContext里
    UseListT _list_as_usee; // 被其他值使用的use列表
    std::string _name;      // 名称, 可以不写
    uint32_t    _id;        // 每个Value都有唯一ID.
    bool     _is_writable;  // 是否为可写,这个是用来标注全部变量这样在变量表里的

    explicit Value(ValueTID type_id, Type *value_type)
        : Value(type_id, value_type, 0){}
    explicit Value(ValueTID type_id, Type *value_type, uint32_t id);
}; // imcomplete class Value

/** @class Use
 * @brief 使用关系
 * TODO: 重构use-def关系，使得Use以结构体的形式存储在集合里。 */
class Use: public Object {
public:
    struct SetResult;
    using self_t = Use;
    using OperandRefT = owned<Value>;

    /** @fn UseeSetHandle
     * @brief  Usee setter 或清除工具.
     * @return 当value == nullptr时, 表示是否应该移除Use. */
    using UseeSetHandle = std::function<SetResult(Value *value)>;
    using UseeGetHandle = std::function<Value*()>;
    using ListT  = RefList<Use>;
    using ProxyT = RefListProxy<Use>;

    struct SetResult {
        bool use_dies;
    }; // struct SetResult
public:
    Use(User *user, UseeGetHandle get_usee, UseeSetHandle set_usee);
    /** @warning unsafe: 没有额外检查 */
    Use(User *user, Value *&usee);
    /** @warning unsafe: 没有额外检查 */
    Use(User *user, owned<Value> &usee);

    ~Use() override;

    /** @property usee{get;set;}
     * @brief 被使用的值/操作数，由两个handle组成。
     * 该属性调用的大部分set函数都会自动维护use-def关系，所以swapUseeOut好像没有存在的必要。
     *
     * @section 属性声明
     *
     * 原版本的usee直接存储指针，后来改为存储`std::variant<Value**, owned<Value>*>`.
     * 由于指针方案无法联动修改User子类的元素, Variant方案不能在修改时检查，所以该属性改成了
     * 两个函数闭包 -- 一个getter一个setter.
     *
     * @note User.addValue{()=>Value, (value)=>void}
     * @note User.addUncheckedValue(Value *&value)
     * @note User.addUncheckedValue(owned<Value> &value) */
    Value *get_usee() const { return _get_handle(); }
    /** @note 属性usee */
    void set_usee(Value *usee) { _set_handle(usee); }
    /** @note 属性usee
     * @fn remove_usee
     * @brief 清除该Usee.
     * @ */
    bool remove_usee();

    /** @property user{get;set;}
     * @brief 使用Usee的值. */
    User *get_user() const { return _user; }

    /** @fn swapUseeOut
     * @brief 把usee属性换成new_usee。该方法会自动维护usee的use关系. */
    owned<Value> swapUseeOut(Value *new_usee);

/** Use as RefList<Use>.Node */
    ListT::Iterator get_iterator() {
        return _proxy.get_iterator();
    }
    ListT::Modifier get_modifier() {
        return ListT::Modifier{get_iterator()};
    }
    ProxyT &reflist_item_proxy() { return _proxy; }
protected:
    friend class User;
    void set_user(User *user) { _user = user; }

    User           *_user;
    UseeGetHandle   _get_handle;
    UseeSetHandle   _set_handle;
    ProxyT          _proxy;
}; // class Use

imcomplete class User: public Value {
public:
    using OwnedUseListT = Use::ListT;

    ~User() override;
public:
    /** @property list_as_user{get;access;}
     *  @brief 操作数链表, 存储使用关系. */
    Use::ListT const &get_list_as_user() const { return _list_as_user; }
    /** @fn list_as_user()
     * @see property:list_as_user */
    Use::ListT &      list_as_user()           { return _list_as_user; }

    bool addUseAsUser(owned<Use> use);
    bool removeUseAsUser(Use *use);

    Use *addValue(Use::UseeGetHandle get_usee, Use::UseeSetHandle set_usee);

    Use *addUncheckedValue(Value *&value);
    Use *addUncheckedValue(owned<Value> &value);

    void clearUseAsUser();
    virtual size_t replaceAllUsee(Value *pattern, Value *new_usee);
protected:
    OwnedUseListT _list_as_user;

    explicit User(ValueTID type_id, Type *value_type);
}; // imcomplete class User

struct ValueUsePair {
    owned<Value> value;
    Use         *use;
public:
    operator owned<Value>&() { return value; }
    operator Value*() const  { return value; }
}; // struct ValueUsePair
using ValueUseArrayT = std::vector<ValueUsePair>;
using ValueRefT = std::variant<Value*&, owned<Value>&>;


class Value::InvalidAccessException: public MTB::Exception {
public:
    enum Permission: uint8_t {
        NONE  = 0b0000'0000,
        READ  = 0b0000'0001,
        WRITE = 0b0000'0010,
        READ_WRITE = 0b0011,
    }; // enum Permission

    InvalidAccessException(owned<Value> access_target,
                           Permission requried, Permission real,
                           std::string    reason,
                           SourceLocation src_location);
    
    owned<Value> access_target;
    std::string  reason;
    Permission   required, real;
}; // class InvalidAccessException

} // namespace MYGL::IRBase

#if __cplusplus >= 202002L || defined(_USE_ISOC23)
#define MYGL_ADD_USEE_PROPERTY(property, ...) \
    addValue([this __VA_OPT__(,) __VA_ARGS__](){ return get_##property(); },\
             [this __VA_OPT__(,) __VA_ARGS__](Value *usee){\
                set_##property(usee);\
                return Use::SetResult{false};\
             })
#else
#define MYGL_ADD_USEE_PROPERTY(property) \
    addValue([this](){ return get_##property(); },\
             [this](Value *usee){\
                set_##property(usee);\
                return Use::SetResult{false};\
             })
#endif

#define MYGL_ADD_TYPED_USEE_PROPERTY(Type, property) \
    addValue([this](){ return get_##property(); },\
             [this](Value *usee)-> Use::SetResult { \
                Type* tusee = dynamic_cast<Type*>(usee);\
                if ((tusee == nullptr) != (usee == nullptr))\
                    return Use::SetResult{false};\
                set_##property(tusee);\
                return Use::SetResult{false};\
             })
#define MYGL_ADD_TYPED_USEE_PROPERTY_FINAL(Type, VALUE_TID, property)\
    addValue([this](){ return get_##property(); },\
            [this](Value *usee)-> Use::SetResult { \
                if (usee != nullptr &&\
                    usee->get_type_id() != ValueTID::VALUE_TID)\
                    return Use::SetResult{false};\
                Type* tusee = static_cast<Type*>(usee);\
                set_##property(tusee);\
                return Use::SetResult{false};\
            })
#endif

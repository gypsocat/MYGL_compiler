#ifndef __MYGL_IR_BASIC_VALUE_H__
#define __MYGL_IR_BASIC_VALUE_H__

#include "base/mtb-getter-setter.hxx"
#include "irbase-use-def.hxx"
#include "irbase-type.hxx"
#include <cstdint>
#include <deque>

namespace MYGL::IR {
using namespace IRBase;

class Module;
class Function;
class Argument;
class BasicBlock;
class MoveInst;

/** @class Argument final
 * @brief 函数参数 */
class Argument final: public Value {
public:
    explicit Argument(Type *value_type, std::string_view name, Function *parent);

    /** @property parent{get;set;} */
    Function *get_parent()      const { return _parent; }
    void set_parent(Function *value) { _parent = value; }

    OVERRIDE_GET_TRUE(is_readable)
    OVERRIDE_GET_TRUE(is_writable)

    void accept(IValueVisitor &visitor) final;
private:
    Function *_parent;
}; // class Argument final

/** @class Mutable
 *  @brief 函数的可变寄存器. 由 `Function` 独占所有权. */
class Mutable final: public Value {
public:
    using RefT    = owned<Mutable>;
    using URefT   = std::unique_ptr<Mutable>;
    using ArrayT  = std::deque<RefT>;
    using UArrayT = std::deque<URefT>;

    static RefT Create(Type *reg_type, Function *parent, int32_t mutable_index) noexcept(false);
public:
    explicit Mutable(Type *reg_type, Function *parent, int32_t mutable_index);
    ~Mutable() final;

    /** @property index{get;set;}
     * @brief 表示该可变寄存器在 `Function` 中的索引. */
    int32_t get_index() const     { return  _index; }
    void set_index(int32_t index) { _index = index; }

    /** @property parent{get;set;} */
    Function *get_parent() const      { return  _parent;  }
    void set_parent(Function *parent) { _parent = parent; }

    void accept(IValueVisitor &visitor) final;
private:
    int32_t   _index;
    Function *_parent;
}; // class Mutable final

} // namespace MYGL::IR
#endif
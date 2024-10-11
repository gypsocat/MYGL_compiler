#ifndef __MYGL_IR_INSTRUCTIONS_H__
#define __MYGL_IR_INSTRUCTIONS_H__

#include "base/mtb-classed-enum.hxx"
#include "base/mtb-object.hxx"
#include "ir-instruction-base.hxx"
#include "ir-basicblock.hxx"
#include "irbase-type.hxx"
#include "irbase-use-def.hxx"
#include "ir-constant.hxx"
#include "irbase-type-context.hxx"
#include "ir-constant-function.hxx"
#include <cstdint>
#include <array>
#include <vector>
#include <list>
#include <map>
#include <string_view>
#include <utility>

namespace MYGL::IR {

/** @class PhiSSA
 * @brief phi指令，一个汇合其他Value的指令. 类型就是操作数的类型.
 *
 * - 语法: `<id> = phi <type> [<value0>,<label0>], ...`
 * - 使用关系: PhiSSA会使用它的每一个操作数，所以你只能使用getValueFrom()与setValueFrom()来操作
 * Phi的参数列表.
 *
 * @warning 由于MYGL-IR中不允许出现隐式类型转换，Phi指令要求所有的操作数类型都相同。
 * @warning MYGL的Phi指令不允许「一个BasicBlock取得多个Value」的情况，每个BasicBlock
 *          至多对应一个Value. */
class PhiSSA final: public Instruction {
public:
    using BlockValueMapT  = std::map<BasicBlock*, ValueUsePair>;
    using BlockValuePair  = std::pair<BasicBlock*, owned<Value>>;
    using BlockValueListT = std::list<BlockValuePair>;
public:
    explicit PhiSSA(BasicBlock *parent, Type *value_type, size_t id = 0);
    ~PhiSSA() final;

    /** @property is_complete{get;} bool */
    DECLARE_BOOL_GETTER(is_complete)

    /** @property operands{get;access;} */
    DEFINE_TYPED_GETTER(BlockValueMapT const&, operands)
    DEFINE_ACCESSOR(BlockValueMapT, operands)

    /** @fn getValueFrom */
    Value *getValueFrom(BasicBlock *block) const noexcept(false) {
        return _operands.at(block).value;
    }
    /** @fn setValueFrom[block] = value
     * @brief 带有类型检查的BasicBlock set函数. */
    bool   setValueFrom(BasicBlock *block, owned<Value> value);
    /** @fn setValueFrom(Instruction)
     * @brief MYGL中有很多Phi的操作数都是从指令中来的，所以单独做了这个函数用来简化写法. */
    bool   setValueFrom(owned<Instruction> inst) {
        return setValueFrom(inst->get_parent(), inst);
    }
    /** @fn findIncomingBlock
     * @brief 找到参数所示的 Value 来自哪个 BasicBlock. */
    BasicBlock *findIncomingBlock(Value *value);

    /** @fn remove(BasicBlock) */
    owned<Value> remove(BasicBlock *block) noexcept(false);

    /** @fn operator[BasicBlock]
     * @warning 由于没有类型检查，该方法不安全 */
    owned<Value> &operator[](BasicBlock *block) noexcept(false) {
        return _operands.at(block);
    }

/* 继承自父类的方法与属性 */
    void accept(IValueVisitor &visitor)   final;
    size_t get_operand_nmemb() const final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
private:
    BlockValueMapT  _operands;

private: /* ==== [Signal Handler] ==== */
    void on_parent_finalize() final;
    void on_function_finalize() final;
}; // final class PhiSSA

/** @class UnreachableSSA
 * @brief 表示当前代码不可达
 * 
 * 使用关系: 未使用任何操作数 */
class UnreachableSSA final: public Instruction,
                            public IBasicBlockTerminator {
public:
    using RefT = owned<UnreachableSSA>;
public:
    explicit UnreachableSSA()
        : Instruction(ValueTID::UNREACHEBLE_SSA,
                      VoidType::voidty,
                      OpCode::UNREACHABLE),
          IBasicBlockTerminator(this) {
        MYGL_IBASICBLOCK_TERMINATOR_INIT_OFFSET();
    };
    ~UnreachableSSA() final;

public: /* overrides parent */
    void accept(IValueVisitor &visitor) final;

    size_t get_operand_nmemb() const final { return 0; }
    Value *operator[](size_t index)  final { return nullptr; }
private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize() final {}
    void on_function_finalize() final {}

public: /* overrides interface */
    void traverse_targets(TraverseFunc    fn) final { (void)fn; }
    void traverse_targets(TraverseClosure fn) final { (void)fn; }

    void remove_target(BasicBlock *target)    final { (void)target; }
    void remove_target_if(TraverseFunc    fn) final { (void)fn; }
    void remove_target_if(TraverseClosure fn) final { (void)fn; }
    void replace_target(BasicBlock *old, BasicBlock *new_target) final {
        (void)old; (void)new_target;
    }
    void replace_target_by(ReplaceClosure fn) final { (void)fn; }

    void clean_targets() final {}
}; // class UnreachableSSA final


/** @class JumpBase abstract
 * @brief 跳转指令的基类
 *
 * 使用关系: 使用default_target */
imcomplete class JumpBase: public Instruction,
                           public IBasicBlockTerminator {
public:
    /** @property default_target{get;set;} */
    DEFINE_TYPED_GETTER(BasicBlock*, default_target)
    DECLARE_SETTER(BasicBlock*, default_target)

    /** @property has_condition{get;default=true} bool */
    DECLARE_ABSTRACT_BOOL_GETTER(has_condition)

    /** @property condition_position{get;} abstract
     * @brief 获取条件变量所属的 `Use` 在 `list_as_user`
     *        的哪一项.
     * @return int32_t 值大于或等于0时, 表示条件所属的位置;
     *         值为负时, 表示有一个错误.
     *   @retval -1 没有条件属性. */
    abstract int32_t get_condition_position() const = 0;
protected:
    BasicBlock *_default_target;

    explicit JumpBase(BasicBlock *parent, ValueTID type_id, OpCode opcode, BasicBlock *default_target);
    explicit JumpBase(ValueTID type_id, OpCode opcode, BasicBlock *default_target)
        : JumpBase(nullptr, type_id, opcode, default_target){}

    void _register_target(BasicBlock *target);
    void _unregister_target(BasicBlock *target);
}; // imcomplete class JumpBase

/** @class JumpSSA final
 * @brief 无条件跳转指令
 *
 * - 语法: `jump label <dest>`
 * - 使用关系: 使用default_target */
class JumpSSA final: public JumpBase {
public:
    using RefT = owned<JumpSSA>;

    static RefT Create(BasicBlock *parent, BasicBlock *target);
public:
    explicit JumpSSA(BasicBlock *parent, BasicBlock *target);
    ~JumpSSA() final;

    /** @property target{get;set;} = default_target */
    BasicBlock *get_target() const { return get_default_target(); }
    void        set_target(BasicBlock *target) { set_default_target(target); }

public: /* overrides parent class */
    OVERRIDE_GET_FALSE(has_condition)

    /** @property condition_position{get => -1} final
     *  @brief `JumpSSA` 没有条件. */
    int32_t get_condition_position() const final { return -1; }

    void accept(IValueVisitor &visitor)   final;

    size_t get_operand_nmemb() const final { return 1; }
    Value *operator[](size_t index) final {
        return (index == 0)? _default_target: nullptr;
    }

public: /* override parent interface */
    void traverse_targets(TraverseFunc    fn) final { fn(_default_target); }
    void traverse_targets(TraverseClosure fn) final { fn(_default_target); }

    void remove_target(BasicBlock *target) final {
        if (target == _default_target)
            set_target(nullptr);
    }
    void remove_target_if(TraverseFunc fn) final {
        if (fn(_default_target))
            set_target(nullptr);
    }
    void remove_target_if(TraverseClosure fn) final {
        if (fn(_default_target))
            set_target(nullptr);
    }
    void replace_target(BasicBlock *old, BasicBlock *new_target) final {
        if (_default_target == old && old != new_target)
            set_default_target(new_target);
    }
    void replace_target_by(ReplaceClosure fn_terminates) final
    {
        BasicBlock *new_target = nullptr;
        if (fn_terminates(get_target(), new_target))
            return;
        if (new_target != nullptr && new_target != get_target())
            set_target(new_target);
    }

    void clean_targets() final;

private: /* ==== [Signal Handler] ==== */
    BasicBlock *on_parent_unplug() final;
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;
}; // class JumpSSA final

/** @class BranchSSA final
 * @brief 分支跳转指令
 *
 * - 语法: `br i1 <cond>, label <if true>, label <if false>`
 * - 使用列表: 两个条件所示的 `BasicBlock` 和 `condition` 所示的 `Value` */
class BranchSSA final: public JumpBase {
public:
    using RefT = owned<BranchSSA>;

    /** @fn Create(condition? if_true: if_false)
     * @brief 创建一个带条件 condition 的分支跳转语句, 条件满足时跳转到
     *        if_true, 条件不满足时跳转到 if_false
     * @param {condition} 类型必须是 i1, 否则报 `TypeMismatchException` 异常.
     * @param {if_true}   不能为空, 否则报 `NullException` 异常.
     * @param {if_false}  不能为空, 但是可以和 `if_true` 指向同样的基本块.
     *
     * @throws TypeMismatchException
     * @throws NullException          */
    static RefT Create(owned<Value> condition, BasicBlock *if_true, BasicBlock *if_false);
public:
    /** @brief 构造函数, 在 parent 基本块下创建一个带条件 condition 的分支
     *  跳转指令, 成功则跳转到 if_true, 失败则跳转到 if_false
     * @param {parent} 可以为空. 不为空时也仅仅表示填充一下父结点的位置. 填充
     *              操作可以等待 */
    MTB_NONNULL(4, 5)
    explicit BranchSSA(BasicBlock *parent,  owned<Value> condition,
                       BasicBlock *if_true, BasicBlock  *if_false);
    ~BranchSSA() final;

    /** @property if_false{get;set;} = default_target
     *  @brief 条件为假的时候取该分支. 注册为 `operands[0]` */
    BasicBlock *get_if_false() const { return get_default_target(); }
    void        set_if_false(BasicBlock *if_false) { set_default_target(if_false); }

    /** @property if_true{get;set;}
     *  @brief 条件为真的时候取该分支. 注册为 `operands[1]` */
    BasicBlock* get_if_true() const { return _if_true; }
    void        set_if_true(BasicBlock* value);
    
    /** @property condition{get;set;}
     *  @brief 条件, 要求类型为 i1. 注册为 `operands[2]`. */
    Value *get_condition() const { return _condition; }
    void   set_condition(owned<Value> value);

    /** @fn swapTargets()
     * @brief 交换两个跳转目标. 尽管 BasicBlock 上的 comes_from
     *        和 jumps_to 集合没有变化, 但是实际的 CFG 含义发生了
     *        改变. */
    void swapTargets();

    /* overrides parent */
    OVERRIDE_GET_TRUE(has_condition)

    /** @property condition_position{get => 2} final */
    int32_t get_condition_position() const final { return 2; }

    size_t get_operand_nmemb() const final { return 3; }
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;

    void accept(IValueVisitor &visitor) final;

public: /* override parent interface */
    void traverse_targets(TraverseFunc fn) final {
        fn(_default_target) || fn(_if_true);
    }
    void traverse_targets(TraverseClosure fn) final {
        fn(_default_target) || fn(_if_true);
    }

    void remove_target(BasicBlock *target) final {
        if (_default_target == target) set_default_target(nullptr);
        if (_if_true == target)        set_if_true(nullptr);
    }
    void remove_target_if(TraverseFunc fn) final {
        if (fn(_default_target)) set_default_target(nullptr);
        if (fn(_if_true))        set_if_true(nullptr);
    }
    void remove_target_if(TraverseClosure fn) final {
        if (fn(_default_target)) set_default_target(nullptr);
        if (fn(_if_true))        set_if_true(nullptr);
    }
    void replace_target(BasicBlock *old, BasicBlock *new_target) final;
    void replace_target_by(ReplaceClosure fn) final;

    void clean_targets() final;
private: /* ==== [Signal Handler] ==== */
    BasicBlock *on_parent_unplug() final;
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;
private:
    BasicBlock   *_if_true;
    owned<Value>  _condition;
}; // class BranchSSA final

/** @class SwitchSSA final
 * @brief 表跳转指令，类型为void.
 *
 * 根据整数常量跳转到对应的BasicBlock.
 * - 语法:
 * ```
 * switch i32 <condition>, label <default> [  
 *     i32 <case0>, label <target0>
 *     i32 <case1>, label <target1>  
 *     ...  
 * ]  
 * ```
 * - 使用关系: 使用condition以及所有的跳转标签 */
class SwitchSSA final: public JumpBase {
public:
    struct CaseBlockPair {
        int64_t     case_number;
        BasicBlock *target;
    }; // struct CasePair

    struct BlockUsePair {
        BasicBlock *target;
        Use    *target_use;
        operator BasicBlock*() const {
            return target;
        }
        operator BasicBlock*&() {
            return target;
        }
    }; // struct BlockUsePair
    using CaseListT = std::list<CaseBlockPair>;
    using CaseMapT  = std::map<int64_t, BlockUsePair>;

    /** @struct case_proxy
     * @brief SwitchSSA元素的代理结点。使用这个结构体可以假装把
     *        SwitchSSA变成一个包含所有case的集合。 */
    struct case_proxy {
        SwitchSSA *instance;
        int64_t    case_number;

        operator BasicBlock*() {
            return instance->getCase(case_number);
        }
        void operator=(BasicBlock *block) {
            if (block == nullptr)
                instance->removeCase(case_number);
            else
                instance->setCase(case_number, block);
        }
        void operator=(std::nullptr_t) {
            instance->removeCase(case_number);
        }
    }; // struct proxy

    /** @struct BlockCaseIterator */
    struct BlockCaseIterator {
        using IterT = CaseMapT::iterator;
    public:
        CaseMapT *map;
        IterT    iter;
    public:
        bool   ends() const { return iter == map->end(); }

        int64_t get() const {
            if (iter == map->end()) return 0;
            return iter->first;
        }
        Use *get_use() const {
            if (iter == map->end()) return 0;
            return iter->second.target_use;
        }
        BasicBlock *get_basic_block() const {
            if (iter == map->end())
                return nullptr;
            return iter->second.target;
        }

        int64_t get_next();
        int64_t get_next(bool &out_ends);
    }; // struct BlockCaseIterator

    using RefT = owned<SwitchSSA>;
    static RefT Create(owned<Value> condition, BasicBlock *default_target);

    static RefT CreateWithCases(owned<Value> condition, BasicBlock *default_tgt,
                                CaseListT const& cases);

public:
    explicit SwitchSSA(BasicBlock *parent, owned<Value> condition, BasicBlock *default_target);

    [[deprecated("该函数不是异常安全的, 请替换为 SwitchSSA:: CreateWithCases() 工厂函数.")]]
    explicit SwitchSSA(BasicBlock *parent,         owned<Value> condition,
                       BasicBlock *default_target, CaseListT const &cases);
    ~SwitchSSA() final ;
    
    /** @property cases{get;access;} */
    CaseMapT const &get_cases() const { return _cases; }
    CaseMapT &      cases()           { return _cases; }
    
    /** @property condition{get;set;} owned */
    Value *get_condition() const { return _condition; }
    void set_condition(owned<Value> condition);

    /* Case getter/setter/removal/indexing */

    /** @fn getCase[i64 case_number]
     * @brief 根据条件整数索引得到跳转目标. 该方法把 `SwitchSSA` 视为
     *        一个无穷大的 `case-target` 对组成的集合.
     * @param case_number 表示条件的整数, 是一个 `int64` 整数.
     * @return BasicBlock 返回 `case` 对应的跳转目标. 倘若跳转目标不
     *         存在, 则返回 `default_target`. */
    BasicBlock *getCase(int64_t case_number) const;

    /** @fn getCaseUse[i64 case_number]
     * @brief 返回索引为 `case_number` 的跳转目标所属的 `Use`. 该方法
     *        把 `switch` 语句看成一个跳转目标有限集合.
     * @retval nullptr `cases` 集合中没有索引 `case_number` 时, 直接
     *         返回 null. */
    Use *getCaseUse(int64_t case_number) const;

    /** @fn setCase[i64 case_number] = block
     * @brief 把条件整数索引 `case_number` 处的跳转目标设为 `block`.
     *        倘若 `cases` 集合里有 `case_number` 对应的结点, 则设置
     *        跳转目标为 `block`; 否则, 插入一个新结点.
     * @throw MTB::NullException 当传入参数为 `null` 时丢出该异常.
     *
     * @note  为保证语义不随 `default_target` 的修改而变化, 保护
     *        `use-def` 关系不失效, 该方法不接受 null 作为跳转目标参数
     *        , 更不会删除任何已经注册进结点的 `case`.
     * @note  传入的目标结点等于 `default_target` 时, 该方法不会做特殊
     *        处理, 插入结点或者读写结点的操作与一般情况等价. */
    bool setCase(int64_t case_number, BasicBlock *block);
    bool removeCase(int64_t case_number);

    /** @fn caseAt[case_number]
     * @brief 跳转条件索引函数, 该方法把 `SwitchSSA` 视为一个无穷大的
     *        `case-target` 对组成的集合.
     * @param case_number 表示条件的整数, 是一个 `int64` 整数.
     * @return case_proxy 代理结构体. 对该结构体进行的任何赋值操作,
     *         都会被当成 `setCase()` 处理; 倘若把该结构体赋值给
     *         `BasicBlock`, 则会调用 `getCase[]` 方法. */
    case_proxy caseAt(int64_t case_number) noexcept {
        return {this, case_number};
    }

    /** @fn findCaseCondition[jump_target]
     * @brief 获取参数所示的跳转目标对应哪些 `case`.
     *
     * @return `BlockCaseIterator`: 一个 `BasicBlock` 序列生成器,该
     *         生成器生成的每个 `int64_t` 值都是 `jump_target` 对应
     *         的 `case`. 你可以调用它的 `get()` 方法查看当前所在的
     *         case, 调用 get_next() 方法往前步进一格, 调用 `ends()`
     *         方法查看遍历是否结束. */
    BlockCaseIterator findCaseCondition(BasicBlock *jump_target);

/* overrides parent */
    OVERRIDE_GET_TRUE(has_condition)

    /** @property condition_position{get => 1} final */
    int32_t get_condition_position() const final { return 1; }

    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final;

public: /* override parent interface */
    void traverse_targets(TraverseFunc    fn) final;
    void traverse_targets(TraverseClosure fn) final;

    void remove_target(BasicBlock *target) final;
    void remove_target_if(TraverseFunc fn) final;
    void remove_target_if(TraverseClosure fn) final;

    void replace_target(BasicBlock *old, BasicBlock *new_target) final;
    void replace_target_by(ReplaceClosure fn) final;

    void clean_targets() final;
private: /* ==== [Signal Handler] ==== */
    BasicBlock *on_parent_unplug() final;
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;
private:
    CaseMapT     _cases;
    owned<Value> _condition;
}; // class SwitchSSA final

/** @class BinarySelectSSA final
 *  @brief 二分选择指令, 是二分跳转指令的泛化/加速版本
 *
 * - 语法: `%result = select i1 <cond>, <rty> <if true>, <rty> <if false>`
 * - 使用关系: if_false, if_true, condition
 * - 返回类型: if_false 和 if_true 的类型. 要求 if_false 和 if_true 类型相同. */
class BinarySelectSSA final: public Instruction {
public:
    using RefT        = owned<BinarySelectSSA>;
    using OperandRefT = owned<Value>;
    enum class OperandOrder: uint8_t {
        IF_FALSE  = 0,
        IF_TRUE   = 1,
        CONDITION = 2,
        RESERVED_FOR_COUNTING
    }; // enum class OperandOrder
    

    /** @fn Create(condition, if_true, if_false)
     * @brief 检查参数并构造一个 BinarySelectSSA 指令结点.
     * @throws NullException 当三个参数至少有一个为 null 时抛出该异常.
     * @throws TypeMismatchException 当 condition 不是布尔(i1)值时抛出该异常.
     * @throws TypeMismatchException 当 if_true 和 if_false 类型不同时抛出该异常. */
    static RefT Create(OperandRefT condition, OperandRefT if_true, OperandRefT if_false);
public:
    /** @fn constructor(result_type, condition, if_true, if_false)
     * @brief 构造一个BinarySelectSSA 指令结点, 但是不检查参数, 不异常安全. */
    explicit BinarySelectSSA(Type   *result_type, OperandRefT condition_i1,
                             OperandRefT if_true, OperandRefT if_false);
    ~BinarySelectSSA() final;

    /** @property if_false{get;set;} = [0] */
    Value *get_if_false() const { return _if_false.get(); }
    /** @fn set_if_false(Value if_false)
     * @see property: if_false
     * @brief 要求 if_false 要么为 null, 要么类型和该指令的 value_type 相同. */
    void   set_if_false(OperandRefT if_false);

    /** @property if_true{get;set;}  = [1] */
    Value *get_if_true() const { return _if_true.get(); }
    /** @fn set_if_true(Value if_true)
     * @see property: if_true
     * @brief 要求 if_true 要么为 null, 要么类型和该指令的 value_type 相同. */
    void   set_if_true(OperandRefT if_true);

    /** @property condition{get;set;} = [2] */
    Value *get_condition() const { return _condition.get(); }
    void   set_condition(OperandRefT condition);

public:  /* ================ [overrides Instruction] ================ */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final {
        return size_t(OperandOrder::RESERVED_FOR_COUNTING);
    }
private: /* ================ [overrides signal] ================ */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;
private:
    OperandRefT _if_false;  // [0]
    OperandRefT _if_true;   // [1]
    OperandRefT _condition; // [2]
}; // class BinarySelectSSA final

/** @class AllocaSSA
 * @brief 栈内存申请指令，类型为指向申请的元素的指针。
 *
 * - 语法:
 *   - `<id> = alloca <type>`
 *   - `<id> = alloca <type>, align <align>`
 *   <下面的指令目前不可用>
 *   - `<id> = alloca <type>, <nmemb_type> <nmemb>`
 *   - `<id> = alloca <type>, <nmemb_type> <nmemb>, align <align>`
 * - 使用关系: 因为不支持变长数组，所以没有使用任何 Value.
 * - 指令返回类型: 指向 `type` 的指针类型. */
class AllocaSSA final: public Instruction {
public:
    using RefT = owned<AllocaSSA>;

    /** @fn CreateAutoAligned(Type)
     * @brief 创建一条 `alloca` 指令, 该指令会申请一段类型为 `Type`, 字节对齐方式默认
     *        的内存单元. */
    static RefT CreateAutoAligned(Type *element_type);

    static always_inline RefT
    Create(Type *element_type) {
        return CreateAutoAligned(element_type);
    }

    /** @fn CreateWithAlign(TypeContext, Type, align)
     * @brief 创建一条 `alloca` 指令, 该指令会申请一段类型为 `Type`, 字节对齐方式为
     *        `align` 的内存单元. */
    static RefT CreateWithAlign(TypeContext &ctx, Type *element_type, size_t align);
public:
    /** @fn AllocaSSA(TypeContext, Type, align)
     * @brief 创建一条 `alloca` 指令, 该指令会申请一段类型为 `Type`, 字节对齐方式为
     *        `align` 的内存单元. */
    explicit AllocaSSA(PointerType *value_type, size_t align);

    ~AllocaSSA() final;

    /** @property align{get;set;} */
    size_t get_align() const { return _align; }
    void   set_align(size_t value);

    /** @property element_type{get;} */
    Type* get_element_type() const;

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final {
        (void)index; return nullptr;
    }
    size_t get_operand_nmemb() const final { return 0; }

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

private:
    size_t _align;
}; // class AllocaSSA final

/** @class UnarySSA
 * @brief 单操作数指令
 *
 * - 使用关系: 所有的单操作数指令只有一个操作数(废话), 只有这个操作数会被加入列表 */
imcomplete class UnarySSA: public Instruction {
public:
    /** @property operand{get;set;} = operands[0]
     * @brief 操作数.
     * 注意, set_operand不仅要维护使用关系, 还要调用checkOperand()检查操作数是否合格. */
    Value *get_operand() const { return _operand; }
    bool   set_operand(owned<Value> value);

/* overrides parent */
    Value *operator[](size_t index) final {
        return index == 0 ? get_operand(): nullptr;
    }
    void traverseOperands(ValueAccessFunc fn) final {
        fn(*get_operand());
    }
    size_t get_operand_nmemb() const final { return 1; }
    size_t replaceAllUsee(Value *from, Value *to) final {
        if (_operand == from) {
            set_operand(to); return 1;
        }
        return 0;
    }
protected:
    owned<Value> _operand;

    explicit UnarySSA(ValueTID type_id, Type     *value_type,
                      OpCode   opcode,  owned<Value> operand);

    /** @fn checkOperand(owned<Value>)
     * @brief 检查操作数是否合格.你需要在子类中实现这个虚方法.
     * @return true表示检查通过, false表示检查不通过. */
    abstract bool checkOperand(Value *operand) = 0;
}; // imcomplete class UnarySSA

/** @class CastSSA
 * @brief 类型转换指令.
 *
 * - 使用关系: 见UnarySSA.
 * - 语法: `<id> = opcode <operand type> <operand> to <target type>`
 * - 详细语法: 见各种Cast结点的工厂函数 */
class CastSSA final: public UnarySSA {
public:
    using RefT = owned<CastSSA>;
    using CastCreationFunc = std::function<owned<CastSSA>(Type*, Value*)>;
public:
    static OpCode GetCastType(Type *src, Type *dst, bool dst_unsigned = false);

    /** @fn CreateIToFCast
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的类型不是整数时抛出该异常.
     * - 语法: `<id> = itof <int type> <operand> to <float type>` */
    static RefT CreateIToFCast(FloatType *target, owned<Value> operand);

    /** @fn CreateUToFCast
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的类型不是整数时抛出该异常.
     * - 语法: `<id> = uitofp <int type> <operand> to <float type>` */
    static RefT CreateUToFCast(FloatType *target, owned<Value> operand);

    /** @fn CreateFToICast
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的类型不是整数时抛出该异常.
     * - 语法: `<id> = ftoi <float type> <operand> to <int type>` */
    static RefT CreateFToICast(IntType *target, owned<Value> operand);

    /** @fn CreateZExtCast
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的类型不是整数, 或者当 `operand`
     *         的二进制位数大于 `target` 时抛出该异常.
     * @brief 零扩展指令. 该指令把int当成无符号看待，扩展为目标类型并在高位填0。
     * - 语法: `<id> = zext <int type> <operand> to <int type>` */
    static RefT CreateZExtCast(IntType *target, owned<Value> operand);

    /** @fn CreateSExtCast
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的类型不是整数, 或者当 `operand`
     *         的二进制位数大于 `target` 时抛出该异常.
     * @brief 符号扩展指令. 该指令把int当成有符号看待，扩展为目标类型并在高位填符号。
     * - 语法: `<id> = sext <int type> <operand> to <int type>` */
    static RefT CreateSExtCast(IntType *target, owned<Value> operand);

    /** @fn CreateTruncCast
     * @brief 位裁切指令. 该指令把int缩减到目标类型, 高位舍去.
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的类型不是整数, 或者当 `operand`
     *         的二进制位数小于 `target` 时抛出该异常.
     * - 语法: `<id> = trunc <int type> <operand> to <int type>` */
    static RefT CreateTruncCast(IntType *target, owned<Value> operand);

    /** @fn CreateBitCast
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的二进制位数与 `target` 的不等时
     *         抛出该异常.
     * @brief 等大小重解释转换. 要求两个类型的二进制位相同. 常用于两个指针之间的转换
     * - 语法: `<id> = bitcast <old type> <operand> to <new type>` */
    static RefT CreateBitCast(Type *target, owned<Value> operand);

    /** @fn CreateFpExtCast
     * @brief 小浮点变大浮点转换.
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的类型不是浮点, 或者当 `operand`
     *         的二进制位数大于 `target` 时抛出该异常.
     * - 语法: `<id> = fpext <small float type> <operand> to <big float type> */
    static RefT CreateFpExtCast(FloatType *target, owned<Value> operand);

    /** @fn CreateFpTruncCast
     * @brief 大浮点变小浮点转换.
     * @throws NullException 参数为 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `operand` 的类型不是浮点, 或者当 `operand`
     *         的二进制位数小于 `target` 时抛出该异常.
     * - 语法: `<id> = fptrunc <big float type> <operand> to <small float type> */
    static RefT CreateFpTruncCast(FloatType *target, owned<Value> operand);

    /** @fn CreateTruncOrExtCast
     * @brief 工具函数, 通过传入的类型决定要进行怎样的同类型转换
     * @param target  转换的目标类型, 不得为 `null`
     * @param operand 操作数, 「基础类型」或者「二进制位数」至少有一个与 `target` 一致
     * @param keeps_value 整数间转换时表示要不要把整数当成有符号的, 浮点数转换时表示要不要
     *        保证浮点的值不变(反义是保证二进制表示不变), 做 `bitcast` 时无意义
     * @return 创建好的 `cast` 指令. 倘若函数返回了值，则执行总是成功的; 倘若是同类型互转
     *         , 则返回 `null`.
     * @throws NullException 「操作数」或「目标类型」至少有一个是 `null`.
     * @throws TypeMismatchException 操作数类型与目标类型不符合要求 (具体要求见 see 注释
     *         章节中的 Ext 或 Trunc 工厂函数), 或者转换不能一步完成.
     * @see CreateZExtCast
     * @see CreateSExtCast
     * @see CreateTruncCast
     * @see CreateBitCast
     * @see CreateFpExtCast
     * @see CreateFpTruncCast */
    static RefT CreateTruncOrExtCast(Type *target, owned<Value> operand, bool keeps_value);

    /** @fn CreateCheckedCast
     * @brief  根据 opcode 检查参数所示的类型转换目标和操作数. 倘若都合格, 则根据 `opcode`
     *         创建合适的类型转换指令. 该函数会根据 opcode 的内容选择上面的 Create 函数之一
     *         进行实际的创建操作.
     * @throws OpCodeMismatchException 当操作码不是类型转换操作时丢出该异常.
     * @throws NullException           当参数中有 `null` 时抛出该异常.
     * @throws TypeMismatchException   由该方法或者它选择的方法抛出, 在类型不符合 `opcode`
     *                                 规定的要求时会丢出该异常. */
    static RefT CreateCheckedCast(OpCode  opcode,
                                            Type   *target,
                                            owned<Value> operand);
public:
    explicit CastSSA(OpCode opcode, Type *target_type, owned<Value> operand);

    /** @property cast_mode{get;}
     * @brief 由OpCode推到得出，表示这种类型转换应当具有什么性质。 */
    IRBase::CastMode get_cast_mode() const;

    /** @property keeps_value{get;}
     * @brief 是否保持原来的字面量不变或者相近 */
    bool keeps_value() const;

    /** @property keeps_binary
     * @brief 是否保持原来的二进制布局不变或者相近 */
    bool keeps_binary() const;

    /** @property keeps_sign
     * @brief 是否保持原来的符号不变 */
    bool keeps_sign() const;

    /** @property new_type{get;} */
    Type *get_new_type() const { return get_value_type(); }

    void accept(IValueVisitor &visitor) final;
private:
    /** 检查operand的类型是否与opcode的要求相符 */
    bool checkOperand(Value *operand) final;

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;
}; // class CastSSA final

/** @class LoadSSA final
 * @brief 从指针指向的内存单元加载一个值. 类型为指针的目标.
 *
 * - 语法: `<id> = load <type>, <operand pointer type> <operand>, align <align>`
 * - 使用关系: 与UnarySSA相同, 只使用一个操作数 */
class LoadSSA final: public UnarySSA {
public:
    /** @fn LoadSSA(Value operand)
     * @brief 从指针变量创建一个 `LoadSSA` 指令结点. 不论指针变量实际代表什么(甚至是
     *        有额外对齐要求的 `alloca`), 其对齐要求就是操作数类型的对齐要求.
     * @throws TypeMismatchException 当操作数的类型不是指针类型, 或者操作数类型是
     *         `void*`时, 抛出该异常. */
    explicit LoadSSA(owned<Value> pointer_operand);

    /** @fn LoadSSA(operand, align)
     * @brief 根据参数所示的额外对齐要求, 从指针变量 `operand` 创建一个 `LoadSSA`
     *        指令结点.
     * @throws TypeMismatchException 当操作数的类型不是指针类型, 或者操作数类型是
     *         `void*`时, 抛出该异常. */
    explicit LoadSSA(owned<Value> pointer_operand, size_t align);

    void accept(IValueVisitor &visitor) final;

    /** @property align{get;set;} */
    size_t get_align() const { return _align; }

    /** @fn set_align{ = size_t }
     * @see align property{get;set;}
     * @note 对齐条件必须是2的次方倍, 否则 LoadSSA 会向上取整为2的次方倍.
     *       例如:
     * - `set_align(8)`  -> `get_align() = 8`
     * - `set_align(12)` -> `get_align() = 16`
     * - `set_align(0)`  -> `get_align() = 0` */
    void   set_align(size_t value);

    /** @property source_pointer_type{get;} */
    PointerType *get_source_pointer_type() const {
        return _source_pointer_type;
    }

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

private:
    /** 检查operand的类型是否为指针类型, 且与自己的相等 */
    bool checkOperand(Value *operand) final;

    PointerType *_source_pointer_type;
    size_t       _align;
}; // class LoadSSA final

/** @class UnaryOperationSSA final
 * @brief 单操作数标量处理指令, 类型与操作数的类型相同.
 *
 * - 语法: <id> = <opcode> <operand type> <operand>
 * - 使用关系: 见UnarySSA */
class UnaryOperationSSA final: public UnarySSA {
public:
    using UPtrT = owned<UnaryOperationSSA>;

    /** @fn CreateINeg
     * @brief ineg 指令: 整数取负. 要求 Value 是整数类型.
     * @throws NullException
     * @throws TypeMismatchException */
    static UPtrT CreateINeg(owned<Value> operand);

    /** @fn CreateFNeg
     * @brief fneg 指令: 浮点数取负. 要求 Value 是浮点数类型.
     * @throws NullException
     * @throws TypeMismatchException */
    static UPtrT CreateFNeg(owned<Value> operand);

    /** @fn CreateNot
     * @brief not 指令: 整数按位取反. 要求 Value 是整数类型.
     * @throws NullException
     * @throws TypeMismatchException */
    static UPtrT CreateNot(owned<Value> operand);

    /** @fn CreateChecked
     * @brief 根据 `opcode`, 调用其他 `UnaryOperationSSA::Create` 函数
     *        创建一个 `UnaryOperationSSA` 结点.
     * @throws NullException
     * @throws TypeMismatchException */
    static UPtrT CreateChecked(OpCode opcode, owned<Value> operand);
public:
    explicit UnaryOperationSSA(OpCode opcode, owned<Value> operand);

    void accept(IValueVisitor &visitor) final;
private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

protected:
    /** 检查operand的类型是否与自己的相等 */
    bool checkOperand(Value *operand) final;
}; // class UnaryOperationSSA final

/** @class MoveInst
 *  @brief 值拷贝指令. 一旦 IR 中存在该指令, 那么 IR 就不是 SSA 的.
 *
 * - 语法: `move <type> %id(r<mutable index>), %<source>`
 * - 使用关系: move 指令只会出现在优化的 `ModuleStage::NON_SSA` 阶段, 没有 `use-def`
 *            关系. 这时, 你不能通过 `list_as_user` 属性获取操作数.  */
class MoveInst final: public Instruction {
public:
    using RefT = owned<MoveInst>;

    /** @fn Create
     * @brief 创建一个父结点为 parent, 目标寄存器为 mut 的 move 指令结点.
     * @return owned<MoveInst> 创建好的 Move 指令, 恒不为 null.
     *
     * @throws NullException         当三个参数至少有一个是 null 时抛出该异常.
     * @throws TypeMismatchException 当 operand 类型和 mut 类型不一致时抛出该异常.
     * @throws Exception        当 mut 的父结点和 parent 的父结点不一致时抛出该异常. */
    static RefT Create(Mutable *mut, BasicBlock *parent, owned<Value> operand);

    /** @fn CreateWithNewMutable
     * @brief 创建一个父结点为 parent 的 move 指令结点, 并为该结点分配一个
     *        没有用的 Mutable 寄存器.
     * @return owned<MoveInst> 创建好的 Move 指令, 恒不为 null.
     * @throws NullException 当参数至少有一个是 null 时抛出该异常. */
    static RefT CreateWithNewMutable(BasicBlock *parent, owned<Value> operand);
public:
    explicit MoveInst(Mutable *mut, BasicBlock *parent, owned<Value> operand);
    ~MoveInst() final;

    /** @property mutable{get;set;} */
    Mutable *get_mutable() const   { return _mutable; }
    /** @fn set_mutable(mut)
     * @brief 要求 mut 的类型和自身类型一致、父结点和自身一致. */
    void set_mutable(Mutable *mut);

    /** @property operand{get;set;} */
    Value *get_operand() const { return _operand; }

public:  /* ================ [overrrides parent] ================ */
    void   accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    size_t get_operand_nmemb() const final { return 2; }

private: /* ================ [overrrides signal] ================ */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;
private:
    Mutable      *_mutable;
    owned<Value>  _operand;
}; // class MoveInst final

/** @class BinarySSA
 * @brief 双操作数指令
 *
 * - 使用关系: 该指令有两个操作数，BinarySSA会使用它们.
 * - 语法:
 *   - `<ret> = <opcode> <type> <op1>, <op2>`
 *   - `<ret> = <opcode> <flag> <type> <op1>, <op2>` */
class BinarySSA final: public Instruction {
public:
    enum class SignFlag: uint8_t {
        NONE, NSW, NUW
    }; // enum class SignFlag

    // BinarySSA 默认智能引用
    using RefT = owned<BinarySSA>;
    using OperandCheckClosure = std::function<bool(BinarySSA*, Value*)>;
    using OperandCheckFunc    = bool(*)(BinarySSA*, Value*);

    friend OperandCheckFunc;
    friend OperandCheckClosure;
public: /** 工厂函数 */
    /** @fn CreateAdd
     * - 语法: `add <flag> <type> <lhs>, <rhs>` */
    [[nodiscard("new add instruction")]]
    static RefT CreateAdd(owned<Value> lhs, owned<Value> rhs, SignFlag sign_flag);

    /** @fn CreateSub
     * - 语法: `sub <flag> <type> <lhs>, <rhs>` */
    [[nodiscard("new sub instruction")]]
    static RefT CreateSub(owned<Value> lhs, owned<Value> rhs, SignFlag sign_flag);

    /** @fn CreateMul
     * - 语法: `mul <flag> <type> <lhs>, <rhs>` */
    static RefT CreateMul(owned<Value> lhs, owned<Value> rhs, SignFlag sign_flag);
    
    /** @fn CreateIDiv
     * - 语法: `sdiv/udiv <flag> <int type> <lhs>, <rhs>` */
    static RefT CreateIDiv(owned<Value> lhs, owned<Value> rhs, SignFlag sign_flag);
    static RefT CreateIDiv(owned<Value> lhs, owned<Value> rhs, bool as_signed);

    static RefT CreateSDiv(owned<Value> lhs, owned<Value> rhs) {
        return CreateIDiv(std::move(lhs), std::move(rhs), true);
    }
    static RefT CreateUDiv(owned<Value> lhs, owned<Value> rhs) {
        return CreateIDiv(std::move(lhs), std::move(rhs), false);
    }

    /** @fn CreateFDiv
     * - 语法: `fdiv <flag> <float type> <lhs>, <rhs>` */
    static RefT CreateFDiv(owned<Value> lhs, owned<Value> rhs);

    /** @fn CreateSRem
     * - 语法: `srem <int type> <lhs>, <rhs>` */
    static RefT CreateSRem(owned<Value> lhs, owned<Value> rhs) {
        return CreateIRem(std::move(lhs), std::move(rhs), true);
    }

    /** @fn CreateURem
     * - 语法: `srem <int type> <lhs>, <rhs>` */
    static RefT CreateURem(owned<Value> lhs, owned<Value> rhs) {
        return CreateIRem(std::move(lhs), std::move(rhs), false);
    }

    /** @fn CreateURem
     * - 语法: `srem/urem <int type> <lhs>, <rhs>` */
    static RefT CreateIRem(owned<Value> lhs, owned<Value> rhs, bool as_signed);

    /** @fn CreateURem
     * - 语法: `srem <int type> <lhs>, <rhs>` */
    static RefT CreateFRem(owned<Value> lhs, owned<Value> rhs);

    /** @fn CreateAnd
     * - 语法: `and <int type> <lhs>, <rhs>` */
    static RefT CreateAnd(owned<Value> lhs, owned<Value> rhs);

    /** @fn CreateOr
     * - 语法: `or <int type> <lhs>, <rhs>` */
    static RefT CreateOr(owned<Value> lhs, owned<Value> rhs);

    /** @fn CreateXor
     * - 语法: `xor <int type> <lhs>, <rhs>` */
    static RefT CreateXor(owned<Value> lhs, owned<Value> rhs);

    /** @fn CreateShL
     * @brief 左移指令, 把lhs与rhs视为无符号数.
     * - 语法: `shl <int type> <lhs>, <rhs>`
     * - 注意: 移位指令只要求lhs和rhs都是整数, 但是不要求二者类型相同.
     *         移位指令的返回类型是lhs的类型。 */
    static RefT CreateShL(owned<Value> lhs, owned<Value> rhs);

    /** @fn CreateLShR
     * @brief 逻辑右移指令, 把lhs与rhs视为无符号数.
     * - 语法: `lshr <int type> <lhs>, <rhs>`
     * - 注意: 移位指令只要求lhs和rhs都是整数, 但是不要求二者类型相同.
     *         移位指令的返回类型是lhs的类型。 */
    static RefT CreateLShR(owned<Value> lhs, owned<Value> rhs);

    /** @fn CreateAShR
     * @brief 算术右移指令, 把lhs视为有符号数, rhs视为无符号数.
     * - 语法: `ashr <int type> <lhs>, <rhs>`
     * - 注意: 移位指令只要求lhs和rhs都是整数, 但是不要求二者类型相同.
     *         移位指令的返回类型是lhs的类型。 */
    static RefT CreateAShR(owned<Value> lhs, owned<Value> rhs);

    /** @fn Create
     * @brief 根据lhs与rhs的类型自动匹配合适的OpCode,然后做BinarySSA Creation. */
    static RefT Create(OpCode opcode, owned<Value> lhs, owned<Value> rhs,
                       bool int_as_signed = false);

    static std::string_view SignFlagGetString(SignFlag flag);
public:
    explicit BinarySSA(OpCode opcode, Type *value_type, SignFlag sign_flag,
                       owned<Value> lhs, owned<Value> rhs);
    ~BinarySSA() final;

    /** @property lhs{get;set;} */
    Value *get_lhs() const { return _operands[0]; }
    bool   set_lhs(owned<Value> lhs);

    /** @property rhs{get;set;} */
    Value *get_rhs() const { return _operands[1]; }
    bool   set_rhs(owned<Value> rhs);

    /** @property sign_flag{get;set;} */
    SignFlag get_sign_flag() const         { return  _sign_flag; }
    void     set_sign_flag(SignFlag value) { _sign_flag = value; }

    bool trySwapOperands() {
        if (!get_opcode().is_swappable())
            return false;
        std::swap(_operands[0], _operands[1]);
        return true;
    }

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;

    size_t get_operand_nmemb() const final { return 2; }
private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent);
    void on_parent_finalize()   final;
    void on_function_finalize() final;

private:
    owned<Value> _operands[2];
    SignFlag     _sign_flag;

    bool _checkLhs(Value *lhs);
    bool _checkRhs(Value *rhs);
}; // class BinarySSA final

/** @class CallSSA final
 * @brief 调用指令. 类型为函数返回值的类型.
 *
 * - 使用关系: 使用被调用的函数与所有参数
 * - 语法:
 * > <id> = call <return type> <function name> ([Argument List])
 * > Argument List: <arg1type> <arg1>, ... */
class CallSSA final: public Instruction {
public:
    struct ArgUseTuple {
        owned<Value> arg;
        Type        *arg_type;
        Use         *arg_use;
    }; // struct ArgUseTuple
    using ArgArrayT = std::vector<ArgUseTuple>;
public:
    explicit CallSSA(owned<Function> callee, ValueArrayT const &arguments);

    /** @property callee{get;set;} owned */
    Function *get_callee() const { return _callee; }
    void      set_callee(owned<Function> value);

    /** @property arguments{get;access;} */
    ArgArrayT const &get_arguments() const { return _arguments; }
    ArgArrayT &      arguments()           { return _arguments; }

    /** @fn getArgument(index) const
     * @throw std::out_of_range 当 index 超出界限时丢出该异常. */
    const Value *getArgument(size_t index) const {
        return get_arguments().at(index).arg;
    }
    /** @fn setArgument(index, Value)
     * @brief 设置参数, 检查参数类型, 同时处理使用关系
     * @throw std::out_of_range 当 index 超出界限时丢出该异常. */
    void setArgument(size_t index, owned<Value> argument);

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final;

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

private:
    owned<Function> _callee;
    ArgArrayT     _arguments;
}; // class CallSSA final

/** @class GetElemPtrSSA final
 * @brief 获取数组或指针值的索引. 返回类型在构造时决定.
 *
 * - 语法: `<id> = getelementptr <type>, ptr <collection>, <ty1> <index1>, ...`
 * - 使用关系: 使用collection与所有下标值
 *
 * 为了与LLVM兼容, 索引表达式的第一个index是对指针解索引. 由于MYGL-IR不支持变长数组,
 * 索引表达式的第一个参数一直是0. */
class GetElemPtrSSA final: public Instruction {
public:
    using TypeLayerStackT = std::vector<Type*>;
    using RefT = owned<GetElemPtrSSA>;

    static RefT CreateFromPointer(owned<Value> ptr_collection, ValueArrayT const& indexes);
public:
    explicit GetElemPtrSSA(PointerType *value_type, owned<Value> collection, ValueArrayT const &indexes);
    explicit GetElemPtrSSA(TypeContext &ctx,        Type *elem_type,
                           owned<Value> collection, ValueArrayT const &indexes)
        : GetElemPtrSSA(ctx.getPointerType(elem_type), std::move(collection), indexes) {}

    /** @property collection{get;set;} owned
     * @brief 被索引的集合值. 要求类型为指针, 索引表达式的第一层也是解这个指针的索引.
     * - 修改时: 倘若collection的类型发生了变化, 那么需要重新计算索引列表的长度.
     *          我(@medihbt)认为应当禁止类型出现变化的情况. */
    Value *get_collection() const { return _collection; }
    /** @see get_collection()
     * @throw TypeMismatchException 参数的类型和属性类型不等时抛出该异常. */
    void   set_collection(owned<Value> collection);

    /** @property indexes{get;access;} */
    ValueUseArrayT const& get_indexes() const { return _indexes; }
    ValueUseArrayT      & indexes()           { return _indexes; }

    /** @property collection_type{get;}
     *  @brief 集合指针类型的目标类型 */
    Type *get_collection_type() const;

    /** @property target_type{get;}
     *  @brief 索引目标类型的指针类型 */
    PointerType *get_target_type() const {
        return static_cast<PointerType*>(get_value_type());
    }

    /** @property elem_type{get;} */
    Type *get_elem_type() const {
        return get_target_type()->get_target_type();
    }

    Value const* getIndex(size_t index_order) const;
    Value      * getIndex(size_t index_order);
    bool setIndex(size_t index_order, owned<Value> index);
    uint32_t countIndex() const { return _indexes.size(); }

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final;

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

private:
    owned<Value>     _collection;

    /* 索引数组. 要求每个元素的类型都是整数类型. */
    ValueUseArrayT      _indexes;

    /* 逐级展开栈. 第0级为集合类型, 每往上一级都是下一级的元素/指向目标,
     * 最上层是目标类型(目标指针类型的指向目标) */
    TypeLayerStackT _layer_stack;
}; // class GetElemPtrSSA final

/** @class ExtractElemSSA
 *  @brief 取出数组 array 中以 index 为索引的元素
 *
 * - 语法: `extractelement <type> <aggr type> <array>, <int type0> index`
 * - 操作数顺序:
 *  - [0]={array} 数组元素
 *  - [1]={index} 下标
 * - 返回值: 数组 array 的容器元素类型, 表示取得的元素 */
class ExtractElemSSA final: public Instruction {
public:
    using RefT       = owned<ExtractElemSSA>;

    /** @fn Create(array[index])
     * @brief 创建一个从数组 array 中取出下标 index 的 extractelement 指令.
     * @param {array (nonnull)} 要取元素的数组, 类型必须是 ArrayType
     * @param {index (nonnull)} 下标, 类型必须是 IntType.
     * @throws NullException
     * @throws TypeMismatchException */
    static RefT Create(owned<Value> array, owned<Value> index);

    /** @fn CreateFromComptimeIndex(array[ity index])
     * @brief 创建一个从数组 array 中取出下标 index 的 extractelement 指令.
     *        在 IR 的优化阶段, index 应当是一个 IR 编译时常量, 类型为 ity.
     * @param {ity nonnull} 下标的类型
     * @param {index}     整数常量下标 */
    static RefT CreateFromComptimeIndex(owned<Value> array, IntType *ity, uint32_t index);
public:
    explicit ExtractElemSSA(owned<Value> array, owned<Value> index,
                            ArrayType *arr_type, Type *elem_type);
    ~ExtractElemSSA() final;
public:
    /** @property array{get;set;} */
    Value *get_array() const { return _array; }
    void   set_array(owned<Value> array);

    /** @property index{get;set;} */
    Value *get_index() const { return _index; }
    void   set_index(owned<Value> index);

    /** @property array_type{get;} */
    ArrayType *get_array_type() const { return _array_type; }

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final { return 2; }

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;
private:
    owned<Value>    _array, _index;
    ArrayType      *_array_type;
}; // class ExtractElemSSA final

/** @class InsertElemSSA
 *  @brief 把元素 element 存放到数组 array 中 index 为索引的位置, 得到一个新数组
 *
 * - 语法: `insertelement <aggr type> <array>, <elem type> <elem>, <int type> <index>`
 * - 操作数顺序:
 *  - [0]={array}   数组
 *  - [1]={element} 元素
 *  - [2]={index}   下标
 * - 返回值: 原数组类型, 表示新数组 */
class InsertElemSSA final: public Instruction {
public:
    using RefT = owned<InsertElemSSA>;

    /** @fn Create(): {copy(array)[index] = element} */
    RefT Create(owned<Value> array, owned<Value> element, owned<Value> index);
public:
    explicit InsertElemSSA(owned<Value> array, owned<Value> element,
                           owned<Value> index);
    ~InsertElemSSA() final;
public:
    /** @property array{get;set;} */
    Value *get_array() const { return _array; }
    void   set_array(owned<Value> array);

    /** @property index{get;set;} */
    Value *get_index() const { return _index; }
    void   set_index(owned<Value> index);

    /** @property element{get;set;} */
    Value *get_element() const { return _element; }
    void   set_element(owned<Value> element);

    /** @property array_type{get;} */
    ArrayType *get_array_type() const { return _array_type; }

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final { return 3; }

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;
private:
    owned<Value>    _array, _index, _element;
    ArrayType      *_array_type;
}; // class InsertValue final


/** @class ReturnSSA final
 * @brief 返回指令. 类型为返回值的类型.
 *
 * - 语法:
 *   - `ret <type> <value>`
 *   - `ret void`
 * - 操作数约定: 不论返回类型是不是 `void`, `ReturnSSA` 指令都有一个
 *   作为返回值的操作数, 类型即为函数的返回类型. 当返回类型是 `void`
 *   时, 返回值是 `void undefined`. 遇到 `undefined` 时可以认为该
 *   指令什么都不返回.
 * - IBasicBlockTerminator:没有跳转目标. */
class ReturnSSA final: public Instruction,
                       public IBasicBlockTerminator {
public:
    using  RefT = owned<ReturnSSA>;
    friend RefT;

    /** 创建指令 `ret <type> <value>` */
    static RefT Create(Function *parent, owned<Value> result);

    /** 创建指令 `ret void` */
    static RefT CreateDefault(Function *parent);
public:
    explicit ReturnSSA(Function *parent, Type *return_type, owned<Value> result);
    ~ReturnSSA() final;

    /** @property result{get;set;} owned */
    Value *get_result() const { return _result; }
    void   set_result(owned<Value> value);

    /** @property return_type{get;set;} */
    Type *get_return_type() const { return _return_type; }
    void  set_return_type(Type *value) {
        _return_type = value;
    }

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final {
        return index == 0 ? _result.get(): nullptr;
    }
    void traverseOperands(ValueAccessFunc fn) final {
        if (_result->is_defined()) fn(*_result);
    }
    size_t get_operand_nmemb() const final { return 1; }
public: /* overrides interface IBasicBlockTerminator */
    void traverse_targets(TraverseFunc    fn) final { (void)fn; }
    void traverse_targets(TraverseClosure fn) final { (void)fn; }

    void remove_target(BasicBlock *target)    final { (void)target; }
    void remove_target_if(TraverseFunc    fn) final { (void)fn; }
    void remove_target_if(TraverseClosure fn) final { (void)fn; }

    void replace_target(BasicBlock *old, BasicBlock *new_target) final {}
    void replace_target_by(ReplaceClosure fn) final { (void)fn; }

    void clean_targets() final {}

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

private:
    owned<Value> _result;
    Type   *_return_type;
}; // class ReturnSSA final

/** @class StoreSSA final
 * @brief 存储一个值到一个指针对应的内存单元. 类型为void.
 *
 * - 语法: `store <type> <source>, ptr <target>, [align <align>]`
 * - 操作数:
 *   - `source`[0]: 源操作数, 表示等待存入的元素
 *   - `target`[1]: 目的操作数, 表示指向目的内存的指针
 * - 返回类型: `void`, 不返回任何值.
 * - 构造时: 检查target的类型是否为指针类型, 且是否指向source的类型 */
class StoreSSA final: public Instruction {
public:
    using RefT = owned<StoreSSA>;

    /** @fn Create(source, pointer_target)
     * @brief 创建一个 `StoreSSA` 指令结点, 对齐方式与 `source` 的 IR 类型要求一致.
     * @param source [0]源操作数, 表示等待存入内存的元素
     * @param target [1]目的操作数, 表示指向目标内存的指针
     * @return 返回值确定不为 `null`. 出错时通过抛异常中断程序.
     * @throws NullException 当 `source` 和 `target` 二者有一个是 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `target` 不是指针, `target` 指向不可实例化的
     *         对象或者 `source` 的类型不是 `target` 指向目标的类型时, 抛出该异常. */
    static RefT Create(owned<Value> source, owned<Value> target);

    /** @fn CreateWithAlign(source, pointer_target, align = 2 ^ n)
     * @brief 按照 `align` 所示的对齐要求创建一个 `StoreSSA` 指令结点.
     * @param source [0]源操作数, 表示等待存入内存的元素
     * @param target [1]目的操作数, 表示指向目标内存的指针
     * @param align  内存实例对齐要求, 数值必须是 2 的次方. 否则会在内部向上取整为 2 的次方.
     * @return 返回值确定不为 `null`. 出错时通过抛异常中断程序.
     * @throws NullException 当 `source` 和 `target` 二者有一个是 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `target` 不是指针, `target` 指向不可实例化的
     *         对象或者 `source` 的类型不是 `target` 指向目标的类型时, 抛出该异常. */
    static RefT CreateWithAlign(owned<Value> source, owned<Value> target, size_t align);

    /** @fn CreateFromAlloca(source, alloca_target)
     * @brief 创建一个 `StoreSSA` 结点, 其语义为 "把 source 存入通过 alloca_ssa 申请的
     *        内存里". 该方法会自动采用 `alloca_ssa` 的对齐要求.
     * @param source     [0]源操作数, 表示等待存入内存的元素
     * @param alloca_ssa [1]目的操作数, 表示通过 `alloca_ssa` 申请得到的内存
     * @return 返回值确定不为 `null`. 出错时通过抛异常中断程序.
     * @throws NullException 当 `source` 和 `target` 二者有一个是 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `target` 不是指针, `target` 指向不可实例化的
     *         对象或者 `source` 的类型不是 `target` 指向目标的类型时, 抛出该异常. */
    static RefT CreateFromAlloca(owned<Value> source, AllocaSSA *alloca_ssa);

    /** @fn CreateFromGlobalVariable(source, gvar_target)
     * @brief 创建一个 `StoreSSA` 结点, 其语义为 "把 source 存入通过 gvar 指向的内存
     *        里". 该方法会自动采用 `gvar` 的对齐要求.
     * @param source [0]源操作数, 表示等待存入内存的元素
     * @param gvar   [1]目的操作数, 表示全局变量 `gvar` 指向的内存
     * @return 返回值确定不为 `null`. 出错时通过抛异常中断程序.
     * @throws NullException 当 `source` 和 `target` 二者有一个是 `null` 时抛出该异常.
     * @throws TypeMismatchException 当 `target` 不是指针, `target` 指向不可实例化的
     *         对象或者 `source` 的类型不是 `target` 指向目标的类型时, 抛出该异常.
     * @throws InvalidAccessException 当 `gvar` 是常量时, 抛出该异常 */
    static RefT CreateFromGlobalVariable(owned<Value> source, GlobalVariable *gvar);
public:
    /** @fn constructor
     * @brief   不进行任何检查, 根据参数所示的源操作数、
     * @warning 不要直接调用该构造函数. 要创建一个 `StoreSSA`, 请使用带参数检查且具备
     *          异常安全的 `Create*` 函数族. */
    explicit StoreSSA(owned<Value> source, owned<Value> target,
                      PointerType *target_pointer_type, size_t align);
    ~StoreSSA() final;

    /** @property source{get;set;} owned */
    Value *get_source() const { return _source; }
    /** @see property source{get;set;}
     * @throw TypeMismatchExeption 参数类型与返回类型不同 */
    void   set_source(owned<Value> source);

    /** @property target{get;set;} owned */
    Value *get_target() const { return _target; }
    /** @see property target{get;set;}
     * @throw TypeMismatchExeption 参数类型与目标指针类型不同 */
    void   set_target(owned<Value> value);

    /** @property align{get;set;} owned */
    size_t get_align() const { return (size_t)(_align); }
    void set_align(size_t align) {
        _align = fill_to_power_of_two(align);
    }

    /** @fn setSourceTarget(source, target)
     * @brief 由于分开设置 source 与 target 的时候会各自受到对方类型的约束,
     *        所以写这个函数来捆绑设置 source 与 target.
     * @warning 该方法已经被废弃.
    [[deprecated("直接修改 source 和 target 可能会遇到对齐不匹配等问题. 倘若真的有"
                 "同时修改二者的需求的话, 建议删除该指令, 然后重新 new 一个.")]]
    void setSourceTarget(owned<Value> source, owned<Value> target); */

    /** @property target_pointer_type{get;}
     *  @brief 目的操作数指针类型. */
    PointerType *get_target_pointer_type() const {
        return _target_pointer_type;
    }

    /** @property source_type{get;}
     *  @brief 源操作数类型. */
    Type *get_source_type() const {
        return get_target_pointer_type()->get_target_type();
    }

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final { return 2; }

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

private:
    owned<Value> _source; // 源操作数
    owned<Value> _target; // 目的操作数
    PointerType *_target_pointer_type; // 目的操作数指针类型
    size_t       _align;
}; // class StoreSSA final

/** @class MemoryIntrinSSA
 * @brief 以指令形式出现的内联汇编函数, 一般类型为void.
 *
 * - 使用三个值: 源指针、目标指针、元素数量. */
imcomplete class MemoryIntrinSSA: public Instruction {
public:
    DECLARE_OWNED_GETSET(Value, source)
    DECLARE_OWNED_GETSET(Value, target)
    DECLARE_OWNED_GETSET(Value, nmemb)

/* overrides parent */
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final { return 3; }

protected:
    owned<Value> _source;
    owned<Value> _target;
    owned<Value> _nmemb;

    explicit MemoryIntrinSSA(OpCode       opcode,
                             owned<Value> source,
                             owned<Value> target,
                             owned<Value> nmemb);
    
    abstract bool _checkSource(Value *source) = 0;
    abstract bool _checkTarget(Value *target) = 0;
    abstract bool _checkNMemb(Value *nmemb) = 0;
private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

}; // class MemoryIntrinSSA abstract

/** @class MemMoveSSA final
 * @brief 内存移动内联汇编函数, value type是移动的元素的类型
 *
 * - 语法: `memmove <source> to <target>, <nmemb> <value type>`
 * - 构造: memmove内部函数要求source的类型与target的类型都是指针,
 *        并且source的元素类型与target的最终元素类型相同。 */
class MemMoveSSA final: public MemoryIntrinSSA {
public:
    explicit MemMoveSSA(owned<Value> source, owned<Value> target, owned<Value> nmemb);

    void accept(IValueVisitor &visitor) final;
private:
    bool _checkSource(Value *source) final;
    bool _checkTarget(Value *target) final;
    bool _checkNMemb(Value *nmemb) final;
}; // class MemMoveSSA final

/** @class MemSetSSA final
 * @brief 把内存批量设置为某个值的内联汇编函数
 *
 * - 语法: `memset <nmemb> <value type> <source> to <target>`
 * - 构造: memmove内部函数要求target的类型都是指针, 并且source的类型
 *        与target的最终元素类型相同. */
class MemSetSSA final: public MemoryIntrinSSA {
public:
    explicit MemSetSSA(owned<Value> source, owned<Value> target, owned<Value> nmemb);

    void accept(IValueVisitor &visitor) final;
private:
    bool _checkSource(Value *source) final;
    bool _checkTarget(Value *target) final;
    bool _checkNMemb(Value *nmemb) final;
}; // class MemMoveSSA final

/** @class CompareSSA final
 * @brief 比较指令, 类型为i1(布尔类型)
 *
 * - 语法: `opcode <cond> <value type> <lhs>, <rhs>`
 * - 使用关系: 使用了lhs与rhs */
class CompareSSA final: public Instruction {
public:
    /** @enum CompareSSA::Condition
     * @brief 二进制掩码, 存放比较的条件、是否为浮点数比较、是否有符号/有序等. */
    struct Condition {
        using u8string8_t = std::array<char, 8>;
    public:
        enum self_t: uint8_t {
            NONE = 0,   FALSE = 0b0'0000, TRUE = 0b0'0111,
            LT = 0b0'0001, EQ = 0b0'0010, GT = 0b0'0100,
            LE = 0b0'0011, GE = 0b0'0110, NE = 0b0'0101,
        /* ordered(float) | signed(int) */
            SIGNED_ORDERED = 0b0'1000,
            OP_INTEGER = 0, OP_FLOAT = 0b1'0000,
        }; // enum self_t

        MTB_MAKE_CLASSED_ENUM(Condition)
        constexpr Condition(CompareResult compare_result,
                            bool    is_float_op,
                            bool    is_signed_or_ordered) noexcept
            : Condition(uint8_t(compare_result) |
                        (is_float_op << 4)      |
                        (is_signed_or_ordered << 3)) {}

        constexpr bool is_lt() const noexcept { return (self & LT) != 0; }
        constexpr bool is_eq() const noexcept { return (self & EQ) != 0; }
        constexpr bool is_gt() const noexcept { return (self & GT) != 0; }

        constexpr bool is_signed() const noexcept {
            return (self & SIGNED_ORDERED) != 0 && (self & OP_FLOAT) == 0;
        }
        constexpr bool is_ordered() const noexcept {
            return (self & SIGNED_ORDERED) != 0 && (self & OP_FLOAT) != 0;
        }
        constexpr bool is_signed_ordered() const noexcept {
            return (self & SIGNED_ORDERED) != 0;
        }
        constexpr bool is_integer_op() const noexcept {
            return (self & OP_FLOAT) == 0;
        }
        constexpr bool is_float_op() const noexcept {
            return (self & OP_FLOAT) != 0;
        }

        constexpr CompareResult
        get_compare_result() const noexcept { return CompareResult(self & TRUE); }
        constexpr operator CompareResult() const noexcept {
            return get_compare_result();
        }

        /** @fn   getString(is_float_op) -> char[8]
         * @brief 返回一个以0结尾的8字节小字符串, 以存储该枚举的字符串形式. */
        u8string8_t getString() const noexcept;
    }; // enum Condition

    using RefT = owned<CompareSSA>;
public:
    /** @fn CreateICmp
     * @brief 整数比较指令
     * - 语法: `icmp <cond> <int type> <lhs>, <rhs>` */
    static RefT CreateICmp(CompareResult condition, bool is_signed,
                           owned<Value> lhs, owned<Value> rhs);

    static inline RefT
    CreateICmp(Condition condition, owned<Value> lhs, owned<Value> rhs) {
        return CreateICmp(condition.get_compare_result(),
                          condition.is_signed_ordered(),
                          std::move(lhs), std::move(rhs));
    }
    /** @fn CreateFCmp
     * @brief 浮点比较指令
     * - 语法: `fcmp <cond> <float type> <lhs>, <rhs>`
     * - 比较条件/谓词: 分为有序比较与无序比较两大类, 有序比较前缀为'o', 无序比较前缀为'u'.
     *                其余部分与 CompareResult 的字符串格式一致。 */
    static RefT CreateFCmp(CompareResult condition, bool is_ordered,
                           owned<Value> lhs, owned<Value> rhs);

    static inline RefT
    CreateFCmp(Condition condition, owned<Value> lhs, owned<Value> rhs) {
        return CreateFCmp(condition.get_compare_result(),
                          condition.is_signed_ordered(),
                          std::move(lhs), std::move(rhs));
    }

    /** @fn CreateChecked
     * @brief 根据传入的操作数与 condition, 调用上面的 Create 函数创建一个
     *        CompareSSA 指令结点. 该方法会忽略 condition 的 int-float 位.
     * @throws NullException           参数至少有一个是 null
     * @throws OpCodeMismatchException OpCode 不是比较指令的操作码
     * @throws TypeMismatchException   lhs 和 rhs 的类型不相等, 或者不是标量类型 */
    static RefT CreateChecked(OpCode opcode, Condition condition,
                              owned<Value> lhs, owned<Value> rhs);
public:
    CompareSSA(OpCode opcode, Condition condition, Type *operand_type,
               owned<Value> lhs, owned<Value> rhs);

    ~CompareSSA() final;

    /** @property lhs{get;set;} owned */
    Value *get_lhs() const { return _lhs; }
    void   set_lhs(owned<Value> lhs);

    /** @property rhs{get;set;} owned */
    Value *get_rhs() const { return _rhs; }
    void   set_rhs(owned<Value> rhs);

    /** @property operand_type{get;} */
    Type *get_operand_type() const { return _operand_type; }

    /** @property condition{get;set;} */
    Condition get_condition() const { return _condition; }
    void      set_condition(Condition condition) { _condition = condition; }
    void set_condition(CompareResult value) {
        _condition = Condition(value,
                               _condition.is_float_op(),
                               _condition.is_signed_ordered());
    }

/* overrides parent */
    void accept(IValueVisitor &visitor) final;
    Value *operator[](size_t index) final;
    void traverseOperands(ValueAccessFunc fn) final;
    size_t get_operand_nmemb() const final { return 2; }

private: /* ==== [Signal Handler] ==== */
    void on_parent_plug(BasicBlock *parent) final;
    void on_parent_finalize()   final;
    void on_function_finalize() final;

private:
    owned<Value>  _lhs, _rhs;
    Type      *_operand_type;
    Condition     _condition;
}; // class CompareSSA final

/* ================ [Instruction Subclasses] ================ */
} // namespace MYGL::IR

#endif

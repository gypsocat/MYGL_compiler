#pragma once
#ifndef __MYGL_IR_BASIC_BASICBLOCK_H__
#define __MYGL_IR_BASIC_BASICBLOCK_H__

#include "base/mtb-reflist.hxx"
#include "ir-instruction-base.hxx"
#include <set>

namespace MYGL::IR {

using namespace MTB;
using namespace IRBase;

class Module;
class Function;
class BasicBlock;

/** @class BasicBlock
 * @brief 指令系统的基本块, 在IR中以标签的形式出现. 类型是标签类型LabelType.
 *
 * TODO: 完成注释章节: BasicBlock的作用、两个BasicBlock的连接方式、作为
 *       指令列表的性质、作为Function结点的性质 */
class BasicBlock final: public Value {
public:
    struct ActionPred;
    struct TargetInfo;
    class  IllegalTerminatorException;
    using  TerminatorT = IBasicBlockTerminator;
    using  TerminatorTraitT = IBasicBlockTerminator::TraitOwner;
    using  RefT        = owned<BasicBlock>;
    using  InstListT   = Instruction::ListT;
    using  UBlockListT = std::list<BasicBlock*>;
    using  ListT       = RefList<BasicBlock, ActionPred>;
    using  ProxyT      = RefListProxy<BasicBlock, ActionPred>;
    using  USetT       = std::set<TargetInfo>;
    using  ConnectStatus = Instruction::ConnectStatus;

    /* BasicBlock结点访问函数 */
    using AccessFunc  = std::function<bool(BasicBlock &)>;
    using ReadFunc    = std::function<bool(BasicBlock const &)>;

    friend struct ActionPred;
    friend struct TargetInfo;
    friend class  Function;
    friend RefT;
public:
    struct TargetInfo {
        BasicBlock *target_block;
        size_t      use_count;

        bool operator<(TargetInfo const &rhs) const {
            return target_block < rhs.target_block;
        }
        bool operator==(TargetInfo const &rhs) const {
            return target_block == rhs.target_block;
        }
    }; // struct TargetInfo
public:
    /** @fn Create(parent)
     * @brief 创建一个父结点为 `parent` 的空基本块, 以 `unreachable` 指令结尾
     * @return 返回值恒不为 `null`. 出错时会抛异常.
     * @throws NullException `parent` 为 null 时抛该异常. */
    static RefT Create(Function *parent);

    /** @fn CreateWithEnding(parent, ending)
     * @brief 创建一个父结点为 `parent`, 结尾指令为 `ending` 的基本块.
     * @throws NullException `parent` 或者 `ending` 为 null 时抛出该异常.
     * @throws std::bad_cast `ending` 不是终止指令时抛出该异常 */
    static RefT CreateWithEnding(Function *parent, owned<Instruction> ending);

    /** @fn CreateWithEndingTrait(parent, dyn trait ending)
     * @brief 创建一个父结点为 `parent`, 结尾指令为 `ending` 的基本块. 该方法
     *        稍微比 `CreatWithEnding` 高效一点, 因为它没有侧向动态转换.
     * @throws NullException 当 `parent` 或者 `ending_trait` 为 null 时抛出该异常.
     * @throws std::bad_cast 当 `ending_trait` 自身出错 (例如, `iface` 指针和
     *         `instance` 引用对不上) 时抛出该异常. */
    static RefT CreateWithEndingTrait(Function *parent, TerminatorTraitT ending_trait);

    /** @fn CreateWithEndingIface(parent, dyn interface ending)
     * @brief 创建一个父结点为 `parent`, 结尾指令为 `ending` 的基本块. 该方法
     *        稍微比 `CreatWithEnding` 高效一点, 因为它没有侧向动态转换.
     * @throws NullException 当 `parent` 或者 `ending` 为 null 时抛出该异常. */
    static RefT CreateWithEndingIface(Function *parent, IBasicBlockTerminator *ending);
public:
    explicit BasicBlock(Function *parent, IBasicBlockTerminator::TraitOwner ending);
    ~BasicBlock() final;

public: /* ==== [BasicBlock as List<Instruction>] ====*/
    /** @property instruction_list{get;access;} */
    InstListT const &get_instruction_list() const { return _instruction_list; }
    InstListT &      instruction_list()           { return _instruction_list; }

    /** @property end_instruction{get;set;}
     *  @brief 最后一个指令, 要求属性ends_basicblock为true. */
    IBasicBlockTerminator *get_terminator() const {
        return _terminator;
    }
    void set_terminator(Instruction::RefT ending);
    void set_terminator(TerminatorTraitT  ending);

    owned<Instruction> swapOutEnding(owned<Instruction> terminator);

    /** @property terminator_trait{get;}
     * @brief 取得最后一个指令，但是返回值是trait胖指针形式. */
    IBasicBlockTerminator::TraitOwner get_terminator_trait() {
        IBasicBlockTerminator *terminator = get_terminator();
        return terminator->get_trait();
    }

    /* ======== [BasicBlock as CFG Node] ======== */

    /** @property jumps_to{get;access;}
     * @brief `BasicBlock` 的出口基本块集合, 存放跳转目标.
     * @warning ends_basicblock为true的属性要维护该集合. */
    USetT const &get_jumps_to() const { return _jumps_to; }
    /** @see get_jumps_to() */
    USetT &      jumps_to()           { return _jumps_to; }

    /** @fn count_node_jumps_to() const
     * @brief 计算 jumps_to 集合中有多少基本块. */
    size_t count_node_jumps_to() const {
        return get_jumps_to().size();
    }
    /** @fn count_refs_jumps_to() const
     * @brief 计算 jumps_to 中所有基本块一共被引用了多少次. */
    size_t count_refs_jumps_to() const;

    /** @property comes_from{get;access;}
     * @warning ends_basicblock为true的属性要维护该集合. */
    USetT const &get_comes_from() const { return _comes_from; }
    USetT &      comes_from()           { return _comes_from; }

    /** @fn count_node_comes_from() const
     * @brief 计算 comes_from 集合中有多少基本块. */
    size_t count_node_comes_from() const {
        return _comes_from.size();
    }
    /** @fn count_refs_comes_from() const
     * @brief 计算 comes_from 中所有基本块一共被引用了多少次. */
    size_t count_refs_comes_from() const;

public: /* ========== [BasicBlock Comes from & Jumps to] ==========*/

    /** @fn countJumpsToRef(jumps_to)
     * @brief 检查参数 `jumps_to` 所示的基本块在跳转目标集合中被引用了多少次.
     * @return 被引用的次数. 倘若 `jumps_to == null` 或者 `jumps_to` 不是
     *         跳转目标, 则返回 0. */
    size_t countJumpsToRef(BasicBlock *jumps_to) const;

    /** @fn hasJumpsTo(jumps_to)
     * @brief 检查出口基本块集合中是否含有参数 `jumps_to` 所示的基本块. */
    bool   hasJumpsTo(BasicBlock *jumps_to) const {
        return countJumpsToRef(jumps_to) > 0;
    }

    /** @fn countJumpsToRef(comes_from)
     * @brief 检查参数 `comes_from` 所示的基本块在入口基本块集合中被引用了多
     *        少次.
     * @return 被引用的次数. 倘若 `comes_from == null` 或者 `comes_from`
     *         不是入口基本块, 则返回 0. */
    size_t countComesFromRef(BasicBlock *comes_from) const;

    /** @fn hasComesFrom(comes_from)
     * @brief 检查入口基本块集合中是否含有参数 `comes_from` 所示的基本块 */
    bool   hasComesFrom(BasicBlock *comes_from) const {
        return countComesFromRef(comes_from) > 0;
    }


public: /* ================ [IBasicBlockTerminator Only] ================ */
    /** @fn addJumpsTo(jumps_to)
     * @brief 往出口基本块集合中插入 `jumps_to`. 倘若出口基本块集合中已经有
     *        `jumps_to`, 则其计数加 1.
     * @return 插入成功以后, `jumps_to` 在基本块集合中的计数.
     * @warning 只有 Instruction 的子类可以调用该方法. 否则会出现难以预料的
     *          行为. */
    size_t addJumpsTo(BasicBlock *jumps_to);

    /** @fn removeJumpsTo(jumps_to)
     * @brief 移除 `jumps_to` 基本块在出口集合中的一次引用. 倘若 `jumps_to`
     *        原本被引用了不止一次, 那么该方法只会减少一次计数; 倘若引用计数降
     *        到 0, 则移除 `jumps_to`.
     * @return bool 原本是否存在对 `jumps_to` 的引用.
     * @warning 只有 Instruction 的子类可以调用该方法. 否则会出现难以预料的
     *          行为. */
    bool removeJumpsTo(BasicBlock *jumps_to);

    /** @fn clearJumpsTo(jumps_to)
     * @brief 移除 `jumps_to` 基本块在出口集合中的所有引用, 并删除 `jump_to`
     *        在集合中保留的元数据.
     * @warning 该方法只会清除 `jumps_to` 结点在出口集合的所有引用, 不负责管
     *          理 `jumps_to` 上的跳转关系集合.
     * @warning 只有 Instruction 的子类可以调用该方法. 否则会出现难以预料的
     *          行为. */
    void clearJumpsTo(BasicBlock *jumps_to);

    /** @fn addComesFrom(comes_from)
     * @brief 往入口基本块集合中插入 `comes_from`. 倘若入口基本块集合中已经有
     *        `comes_from`, 则其计数加 1.
     * @return 插入成功以后, `comes_from` 在基本块集合中的计数.
     * @warning 该方法只会操作此基本块的 `comes_from` 属性, 不负责管理与此
     *          基本块关联的其他基本块.
     * @warning 只有 Instruction 的子类可以调用该方法. 否则会出现难以预料的
     *          行为. */
    size_t addComesFrom(BasicBlock *comes_from);

    /** @fn removeComesFrom(comes_from)
     * @brief 移除 `comes_from` 基本块在入口集合中的一次引用. 倘若 `comes_from`
     *        原本被引用了不止一次, 那么该方法只会减少一次计数; 倘若引用计数降
     *        到 0, 则移除 `comes_from`.
     * @return bool 原本是否存在对 `comes_from` 的引用.
     * @warning 只有 Instruction 的子类可以调用该方法. 否则会出现难以预料的
     *          行为. */
    bool removeComesFrom(BasicBlock *comes_from);

    /** @fn clearComesFrom(comes_from)
     * @brief 移除 `comes_from` 基本块在入口集合中的所有引用, 并删除 `comes_from`
     *        在集合中保留的元数据.
     * @warning 该指令只会清除 `comes_from` 结点在入口集合的所有引用, 不负责管
     *          理 `comes_from` 上的跳转关系集合.
     * @warning 只有 Instruction 的子类可以调用该方法. 否则会出现难以预料的
     *          行为. */
    void clearComesFrom(BasicBlock *comes_from);

public: /* ============ [BasicBlock as Value(override)] ============ */

    OVERRIDE_GET_TRUE(is_readable)
    OVERRIDE_GET_TRUE(is_writable)

    void accept(IValueVisitor &visitor) final;

public: /* BasicBlock as List<Instruction> */
    /** @fn append(Instruction)
     * @brief 在基本块末尾的终止子前插入一条指令 `inst`. 要求指令不能是终止子.
     * @throws NullException `inst` 为空引用时抛出该异常.
     * @throws IllegalTerminatorException 当 `inst` 是基本块终止子时抛出该异常. */
    void append(owned<Instruction> inst);

    /** @fn appendOrReplaceBack(owned<Instruction> ending)
     * @brief 倘若 ending 是终止子, 就替换原有的终止子; 否则在终止子前插入 ending.
     * @return `dyn trait IBasicBlockTerminator` 被替换出去的终止子.
     * @retval null `ending` 不是终止子, 不发生替换.
     * @throws NullException `ending` 为空时抛出该异常. */
    TerminatorTraitT appendOrReplaceBack(owned<Instruction> ending);

    /** @fn prepend(Instruction)
     * @brief 在基本块的开头前插入一条指令 `inst`. 要求指令不能是终止子.
     * @throws NullException `inst` 为空引用时抛出该异常.
     * @throws IllegalTerminatorException 当 `inst` 是基本块终止子时抛出该异常. */
    void prepend(owned<Instruction> inst);

    template<typename InstT, typename...Args>
    MTB_REQUIRE((std::is_base_of_v<Instruction, InstT>))
    [[deprecated("为了消除重载, 大多数 Instruction 子类的构造函数都被 Create... 函数"
                 "替代了. 该模板方法不会实现, 随时可能删除. 尽管 `append(Create(...))`"
                 "的模式显得有些啰嗦, 但它目前是唯一可用的方法. ")]]
    InstT *emplaceBack(Args...args);

    /** @fn clean{(Instruction &)->false} 
     * @brief 主动清除所有指令的引用，然后清空指令列表。在指令结点被清除之前会调用fn函数.
     *        这个方法适合在有自定义清除需求的时候使用.
     * @warning 这个函数**无视fn的返回值**, 返回什么bool常量都不会中断该方法.
     *          **建议返回一个false**来与其他使用这种遍历函数的方法保持一致. */
    [[deprecated("该方法作用不明, 可能移除. 倘若有异议, 可以在群里反馈. 记得带上方法名称.")]]
    void clean(Instruction::AccessFunc fn);

    /** @fn clean()
     * @brief 主动清除所有指令的引用，然后清空指令列表。该方法不接受任何函数作为参数.
     * @ref clean(Instruction::AccessFunc) */
    void clean() {
        for (Instruction *i: _instruction_list)
            i->on_parent_finalize();
        _instruction_list.clean();
    }

    /** @fn split(unowned Instruction)
     * @brief 以 new_begin 为界, 把 BasicBlock 拆分成两部分.
     * @return [nullable] 以 new_begin 为首的新 BasicBlock. 倘若 new_begin 不存在于 BasicBlock 中,
     *         那么会返回 nullptr.
     *
     * 该方法遵循以下规则:
     * - 此基本块会保留原基本块的前半部分, 后半部分会以返回值形式给出.
     * - 后半部分的第一条指令就是 `new_begin`, 终止子(最后一条指令)就是原基本块的终止子.
     * - 前半部分的终止子会被替换成一条跳转到后半部分的 `JumpSSA` 指令, 以连接二者.
     * - 该方法会自动修改跳转关系以保证 CFG 一致性, 但是不会自动修改数据流关系. 例如
     *   该方法不会修改原基本块跳转目标的 φ 结点, 需要手动修改. 接下来可能会写一个工具类
     *   来完成带数据流探查的基本块拆分操作. */
    RefT split(Instruction *new_begin);

public: /* ==== [BasicBlock as child of Function] ==== */
    /** @property parent{get;}
     *  @brief 该BasicBlock的父结点是哪个Function. */
    Function *get_parent() const { return (Function *)(_parent); }
    void      set_parent(Function *value) { _parent = value; }

    /** @property module{get;}
     *  @brief 该BasicBlock在哪个Module里. */
    Module *get_module() const;

    /** @property is_ending_block{get;} bool
     *  @brief 该 BasicBlock 是否是控制流终点之一. 当结尾指令不是跳转指令时,
     *         该属性为 true. */
    bool is_ending_block() const;

public: /* ====== [BasicBlock as signal handler of Function] ====== */

    /** @fn on_function_plug(Function parent) */
    void on_function_plug(Function *parent);

    /** @fn on_function_unplug() -> Function */
    Function *on_function_unplug();

    /** @fn on_function_finalize() */
    void on_function_finalize();
public: /* BasicBlock as List<BasicBlock>.Node */

    /** @property default_next{get;} nullable
     * @brief BasicBlock 作为 CFG 结点时, 倘若存在跳转目标, 则获取默认的跳转目标.
     *        否则, 返回 null.
     * @return nullable BasicBlock 跳转目标或者 null.
     *
     * 对于 BranchSSA, 该跳转目标是条件为 false 时的跳转目标.
     * 对于 SwitchSSA, 该跳转目标是 default 标签. */
    BasicBlock *get_default_next() const;

    /** @property iterator{get;} */
    ListT::Iterator get_iterator();

    /** @property modifier{get;}
     * @brief BasicBlock 自己的局部修改迭代器 */
    ListT::Modifier get_modifier() {
        return ListT::Modifier{get_iterator()};
    }

    /** @property reflist_item_proxy{access;}
     *  @warning 该方法是 `RefList<BasicBlock>` 要求实现的方法之一, 请
     *           注意不要调用该方法. */
    ProxyT &reflist_item_proxy() { return _reflist_item_proxy; }
    operator ProxyT&() { return reflist_item_proxy(); }


public: /* ======================== [deprecated] ======================== */

    [[deprecated("该构造函数不是异常安全的. 请更换为 `BasicBlock::Create(parent)`.")]]
    explicit BasicBlock(Function *parent);

    [[deprecated("该构造函数不是异常安全的; 同时, 由于涉及重载, 该函数被弃用. "
                 "请更换为 `BasicBlock::CreateWithEnding(parent, ending)`.")]]
    explicit BasicBlock(Function *parent, owned<Instruction> ending);

private:
    InstListT _instruction_list;
    IBasicBlockTerminator *_terminator;
    USetT     _jumps_to, _comes_from;
    Function *_parent;
    ProxyT    _reflist_item_proxy;
    ConnectStatus _connect_status;
}; // class BasicBlock final

/** @struct ActionPred
 * @brief `BasicBlock` 作为 `Function` 子结点的信号接收器
 *
 * 该接收器需要实现的约束是: 阻止任何会移除 `Function` 第一个基本块的操作。 */
struct BasicBlock::ActionPred: MTB::IRefListItemAction<BasicBlock> {
    using BaseT    = IRefListItemAction<BasicBlock>;
    using IterT    = ListT::Iterator;
    using ElemT    = BasicBlock;
    using Modifier = BasicBlock::ListT::Modifier;

    Function *cached_parent = nullptr;
public: /* ==== [CRTP implements PreprocessorT] ==== */
    /** @fn on_modifier_append_preprocess(ref Modifier, BasicBlock)
     * @brief 兼容考虑，什么也不做. */
    bool on_modifier_append_preprocess(Modifier *modifier, BasicBlock *elem) {
        ListT *list = modifier->list;
        if (!list->empty())
            cached_parent = list->back()->get_parent();
        return BaseT::on_modifier_append_preprocess(
            reinterpret_cast<BaseT::Modifier*>(modifier), elem);
    }

    /** @fn on_modifier_prepend_preprocessr(ref Modifier, BasicBlock)
     * @brief 兼容考虑，什么也不做. */
    bool on_modifier_prepend_preprocess(Modifier *modifier, BasicBlock *elem) {
        ListT *list = modifier->list;
        if (!list->empty())
            cached_parent = list->back()->get_parent();
        return BaseT::on_modifier_prepend_preprocess(
            reinterpret_cast<BaseT::Modifier*>(modifier), elem);
    }

    /** @fn on_modifier_replace_preprocess(ref Modifier, BasicBlock)
     * @brief 替换自己的信号接收器, 条件是「自己不是 Function 的第一个基本块」. */
    bool on_modifier_replace_preprocess(Modifier *modifier, BasicBlock *replacement);

    /** @fn on_modifier_disable_preprocess(ref Modifier)
     * @brief 在 Modifier 被禁用时触发, 条件是 "自己不是 Function 的入口基本块" */
    bool on_modifier_disable_preprocess(Modifier *modifier);

public: /* ==== [CRTP implements SignalHandlerT] ==== */
    /** @fn on_modifier_append(Modifier, BasicBlock)
     * @brief 为新增的 BasicBlock 执行插入信号. */
    void on_modifier_append(Modifier *origin, BasicBlock *block)
    {
        if (cached_parent == nullptr)
            return;
        block->on_function_plug(cached_parent);
        block->set_parent(cached_parent);
    }

    /** @fn on_modifier_prepend(Modifier, BasicBlock)
     * @brief 为新增的 BasicBlock 执行插入信号. */
    void on_modifier_prepend(Modifier *origin, BasicBlock *block)
    {
        if (cached_parent == nullptr)
            return;
        block->on_function_plug(cached_parent);
        block->set_parent(cached_parent);
    }

    /** @fn on_modifier_replace(Modifier, BasicBlock)
     * @brief 为新增的 BasicBlock 执行插入信号. */
    void on_modifier_replace(Modifier *modifier, BasicBlock *old_self);
}; // struct ActionPred


class BasicBlock::IllegalTerminatorException: public Exception {
public:
    enum ErrorCode {
        INST_MISSING,
        INST_ILLEGAL,
        ERR_CODE_COUNT
    }; // enum ErrorCode
public:
    IllegalTerminatorException(BasicBlock  *block,
                               ErrorCode err_code, std::string &&info,
                               SourceLocation srcloc = CURRENT_SRCLOC);
    IllegalTerminatorException(BasicBlock  *block, Instruction *ending,
                               ErrorCode err_code, std::string &&info,
                               SourceLocation srcloc = CURRENT_SRCLOC);

    std::string_view error_code_stringfy() const;
public:
    BasicBlock        *block;
    owned<Instruction> ending;
    ErrorCode          err_code;
}; // IllegalEndingInstructionException

/* ==========[IMPL for BasicBlock]========== */

inline void BasicBlock::clean(Instruction::AccessFunc fn) {
    for (Instruction *i: _instruction_list) {
        fn(*i); i->on_parent_finalize();
    }
    _instruction_list.clean();
}
inline bool BasicBlock::is_ending_block() const
{
    Instruction *end_inst = get_terminator()->get_instance();
    ValueTID     tid = end_inst->get_type_id();
    return tid == ValueTID::RETURN_SSA ||
           tid == ValueTID::UNREACHEBLE_SSA;
}

} // namespace MYGL::IR

#endif
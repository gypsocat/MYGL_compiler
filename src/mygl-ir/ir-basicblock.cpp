#include "base/mtb-compatibility.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include <iostream>

namespace MYGL::IR {

/* +================ [interface BasicBlock::ActionPred] ================+ */
    /** @fn on_modifier_replace_preprocess(ref Modifier, BasicBlock)
     * @brief 替换自己的信号接收器, 条件是「自己不是 Function 的第一个基本块」. */
    bool BasicBlock::ActionPred::
    on_modifier_replace_preprocess(Modifier *modifier, BasicBlock *replacement)
    {
        BasicBlock  *old = modifier->get();
        Function *parent = old->get_parent();
        if (parent->get_entry() == old)
            return false;
        return true;
    }

    bool BasicBlock::ActionPred::
    on_modifier_disable_preprocess(Modifier *modifier)
    {
        BasicBlock  *old = modifier->get();
        Function *parent = old->get_parent();
        return parent->get_entry() != old;
    }

    void BasicBlock::ActionPred::
    on_modifier_replace(Modifier *modifier, BasicBlock *old_self)
    {
        RefT   &new_self = modifier->get();
        Function *parent = old_self->get_parent();
        new_self->on_function_plug(parent);
        new_self->set_parent(parent);
        old_self->on_function_unplug();
    }
 
/* |============== [end interface BasicBlock::ActionPred] ==============| */


inline namespace basic_block_impl {
    using namespace MTB::Compatibility;
    using namespace std::string_literals;
    using namespace std::string_view_literals;
    using TerminatorErrorCodeT = BasicBlock::IllegalTerminatorException::ErrorCode;

    static inline std::string
    terminator_exception_get_msg(TerminatorErrorCodeT err_code,
                                 BasicBlock *block, std::string const& info,
                                 SourceLocation const& srcloc)
    {
        return osb_fmt("BasicBlock::IllegalTerminatorException at \n"sv,
            source_location_stringfy(srcloc), "\n"
            "BasicBlock "sv, (void*)block, " (%", block->get_name_or_id(), ")\n"
            "INFO: ", info);
    }
} // inline namespace basic_block_impl

/* +================ [class BasicBlock::IllegalTerminatorException] ================+ */
    BasicBlock::IllegalTerminatorException::
    IllegalTerminatorException(BasicBlock  *block, ErrorCode    err_code,
                               std::string &&info, SourceLocation srcloc)
        : MTB::Exception(ErrorLevel::CRITICAL,
                         terminator_exception_get_msg(err_code, block, info, srcloc),
                         std::move(srcloc)),
          block(block), err_code(err_code), ending(nullptr) {
    }
    BasicBlock::IllegalTerminatorException::
    IllegalTerminatorException(BasicBlock  *block, Instruction *ending,
                               ErrorCode err_code, std::string &&info,
                               SourceLocation srcloc)
        : MTB::Exception(ErrorLevel::CRITICAL,
                         terminator_exception_get_msg(err_code, block, info, srcloc),
                         std::move(srcloc)),
          block(block), err_code(err_code), ending(ending) {
    }
/* |============== [end class BasicBlock::IllegalTerminatorException] ==============| */


inline namespace basic_block_impl {
#define IMPL_InstModifier Instruction::ListT::Modifier
#define IMPL_BRef         BasicBlock::RefT

    static BasicBlock::RefT block_move_all(BasicBlock *from)
    try {
        if (from->get_parent() == nullptr) throw NullException {
            "block_move_all(from)::from",
            {}, CURRENT_SRCLOC_F
        };
        BasicBlock::RefT ret = BasicBlock::Create(from->get_parent());
        if (from->get_modifier().append(ret) == false)
            abort();
        Instruction::ListT &inst_list = from->instruction_list();
        while (inst_list.get_length() != 1) {
            owned<Instruction> inst = inst_list.front();
            inst->unplugThis();
            ret->instruction_list().append(std::move(inst));
        }
        owned<Instruction> terminator = from->swapOutEnding(
            JumpSSA::Create(from, ret)
        );
        ret->set_terminator(terminator->unsafe_try_cast_to_terminator());
        return ret;
    } catch (MTB::EmptySetException const& e) {
        std::cerr << std::endl
                  << osb_fmt("at basic_block_impl::block_move_all"
                             "(BasicBlock *from), line 96") << std::endl;
        throw e;
    }
    static BasicBlock::RefT
    block_move_back(BasicBlock *from, IMPL_InstModifier before_new_begin)
    {
        IMPL_BRef ret = BasicBlock::Create(from->get_parent());
        /* 把 ret 附加在自己后面 */
        if (from->get_modifier().append(ret) == false)
            crash_with_stacktrace(true, CURRENT_SRCLOC_F, "Failed to append current block");
        Instruction::ListT &old_list = from->instruction_list();

        while (true) {
            IMPL_InstModifier modifier = before_new_begin.get_next_iterator();
            if (modifier->ends_basic_block())
                break;

            owned<Instruction> inst = modifier.remove_this();
            ret->append(inst);
        }

        ret->set_terminator(from->swapOutEnding(
            JumpSSA::Create(from, ret)
        ));
        return ret;
    }

#undef IMPL_InstModifier
#undef IMPL_BRef
} // inline namespace basic_block_impl
/* +======================== [class BasicBlock] ========================+ */
inline namespace basic_block_impl {
    using RefT   = BasicBlock::RefT;
    using USetT  = BasicBlock::USetT;
    using TTrait = BasicBlock::TerminatorTraitT;

    static inline RefT
    new_block(Function *parent, BasicBlock::TerminatorTraitT &&trait) {
        RETURN_MTB_OWN(BasicBlock, parent, std::move(trait));
    }

    static inline void
    check_registered_or_throw(BasicBlock *self)
    {
        if (self == nullptr)
            return;
        bool has_error = false;
        std::string_view ptr_name{};
        if (self->get_parent() == nullptr) {
            ptr_name  = "BasicBlock.parent"sv;
            has_error = true;
        } else if (self->reflist_item_proxy().self_node == nullptr) {
            ptr_name  = "Function.RefList.nodeof(BasicBlock)"sv;
            has_error = true;
        }

        if (has_error) {
            throw NullException {
                ptr_name,
                "You can get an iterator only if BasicBlock is"
                " registered to a RefList in a Function"s,
                CURRENT_SRCLOC_F
            };
        }
    }
    static inline void
    check_function_null_throw(Function *parent, SourceLocation &&srcloc)
    {
        if (parent == nullptr) {
            throw NullException {
                "BasicBlock.parent in construct"sv,
                ""s, std::move(srcloc)
            };
        }
    }
} // inline namespace basic_block_impl

/* [===================== [static class BasicBlock] ====================] */
    RefT BasicBlock::Create(Function *parent)
    {
        check_function_null_throw(parent, CURRENT_SRCLOC_F);
        UnreachableSSA::RefT unreachable_ssa = MTB::own<UnreachableSSA>();
        TTrait terminator{unreachable_ssa};
        BasicBlock::RefT ret = new_block(parent, std::move(terminator));
        ret->on_function_plug(parent);
        return ret;
    }

    RefT BasicBlock::CreateWithEnding(Function *parent, Instruction::RefT terminator)
    {
        check_function_null_throw(parent, CURRENT_SRCLOC_F);
        MTB_UNLIKELY_IF (terminator == nullptr) {
            throw NullException {
                "BasicBlock.terminator in construct"sv,
                ""s, CURRENT_SRCLOC_F
            };
        }
        MTB_UNLIKELY_IF (!terminator->ends_basic_block()) {
            throw IBasicBlockTerminator::BadBaseException {
                terminator, ""s,
                CURRENT_SRCLOC_F
            }; // throw BadBaseException
        }

        /* 现在 terminator != null, 可以使用不安全的转换方法. */
        BasicBlock::RefT ret = new_block(parent, {
            terminator.get(),
            terminator->unsafe_try_cast_to_terminator()
        });
        ret->on_function_plug(parent);
        return ret;
    }

    RefT BasicBlock::CreateWithEndingIface(Function *parent, IBasicBlockTerminator *ending)
    {
        check_function_null_throw(parent, CURRENT_SRCLOC_F);
        if (ending == nullptr) {
            throw NullException {
                "BasicBlock.terminator in construct"sv,
                ""s, CURRENT_SRCLOC_F
            };
        }
        return new_block(parent, TTrait{ending});
    }

    RefT BasicBlock::CreateWithEndingTrait(Function *parent, TerminatorTraitT ending_trait)
    {
        using namespace Compatibility;
        check_function_null_throw(parent, CURRENT_SRCLOC_F);
        if (ending_trait.get() == nullptr) {
            throw NullException {
                "BasicBlock.terminator in construct"sv,
                ""s, CURRENT_SRCLOC_F
            };
        }
        if (ending_trait.get_iface()->get_instance() != ending_trait.get()) {
            throw IBasicBlockTerminator::BadBaseException {
                ending_trait.get(),
                osb_fmt(""sv),
                CURRENT_SRCLOC_F
            };
        }
        return new_block(parent, std::move(ending_trait));
    }
/* [==================== [dynamic class BasicBlock] ====================] */
    BasicBlock::BasicBlock(Function *parent, TerminatorTraitT ending)
        : Value(ValueTID::BASIC_BLOCK, LabelType::labelty),
        _parent(parent), _connect_status(ConnectStatus::DISCONNECTED) {
        Instruction *inst = ending;
        _instruction_list.append(ending.get());
        inst->on_parent_plug(this);
        inst->set_parent(this);
        _terminator = ending.get_iface();
    }

    BasicBlock::~BasicBlock()
    {
        if (_connect_status == ConnectStatus::FINALIZED)
            return;

        if (_parent         == nullptr ||
            _connect_status != ConnectStatus::CONNECTED) {
            // 处于游离状态, 则向每个 Instruction 结点发出自己析构的信号.
            for (Instruction *i: _instruction_list)
                i->on_parent_finalize();
        } else {
            // 连接到函数时析构意味着整个函数都要析构, 则向每个 Instruction
            // 都发出函数析构的信号.
            for (Instruction *i: _instruction_list)
                i->on_function_finalize();
        }
        _connect_status = ConnectStatus::FINALIZED;
    }

    Module *BasicBlock::get_module() const
    {
        Function *parent = get_parent();
        if (parent == nullptr)
            return nullptr;
        return parent->get_parent();
    }

/* [================= [BasicBlock as List<Instruction>] ================] */

    void BasicBlock::set_terminator(owned<Instruction> terminator)
    {
        if (_terminator->get_instance() == terminator)
            return;
        if (terminator == nullptr) {
            throw NullException {
                "BasicBlock.terminator"sv,
                osb_fmt("BasicBlock (%"sv,
                get_name_or_id(), ") terminator cannot be null or deleted."
                " Otherwise, the control flow will be broken."sv),
                CURRENT_SRCLOC_F
            }; // throw NullException
        }
        if (!terminator->ends_basic_block()) {
            throw IBasicBlockTerminator::BadBaseException {
                terminator, ""s,
                CURRENT_SRCLOC_F
            }; // throw BadBaseException
        }

        /* 现在 terminator 不是 null, 调用 unsafe 方法执行类型转换是安全的. */
        IBasicBlockTerminator *iface = terminator->unsafe_try_cast_to_terminator();
        InstListT::Modifier old = _instruction_list.rbegin();
        old.replace_this(std::move(terminator));
        _terminator = iface;
    }
    void BasicBlock::set_terminator(TerminatorTraitT terminator)
    {
        if (_terminator == terminator.get_iface())
            return;
        if (terminator == nullptr) {
            throw NullException {
                "BasicBlock.terminator"sv,
                osb_fmt("BasicBlock (%"sv,
                get_name_or_id(), ") terminator cannot be null or deleted."
                " Otherwise, the control flow will be broken."sv),
                CURRENT_SRCLOC_F
            }; // throw NullException
        }

        InstListT::Modifier old = _instruction_list.rbegin();
        old.replace_this(terminator.get());
        _terminator = terminator.get_iface();
    }
    owned<Instruction> BasicBlock::swapOutEnding(owned<Instruction> ending)
    {
        InstListT::Modifier old = _instruction_list.rbegin();
        owned<Instruction> ret{old.get()};
        set_terminator(std::move(ending));
        return ret;
    }

/* [===================== [BasicBlock as CFG Node] =====================] */

    size_t BasicBlock::count_refs_jumps_to() const
    {
        size_t ret = 0;
        for (TargetInfo const& info: _jumps_to)
            ret += info.use_count;
        return ret;
    }
    size_t BasicBlock::count_refs_comes_from() const
    {
        size_t ret = 0;
        for (TargetInfo const& info: _comes_from)
            ret += info.use_count;
        return ret;
    }

    size_t BasicBlock::countJumpsToRef(BasicBlock *jumps_to) const
    {
        if (jumps_to == nullptr)
            return 0;
        auto iter = _jumps_to.find({jumps_to, 0});
        if (iter == _jumps_to.end())
            return 0;
        return iter->use_count;
    }
    size_t BasicBlock::countComesFromRef(BasicBlock *comes_from) const
    {
        if (comes_from == nullptr)
            return 0;
        auto iter = _comes_from.find({comes_from, 0});
        if (iter == _comes_from.end())
            return 0;
        return iter->use_count;
    }

/* [================ [BasicBlock::IBasicBlockTerminator Only] ================] */

    size_t BasicBlock::addJumpsTo(BasicBlock *jumps_to)
    {
        MTB_UNLIKELY_IF (jumps_to == nullptr) {
            throw NullException {
                "BasicBlock.jumps_to[...]"sv,
                osb_fmt("BasicBlock jumps_to and comes_from"
                    " setter requires the argument not be null, while"
                    " BasicBlock "sv, (void*)this , "(%", get_name_or_id(), ")"
                    " got one"),
                CURRENT_SRCLOC_F
            }; // throw NullException
        }

        TargetInfo      info{jumps_to, 1};
        USetT::iterator target_it = _jumps_to.find(info);

        if (target_it == _jumps_to.end()) {
            _jumps_to.insert(info);
            return 1;
        }

        size_t const& use_count = target_it->use_count;
        size_t &mut_use_count   = const_cast<size_t&>(use_count);
        mut_use_count++;
        return mut_use_count;
    }
    bool BasicBlock::removeJumpsTo(BasicBlock *jumps_to)
    {
        TargetInfo info{jumps_to, 1};
        USetT::iterator target_it = _jumps_to.find(info);
        if (target_it == _jumps_to.end())
            return false;

        size_t const& use_count = target_it->use_count;
        if (use_count == 1) {
            _jumps_to.erase(target_it);
        } else {
            size_t &mut_use_count = const_cast<size_t&>(use_count);
            mut_use_count--;
        }
        return true;
    }
    void BasicBlock::clearJumpsTo(BasicBlock *jumps_to)
    {

        TargetInfo info{jumps_to, 1};
        USetT::iterator target_it = _jumps_to.find(info);
        if (target_it == _jumps_to.end())
            return;
        _jumps_to.erase(target_it);
    }

    size_t BasicBlock::addComesFrom(BasicBlock *comes_from)
    {
        MTB_UNLIKELY_IF (comes_from == nullptr) {
            throw NullException {
                "BasicBlock.comes_from[...]"sv,
                osb_fmt("BasicBlock jumps_to and comes_from"
                    " setter requires the argument not be null, while"
                    " BasicBlock "sv, (void*)this , "(%", get_name_or_id(), ")"
                    " got one"),
                CURRENT_SRCLOC_F
            }; // throw NullException
        }

        TargetInfo      info{comes_from, 1};
        USetT::iterator target_it = _comes_from.find(info);

        if (target_it == _comes_from.end()) {
            _comes_from.insert(info);
            return 1;
        }

        size_t const& use_count = target_it->use_count;
        size_t &mut_use_count   = const_cast<size_t&>(use_count);
        mut_use_count++;
        return mut_use_count;
    }

    bool BasicBlock::removeComesFrom(BasicBlock *comes_from)
    {
        TargetInfo info{comes_from, 1};
        USetT::iterator target_it = _comes_from.find(info);
        if (target_it == _comes_from.end())
            return false;

        size_t const& use_count = target_it->use_count;
        if (use_count == 1) {
            _comes_from.erase(target_it);
        } else {
            size_t &mut_use_count = const_cast<size_t&>(use_count);
            mut_use_count--;
        }
        return true;
    }

    void BasicBlock::clearComesFrom(BasicBlock *comes_from)
    {

        TargetInfo info{comes_from, 1};
        USetT::iterator target_it = _comes_from.find(info);
        if (target_it == _comes_from.end())
            return;
        _comes_from.erase(target_it);
    }

    BasicBlock *BasicBlock::get_default_next() const
    {
        IBasicBlockTerminator *terminator = get_terminator();
        Instruction *terminator_inst = terminator->get_instance();
        auto *jb = dynamic_cast<JumpBase*>(terminator_inst);
        if (jb == nullptr)
            return nullptr;
        return jb->get_default_target();
    }
/* [================= [BasicBlock as List<Instruction>] ================] */
    void BasicBlock::append(owned<Instruction> inst)
    {
        MTB_UNLIKELY_IF (inst == nullptr) {
            throw NullException {
                "BasicBlock.append(ending)::ending"sv,
                "Instruction as BasicBlock child cannot be null"s,
                CURRENT_SRCLOC_F
            }; // throw NullException
        }
        MTB_UNLIKELY_IF (inst->ends_basic_block()) {
            throw IllegalTerminatorException {
                this, inst,
                IllegalTerminatorException::INST_ILLEGAL,
                "BasicBlock.append(ending)::ending cannot "
                "be terminator"s, CURRENT_SRCLOC_F
            };
        }

        /* 现在 inst 既不是 null, 也不是 terminator 了. 执行插入操作. */
        InstListT::Modifier modifier = _instruction_list.rbegin();

        /* 状态检查、状态设置等操作均在该方法内完成. */
        modifier.prepend(std::move(inst));
    }
    void BasicBlock::prepend(owned<Instruction> inst)
    {
        MTB_UNLIKELY_IF (inst == nullptr) {
            throw NullException {
                "BasicBlock.prepend(front)::front"sv,
                "Instruction as BasicBlock child cannot be null"s,
                CURRENT_SRCLOC_F
            }; // throw NullException
        }
        MTB_UNLIKELY_IF (inst->ends_basic_block()) {
            throw IllegalTerminatorException {
                this, inst,
                IllegalTerminatorException::INST_ILLEGAL,
                "BasicBlock.prepend(front)::front cannot "
                "be terminator"s, CURRENT_SRCLOC_F
            };
        }
        /* 现在 inst 既不是 null, 也不是 terminator 了. 执行插入操作. */
        InstListT::Modifier modifier = _instruction_list.begin();

        /* 状态检查、状态设置等操作均在该方法内完成. */
        modifier.prepend(std::move(inst));
    }

    IBasicBlockTerminator::TraitOwner
    BasicBlock::appendOrReplaceBack(owned<Instruction> ending)
    {
        if (ending == nullptr) {
            throw NullException {
                "BasicBlock.appendOrReplaceBack(ending)::ending"sv,
                "", CURRENT_SRCLOC_F
            }; // throw NullException
        }
        if (!ending->ends_basic_block()) {
            append(std::move(ending));
            return {};
        }
        IBasicBlockTerminator *terminator = ending->unsafe_try_cast_to_terminator();
        TerminatorTraitT ret = get_terminator_trait();
        set_terminator(terminator);
        return ret;
    }

    RefT BasicBlock::split(Instruction *new_begin)
    try {
        if (new_begin == nullptr ||
            new_begin->get_connect_status() != ConnectStatus::CONNECTED ||
            new_begin->get_parent() != this) {
            debug_print("[Line %d] New begin %p: Connect Status %d(CONNECTED=%d), parent %p\n",
                   __LINE__, new_begin, int(new_begin->get_connect_status()),
                   int(ConnectStatus::CONNECTED), new_begin->get_parent());
            return nullptr;
        }

        if (new_begin == _instruction_list.front()) {
            debug_print("[%d:\033[01;33m%s\033[0m]Split from front: new_begin %p, front %p\n",
                        __LINE__, __PRETTY_FUNCTION__, new_begin, _instruction_list.front().get());
            return block_move_all(this);
        }

        debug_print("[%d:\033[01;33m%s\033[0m]Split from middle: new_begin %p, front %p\n",
                    __LINE__, __PRETTY_FUNCTION__, new_begin, _instruction_list.front().get());
        return block_move_back(
            this, new_begin->get_modifier().get_prev_iterator());
    } catch (EmptySetException &e) {
        std::cerr << std::endl
                  << osb_fmt("at ", source_location_stringfy(CURRENT_SRCLOC_F))
                  << std::endl;
        throw e;
    }
/* [=============== [BasicBlock as List<BasicBlock>.Node] ==============] */
    BasicBlock::ListT::Iterator BasicBlock::get_iterator()
    {
        if (_parent == nullptr) throw NullException {
            "BasicBlock.parent"sv,
            osb_fmt("BasicBlock ", get_name_or_id(),
                " requires to be connected to parent function"sv),
            CURRENT_SRCLOC_F
        }; // throw NullException
        if (reflist_item_proxy().self_node == nullptr) throw NullException {
            "BasicBlock.reflist_item_proxy.self_node"sv,
            osb_fmt("BasicBlock ", get_name_or_id(),
                " requires to be connected to parent function list"sv),
            CURRENT_SRCLOC_F
        };
        return {_reflist_item_proxy.self_node, &_parent->body()};
    }

/* [=================== [BasicBlock::override parent] ==================] */
    void BasicBlock::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }
/* [======================= [BasicBlock::signals] ======================] */
    void BasicBlock::on_function_plug(Function *parent) {
        _parent = parent;
        _connect_status = ConnectStatus::CONNECTED;
    }
    Function *BasicBlock::on_function_unplug() {
        _connect_status = ConnectStatus::DISCONNECTED;
        return _parent;
    }
    void BasicBlock::on_function_finalize()
    {
        for (Instruction *i: _instruction_list)
            i->on_function_finalize();
        _connect_status = ConnectStatus::FINALIZED;
    }
/* [=================== [deprecated class BasicBlock] ==================] */
    [[deprecated("该构造函数不是异常安全的. 请更换为 `BasicBlock::Create(parent)`.")]]
    BasicBlock::BasicBlock(Function *parent)
        : Value(ValueTID::BASIC_BLOCK, LabelType::labelty),
        _parent(parent) {
        _instruction_list.setInlineInit();
        Instruction *default_ending = new UnreachableSSA();

        default_ending->set_parent(this);
        default_ending->on_parent_plug(this);
        _instruction_list.append(default_ending);
    }

    [[deprecated("该构造函数不是异常安全的; 同时, 由于涉及重载, 该函数被弃用. "
                 "请更换为 `BasicBlock::CreateWithEnding(parent, ending)`.")]]
    BasicBlock::BasicBlock(Function *parent, Instruction::RefT ending)
        : Value(ValueTID::BASIC_BLOCK, LabelType::labelty),
        _parent(parent) {
        _instruction_list.setInlineInit();

        MTB_UNLIKELY_IF (ending->ends_basic_block() == false) {
            Instruction &inst = *ending;
            /* 好像就没几个demagle库能带进赛场的, 所以各位还是理解一下magle过的类型名吧.
            * 比如说, Instruction类编译后的符号名称可能是:
            * - `N4MYGL2IR11InstructionE` (Itanium ABI)
            * - `.?AVInstruction@IR@MYGL@@` (MSVC ABI) */
            throw IllegalTerminatorException {
                this, IllegalTerminatorException::INST_ILLEGAL,
                osb_fmt("Ending instruction should implement interface `IBasicBlockTerminator` ",
                        "while instruction ", ending->get_name_or_id(),
                        " (type", typeid(inst).name(),") isn't\n"),
                CURRENT_SRCLOC_F
            };
        }

        ending->set_parent(this);
        ending->on_parent_plug(this);
        _instruction_list.append(std::move(ending));
    }
/* |====================== [end class BasicBlock] ======================| */

} // namespace MYGL::IR

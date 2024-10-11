#include "base/mtb-compatibility.hxx"
#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-instruction-base.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "irbase-capatibility.private.hxx"
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

#define INST_CONNECTED(inst)    ((inst)->get_connect_status() == Instruction::ConnectStatus::CONNECTED)
#define INST_DISCONNECTED(inst) ((inst)->get_connect_status() == Instruction::ConnectStatus::DISCONNECTED)
#define INST_FINALIZED(inst)    ((inst)->get_connect_status() == Instruction::ConnectStatus::FINALIED)

namespace MYGL::IR {

/** @enum Instruction::OpCode */

inline namespace instruction_impl {

    using OpCodeNameMapT = std::unordered_map<Instruction::OpCode::self_t, std::string_view>;
    using OpCodeT = Instruction::OpCode::self_t;
    using namespace std::string_view_literals;
    using namespace MTB::Compatibility;

    static OpCodeNameMapT opcode_name_map {
        {OpCodeT::PHI,  "phi"}, {OpCodeT::UNREACHABLE, "unreachable"},
        {OpCodeT::JUMP, "jump"}, {OpCodeT::BR, "br"}, {OpCodeT::SWITCH, "switch"},
        {OpCodeT::MOVE, "move"},
        {OpCodeT::ALLOCA, "alloca"},
        {OpCodeT::LOAD, "load"}, {OpCodeT::STORE, "store"},

        /* Cast */
        {OpCodeT::ITOF, "sitofp"}, {OpCodeT::UTOF, "uitofp"},
        {OpCodeT::FTOI, "ftoi"},
        {OpCodeT::ZEXT, "zext"}, {OpCodeT::SEXT, "sext"},
        {OpCodeT::BITCAST, "bitcast"},
        {OpCodeT::TRUNC, "trunc"},
        {OpCodeT::FPEXT, "fpext"}, {OpCodeT::FPTRUNC, "fptrunc"},

        {OpCodeT::INEG, "ineg"},
        {OpCodeT::FNEG, "fneg"},
        {OpCodeT::NOT,  "not"},

        /* Binary Operaion */
        {OpCodeT::ADD,  "add"},  {OpCodeT::SUB,  "sub"},  {OpCodeT::MUL, "mul"},
        {OpCodeT::FADD, "fadd"}, {OpCodeT::FSUB, "fsub"}, {OpCodeT::FMUL,"fmul"},
        {OpCodeT::SDIV, "sdiv"}, {OpCodeT::UDIV, "udiv"}, {OpCodeT::FDIV, "fdiv"},
        {OpCodeT::SREM, "srem"}, {OpCodeT::UREM, "urem"}, {OpCodeT::FREM, "frem"},
        {OpCodeT::AND,  "and"}, {OpCodeT::OR, "or"}, {OpCodeT::XOR, "xor"},
        {OpCodeT::SHL,  "shl"}, {OpCodeT::LSHR, "lshr"}, {OpCodeT::ASHR, "ashr"},

        {OpCodeT::CALL, "call"}, {OpCodeT::RET, "ret"},
        {OpCodeT::GET_ELEMENT_PTR, "getelementptr"},
        {OpCodeT::MEMMOVE, "memmove"},
        {OpCodeT::MEMSET, "memset"},

        {OpCodeT::ICMP, "icmp"}, {OpCodeT::FCMP, "fcmp"}
    }; // <map[opcode]=>{name}> opcode_name_map

/* ========== [Instruction Error Handling] ========== */
    static void print_all_users(Instruction *self)
    {
        for (Use *use: self->list_as_usee()) {
            User *user = use->get_user();
            std::string user_name = user->get_name_or_id();
            if (Instruction *i = dynamic_cast<Instruction*>(user);
                i != nullptr) {
                Instruction::OpCode opcode = i->get_opcode();
                std::cout << osb_fmt("  [Instruction (id=", user_name,
                                     ", address=", i,
                                     ", opcode=", opcode.getString(),
                                     ")]") << std::endl;
            } else {
                std::cout << osb_fmt("  [User (id=", user_name,
                                     ", address=", i, ")]")
                          << std::endl;
            }
        }
    }
} // namespace instruction_opcode

std::string_view Instruction::OpCode::getString() const
{
    using namespace instruction_impl;
    const OpCodeNameMapT& name_map = opcode_name_map;
    if (name_map.find(self) == name_map.end())
        return "<undefined>"sv;
    return name_map.at(self);
}

/** end enum Instruction::OpCode */

/** @class Instruction.ListAction */
    bool Instruction::ListAction::
    on_modifier_append_preprocess(Modifier *it, Instruction *elem)
    {
        if (it->node_ends()) {
            debug_print("[%s:%d] it{%p}: 私有尾结点不能向后附加元素\n",
                        __FILE__, __LINE__, it);
            return false;
        }
        debug_print("[%s:%d] new_inst{%p}: opcode{%*s}, parent{%p}\n",
                __FILE__, __LINE__, elem,
                int(elem->get_opcode().getString().length()),
                elem->get_opcode().getString().begin(),
                elem->get_parent());
        
        // 情况0: 列表是空的(初始化), 则附加的元素必须是终止子
        ListT *list = it->list;
        if (list->empty())
            return INST_DISCONNECTED(elem) && elem->ends_basic_block();

        // 然后缓存一次父结点. 倘若没有缓存，那么就不会设置父结点.
        old_parent = list->back()->get_parent();
        // 情况1: 列表有元素
        return INST_DISCONNECTED(elem) && !elem->ends_basic_block();
    }
    void Instruction::ListAction::
    on_modifier_append(Modifier *modifier, ElemT *elem)
    {
        if (old_parent == nullptr)
            return;
        elem->on_parent_plug(old_parent);
    }

    bool Instruction::ListAction::
    on_modifier_prepend_preprocess(Modifier *it, Instruction *elem)
    {
        if (it->node_begins()) {
            debug_print("[%s:%d] it{%p}: 私有头结点不能向前附加元素",
                        __FILE__, __LINE__, it);
            return false;
        }
        debug_print("[%s:%d] new_inst{%p}: opcode{%*s}, parent{%p}\n",
                __FILE__, __LINE__, elem,
                int(elem->get_opcode().getString().length()),
                elem->get_opcode().getString().begin(),
                elem->get_parent());
        
        // 情况0: 列表是空的(初始化), 则附加的元素必须是终止子
        ListT *list = it->list;
        if (list->empty())
            return INST_DISCONNECTED(elem) && elem->ends_basic_block();

        // 然后缓存一次父结点. 倘若没有缓存，那么就不会设置父结点.
        old_parent = list->back()->get_parent();
        // 情况1: 列表有元素
        return INST_DISCONNECTED(elem) && !elem->ends_basic_block();
    }
    void Instruction::ListAction::
    on_modifier_prepend(Modifier *it, Instruction *elem)
    {
        if (old_parent == nullptr)
            return;
        elem->on_parent_plug(old_parent);
    }

    bool Instruction::ListAction::
    on_modifier_replace_preprocess(Modifier *it, Instruction *new_inst)
    {
        Instruction *old_inst = it->get();
        if (new_inst == old_inst)
            return false;
        if (!INST_DISCONNECTED(new_inst))
            return false;
        if (old_inst->ends_basic_block() != new_inst->ends_basic_block())
            return false;
        /* 发送断连信号 */
        old_parent = old_inst->on_parent_unplug();
        old_inst->set_connect_status(ConnectStatus::DISCONNECTED);
        return true;
    }
    void Instruction::ListAction::
    on_modifier_replace(Modifier *modifier, Instruction *old_inst)
    {
        if (old_parent == nullptr) {
            debug_print("[%s:%d] replace(): 旧指令的父结点缓存不可能是 null. "
                    "检查一下程序，这是怎么回事?\n", __FILE__, __LINE__);
        }
        Instruction *new_inst = modifier->get();
        new_inst->on_parent_plug(old_parent);
    }

    bool Instruction::ListAction::on_modifier_disable_preprocess(Modifier *it)
    {
        Instruction *self = it->get();
        if (self->ends_basic_block())
            return false;
        old_parent = self->on_parent_unplug();
        return true;
    }
/** end class Instruction.ListAction */

/** @class Instruction */
    using InstListT = Instruction::ListT;

    IBasicBlockTerminator *
    Instruction::TryCastToTerminator(Instruction *self)
    {
        if (self == nullptr)
            return nullptr;
        return self->unsafe_try_cast_to_terminator();
    }

    Instruction::Instruction(ValueTID tid,  Type *value_type,
                             OpCode opcode, BasicBlock *parent)
        : User(tid, value_type),
        _opcode(opcode),
        _parent(parent),
        _connect_status(ConnectStatus::DISCONNECTED), // 没有连接, 设为 DISCONNECTED
        _ibasicblock_terminator_iface_offset(0),
        _instance_is_basicblock_terminator(false) {
    }

    Instruction::~Instruction()
    {
        if (_connect_status == ConnectStatus::FINALIZED ||
            _connect_status == ConnectStatus::REPARENT)
            return;
        if (!_list_as_usee.empty()) {
            std::clog << osb_fmt("Instruction id %", get_name_or_id(),
                                 "[address", this, "] still has [",
                                 _list_as_usee.size(), "] users!")
                      << std::endl;
            instruction_impl::print_all_users(this);
            std::terminate();
        }
    }

/* ========== [Instruction as User] ========== */
    bool Instruction::uses_global_variable() const
    {
        bool ret = false;
        traverseOperands([&ret](Value const& value){
            ValueTID tid = value.get_type_id();
            ret |= (tid == ValueTID::GLOBAL_VARIABLE);
            ret |= (tid == ValueTID::FUNCTION);
            return ret;
        });
        return ret;
    }
    bool Instruction::uses_argument() const
    {
        bool ret = false;
        traverseOperands([&ret](Value const& value){
            ret |= (value.get_type_id() == ValueTID::ARGUMENT);
            return ret;
        });
        return ret;
    }

/* ========== [Instruction as List<BasicBlock>.Node] ========== */

    Instruction *Instruction::get_prev() const {
        using Iterator = ListT::Iterator;
        if (_proxy.self_node == nullptr)
            return nullptr;
        Iterator it = _proxy.get_iterator();
        return it.get_prev_iterator().get();
    }
    Instruction *Instruction::get_next() const {
        using Iterator = ListT::Iterator;
        if (_proxy.self_node == nullptr)
            return nullptr;
        Iterator it = _proxy.get_iterator();
        return it.get_next_iterator().get();
    }

    owned<Instruction> Instruction::unplugThis() noexcept(false)
    {
        owned<Instruction> ret = get_modifier().remove_this();
        _connect_status = ConnectStatus::DISCONNECTED;
        return ret;
    }
    owned<Instruction> Instruction::removeThisIfUnused() noexcept(false)
    {
        if (!get_list_as_usee().empty())
            return nullptr;

        owned<Instruction> ret = unplugThis();
        for (Use *i: list_as_user()) {
            if (Value *usee = i->get_usee(); usee != nullptr)
                usee->removeUseAsUsee(i);
        }
        _connect_status = ConnectStatus::FINALIZED;
        return ret;
    }

/* ========== [Instruction as Child of BasicBlock] ========== */

    Function *Instruction::get_function() const
    {
        BasicBlock *parent = get_parent();
        if (parent == nullptr)
            return nullptr;
        return parent->get_parent();
    }

    Module *Instruction::get_module() const
    {
        Function *parent = get_function();
        if (parent == nullptr)
            return nullptr;
        return parent->get_parent();
    }

    bool Instruction::isUsedOutsideOfBlock(BasicBlock const* block) const
    {
        for(Use *use: get_list_as_usee()) {
            User        *user      = use->get_user();
            Instruction *user_inst = dynamic_cast<Instruction*>(user);
            /* 几乎不会出现指令被非指令使用的情况...
             * 遇到这种情况直接返回false吧. */
            MTB_UNLIKELY_IF (user_inst == nullptr)
                return false;

            /* 假定使用自己的指令是Phi函数, 那么还要判断Phi函数是否正确记录了
             * 自己所属的基本块结点. */
            if (user_inst->get_type_id() == IRBase::ValueTID::PHI_SSA) {
                PhiSSA *phi_user = static_cast<PhiSSA*>(user);
                for (auto &[come_block, value]: phi_user->get_operands()) {
                    if (come_block != block && this == value)
                        return false;
                }
            } // end if (user_inst is PhiSSA)

            if (user_inst->get_parent() != block)
                return false;
        }
        return true;
    }

    bool Instruction::isUsedOutsideOfBlocks(BasicBlockUSetT const &blocks) const
    {
        using namespace capatibility;

        /* 大体步骤和isUsedOutsideOfBlock方法一致，只是把使用者指令是否为block
         * 变为使用者指令是否在blocks集合内. */
        for (Use *use: get_list_as_usee()) {
            User *user = use->get_user();
            Instruction *user_inst = dynamic_cast<Instruction*>(user);
            MTB_UNLIKELY_IF (user_inst == nullptr)
                return false;

            if (user_inst->get_type_id() == ValueTID::PHI_SSA) {
                PhiSSA *phi_user = static_cast<PhiSSA*>(user);
                for (auto &[come_block, value]: phi_user->get_operands()) {
                    if (!contains(blocks, come_block) &&
                        this == value)
                        return false;
                }
            } // end if (user_inst is PhiSSA)

            if (!contains(blocks, user_inst->get_parent()))
                return false;
        }

        return true;
    }

/** end class Instruction */

/** interface IBasicBlockTerminator */
namespace ibb_terminator_impl {
    struct ReplaceOnceClosure {
        BasicBlock *old_pattern;
        BasicBlock *new_target;
    public:
        always_inline bool replace_terminates(BasicBlock *old_real, BasicBlock *&out_new)
        {
            if (new_target == nullptr)
                return true;
            if (old_pattern != old_real)
                return false;
            out_new = new_target;
            new_target = nullptr;
            return false;
        }

        bool operator()(BasicBlock *old_real, BasicBlock *&out_new) {
            return replace_terminates(old_real, out_new);
        }
    }; // struct ReplaceOnceClosure
} // namespace ibb_terminator_impl

extern "C" {
static std::string opcode_mismatch_make_msg(
                                Instruction::OpCode err_opcode,
                                std::string mismatch_reason,
                                std::string additional_info,
                                SourceLocation src_location) {
    return Compatibility::osb_fmt("OpCodeMismatchException at ",
        MTB::source_location_stringfy(src_location),
        " (opcode ): ",   err_opcode.getString(), "\n"
        "\tBecause of: ", mismatch_reason, "\n"
        "\tMessage: ", additional_info);
}

static std::string badbase_make_msg(Instruction *instance,
                                    std::string const& additional_msg,
                                    SourceLocation srcloc) {
    return Compatibility::osb_fmt("IBasicBlockTerminator::BadBaseException at ",
        MTB::source_location_stringfy(srcloc),
        " (instance: )", instance, "\n"
        "Message: ", std::move(additional_msg), "\n");
}
}

IBasicBlockTerminator::BadBaseException::BadBaseException(Instruction *instance,
                            std::string &&additional_msg, SourceLocation srcloc)
    : MTB::Exception(ErrorLevel::CRITICAL,
                    badbase_make_msg(instance, additional_msg, srcloc)),
      instance(instance), additional_msg(std::move(additional_msg)) {
}

/* ================ [dynamic class IBasicBlockTerminator] ================ */
    IBasicBlockTerminator::TraitOwner &
    IBasicBlockTerminator::TraitOwner::operator=(Instruction *inst) {
        if (inst == nullptr)
            return (*this = nullptr);
        if (inst == instance)
            return *this;
        if (!inst->ends_basic_block())
            return (*this = nullptr);
        instance = inst;
        iface    = Instruction::TryCastToTerminator(inst);
        return *this;
    }

    IBasicBlockTerminator::~IBasicBlockTerminator() = default;

    void IBasicBlockTerminator::replace_target_once(BasicBlock *old,
                                                    BasicBlock *new_target)
    {
        using namespace ibb_terminator_impl;
        replace_target_by(ReplaceOnceClosure{old, new_target});
    }
/** end interface IBasicBlockTerminator */

/** class OpCodeMismatchException */
Instruction::OpCodeMismatchException::OpCodeMismatchException(
                            Instruction::OpCode err_opcode,
                            std::string mismatch_reason,
                            std::string additional_info,
                            SourceLocation src_location)
    : MTB::Exception(ErrorLevel::CRITICAL,
                     opcode_mismatch_make_msg(err_opcode,
                            mismatch_reason, additional_info,
                            src_location),
                     src_location),
      opcode(err_opcode),
      mismatch_reason(mismatch_reason) {}

} // namespace MYGL::IR

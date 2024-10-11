#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include "ir-instruction-shared.private.hxx"
#include "irbase-capatibility.private.hxx"
#include <string_view>

#define ST_CONNECTED(status) ((status) == Instruction::ConnectStatus::CONNECTED)

namespace MYGL::IR {
    using namespace std::string_view_literals;
    using namespace std::string_literals;

/** @class PhiSSA */
namespace phi_ssa_impl {
    delegate bvpair_vgetter {
        PhiSSA     *self;
        BasicBlock *block;
    public:
        Value *operator()() const {
            return self->get_operands().at(block).value;
        }
    }; // delegate bvpair_vgetter

    delegate bvpair_vsetter {
        PhiSSA     *self;
        BasicBlock *block;
    public:
        Use::SetResult operator()(Value *rhs)
        {
            /* `rhs` 为 `null` 是移除指令, 所以执行 `remove_this()` */
            if (rhs == nullptr) {
                return remove_this();
            }
            MTB_UNLIKELY_IF (rhs->get_value_type() != self->get_value_type())
                return Use::SetResult{false};

            auto &operands = self->operands();
            auto self_it = operands.find(block);
            if (self_it == operands.end())
                return Use::SetResult{false};
            self_it->second.value = rhs;
            return Use::SetResult{false};
        }

        Use::SetResult remove_this()
        {
            auto &operands = self->operands();
            auto self_it = operands.find(block);
            if (self_it == operands.end())
                return Use::SetResult{false};
            operands.erase(self_it);
            return Use::SetResult{true};
        }
    }; // struct bvpair_vsetter

} // namespace phi_ssa_impl

/* ========== [ public class PhiSSA ] ========== */
    PhiSSA::PhiSSA(BasicBlock *parent, Type *value_type, size_t id)
        : Instruction(ValueTID::PHI_SSA, value_type, OpCode::PHI) {
        _id = id;
        _parent = parent;
    }
    PhiSSA::~PhiSSA() = default;

    bool PhiSSA::setValueFrom(BasicBlock *block, owned<Value> value)
    {
        using namespace phi_ssa_impl;

        /* 检查 Value 的类型是否与自己的返回类型匹配 */
        if (value->get_value_type() != get_value_type()) {
            throw ValueTypeUnmatchException {
                get_value_type(), value->get_value_type(),
                "Value type does not match return type of this",
                CURRENT_SRCLOC_F
            }; 
        }

        auto it = _operands.find(block);
        if (it == _operands.end()) {
        /* 没有注册 block, 则要为新的 BasicBlock 注册两个 Use */
            auto [it, success] = _operands.insert({
                block, {std::move(value)}
            });
            /* 注册 `Value`
             * `BasicBlock` 只是 Value 的入口条件, 不作为操作数处理. */
            Use *uvalue = addValue(bvpair_vgetter{this, block},
                                   bvpair_vsetter{this, block});
            it->second.use = uvalue;
        } else {
        /* 注册了 block, 则直接移动 */
            it->second.value = std::move(value);
        }
        return true;
    }

    BasicBlock *PhiSSA::findIncomingBlock(Value *value)
    {
        for (auto &[block, vupair]: _operands) {
            if (vupair.value == value)
                return block;
        }
        return nullptr;
    }

    owned<Value> PhiSSA::remove(BasicBlock *block)
    {
        auto iter = _operands.find(block);
        if (iter == _operands.end())
            return nullptr;

        ValueUsePair &vupair = iter->second;
        owned<Value> ret = std::move(vupair.value);
        owned<Use>   use = vupair.use->get_modifier().remove_this();
        _operands.erase(iter);
        return ret;
    }

    bool PhiSSA::is_complete() const
    {
        BasicBlock *parent = get_parent();
        if (parent == nullptr)
            return false;
        for (auto &[b, v]: _operands) {
            if (parent->hasComesFrom(b))
                return false;
        }
        return true;
    }

    void PhiSSA::accept(IValueVisitor &visitor)
    {
        visitor.visit(this);
    }

    // void PhiSSA::registerThis(BasicBlock *parent)
    // {
    //     MTB_UNLIKELY_IF (parent == nullptr) {
    //         throw NullException {
    //             "PhiSSA.parent",
    //             "Encounted null when setting parent basic block for a Phi node",
    //             CURRENT_SRCLOC_F
    //         };
    //     }
    //     if (parent == get_parent()) return;
    //     BasicBlock *orig = get_parent();
    //     _parent = parent;
    //     if (!is_complete())
    //         _parent = orig;
    // }

    size_t PhiSSA::get_operand_nmemb() const
    {
        return 2 * _operands.size();
    }

    Value *PhiSSA::operator[](size_t index)
    {
        size_t index_backup = index;
        if (index < 0) return nullptr;
        for (auto &[b, v]: _operands) {
            switch (index) {
                case 0: return b;
                case 1: return v;
                default: index--;
            }
        }
        throw std::out_of_range {
            std::format("This PhiSSA has only {} items while you try to get item {}",
                        get_operand_nmemb(), index_backup)
        };
    }

    void PhiSSA::traverseOperands(ValueAccessFunc fn)
    {
        for (auto &[b, v]: _operands) {
            fn(*b); fn(*v);
        }
    }

    void PhiSSA::on_parent_finalize()
    {
        for (auto &[b, c]: _operands)
            c.value->removeUseAsUsee(c.use);
        _operands.clear();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
    void PhiSSA::on_function_finalize() {
        _operands.clear();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
/** end class PhiSSA */

/** @class UnreachableSSA */
    void UnreachableSSA::on_parent_plug(BasicBlock *parent)
    {
        _parent = parent;
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void UnreachableSSA::accept(IValueVisitor &visitor)
    {
        visitor.visit(this);
    }

    UnreachableSSA::~UnreachableSSA() = default;
/** end class UnreachableSSA */

/** @class JumpBase */
    JumpBase::JumpBase(BasicBlock *parent, ValueTID type_id, OpCode opcode, BasicBlock *default_target)
        : Instruction(type_id, VoidType::voidty, opcode, parent),
          IBasicBlockTerminator(this),
          _default_target(default_target) {
        MYGL_IBASICBLOCK_TERMINATOR_INIT_OFFSET();
#   if __DEBUG__ != 0
        printf("JumpBase:  this = %p, vthis = %p, offset = %hhd, "
            "iface_calc = %p, real iface=%p\n",
            this, iterminator_virtual_this, _ibasicblock_terminator_iface_offset,
            unsafe_try_cast_to_terminator(), static_cast<IBasicBlockTerminator*>(this));
#   endif
        MYGL_ADD_BBLOCK_PROPERTY(default_target);
    }

    void JumpBase::set_default_target(BasicBlock *target)
    {
        if (_default_target == target)
            return;
        Use *use = _list_as_user.at(0);

        /* 旧值不为NULL, 则破坏旧连接关系 */
        if (_parent != nullptr && _default_target != nullptr) {
            _parent->removeJumpsTo(_default_target);
            _default_target->removeComesFrom(_parent);
            _default_target->removeUseAsUsee(use);
        }
        /* 新值不为NULL, 则重建连接关系 */
        if (_parent != nullptr && target != nullptr) {
            _parent->addJumpsTo(target);
            target->addComesFrom(_parent);
            target->addUseAsUsee(use);
        }
        _default_target = target;
    }

    void JumpBase::_register_target(BasicBlock *target)
    {
        if (_parent != nullptr && target != nullptr) {
            _parent->addJumpsTo(target);
            int c = target->addComesFrom(_parent);
        }
#   if __DEBUG__ != 0
        if (_parent == nullptr)
            printf("JB:line %d: parent null\n", __LINE__);
        if (target == nullptr)
            printf("JB:line %d: target null\n", __LINE__);
#   endif
    }

    void JumpBase::_unregister_target(BasicBlock *target)
    {
        if (_parent != nullptr && target != nullptr) {
            _parent->removeJumpsTo(target);
            target->removeComesFrom(_parent);
        }
    }
/** end class JumpBase */

/** @class JumpSSA */
    JumpSSA::RefT JumpSSA::Create(BasicBlock *parent, BasicBlock *target)
    {
        if (parent == nullptr) throw NullException {
            "JumpSSA.parent"sv, {}, CURRENT_SRCLOC_F
        };
        if (target == nullptr) throw NullException {
            "JumpSSA.target"sv, {}, CURRENT_SRCLOC_F
        };
        RETURN_MTB_OWN(JumpSSA, parent, target);
    }

    JumpSSA::JumpSSA(BasicBlock *parent, BasicBlock *target)
        : JumpBase(parent, ValueTID::JUMP_SSA, OpCode::JUMP, target) {
        /* 由于此时还没有接收到连接到父结点的信号, 所以不做任何初始化, 包括基本块
         *  CFG 结点的维护操作.
         * 在 Instruction 的 C1 构造函数中就有设置结点状态的代码, 这里不做重复. */
    }
    JumpSSA::~JumpSSA() = default;

    void JumpSSA::accept(IValueVisitor &visitor) {
        return visitor.visit(this);
    }

    void JumpSSA::clean_targets()
    {
        _unregister_target(_default_target);
        _default_target = nullptr;
    }

/** ========= [ JumpSSA: Signal Handlers ] ========= */
    BasicBlock *JumpSSA::on_parent_unplug()
    {
        _unregister_target(_default_target);
        /* 链式调用父类的断联信号处理函数, 就像信号系统一样 */
        return Instruction::on_parent_unplug();
    }
    void JumpSSA::on_parent_plug(BasicBlock *parent)
    {
        if (ST_CONNECTED(_connect_status)) {
            debug_print("[%s:%d]: JumpSSA[%p] 在被连接到 [%p] 的同时"
                        "又被 [%p] 连接了一次\n",
                        __FILE__, __LINE__, this, _parent, parent);
            return;
        }
        Instruction::on_parent_plug(parent);

        if (_parent == nullptr) {
            debug_print("JumpSSA line %d: DEAD PARENT\n", __LINE__);
        } else {
            debug_print("JumpSSA line %d: %p parent addr %p\n",
                        __LINE__, this, parent);
        }

        _register_target(_default_target);
        _connect_status = ConnectStatus::CONNECTED;
    }
    void JumpSSA::on_parent_finalize()
    {
        JumpSSA::clean_targets();
        _connect_status = ConnectStatus::FINALIZED;
    }
    void JumpSSA::on_function_finalize()
    {
        JumpSSA::clean_targets();
        _connect_status = ConnectStatus::FINALIZED;
    }

/** ============ [ deprecated methods ] ============ */
    // [[deprecated("")]]
    // void JumpSSA::registerThis(BasicBlock *parent)
    // {
    //     if (_parent != nullptr && _default_target != nullptr) {
    //         _parent->removeJumpsTo(get_target());
    //         _default_target->removeComesFrom(_parent);
    //     }
    //     _parent = parent;
    //     parent->addJumpsTo(_default_target);
    //     _default_target->addComesFrom(parent);
    // }
/** end class JumpSSA */

/** @class BranchSSA */

BranchSSA::RefT BranchSSA::Create(owned<Value> condition, BasicBlock *if_true, BasicBlock *if_false)
{
    std::string_view errptr_name;
    if (condition == nullptr)
        errptr_name = "BranchSSA::Create()::condition"sv;
    else if (if_true == nullptr)
        errptr_name = "BranchSSA::Create()::if_true"sv;
    else if (if_false == nullptr)
        errptr_name = "BranchSSA::Create()::if_false"sv;
    if (!errptr_name.empty()) throw NullException {
        errptr_name, {}, CURRENT_SRCLOC_F
    };
    RETURN_MTB_OWN(BranchSSA, nullptr, std::move(condition), if_true, if_false);
}

/* ================ [ Public class BranchSSA ] ================ */
    BranchSSA::BranchSSA(BasicBlock *parent,  owned<Value> condition,
                        BasicBlock *if_true, BasicBlock  *if_false)
        : JumpBase(parent, ValueTID::BRANCH_SSA, OpCode::BR, if_false),
        _if_true(if_true),
        _condition(std::move(condition)) {

        /* condition 属性是操作数, 不可为 `null`  */
        MTB_UNLIKELY_IF (_condition == nullptr) {
            throw NullException {
                "BranchSSA.condition",
                "Usee property 'condition' cannot be null",
                CURRENT_SRCLOC_F
            };
        }

        MYGL_ADD_BBLOCK_PROPERTY(if_true);
        MYGL_ADD_USEE_PROPERTY(condition);

        /* 由于此时还没有接收到连接到父结点的信号, 所以不做任何初始化, 包括基本块
         *  CFG 结点的维护操作.
         * 在 Instruction 的 C1 构造函数中就有设置结点状态的代码, 这里不做重复. */
    }

    BranchSSA::~BranchSSA() = default;

    void BranchSSA::set_condition(owned<Value> condition)
    {
        if (condition == _condition)
            return;
        Use *use = _list_as_user.at(2);
        if (_condition != nullptr)
            _condition->removeUseAsUsee(use);
        if (condition != nullptr)
            condition->addUseAsUsee(use);
        _condition = std::move(condition);
    }

    void BranchSSA::set_if_true(BasicBlock *if_true)
    {
        if (_if_true == if_true)
            return;
        Use *use = _list_as_user.at(1);

        _unregister_target(_if_true);
        if (_if_true != nullptr)
            _if_true->removeUseAsUsee(use);

        _if_true = if_true;
    
        _register_target(_if_true);
        if (_if_true != nullptr)
            _if_true->addUseAsUsee(use);
    }

    void BranchSSA::swapTargets() {
        std::swap(_default_target, _if_true);
    }

/** overrides {JumpBase, Instruction, ...} */

    Value *BranchSSA::operator[](size_t index)
    {
        using namespace capatibility;

        switch (index) {
            case 0: return get_if_false();
            case 1: return get_if_true();
            case 2: return get_condition();
            MTB_UNLIKELY_DEFAULT:
                throw std::out_of_range {
                osb_fmt(
                    "BranchSSA only has 3 operands while you're tryin' to get operand", index
                )};
        }
    }

    void BranchSSA::traverseOperands(ValueAccessFunc fn)
    {
        if (fn(*get_if_false())) return;
        if (fn(*get_if_true())) return;
        fn(*get_condition());
    }

    // void BranchSSA::registerThis(BasicBlock *parent)
    // {
    //     MTB_LIKELY_IF(_parent != nullptr) {
    //         _parent->removeJumpsTo(get_if_true());
    //         _parent->removeJumpsTo(get_if_false());

    //         _if_true->removeComesFrom(_parent);
    //         _default_target->removeComesFrom(_parent);
    //     }
    //     _parent = parent;
    //     MTB_UNLIKELY_IF(parent == nullptr)
    //         return;

    //     parent->addJumpsTo(get_if_true());
    //     parent->addJumpsTo(get_if_false());

    //     _if_true->addComesFrom(parent);
    //     _default_target->addComesFrom(parent);
    // }

    void BranchSSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }

    void BranchSSA::replace_target(BasicBlock *old,
                                   BasicBlock *new_target)
    {
        MTB_UNLIKELY_IF (old == new_target)
            return;
        if (old == get_if_false())
            set_if_false(new_target);
        if (old == get_if_true())
            set_if_true(new_target);
    }

    void BranchSSA::clean_targets()
    {
        auto iter = _list_as_user.begin();
        if (_parent != nullptr && _default_target != nullptr) {
            _default_target->removeComesFrom(_parent);
            _parent->removeJumpsTo(_default_target);
            _default_target->removeUseAsUsee(iter.get());
        }
        iter = iter.get_next_iterator();
        if (_parent != nullptr && _if_true != nullptr) {
            _if_true->removeComesFrom(_parent);
            _parent->removeJumpsTo(_if_true);
            _if_true->removeUseAsUsee(iter.get());
        }
    }

    void BranchSSA::replace_target_by(ReplaceClosure fn_terminates)
    {
        BasicBlock *out_new_target = nullptr;
        if (fn_terminates(get_if_false(), out_new_target))
            return;
        if (out_new_target != nullptr && out_new_target != get_if_false())
            set_if_false(out_new_target);

        if (fn_terminates(get_if_true(), out_new_target))
            return;
        if (out_new_target != nullptr && out_new_target != get_if_true())
            set_if_true(out_new_target);
    }

/* ======== [Signal BranchSSA] ======== */

    BasicBlock *BranchSSA::on_parent_unplug()
    {
        _unregister_target(_default_target);
        _unregister_target(_if_true);
        return Instruction::on_parent_unplug();
    }

    void BranchSSA::on_parent_plug(BasicBlock *parent)
    {
        /* 在构造时会初始化一次 parent, 这时仅仅判断 parent 是否存在
         * 已经不准确了.
         * 这算设计失误, 但是由于所有的构造/工厂函数都要求父结点是非 null
         * 的, 因此只能这么做了. */
        if (_parent         == parent &&
            _connect_status == Instruction::ConnectStatus::CONNECTED)
            return;
        _parent = parent;
        _register_target(_default_target);
        _register_target(_if_true);
        _connect_status = ConnectStatus::CONNECTED;
    }

    void BranchSSA::on_parent_finalize()
    {
        Use *ufalse = _list_as_user.at(0);
        Use *utrue  = _list_as_user.at(1);
        Use *ucond  = _list_as_user.at(2);

        _unregister_target(_default_target);
        _unregister_target(_if_true);

        if (_default_target != nullptr)
            _default_target->removeUseAsUsee(ufalse);
        if (_if_true != nullptr)
            _if_true->removeUseAsUsee(utrue);
        if (_condition != nullptr)
            _condition->removeUseAsUsee(ucond);

        _condition.reset();
        _connect_status = ConnectStatus::FINALIZED;
    }

    void BranchSSA::on_function_finalize()
    {
        Use *ufalse = _list_as_user.at(0);
        Use *utrue  = _list_as_user.at(1);
        Use *ucond  = _list_as_user.at(2);

        if (_parent != nullptr) {
            _unregister_target(_default_target);
            _unregister_target(_if_true);
        }
        if (_default_target != nullptr &&
            _default_target->get_parent() != get_function())
            _default_target->removeUseAsUsee(ufalse);

        if (_if_true != nullptr &&
            _if_true->get_parent() != get_function())
            _if_true->removeUseAsUsee(utrue);

        _condition.reset();
        _connect_status = ConnectStatus::FINALIZED;
    }

/** end class BranchSSA */

/** @class SwitchSSA */
namespace switch_ssa_impl {
    using namespace capatibility;

    struct vsetter {
        SwitchSSA  *self;
        int64_t case_num;
    public:
        Use::SetResult operator() (Value *value)
        {
            if (value == nullptr)
                return remove_this();
            if (value->get_type_id() != ValueTID::BASIC_BLOCK)
                return {false};
            self->setCase(case_num, static_cast<BasicBlock*>(value));
            return Use::SetResult{false};
        }

        Use::SetResult remove_this()
        {
            SwitchSSA::CaseMapT &case_map = self->cases();
            auto iter = case_map.find(case_num);
            if (iter == case_map.end())
                return {false};
            case_map.erase(iter);
            return {true};
        }
    }; // struct vsetter

    static void verify_condition_type_or_throw(Value *condition, SourceLocation srcloc)
    {
        // 这里是要return还是throw?
        if (condition == nullptr) throw NullException {
            "SwitchSSA::Create...()::condition"sv,
            {}, srcloc
        }; // condition == nullptr

        Type *condition_type = condition->get_value_type();
        TypeContext *ctx = condition_type->get_type_context();
        if (condition_type != ctx->getIntType(1)) {
            throw TypeMismatchException {
                condition_type,
                osb_fmt(
                    "Type `", condition_type->toString(), "` is not boolean type(i1)"),
                CURRENT_SRCLOC_F
            };
        }
    }

    static void unmount_from_parent(SwitchSSA *self)
    {
        BasicBlock *parent = self->get_parent();
        // 一个 BasicBlock 最多只有一条指令能管理 jumps_to, 在这里刚好是SwitchSSA这一条。
        // SwitchSSA 掌控着 BasicBlock 所有的 jumps_to, 所以可以直接清空.
        parent->jumps_to().clear();
        self->get_default_target()->removeComesFrom(parent);
        for (auto &i: self->cases()) {
            BasicBlock *case_target = i.second.target;
            case_target->removeComesFrom(parent);
        }
    }

    static void mount_to_parent(SwitchSSA *self)
    {
        BasicBlock *parent = self->get_parent();
        BasicBlock *default_target = self->get_default_target();

        // 连接 parent 与 default_target
        parent->addJumpsTo(default_target);
        default_target->addComesFrom(parent);

        for (auto &i: self->cases()) {
            BasicBlock *target = i.second.target;
            target->addComesFrom(parent);
            parent->addJumpsTo(target);
        }
    }
} // namespace switch_ssa_impl

int64_t SwitchSSA::BlockCaseIterator::get_next()
{
    if (iter == map->end())
        return 0;

    BasicBlock *block = iter->second.target;
    do {
        if (iter == map->end())
            return 0;
        iter++;
    } while (iter->second.target != block);
    return iter->first;
}

int64_t SwitchSSA::BlockCaseIterator::get_next(bool &out_ends)
{
    if (iter == map->end()) {
        out_ends = true;
        return 0;
    }

    BasicBlock *block = iter->second.target;
    do {
        if (iter == map->end()) {
            out_ends = true;
            return 0;
        }
        iter++;
    } while (iter->second.target != block);
    out_ends = false;
    return iter->first;
}

SwitchSSA::RefT SwitchSSA::Create(owned<Value> condition, BasicBlock *default_target)
{
    using namespace switch_ssa_impl;

    verify_condition_type_or_throw(condition, CURRENT_SRCLOC_F);
    if (default_target == nullptr) throw NullException {
        "SwitchSSA::Create...()::default_target"sv,
        {}, CURRENT_SRCLOC_F
    }; // default_target == nullptr

    RETURN_MTB_OWN(SwitchSSA, nullptr, std::move(condition), default_target);
}

SwitchSSA::RefT SwitchSSA::
CreateWithCases(owned<Value> condition, BasicBlock *default_target, CaseListT const& cases)
try {
    using namespace switch_ssa_impl;
    verify_condition_type_or_throw(condition, CURRENT_SRCLOC_F);
    if (default_target == nullptr) throw NullException {
        "SwitchSSA::Create...()::default_target"sv,
        {}, CURRENT_SRCLOC_F
    }; // default_target == nullptr

    RefT ret = own<SwitchSSA>(nullptr, std::move(condition), default_target);
    for (auto &[c, b]: cases)
        ret->setCase(c, b);

    return ret;
} catch (MTB::Exception &e) {
    SourceLocation curloc = CURRENT_SRCLOC_F;
    debug_print("at line %5d, fn [%s], file [%s]\n",
                curloc.line(), curloc.function_name(), curloc.file_name());
    throw e;
}

/* ================ [ Public class SwitchSSA ] ================*/
    SwitchSSA::SwitchSSA(BasicBlock *parent, owned<Value> condition, BasicBlock *default_target)
        : JumpBase(parent, ValueTID::SWITCH_SSA, OpCode::SWITCH, default_target),
        _condition(std::move(condition)) {
        using namespace switch_ssa_impl;
        MYGL_ADD_USEE_PROPERTY(condition);
    }
    SwitchSSA::SwitchSSA(BasicBlock *parent,         owned<Value> condition,
                         BasicBlock *default_target, CaseListT const &cases)
        : SwitchSSA(parent, std::move(condition), default_target) {
        for (auto &[c, v]: cases)
            setCase(c, v);
    }
    SwitchSSA::~SwitchSSA() = default;

    void SwitchSSA::set_condition(owned<Value> condition)
    {
        using namespace switch_ssa_impl;
        if (condition == _condition)
            return;
        verify_condition_type_or_throw(condition, CURRENT_SRCLOC_F);
        Use *use_condition = _list_as_user.at(1);

        if (_condition != nullptr)
            _condition->removeUseAsUsee(use_condition);
        if (condition != nullptr)
            condition->addUseAsUsee(use_condition);
        _condition = std::move(condition);
    }

    BasicBlock *SwitchSSA::getCase(int64_t case_number) const
    {
        using namespace capatibility;
        MTB_LIKELY_IF (contains(_cases, case_number))
            return _cases.at(case_number).target;
        return _default_target;
    }
    Use *SwitchSSA::getCaseUse(int64_t case_number) const
    {
        using namespace capatibility;
        MTB_LIKELY_IF (contains(_cases, case_number))
            return _cases.at(case_number).target_use;
        return nullptr;
    }
    bool SwitchSSA::setCase(int64_t case_number, BasicBlock *block)
    {
        using namespace capatibility;
        using namespace switch_ssa_impl;

        MTB_UNLIKELY_IF (block == nullptr) {
            throw NullException {
                osb_fmt("BasicBlock in SwitchSSA case {", case_number, "}"),
                "You cannot set a null basic block on a case. For removing cases,"
                " please use 'SwitchSSA.removeCase(case_number)' method instead",
                CURRENT_SRCLOC_F
            };
        }

        auto case_it = _cases.find(case_number);
        if (case_it == _cases.end()) {
            // 出现了新 case: 插入一个新跳转目标
            auto [i, b] = _cases.insert({case_number, {block, nullptr}});

            // 为新出现的跳转目标创建一个新 Use
            Use *target_use = addValue(
                [this, case_number]()->Value* { return getCase(case_number); },
                vsetter{this, case_number});
            i->second.target_use = target_use; // 保存 Use

            // 连接父结点与跳转目标
            if (_parent != nullptr) {
                _parent->addJumpsTo(block);
                block->addComesFrom(_parent);
            }
            block->addUseAsUsee(target_use);
        } else {
            // 复用原有的 case: 移除原有的跳转目标并插入新的
            BasicBlock *orig = case_it->second.target;
            Use *target_use  = case_it->second.target_use;

            /* 原有跳转目标和新跳转目标相同, 不更新 */
            if (orig == block)
                return true;

            // 清理原来的跳转目标
            if (_parent != nullptr) {
                _parent->removeJumpsTo(orig);
                orig->removeComesFrom(_parent);
            }
            orig->removeUseAsUsee(target_use);

            // 附加新的跳转目标
            if (_parent != nullptr) {
                block->addComesFrom(_parent);
                _parent->addJumpsTo(block);
            }
            block->addUseAsUsee(target_use);

            case_it->second.target = block;
        }
        return true;
    }
    bool SwitchSSA::removeCase(int64_t case_number)
    {
        auto case_it = _cases.find(case_number);
        if (case_it == _cases.end())
            return false;

        Use      *case_use = case_it->second.target_use;
        BasicBlock *target = case_it->second.target;
        _cases.erase(case_number);
        if (_parent != nullptr) {
            _parent->removeJumpsTo(target);
            target->removeComesFrom(_parent);
        }
        target->removeUseAsUsee(case_use);
        auto it = case_use->get_modifier();
        it.remove_this();
        return true;
    }
    SwitchSSA::BlockCaseIterator
    SwitchSSA::findCaseCondition(BasicBlock *jump_target)
    {
        CaseMapT      *case_map = &_cases;
        CaseMapT::iterator iter = case_map->begin();
        while (iter != case_map->end() &&
               iter->second.target != jump_target) {
            iter++;
        }
        return BlockCaseIterator{case_map, iter};
    }

/* ====== [SwitchSSA::override from] ====== */
    // void SwitchSSA::registerThis(BasicBlock *parent)
    // {
    //     using namespace switch_ssa_impl;
    //     if (_parent != nullptr)
    //         unmount_from_parent(this);
    //     _parent = parent;
    //     if (parent != nullptr)
    //         mount_to_parent(this);
    // }
    void SwitchSSA::accept(IValueVisitor &visitor)
    {
        visitor.visit(this);
    }
    size_t SwitchSSA::get_operand_nmemb() const
    {
        return 1   // default 标签的跳转目标
             + 1   // 整数条件
             + _cases.size(); // case 的数量
    }
    void SwitchSSA::traverseOperands(ValueAccessFunc fn)
    {
        if (fn(*_default_target)) return;
        if (fn(*_condition))      return;
        for (auto &[c, bu]: _cases)
            if (fn(*bu.target)) return;
    }
    Value *SwitchSSA::operator[](size_t index)
    {
        if (index < 0)
            return nullptr;
        switch (index) {
        case 0: return _default_target;
        case 1: return _condition;
        default:
            for (int cnt = 0; auto &[n, u]: _cases) {
                if (cnt == index - 2)
                    return u.target;
            }
        }
        return nullptr;
    }

/* ====== [SwitchSSA::override interface IBasicBlockTerminator] ====== */

    void SwitchSSA::traverse_targets(IBasicBlockTerminator::TraverseFunc fn)
    {
        if (fn(_default_target)) return;
        for (auto &[c, target]: _cases)
            if (fn(target.target)) return;
    }
    void SwitchSSA::traverse_targets(IBasicBlockTerminator::TraverseClosure fn)
    {
        if (fn(_default_target)) return;
        for (auto &[c, target]: _cases)
            if (fn(target.target)) return;
    }

    void SwitchSSA::remove_target(BasicBlock *target)
    {
        if (target == nullptr)
            return;
        if (target == _default_target) {
            if (_parent != nullptr) {
                _default_target->removeComesFrom(_parent);
                _parent->removeJumpsTo(_default_target);
            }
            _default_target = nullptr;
        }
        std::deque<int64_t> target_cases;
        for (auto const& [c, b]: _cases) {
            if (b.target == target)
                target_cases.push_back(c);
        }
        for (int64_t c: target_cases)
            removeCase(c);
    }
    void SwitchSSA::remove_target_if(TraverseFunc fn)
    {
        if (fn(_default_target)) {
            if (_parent != nullptr) {
                _default_target->removeComesFrom(_parent);
                _parent->removeJumpsTo(_default_target);
            }
            _default_target = nullptr;
        }
        std::deque<int64_t> target_cases;
        for (auto const& [c, b]: _cases) {
            if (fn(b.target))
                target_cases.push_back(c);
        }
        for (int64_t c: target_cases)
            removeCase(c);
    }
    void SwitchSSA::remove_target_if(TraverseClosure fn)
    {
        if (fn(_default_target)) {
            if (_parent != nullptr) {
                _default_target->removeComesFrom(_parent);
                _parent->removeJumpsTo(_default_target);
            }
            _default_target = nullptr;
        }
        std::deque<int64_t> target_cases;
        for (auto const& [c, b]: _cases) {
            if (fn(b.target))
                target_cases.push_back(c);
        }
        for (int64_t c: target_cases) {
            BlockUsePair &b = _cases[c];
            if (_parent != nullptr) {
                _default_target->removeComesFrom(_parent);
                _parent->removeJumpsTo(_default_target);
            }
            b.target->removeUseAsUsee(b.target_use);
            _cases.erase(c);
        }
    }

    void SwitchSSA::replace_target(BasicBlock *old, BasicBlock *new_target)
    {
        if (new_target == nullptr) {
            throw NullException {
                "SwitchSSA.replace_target().new_target"sv,
                "SwitchSSA cannot receive null as target replacement"
                " for concerns about possible removement of dynamic targets"s,
                CURRENT_SRCLOC_F 
            };
        }
        if (old == new_target)
            return;
        if (get_default_target() == old)
            set_default_target(new_target);

        BasicBlock *parent = get_parent();
        for (auto &[c, bu]: cases()) {
            if (bu.target != old)
                continue;
            BasicBlock *&b = bu.target;
            Use        *u  = bu.target_use;
            b->removeComesFrom(parent);
            b->removeUseAsUsee(u);
            parent->removeJumpsTo(b);
            b = new_target;
            new_target->addComesFrom(parent);
            new_target->addUseAsUsee(u);
            parent->addComesFrom(new_target);
        }
    }

    void SwitchSSA::replace_target_by(ReplaceClosure fn_terminates)
    {
        BasicBlock *new_target = nullptr;
        BasicBlock *parent     = get_parent();

        if (fn_terminates(get_default_target(), new_target))
            return;
        if (new_target != nullptr)
            set_default_target(new_target);

        for (auto &[c, bu]: cases()) {
            if (fn_terminates(bu.target, new_target))
                return;
            BasicBlock *&b = bu.target;
            Use        *u  = bu.target_use;
            if (new_target == nullptr || new_target == b)
                continue;
            b->removeComesFrom(parent);
            b->removeUseAsUsee(u);
            parent->removeJumpsTo(b);
            b = new_target;
            new_target->addComesFrom(parent);
            new_target->addUseAsUsee(u);
            parent->addComesFrom(new_target);
        }
    }

    void SwitchSSA::clean_targets()
    {
        /* clean default_target */
        Use *default_target_use = list_as_user().at(0);
        _unregister_target(_default_target);
        _default_target->removeUseAsUsee(default_target_use);

        /* clean dynamic targets */
        for (auto &[c, target] : _cases) {
            BasicBlock *block = target.target;
            Use   *target_use = target.target_use;

            /* clean jump relationship */
            _unregister_target(block);

            /* clean use-def relationship */
            block->removeUseAsUsee(target_use);
            removeUseAsUser(target_use);
        }

        _cases.clear();
    }

/* ================== [SwitchSSA::responses signal] ================== */
    BasicBlock *SwitchSSA::on_parent_unplug()
    {
        _unregister_target(get_default_target());

        for (auto &[c, target]: cases())
            _unregister_target(target.target);
        return Instruction::on_parent_unplug();
    }
    void SwitchSSA::on_parent_plug(BasicBlock *parent)
    {
        [[maybe_unused]] BasicBlock *original = _parent;

        Instruction::on_parent_plug(parent);
        _register_target(_default_target);

        for (auto &[c, target]: cases())
            _register_target(target.target);
        _connect_status = ConnectStatus::CONNECTED;
    }
    void SwitchSSA::on_parent_finalize()
    {
        SwitchSSA::clean_targets();
        Use *condition_use = _list_as_user.at(get_condition_position());
        _condition->removeUseAsUsee(condition_use);
        _connect_status = ConnectStatus::FINALIZED;
    }
    void SwitchSSA::on_function_finalize()
    {
        on_parent_finalize();
    }
/** end class SwitchSSA */

} // namespace MYGL::IR
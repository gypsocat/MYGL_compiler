#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"

namespace MYGL::IR {
    inline namespace return_ssa_impl {
        using namespace MTB::Compatibility;
    } // inline namespace return_ssa_impl

    ReturnSSA::ReturnSSA(Function *parent, Type *return_type, owned<Value> result)
        : Instruction(ValueTID::RETURN_SSA, VoidType::voidty, OpCode::RET),
          IBasicBlockTerminator(this),
          _result(std::move(result)), _return_type(return_type) {
        MYGL_IBASICBLOCK_TERMINATOR_INIT_OFFSET();
        debug_print("ReturnSSA: this = %p, vthis = %p, offset = %hhd, "
                "iface_calc = %p, real iface=%p\n",
                this, iterminator_virtual_this, _ibasicblock_terminator_iface_offset,
                unsafe_try_cast_to_terminator(), static_cast<IBasicBlockTerminator*>(this));
        MYGL_ADD_USEE_PROPERTY(result);
    }

    ReturnSSA::~ReturnSSA() = default;

    void ReturnSSA::set_result(owned<Value> result)
    {
        using namespace std::string_view_literals;
        if (_result == result || _return_type->is_void_type())
            return;
        Use *use = list_as_user().at(0);
        if (_result != nullptr)
            _result->removeUseAsUsee(use);
        if (result == nullptr)
            return;
        Type *new_type = result->get_value_type();
        if (new_type == _return_type ||
            new_type->equals(_return_type)) {
            result->addUseAsUsee(use);
            _result = std::move(result);
            return;
        }
        throw TypeMismatchException {
            new_type,
            osb_fmt(
                "new result type should be equal to the value type "sv,
                get_value_type()
            ),
            CURRENT_SRCLOC_F
        };
    }

    void ReturnSSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }

/* ================ [ReturnSSA::override signal] ================ */
    void ReturnSSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void ReturnSSA::on_parent_finalize()
    {
        if (_result != nullptr)
            _result->removeUseAsUsee(list_as_user().front());
        _result.reset();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
    void ReturnSSA::on_function_finalize()
    {
        _result.reset();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
/* ================ [static class ReturnSSA] ================ */
    ReturnSSA::RefT ReturnSSA::Create(Function *parent, owned<Value> result)
    {
        using namespace std::string_literals;
        if (parent == nullptr) {
            throw NullException {
                "ReturnSSA.parent in construct"s,
                ""s, CURRENT_SRCLOC_F
            };
        }
        if (result == nullptr) {
            throw NullException {
                "ReturnSSA.result in construct"s,
                ""s, CURRENT_SRCLOC_F
            };
        }
        return own<ReturnSSA>(parent, parent->get_return_type(), std::move(result));
    }

    ReturnSSA::RefT ReturnSSA::CreateDefault(Function *parent) {
        return Create(parent, Constant::CreateZeroOrUndefined(parent->get_return_type()));
    }
} // namespace MYGL::IR
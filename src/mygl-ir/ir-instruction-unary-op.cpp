#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/ir-opcode-reflect-map.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include "irbase-capatibility.private.hxx"

namespace MYGL::IR {
/* ================ [class UnaryOperationSSA] ================ */
    inline namespace unary_op_impl {
        static Type *operand_get_type_or_throw(Value *operand, SourceLocation src_loc)
        {
            if (operand == nullptr) throw NullException {
                "UnaryOperationSSA.operand",
                {}, src_loc
            };
            return operand->get_value_type();
        }
        using UPtrT = owned<UnaryOperationSSA>;
    } // inline namespace unary_op_impl
/* ============= [static class UnaryOperationSSA] ============ */
    UPtrT UnaryOperationSSA::CreateINeg(owned<Value> operand)
    {
        using namespace capatibility;
        Type *operand_type = operand_get_type_or_throw(operand, CURRENT_SRCLOC_F);
        if (!operand_type->is_integer_type()) {
            throw TypeMismatchException {
                operand_type,
                osb_fmt("operand type should be"
                    " integer in `ineg` instruction"),
                CURRENT_SRCLOC_F
            }; // throw TypeMismatchException
        }
        return new UnaryOperationSSA(OpCodeT::INEG, std::move(operand));
    }
    UPtrT UnaryOperationSSA::CreateFNeg(owned<Value> operand)
    {
        using namespace capatibility;
        Type *operand_type = operand_get_type_or_throw(operand, CURRENT_SRCLOC_F);
        if (!operand_type->is_float_type()) {
            throw TypeMismatchException {
                operand_type,
                osb_fmt("operand type should be"
                    " float in `fneg` instruction"),
                CURRENT_SRCLOC_F
            }; // throw TypeMismatchException
        }
        return new UnaryOperationSSA(OpCodeT::FNEG, std::move(operand));
    }
    UPtrT UnaryOperationSSA::CreateNot(owned<Value> operand)
    {
        using namespace capatibility;
        Type *operand_type = operand_get_type_or_throw(operand, CURRENT_SRCLOC_F);
        if (!operand_type->is_integer_type()) {
            throw TypeMismatchException {
                operand_type,
                osb_fmt("operand type should be"
                    " integer in `not` instruction"),
                CURRENT_SRCLOC_F
            }; // throw TypeMismatchException
        }
        return new UnaryOperationSSA(OpCodeT::NOT, std::move(operand));
    }
/* ============= [public class UnaryOperationSSA] ============ */
    UnaryOperationSSA::UnaryOperationSSA(OpCodeT opcode, owned<Value> operand)
        : UnarySSA(ValueTID::UNARY_OP_SSA,
                   operand_get_type_or_throw(operand, CURRENT_SRCLOC_F),
                   opcode, operand) {
    }

    bool UnaryOperationSSA::checkOperand(Value *operand)
    {
        return get_value_type()->equals(operand->get_value_type());
    }
    void UnaryOperationSSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }
/* ============= [signal class UnaryOperationSSA] ============ */
    void UnaryOperationSSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void UnaryOperationSSA::on_parent_finalize()
    {
        set_operand(nullptr);
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
    void UnaryOperationSSA::on_function_finalize()
    {
        set_operand(nullptr);
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
/* ============== [end class UnaryOperationSSA] ============== */
} // namespace MYGL::IR

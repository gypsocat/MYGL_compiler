#include "base/mtb-exception.hxx"
#include "base/mtb-compatibility.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include <string_view>

namespace MYGL::IR {

inline namespace compare_ssa_impl {
    using RefT      = CompareSSA::RefT;
    using OpCode    = Instruction::OpCode;
    using Condition = CompareSSA::Condition;
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    using namespace MTB::Compatibility;
    enum OperandOrder {
        LHS, RHS
    }; // enum OperandOrder

    static RefT new_compare(OpCode opcode, Condition condition, Type *operand_type,
                            owned<Value> lhs, owned<Value> rhs)
    {
        RETURN_MTB_OWN(CompareSSA, opcode, condition, operand_type,
                       std::move(lhs), std::move(rhs));
    }

    static Type* construct_value_get_type_or_throw(Value *operand, std::string_view &&name)
    {
        if (operand == nullptr) throw NullException {
            name, {},
            CURRENT_SRCLOC_F
        };
        return operand->get_value_type();
    }

    static IntType *construct_get_boolean(Type *operand_type)
    {
        TypeContext *type_ctx = operand_type->get_type_context();
        if (type_ctx == nullptr) throw NullException {
            "CompareSSA.operand_type.type_context"sv,
            {}, CURRENT_SRCLOC_F
        };
        return type_ctx->getIntType(1);
    }
} // inline namespace compare_ssa_impl
/* ================ [static class CompareSSA] ================ */
    RefT CompareSSA::CreateICmp(CompareResult condition, bool is_signed,
                                owned<Value> lhs, owned<Value> rhs)
    {
        Type *lhs_ty = construct_value_get_type_or_throw(
                            lhs, "CompareSSA<icmp>.lhs in construct"sv);
        Type *rhs_ty = construct_value_get_type_or_throw(
                            rhs, "CompareSSA<icmp>.rhs in construct"sv);
        if (!lhs_ty->is_integer_type()) throw TypeMismatchException {
            lhs_ty,
            "icmp instruction operand type should be integer"s,
            CURRENT_SRCLOC_F
        };
        if (lhs_ty != rhs_ty || !lhs_ty->equals(rhs_ty)) {
            throw TypeMismatchException {
                rhs_ty,
                osb_fmt("icmp instruction rhs type"
                    " should be equal to lhs type "sv,
                    lhs_ty->toString()),
                CURRENT_SRCLOC_F
            };
        }
        Condition construct_cd(condition, false, is_signed);
        return new_compare(OpCode::ICMP, construct_cd,
                lhs_ty, std::move(lhs), std::move(rhs));
    }
    RefT CompareSSA::CreateFCmp(CompareResult condition, bool is_ordered,
                                owned<Value> lhs, owned<Value> rhs)
    {
        Type *lhs_ty = construct_value_get_type_or_throw(
                            lhs, "CompareSSA<fcmp>.lhs in construct"sv);
        Type *rhs_ty = construct_value_get_type_or_throw(
                            rhs, "CompareSSA<fcmp>.rhs in construct"sv);
        if (!lhs_ty->is_float_type()) {
            throw TypeMismatchException {
                lhs_ty,
                "fcmp instruction operand type should be float"s,
                CURRENT_SRCLOC_F
            };
        }
        if (lhs_ty != rhs_ty || !lhs_ty->equals(rhs_ty)) {
            throw TypeMismatchException {
                rhs_ty,
                osb_fmt("icmp instruction rhs type"
                    " should be equal to lhs type "sv,
                    lhs_ty->toString()),
                CURRENT_SRCLOC_F
            };
        }
        Condition construct_cd(condition, true, is_ordered);
        return new_compare(OpCode::FCMP, construct_cd,
                lhs_ty, std::move(lhs), std::move(rhs));
    }

    RefT CompareSSA::CreateChecked(OpCode opcode, Condition condition,
                                   owned<Value> lhs, owned<Value> rhs)
    {
        switch (opcode) {
        case OpCode::ICMP:
            return CreateICmp(condition, std::move(lhs), std::move(rhs));
        case OpCode::FCMP:
            return CreateFCmp(condition, std::move(lhs), std::move(rhs));
        default:
            throw OpCodeMismatchException {
                opcode, "opcode should be FCMP or ICMP"s,
                ""s, CURRENT_SRCLOC_F
            };
        }
    }
/* ================ [public class CompareSSA] ================ */
    CompareSSA::CompareSSA(OpCode opcode, Condition condition, Type *operand_type,
                           owned<Value> lhs, owned<Value> rhs)
        : Instruction(ValueTID::COMPARE_SSA,
                      construct_get_boolean(operand_type), opcode),
          _lhs(std::move(lhs)), _rhs(std::move(rhs)),
          _operand_type(operand_type) {
        MYGL_ADD_USEE_PROPERTY(lhs);
        MYGL_ADD_USEE_PROPERTY(rhs);
    }

    CompareSSA::~CompareSSA() = default;

    void CompareSSA::set_lhs(owned<Value> lhs)
    {
        if (_lhs == lhs)
            return;
        if (lhs != nullptr) {
            Type *lhs_type = lhs->get_value_type();
            if (lhs_type != _operand_type &&
                !lhs_type->equals(_operand_type)) {
                throw TypeMismatchException {
                    lhs_type,
                    osb_fmt("CompareSSA new lhs type"
                    " should be equal to the internal operand type "sv,
                    _operand_type->toString()),
                    CURRENT_SRCLOC_F
                };
            }
        }
        Use *lhs_use = _list_as_user.at(OperandOrder::LHS);
        if (_lhs != nullptr)
            _lhs->removeUseAsUsee(lhs_use);
        if (lhs != nullptr)
            lhs->addUseAsUsee(lhs_use);
        _lhs = std::move(lhs);
    }

    void CompareSSA::set_rhs(owned<Value> rhs)
    {
        if (_rhs == rhs)
            return;
        if (rhs != nullptr) {
            Type *rhs_type = rhs->get_value_type();
            if (rhs_type != _operand_type &&
                !rhs_type->equals(_operand_type)) {
                throw TypeMismatchException {
                    rhs_type,
                    osb_fmt("CompareSSA new RHS type"
                    " should be equal to the internal operand type "sv,
                    _operand_type->toString()),
                    CURRENT_SRCLOC_F
                };
            }
        }
        Use *rhs_use = _list_as_user.at(OperandOrder::RHS);
        if (_rhs != nullptr)
            _rhs->removeUseAsUsee(rhs_use);
        if (rhs != nullptr)
            rhs->addUseAsUsee(rhs_use);
        _rhs = std::move(rhs);
    }

/* ================ [CompareSSA::override parent] ================ */
    void CompareSSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }

    Value *CompareSSA::operator[](size_t index)
    {
        switch (index) {
        case OperandOrder::LHS: return get_lhs();
        case OperandOrder::RHS: return get_rhs();
        default:
            return nullptr;
        }
    }

    void CompareSSA::traverseOperands(ValueAccessFunc fn)
    {
        if (fn(*get_lhs())) return;
        if (fn(*get_rhs())) return;
    }

/* ================ [CompareSSA::override signal] ================ */
    void CompareSSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void CompareSSA::on_parent_finalize()
    {
        auto use_iter = _list_as_user.begin();
        Use *ulhs = use_iter.get();
        ++use_iter;
        Use *urhs = use_iter.get();
        if (_lhs != nullptr) {
            _lhs->removeUseAsUsee(ulhs);
            _lhs.reset();
        }
        if (_rhs != nullptr) {
            _rhs->removeUseAsUsee(urhs);
            _rhs.reset();
        }
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
    void CompareSSA::on_function_finalize()
    {
        _lhs.reset();
        _rhs.reset();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
} // namespace MYGL::IR
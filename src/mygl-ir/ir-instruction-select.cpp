#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"

namespace MYGL::IR {

#define MYGL_STRINGFY(ident) #ident
#define BSCHECK_OPERAND_NULL_THROW(operand) do { \
    MTB_UNLIKELY_IF (operand == nullptr) {\
        throw NullException {\
            "BinarySelectSSA." MYGL_STRINGFY(operand) " in construct"sv,\
            std::string{}, CURRENT_SRCLOC_F\
        };\
    }\
} while (false)
#define BSCHECK_OPERAND_TYPE_THROW(operand) do {\
    if (operand == nullptr) \
        break;\
    Type *operand_type = operand->get_value_type();\
    MTB_UNLIKELY_IF (operand_type != get_value_type()) {\
        throw TypeMismatchException {\
            operand_type, osb_fmt("BinarySelectSSA " \
                MYGL_STRINGFY(operand) " type should be equal to"\
                "instruction return type "sv,\
                get_value_type()->toString()),\
            CURRENT_SRCLOC_F\
        };\
    }\
} while (false)

#define BSCHECK_CONDITION_BOOL_THROW(condition) do {\
    Type *condition_type = condition->get_value_type();\
    MTB_UNLIKELY_IF (!type_is_boolean(condition_type)) {\
        throw TypeMismatchException {\
            condition_type,\
            "BinarySelectSSA condition type should be boolean type"s,\
            CURRENT_SRCLOC_F\
        };\
    }\
} while (false)

#define INIT_CHECK_CONDITION_THROW(condition) do {\
    MTB_UNLIKELY_IF (condition == nullptr) {\
        throw NullException {\
            "BinarySelectSSA." MYGL_STRINGFY(condition) " in construct"sv,\
            std::string{}, CURRENT_SRCLOC_F\
        };\
    }\
    BSCHECK_CONDITION_BOOL_THROW(condition);\
} while (false)

#define MYGL_USE_OF(operand_order) \
    _list_as_user.at(int32_t(OperandOrder::operand_order))

inline namespace select_ssa_impl {
    using RefT         = BinarySelectSSA::RefT;
    using OperandOrder = BinarySelectSSA::OperandOrder;
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    using namespace MTB::Compatibility;

    static inline bool type_is_boolean(Type *type)
    {
        if (!type->is_integer_type())
            return false;
        IntType *ity = static_cast<IntType*>(type);
        return ity->get_binary_bits() == 1;
    }
} // inline namespace select_ssa_impl

/* ================ [static class BinarySelectSSA] ================ */
    RefT BinarySelectSSA::Create(OperandRefT condition, OperandRefT if_true, OperandRefT if_false)
    {
        BSCHECK_OPERAND_NULL_THROW(if_true);
        BSCHECK_OPERAND_NULL_THROW(if_false);
        INIT_CHECK_CONDITION_THROW(condition);

        /* BinarySelectSSA 要求 if_false 的类型与 if_true 的相等. 现在检查二者是否相等, 否则丢异常. */
        Type *if_true_ty  = if_true->get_value_type();
        Type *if_false_ty = if_false->get_value_type();
        if (if_true_ty != if_false_ty &&
            !if_true_ty->equals(if_false_ty)) {
            throw TypeMismatchException {
                if_false_ty,
                osb_fmt("type of `BinarySelectSSA.if_false`"
                    "> should be equal to type of `if_true` <"sv,
                    if_true_ty->toString(), ">"),
                CURRENT_SRCLOC_F
            };
        }

        /* 检查无误后, new 一个实例后直接返回. */
        RETURN_MTB_OWN(BinarySelectSSA, if_false_ty, std::move(condition),
                       std::move(if_true), std::move(if_false));
    }
/* ================ [dyn class BinarySelectSSA] ================ */
    BinarySelectSSA::BinarySelectSSA(Type   *result_type, OperandRefT condition_i1,
                                     OperandRefT if_true, OperandRefT if_false)
        : Instruction(ValueTID::BINARY_SELECT_SSA, result_type, OpCode::SELECT),
          _if_false(std::move(if_false)), _if_true(std::move(if_true)),
          _condition(std::move(condition_i1)) {
        MYGL_ADD_USEE_PROPERTY(if_false);
        MYGL_ADD_USEE_PROPERTY(if_true);
        MYGL_ADD_USEE_PROPERTY(condition);
    }
    BinarySelectSSA::~BinarySelectSSA() = default;

    void BinarySelectSSA::set_if_false(OperandRefT if_false)
    {
        if (_if_false == if_false)
            return;
        BSCHECK_OPERAND_TYPE_THROW(if_false);

        Use *use_if_false = MYGL_USE_OF(IF_FALSE);
        if (_if_false != nullptr)
            _if_false->removeUseAsUsee(use_if_false);
        if (if_false  != nullptr)
            if_false->addUseAsUsee(use_if_false);
        _if_false = std::move(if_false);
    }
    void BinarySelectSSA::set_if_true(OperandRefT if_true)
    {
        if (_if_true == if_true)
            return;
        BSCHECK_OPERAND_TYPE_THROW(if_true);

        Use *use_if_true = MYGL_USE_OF(IF_TRUE);
        if (_if_true != nullptr)
            _if_true->removeUseAsUsee(use_if_true);
        if (if_true  != nullptr)
            if_true->addUseAsUsee(use_if_true);
        _if_true = std::move(if_true);
    }
    void BinarySelectSSA::set_condition(OperandRefT condition)
    {
        if (_condition == condition)
            return;
        BSCHECK_CONDITION_BOOL_THROW(condition);

        Use *use_condition = MYGL_USE_OF(CONDITION);
        if (_condition != nullptr)
            _condition->removeUseAsUsee(use_condition);
        if (condition  != nullptr)
            condition->addUseAsUsee(use_condition);
        _condition = std::move(condition);
    }

/* ================ [BinarySelectSSA::override parent] ================ */
    Value *BinarySelectSSA::operator[](size_t index)
    {
        Value *ret[] = {_if_false, _if_true, _condition};
        return index < get_operand_nmemb() ? ret[index]: nullptr;
    }
    void BinarySelectSSA::traverseOperands(ValueAccessFunc fn)
    {
        if (fn(*_if_false)) return;
        if (fn(*_if_true))  return;
        fn(*_condition);
    }
    void BinarySelectSSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }

/* ================ [BinarySelectSSA::override signal] ================ */
    void BinarySelectSSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void BinarySelectSSA::on_parent_finalize()
    {
        if (_connect_status == Instruction::ConnectStatus::FINALIZED)
            return;

        auto use_iter = _list_as_user.begin();
        Use *use_if_false = use_iter.get();
        ++use_iter;
        Use *use_if_true  = use_iter.get();
        ++use_iter;
        Use *use_cond     = use_iter.get();

        if (_if_false != nullptr) {
            _if_false->removeUseAsUsee(use_if_false);
            _if_false.reset();
        }
        if (_if_true  != nullptr) {
            _if_true->removeUseAsUsee(use_if_true);
            _if_true.reset();
        }
        if (_condition != nullptr) {
            _condition->removeUseAsUsee(use_cond);
            _condition.reset();
        }
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
    void BinarySelectSSA::on_function_finalize()
    {
        _if_false.reset();
        _if_true.reset();
        _condition.reset();
        _list_as_usee.clear();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
} // namespace MYGL::IR

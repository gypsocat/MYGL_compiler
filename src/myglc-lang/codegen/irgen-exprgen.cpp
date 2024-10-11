#include "irgen-expr.priv.hxx"
#include "mygl-ir/ir-instruction.hxx"

namespace MYGL::IRGen::ExprGen {

inline namespace exprgen_impl {
    using namespace shared_impl;
}

/* ======== [public class ExprGenerator] ======== */
ExprGenerator::ExprGenerator(IR::BasicBlock *working_block,
                             FuncLocalMap   *symbol_map)
    : _startup_block(working_block),
      _local_map(symbol_map) {
    _type_ctx = &_startup_block->get_module()->type_ctx();
}

bool ExprGenerator::visit(Ast::BinaryExpr::UnownedPtrT bexp)
{
    Ast::Expression *lhs = bexp->get_lhs().get();
    Ast::Expression *rhs = bexp->get_rhs().get();
    Operator         eop = bexp->get_operator();
    BinGenerator     gen{this, _runtime->current_block};

    switch (eop) {
    case Operator::PLUS: case Operator::SUB:
    case Operator::STAR: case Operator::SLASH: case Operator::PERCENT:
    case Operator::LT: case Operator::EQ: case Operator::GT:
    case Operator::LE: case Operator::NE: case Operator::GE:
        if (!lhs->accept(this))
            return false;
        gen.l = std::move(_runtime->prev_result);

        if (!rhs->accept(this))
            return false;
        gen.r = std::move(_runtime->prev_result);
        gen.generateCalcCmp();
        break;
    
    case Operator::AND: case Operator::OR: case Operator::NOT:
        /* 目前还不支持直接取逻辑表达式为值, 调用该方法会直接报错退出 */
        gen.generateLogical(lhs, rhs);
        break;

    default:
        return false;
    }

    _runtime->prev_result = std::move(gen.result);
    return true;
}

bool ExprGenerator::visit(Ast::UnaryExpr *uexp)
{
    Ast::Expression *operand = uexp->get_expression().get();
    ExprResult  &prev_result = _runtime->prev_result; 
    prev_result = {
        uexp->get_expression().get(),
        nullptr, 0, false,
    };
    if (!operand->accept(this))
        crash_with_stacktrace(true, CURRENT_SRCLOC_F, {});
    Value     *voperand = prev_result.result;
    Type     *operandty = voperand->get_value_type();
    // 有些表达式会生成不止一个基本块, 并修改当前所在的基本块
    BasicBlock *current = _runtime->current_block;

    switch (uexp->get_operator()) {
    case Operator::PLUS:
        // 在 int 和 float 中, 这个表达式什么都不用做
        break;
    case Operator::SUB:
        if (is_data_const(voperand)) {
            auto coperand = static_cast<ConstantData*>(voperand);
            if (!is_boolty(operandty))
                prev_result.result = coperand->neg();
        } else if (operandty->is_integer_type() &&
                   /* bool 值取负运算等于什么也不做 */
                   static_cast<IntType*>(operandty)->is_bool_type()) {
            auto result = UnaryOperationSSA::CreateINeg(voperand);
            current->append(result);
            prev_result.result = std::move(result);
        } else if (operandty->is_float_type()) {
            auto result = UnaryOperationSSA::CreateFNeg(voperand);
            current->append(result);
            prev_result.result = std::move(result);
        } else throw TypeMismatchException {
            operandty,
            "UnaryExpr operand type should be integer of float",
            CURRENT_SRCLOC_F
        };
        break;
    case Operator::NOT: {
        using Condition = CompareSSA::Condition;
        owned<Instruction> result;
        IntType *boolty = get_boolty(operandty);

        if (is_data_const(voperand)) {
            auto coperand = static_cast<ConstantData*>(voperand);
            prev_result.result = new IntConst(boolty, !coperand->is_zero());
            break;
        } else if (is_boolty(operandty)) {
            result = UnaryOperationSSA::CreateNot(voperand);
        } else if (operandty->is_integer_type()) {
            auto rhs = ConstantData::CreateZero(boolty);
            result = CompareSSA::CreateICmp(Condition::NE, voperand, rhs);
        } else if (operandty->is_float_type()) {
            auto rhs = ConstantData::CreateZero(boolty);
            result = CompareSSA::CreateFCmp(Condition::NE, voperand, rhs);
        } else throw TypeMismatchException {
            operandty,
            "UnaryExpr operand type should be integer of float",
            CURRENT_SRCLOC_F
        };

        current->append(result);
        prev_result.result = std::move(result);
    }   break;
    default:
        THROW_WRONG_OP(uexp->get_operator());
    }

    return true;
}

bool ExprGenerator::visit(Ast::IntValue *ival)
{
    _runtime->prev_result = {
        ival,
        new IntConst(_runtime->i32ty, ival->get_value()),
        0, false
    };
    return true;
}
bool ExprGenerator::visit(Ast::FloatValue *fval)
{
    _runtime->prev_result = {
        fval,
        FloatConst::Create(_runtime->f32ty, fval->get_value()),
        0, false
    };
    return true;
}

} // namespace MYGL::IRGen::ExprGen

#include "irgen-expr.priv.hxx"
#include "mygl-ir/ir-instruction.hxx"

namespace MYGL::IRGen::ExprGen {

inline namespace binary_impl {
    using namespace IR;
    using namespace shared_impl;
    using namespace std::string_literals;
    using namespace std::string_view_literals;
    using cost_t = uint32_t;

    template<typename FloatT>
    MTB_REQUIRE((std::is_floating_point_v<FloatT>))
    static inline owned<FloatConst>
    do_eval_float_calc(FloatType *fty, Operator op, FloatT l, FloatT r)
    {
        FloatT ret;
        switch (op) {
        case Operator::PLUS:    ret = l + r; break;
        case Operator::SUB:     ret = l - r; break;
        case Operator::STAR:    ret = l * r; break;
        case Operator::SLASH:
            if (r == 0.0) {
                debug_print("encountered 0 as float divident, return INF\n");
                ret = INFINITY;
            } else {
                ret = l / r;
            }
            break;
        case Operator::PERCENT:
            if (r == 0.0) {
                debug_print("encountered 0 as float divident, return 0.0\n");
                ret = 0.0;
            } else {
                ret = std::fmod<FloatT>(l, r);
            }
            break;
        default:
            return owned<FloatConst>{nullptr};
        }

        return FloatConst::Create(fty, ret);
    }
    template<typename FloatT>
    MTB_REQUIRE((std::is_floating_point_v<FloatT>))
    static inline owned<IntConst>
    do_eval_float_cmp(IntType *boolty, Operator op, FloatT l, FloatT r)
    {
        bool ret;
        switch (op) {
            case Operator::LT:  ret = l <  r; break;
            case Operator::EQ:  ret = l == r; break;
            case Operator::GT:  ret = l >  r; break;
            case Operator::LE:  ret = l <= r; break;
            case Operator::NE:  ret = l != r; break;
            case Operator::GE:  ret = l >= r; break;
            default: return {};
        }
        return new IntConst(boolty, ret);
    }
    static inline owned<ConstantData>
    do_eval_float(FloatType *fty, Operator op, double l, double r)
    {
        if (fty->get_binary_bits() == 64) {
            owned<ConstantData> ret = do_eval_float_calc(fty, op, l, r);
            if (ret != nullptr)
                return ret;
            TypeContext *tctx = fty->get_type_context();
            IntType   *boolty = tctx->getIntType(1);
            return do_eval_float_cmp(boolty, op, l, r);
        } else {
            owned<ConstantData> ret = do_eval_float_calc<float>(fty, op, l, r);
            if (ret != nullptr)
                return ret;
            TypeContext *tctx = fty->get_type_context();
            IntType   *boolty = tctx->getIntType(1);
            return do_eval_float_cmp<float>(boolty, op, l, r);
        }
    }
    static inline owned<IntConst>
    do_eval_int(IntType *ity, int64_t l, int64_t r, Operator op)
    {
        TypeContext *tctx = ity->get_type_context();
        IntType *boolty = tctx->getIntType(1);
        IntType *ret_ty = ity;
        int64_t  result = 0;
        switch (op) {
        case Operator::PLUS:    result = l + r; break;
        case Operator::SUB:     result = l - r; break;
        case Operator::STAR:    result = l * r; break;
        case Operator::SLASH:
            if (r == 0) throw EvalException {
                EvalException::ZERO_DIV_RHS, {}, CURRENT_SRCLOC_F
            };
            result = l / r;
            break;
        case Operator::PERCENT:
            if (r == 0) throw EvalException {
                EvalException::ZERO_REM_RHS, {}, CURRENT_SRCLOC_F
            };
            result = l % r;
            break;
        case Operator::LT:  ret_ty = boolty; result = l <  r; break;
        case Operator::LE:  ret_ty = boolty; result = l <= r; break;
        case Operator::GT:  ret_ty = boolty; result = l >  r; break;
        case Operator::GE:  ret_ty = boolty; result = l >= r; break;
        case Operator::EQ:  ret_ty = boolty; result = l == r; break;
        case Operator::NE:  ret_ty = boolty; result = l != r; break;
        default: THROW_WRONG_OP(op);
        }
        RETURN_MTB_OWN(IntConst, ret_ty, result);
    }

    static inline owned<ConstantData> gen_constop(BinGenerator *bingen)
    {
        Value *const l = bingen->l.result,
              *const r = bingen->r.result;
        const auto lc = static_cast<ConstantData*>(l),
                   rc = static_cast<ConstantData*>(r);
        Type *const lty = l->get_value_type(),
             *const rty = r->get_value_type();
        const TypeTID ltid = lty->get_type_id(),
                      rtid = rty->get_type_id();

        const bool lty_float = ltid == TypeTID::FLOAT_TYPE,
                   rty_float = rtid == TypeTID::FLOAT_TYPE,
                   lty_int   = ltid == TypeTID::INT_TYPE,
                   rty_int   = rtid == TypeTID::INT_TYPE;

        Operator const op = bingen->ast_op;

        /* 都是整数, 则进行整数 eval 运算 */
        if (lty_int && rty_int) {
            IntType *const lity = static_cast<IntType*>(lty),
                    *const rity = static_cast<IntType*>(rty);
            const int lbit = lity->get_binary_bits(),
                      rbit = rity->get_binary_bits();
            return do_eval_int((lbit > rbit)? lity: rity,
                               lc->get_int_value(),
                               rc->get_int_value(), op);
        }
        /* 都是浮点, 则选出位数最大的进行浮点 eval 运算 */
        if (lty_float && rty_float) {
            double lvf64 = lc->get_float_value(),
                   rvf64 = rc->get_float_value();
            FloatType *const lfty = static_cast<FloatType*>(lty),
                      *const rfty = static_cast<FloatType*>(rty);
            const int lbit = lfty->get_binary_bits(),
                      rbit = rfty->get_binary_bits();
            FloatType *const afty = (lbit > rbit)? lfty: rfty;
            return do_eval_float(afty, op, lvf64, rvf64);
        }
        /* 选出类型为浮点的那个进行 eval 运算 */
        auto const afty = static_cast<FloatType*>(lty_float ? lty : rty);
        return do_eval_float(afty, op, lc->get_float_value(), rc->get_float_value());
    }

    static inline owned<Instruction>
    gen_float_calc_cmp_op(BinGenerator *gen)
    {
        using Cond = CompareSSA::Condition;
        Value *const l = gen->l.result,
              *const r = gen->r.result;
        owned<Instruction> ret;
        CompareSSA::Condition cond{CompareSSA::Condition::SIGNED_ORDERED};
        switch (gen->ast_op) {
        case Operator::PLUS:
            ret = BinarySSA::CreateAdd(l, r, BinarySSA::SignFlag::NSW);
            break;
        case Operator::SUB:
            ret = BinarySSA::CreateSub(l, r, BinarySSA::SignFlag::NSW);
            break;
        case Operator::STAR:
            ret = BinarySSA::CreateMul(l, r, BinarySSA::SignFlag::NSW);
            break;
        case Operator::SLASH:
            ret = BinarySSA::CreateFDiv(l, r);
            break;
        case Operator::PERCENT:
            ret = BinarySSA::CreateFRem(l, r);
            break;
        case Operator::LT:
            ret = CompareSSA::CreateFCmp(cond | Cond::LT, l, r);
            break;
        case Operator::EQ:
            ret = CompareSSA::CreateFCmp(cond | Cond::EQ, l, r);
            break;
        case Operator::GT:
            ret = CompareSSA::CreateFCmp(cond | Cond::GT, l, r);
            break;
        case Operator::LE:
            ret = CompareSSA::CreateFCmp(cond | Cond::LE, l, r);
            break;
        case Operator::NE:
            ret = CompareSSA::CreateFCmp(cond | Cond::NE, l, r);
            break;
        case Operator::GE:
            ret = CompareSSA::CreateFCmp(cond | Cond::GE, l, r);
            break;
        default: THROW_WRONG_OP(gen->ast_op);
        }
        gen->current->append(ret);
        return ret;
    }
    MTB_NONNULL(1) static inline owned<Instruction>
    gen_int_calc_cmp_op(BinGenerator *gen)
    {
        using Cond = CompareSSA::Condition;
        Value *const l = gen->l.result,
              *const r = gen->r.result;
        owned<Instruction> ret;
        CompareSSA::Condition cond{CompareSSA::Condition::SIGNED_ORDERED};
        switch (gen->ast_op) {
        case Operator::PLUS:
            ret = BinarySSA::CreateAdd(l, r, BinarySSA::SignFlag::NSW);
            break;
        case Operator::SUB:
            ret = BinarySSA::CreateSub(l, r, BinarySSA::SignFlag::NSW);
            break;
        case Operator::STAR:
            ret = BinarySSA::CreateMul(l, r, BinarySSA::SignFlag::NSW);
            break;
        case Operator::SLASH:
            ret = BinarySSA::CreateSDiv(l, r);
            break;
        case Operator::PERCENT:
            ret = BinarySSA::CreateSRem(l, r);
            break;
        case Operator::LT:
            ret = CompareSSA::CreateICmp(cond | Cond::LT, l, r);
            break;
        case Operator::EQ:
            ret = CompareSSA::CreateICmp(Cond::EQ, l, r);
            break;
        case Operator::GT:
            ret = CompareSSA::CreateICmp(cond | Cond::GT, l, r);
            break;
        case Operator::LE:
            ret = CompareSSA::CreateICmp(cond | Cond::LE, l, r);
            break;
        case Operator::NE:
            ret = CompareSSA::CreateICmp(Cond::NE, l, r);
            break;
        case Operator::GE:
            ret = CompareSSA::CreateICmp(cond | Cond::GE, l, r);
            break;
        default: THROW_WRONG_OP(gen->ast_op);
        }
        gen->current->append(ret);
        return ret;
    }

    MTB_NONNULL(1, 2, 3) static owned<Value>
    cast_iext(IntType *target, Value *operand, BasicBlock *current)
    {
        if (operand->get_type_id() == ValueTID::ZERO_CONST)
            return new IntConst(target, 0);
        auto oty = static_cast<IntType*>(operand->get_value_type());
        bool operand_bool = oty->get_binary_bits() == 1;

        if (operand->get_type_id() == ValueTID::INT_CONST) {
            auto iop = static_cast<IntConst*>(operand);
            int64_t value = operand_bool ?
                            iop->get_uint_value(): // bool 量直接使用 zext 扩展
                            iop->get_int_value();  // 其他量需要使用 sext 扩展
            return new IntConst(target, value);
        }

        owned<CastSSA> ret;
        if (operand_bool)
            ret = CastSSA::CreateZExtCast(target, operand);
        else
            ret = CastSSA::CreateSExtCast(target, operand);

        current->append(ret);
        return ret;
    }
    MTB_NONNULL(1, 2, 3) static owned<Value>
    cast_fpext(FloatType *target, Value *operand, BasicBlock *current)
    {
        if (operand->get_type_id() == ValueTID::ZERO_CONST)
            return new FloatConst(target, 0);
        auto oty = static_cast<FloatType*>(operand->get_value_type());

        if (operand->get_type_id() == ValueTID::FLOAT_CONST) {
            auto iop = static_cast<FloatConst*>(operand);
            return FloatConst::Create(target, iop->get_value());
        }
        auto ret = CastSSA::CreateFpExtCast(target, operand);
        current->append(ret);
        return ret;
    }
    MTB_NONNULL(1, 2, 3) static owned<Value>
    cast_itof(FloatType *target, Value *operand, BasicBlock *current)
    {
        if (operand->get_type_id() == ValueTID::ZERO_CONST)
            return FloatConst::Create(target, 0);
        if (operand->get_type_id() == ValueTID::INT_CONST) {
            return FloatConst::Create(target,
                    static_cast<IntConst*>(operand)->get_float_value());
        }
        auto ret = CastSSA::CreateIToFCast(target, operand);
        current->append(ret);
        return ret;
    }

    MTB_NONNULL(1) static inline void
    make_operand_cast(BinGenerator *gen)
    {
        Value *l   = gen->l.result, *r = gen->r.result;
        Type  *lty = l->get_value_type(),
              *rty = r->get_value_type();
        bool lty_int = lty->is_integer_type(),
             rty_int = rty->is_integer_type(),
             lty_float = lty->is_float_type(),
             rty_float = rty->is_float_type();
        BasicBlock *current = gen->current;
        
        /* LHS 和 RHS 都是整数, 执行位扩展转换 */
        if (lty_int && rty_int) {
            auto lity = static_cast<IntType*>(lty),
                 rity = static_cast<IntType*>(rty);
            uint32_t lbit = lity->get_binary_bits(),
                     rbit = rity->get_binary_bits();
            if (lbit == rbit)
                return;
            if (lbit >  rbit) {
                gen->r.result = cast_iext(lity, r, current);
                return;
            }
            /* else if (lbit < rbit) */
            gen->l.result = cast_iext(rity, l, current);
            return;
        }

        /* LHS、RHS 都是浮点, 执行位扩展转换 */
        if (lty_float && rty_float) {
            auto lfty = static_cast<FloatType*>(lty),
                 rfty = static_cast<FloatType*>(rty);
            uint32_t lbit = lfty->get_binary_bits(),
                     rbit = rfty->get_binary_bits();
            if (lfty == rfty || lfty->equals(rfty))
                return;
            if (lbit >= rbit) {
                gen->r.result = cast_fpext(lfty, r, current);
                return;
            }
            /* else if (lbit < rbit) */
            gen->l.result = cast_fpext(rfty, l, current);
            return;
        }

        /* LHS 是整数, RHS 是浮点, 执行整数变浮点转换 */
        if (rty_float && lty_int) {
            auto rfty = static_cast<FloatType*>(rty);
            gen->l.result = cast_itof(rfty, l, current);
            return;
        }

        /* LHS 是浮点, RHS 是整数, 执行整数变浮点转换 */
        if (lty_float && rty_int) {
            auto lfty = static_cast<FloatType*>(lty);
            gen->r.result = cast_itof(lfty, r, current);
            return;
        }

        throw TypeMismatchException {
            lty->is_value_type()? rty: lty,
            "BinaryExpr operand type must be value type"s,
            CURRENT_SRCLOC_F
        };
    }
}; // inline namespace binary_impl

void BinGenerator::generateCalcCmp()
{
    Value *lv = l.result, *rv = r.result;
    /* 都是编译期常量时, 直接在编译期完成计算 */
    if (use_generator_optimize &&
        is_data_const(lv) && is_data_const(rv)) {
        result.result = gen_constop(this);
        result.vcost  = 0;
        result.has_side_effect = false;
        return;
    }

    make_operand_cast(this);

    if (l.get_ir_type()->is_float_type()) {
        result.result = gen_float_calc_cmp_op(this);
        return;
    }
    if (l.get_ir_type()->is_integer_type()) {
        result.result = gen_int_calc_cmp_op(this);
        return;
    }

    crash_with_stacktrace(true, CURRENT_SRCLOC_F, "UNREACHABLE");
}

void BinGenerator::generateLogical(Ast::Expression *lhs, Ast::Expression *rhs)
{
    crash_with_stacktrace(
        true, CURRENT_SRCLOC_F,
        "MYGL compiler currently cannot process syntax that"
            "regard logic expression as normal value"s
    );
}

} // namespace MYGL::IRGen::ExprGen

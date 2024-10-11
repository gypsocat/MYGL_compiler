#include "base/mtb-compatibility.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/ir-opcode-reflect-map.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include <string_view>

#define OPCHK_FN_PAIR(name, fn1, fn2) \
    constexpr static const BinarySSA::OperandCheckPair name {fn1, fn2}
#define TOKEN_STR(token) #token
#define THROW_IF_NULL(operand, srcloc) \
    MTB_UNLIKELY_IF (operand == nullptr) {\
        throw NullException {\
            "BinarySSA." TOKEN_STR(operand) " in construct"sv,\
            "", srcloc\
        };\
    }
#define THROW_IF_NOT_INT(return_type, operand, OpCode) \
    MTB_UNLIKELY_IF (!return_type->is_integer_type()) {\
        throw TypeMismatchException {\
            return_type,\
            operand " of " OpCode " BinarySSA require Integer type"s,\
            CURRENT_SRCLOC_F\
        }; \
    }
#define THROW_IF_NOT_FLOAT(return_type, operand, OpCode) \
    MTB_UNLIKELY_IF (!return_type->is_float_type()) {\
        throw TypeMismatchException {\
            return_type,\
            operand " of " OpCode " BinarySSA require Float type"s,\
            CURRENT_SRCLOC_F\
        }; \
    }

namespace MYGL::IR {

inline namespace binary_ssa_check {
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    using namespace MTB::Compatibility;
    using std::string_view;
    using OpCode    = Instruction::OpCode;
    using CheckFunc = BinarySSA::OperandCheckFunc;

    /* 原 BinarySSA 检查函数使用哈希表做静态反射. 现在有了静态初始化、不哈希直接寻址的
     * OpCodeReflectMap, 找函数应该会快得多. */
    struct ReflectItem {
        CheckFunc   check_lhs;
        string_view lhs_err_msg;
        CheckFunc   check_rhs;
        string_view rhs_err_msg;
    }; // struct ReflectItem
    using CheckMapT = OpCodeReflectMap<ReflectItem, OpCode::ADD, OpCode::ASHR>;

    /* 类型检查函数. 成功则返回 true, 否则返回 false. 返回 false 时, BinarySSA 需要根据错误信息
     * 丢出合适的异常.
     *
     * 当 operand 为 null 时, 不进行任何检查, 直接返回 true. 这是为了适应部分时刻需要清除操作数
     * 的情况. */

    static bool type_equals_this(BinarySSA *self, Value *operand);
    static bool type_integer(BinarySSA *self, Value *operand);
    static bool type_float(BinarySSA *self, Value *operand);

    /* 检查失败时返回的信息, 用于抛异常. 目前只有类型失配异常, 所以不需要为异常类型专门再写一个枚举. */

    static constexpr string_view type_equal_fail_lhs = "BinarySSA lhs type should be equal to type of this"sv;
    static constexpr string_view type_equal_fail_rhs = "BinarySSA rhs type should be equal to type of this"sv;
    static constexpr string_view type_int_fail_lhs   = "BinarySSA lhs type should be integer"sv;
    static constexpr string_view type_int_fail_rhs   = "BinarySSA rhs type should be integer"sv;
    static constexpr string_view type_float_fail_lhs = "BinarySSA lhs type should be float"sv;
    static constexpr string_view type_float_fail_rhs = "BinarySSA rhs type should be float"sv;
    static constexpr ReflectItem opcheck_type_equal_pair {
        type_equals_this, type_equal_fail_lhs,
        type_equals_this, type_equal_fail_lhs
    };
    static constexpr ReflectItem opcheck_shift_pair {
        type_equals_this, type_equal_fail_lhs,
        type_integer,     type_int_fail_rhs
    };

    static CheckMapT operand_map {
        {OpCode::ADD,  opcheck_type_equal_pair},
        {OpCode::FADD, opcheck_type_equal_pair},
        {OpCode::SUB,  opcheck_type_equal_pair},
        {OpCode::FSUB, opcheck_type_equal_pair},
        {OpCode::MUL,  opcheck_type_equal_pair},
        {OpCode::FMUL, opcheck_type_equal_pair},
        {OpCode::SDIV, opcheck_type_equal_pair},
        {OpCode::UDIV, opcheck_type_equal_pair},
        {OpCode::FDIV, opcheck_type_equal_pair},
        {OpCode::SREM, opcheck_type_equal_pair},
        {OpCode::UREM, opcheck_type_equal_pair},
        {OpCode::FREM, opcheck_type_equal_pair},
        {OpCode::AND,  opcheck_type_equal_pair},
        {OpCode::OR,   opcheck_type_equal_pair},
        {OpCode::XOR,  opcheck_type_equal_pair},
        {OpCode::SHL,  opcheck_shift_pair},
        {OpCode::LSHR, opcheck_shift_pair},
        {OpCode::ASHR, opcheck_shift_pair},
    };

    static inline Type*
    init_type_equal_or_throw(Value *lhs, Value *rhs, SourceLocation &&srcloc)
    {
        THROW_IF_NULL(lhs, srcloc);
        THROW_IF_NULL(rhs, srcloc);
        Type *lhs_ty = lhs->get_value_type();
        Type *rhs_ty = rhs->get_value_type();
        if (lhs_ty != rhs_ty) {
            throw TypeMismatchException {
                rhs_ty, osb_fmt("BinarySSA rhs type"
                    " should be equal to lhs type "sv,
                    lhs_ty->toString(), " (", (void*)lhs_ty, ")"),
                srcloc
            };
        }
        return lhs_ty;
    }

    static bool type_equals_this(BinarySSA *self, Value *operand)
    {
        if (operand == nullptr)
            return true;
        Type *value_type   = self->get_value_type();
        Type *operand_type = operand->get_value_type();
        return value_type == operand_type ||
               value_type->equals(operand_type);
    }
    static bool type_integer(BinarySSA *self, Value *operand)
    {
        if (operand == nullptr)
            return true;
        return operand->get_value_type()->is_integer_type();
    }
    static bool type_float(BinarySSA *self, Value *operand)
    {
        if (operand == nullptr)
            return true;
        return operand->get_value_type()->is_float_type();
    }
} // inline namespace binary_ssa_check

inline namespace binary_ssa_impl {
    using RefT     = BinarySSA::RefT;
    using OpCode   = Instruction::OpCode;
    using SignFlag = BinarySSA::SignFlag;

    static inline RefT
    new_binary(OpCode opcode, Type *value_type, SignFlag sign_flag, owned<Value> lhs, owned<Value> rhs) {
        RETURN_MTB_OWN(BinarySSA, opcode, value_type, sign_flag, std::move(lhs), std::move(rhs));
    }
} // inline namespace binary_ssa_impl

#define RETURN_NEW_BINARY(opcode, sign_flag) \
    return new_binary(opcode, return_type, sign_flag, std::move(lhs), std::move(rhs))
#define RETURN_NEW_NUW_BINARY(opcode) \
    return new_binary(opcode, return_type, SignFlag::NUW, std::move(lhs), std::move(rhs))
#define RETURN_NEW_NSW_BINARY(opcode) \
    return new_binary(opcode, return_type, SignFlag::NSW, std::move(lhs), std::move(rhs))

/* ================ [static class BinarySSA] ================ */

    RefT BinarySSA::CreateAdd(owned<Value> lhs, owned<Value> rhs, SignFlag sign_flag)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);

        if (return_type->is_integer_type())
            RETURN_NEW_BINARY(OpCode::ADD, sign_flag);
        if (return_type->is_float_type())
            RETURN_NEW_NSW_BINARY(OpCode::FADD);

        throw TypeMismatchException {
            return_type,
            "lhs of Add/FAdd BinarySSA expected a value type"s,
            CURRENT_SRCLOC_F
        };
    }
    RefT BinarySSA::CreateSub(owned<Value> lhs, owned<Value> rhs, SignFlag sign_flag)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);

        if (return_type->is_integer_type())
            RETURN_NEW_BINARY(OpCode::SUB, sign_flag);
        if (return_type->is_float_type())
            RETURN_NEW_NSW_BINARY(OpCode::FSUB);

        throw TypeMismatchException {
            return_type,
            "lhs of Sub/FSub BinarySSA expected a value type"s,
            CURRENT_SRCLOC_F
        };
    }
    RefT BinarySSA::CreateMul(owned<Value> lhs, owned<Value> rhs, SignFlag sign_flag)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);

        if (return_type->is_integer_type())
            RETURN_NEW_BINARY(OpCode::MUL, sign_flag);
        if (return_type->is_float_type())
            RETURN_NEW_NSW_BINARY(OpCode::FMUL);

        throw TypeMismatchException {
            return_type,
            "lhs of Mul/FMul BinarySSA expected a value type"s,
            CURRENT_SRCLOC_F
        };
    }

    RefT BinarySSA::CreateIDiv(owned<Value> lhs, owned<Value> rhs, bool as_signed)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);

        MTB_UNLIKELY_IF (!return_type->is_integer_type()) {
            std::string_view msg = as_signed ?
                "lhs and rhs of SDiv BinarySSA requires an Integer type"sv:
                "lhs and rhs of UDiv BinarySSA requires an Integer type"sv;
            throw TypeMismatchException {
                return_type, std::string(msg),
                CURRENT_SRCLOC_F
            };
        }

        OpCode   opcode   = as_signed?  OpCode::SDIV:  OpCode::UDIV;
        SignFlag signflag = as_signed? SignFlag::NSW: SignFlag::NUW;

        RETURN_NEW_BINARY(opcode, signflag);
    }
    RefT BinarySSA::CreateIDiv(owned<Value> lhs, owned<Value> rhs, SignFlag flag)
    {
        bool as_signed = false;
        switch (flag) {
            case SignFlag::NSW:   as_signed = true;  break;
            case SignFlag::NUW:   as_signed = false; break;
            default:
                fputs("IDiv BinarySSA requires a valid (NSW or NUW) sign flag\n",
                      stderr);
                std::terminate();
        }
        return CreateIDiv(std::move(lhs), std::move(rhs), as_signed);
    }
    RefT BinarySSA::CreateFDiv(owned<Value> lhs, owned<Value> rhs)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);
        THROW_IF_NOT_FLOAT(return_type, "lhs and rhs", "FDiv");
        RETURN_NEW_NSW_BINARY(OpCode::FDIV);
    }
    RefT BinarySSA::CreateIRem(owned<Value> lhs, owned<Value> rhs, bool as_signed)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);

        MTB_UNLIKELY_IF (!return_type->is_integer_type()) {
            std::string_view msg = as_signed ?
                "lhs and rhs of SRem BinarySSA requires an Integer type"sv:
                "lhs and rhs of URem BinarySSA requires an Integer type"sv;
            throw TypeMismatchException {
                return_type, std::string(msg),
                CURRENT_SRCLOC_F
            };
        }

        OpCode   opcode   = as_signed?  OpCode::SREM:  OpCode::UREM;
        SignFlag signflag = as_signed? SignFlag::NSW: SignFlag::NUW;

        RETURN_NEW_BINARY(opcode, signflag);
    }
    RefT BinarySSA::CreateFRem(owned<Value> lhs, owned<Value> rhs)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);
        THROW_IF_NOT_FLOAT(return_type, "lhs and rhs", "FRem");
        RETURN_NEW_NSW_BINARY(OpCode::FREM);
    }

    RefT BinarySSA::CreateAnd(owned<Value> lhs, owned<Value> rhs)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);
        THROW_IF_NOT_INT(return_type, "lhs and rhs", "And");
        RETURN_NEW_NUW_BINARY(OpCode::AND);
    }
    RefT BinarySSA::CreateOr(owned<Value> lhs, owned<Value> rhs)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);
        THROW_IF_NOT_INT(return_type, "lhs and rhs", "Or");
        RETURN_NEW_NUW_BINARY(OpCode::OR);
    }
    RefT BinarySSA::CreateXor(owned<Value> lhs, owned<Value> rhs)
    {
        Type *return_type = init_type_equal_or_throw(lhs, rhs, CURRENT_SRCLOC_F);
        THROW_IF_NOT_INT(return_type, "lhs and rhs", "Xor");
        RETURN_NEW_NUW_BINARY(OpCode::XOR);
    }
    RefT BinarySSA::CreateShL(owned<Value> lhs, owned<Value> rhs)
    {
        THROW_IF_NULL(lhs, CURRENT_SRCLOC_F);
        THROW_IF_NULL(rhs, CURRENT_SRCLOC_F);
        Type *return_type = lhs->get_value_type();

        THROW_IF_NOT_INT(return_type, "lhs", "ShL");
        THROW_IF_NOT_INT(rhs->get_value_type(), "rhs", "ShL");
        RETURN_NEW_NUW_BINARY(OpCode::SHL);
    }
    RefT BinarySSA::CreateLShR(owned<Value> lhs, owned<Value> rhs)
    {
        THROW_IF_NULL(lhs, CURRENT_SRCLOC_F);
        THROW_IF_NULL(rhs, CURRENT_SRCLOC_F);
        Type *return_type = lhs->get_value_type();

        THROW_IF_NOT_INT(return_type, "lhs", "LShr");
        THROW_IF_NOT_INT(rhs->get_value_type(), "rhs", "LShr");
        RETURN_NEW_NUW_BINARY(OpCode::LSHR);
    }
    RefT BinarySSA::CreateAShR(owned<Value> lhs, owned<Value> rhs)
    {
        THROW_IF_NULL(lhs, CURRENT_SRCLOC_F);
        THROW_IF_NULL(rhs, CURRENT_SRCLOC_F);
        Type *return_type = lhs->get_value_type();

        THROW_IF_NOT_INT(return_type, "lhs", "AShr");
        THROW_IF_NOT_INT(rhs->get_value_type(), "rhs", "AShr");

        // ASHR 把 lhs 看成有符号的，把 rhs 看成无符号的.
        // 由于操作数 lhs 与返回值的符号属性相同, 故创建一个 nsw 指令.
        RETURN_NEW_NSW_BINARY(OpCode::ASHR);
    }
    
    RefT BinarySSA::Create(OpCode opcode, owned<Value> lhs, owned<Value> rhs, bool int_as_signed)
    {
#       define RETURN_VVS(Name) \
            return Create##Name(std::move(lhs), std::move(rhs), flag)
#       define RETURN_VVSE(Name) \
            return Create##Name(std::move(lhs), std::move(rhs), int_as_signed)
#       define RETURN_VV(Name) \
            return Create##Name(std::move(lhs), std::move(rhs))
        THROW_IF_NULL(lhs, CURRENT_SRCLOC_F);
        THROW_IF_NULL(rhs, CURRENT_SRCLOC_F);

        Type *lty = lhs->get_value_type();
        Type *rty = rhs->get_value_type();
        SignFlag flag = int_as_signed ? SignFlag::NSW: SignFlag::NUW;

        switch (opcode) {
        case OpCode::ADD:   case OpCode::FADD: RETURN_VVS(Add);
        case OpCode::SUB:   case OpCode::FSUB: RETURN_VVS(Sub);
        case OpCode::MUL:   case OpCode::FMUL: RETURN_VVS(Mul);
        case OpCode::SDIV:  case OpCode::UDIV:  case OpCode::FDIV:
            if (lty->is_float_type())
                RETURN_VV(FDiv);
            RETURN_VVSE(IDiv);
        case OpCode::SREM:  case OpCode::UREM:  case OpCode::FREM:
            if (lty->is_float_type())
                RETURN_VV(FRem);
            RETURN_VVSE(IRem);
        case OpCode::AND: RETURN_VV(And);
        case OpCode::OR:  RETURN_VV(Or);
        case OpCode::XOR: RETURN_VV(Xor);
        case OpCode::SHL: RETURN_VV(ShL);
        case OpCode::LSHR: case OpCode::ASHR:
            if (int_as_signed)
                RETURN_VV(AShR);
            RETURN_VV(LShR);
        default:
            throw OpCodeMismatchException {
                opcode,
                "opcode of BinarySSA should be binary opcode"s,
                ""s, CURRENT_SRCLOC_F
            };
        }
#       undef RETURN_VV
#       undef RETURN_VVS
#       undef RETURN_VVSE
    }


/* ================ [dyn class BinarySSA] ================ */
    BinarySSA::BinarySSA(OpCode opcode, Type *value_type, SignFlag sign_flag,
                         owned<Value> lhs, owned<Value> rhs)
        : Instruction(ValueTID::BINARY_SSA, value_type, opcode),
          _sign_flag(sign_flag),
          _operands{std::move(lhs), std::move(rhs)} {
        MYGL_ADD_USEE_PROPERTY(lhs);
        MYGL_ADD_USEE_PROPERTY(rhs);
    }
    BinarySSA::~BinarySSA() = default;

    bool BinarySSA::set_lhs(owned<Value> lhs)
    {
        if (_operands[0] == lhs)
            return true;
        
        /* 检查函数、错误信息等与 OpCode 强相关, 直接查表取索引即可. 不需要在
         * 指令实例中存储函数指针了. */
        ReflectItem &item = binary_ssa_check::operand_map[_opcode];
        if (!item.check_lhs(this, lhs)) {
            throw TypeMismatchException {
                lhs->get_value_type(),
                std::string(item.lhs_err_msg),
                CURRENT_SRCLOC_F
            };
        }
        Use *lhs_use = list_as_user().front();
        if (_operands[0] != nullptr)
            _operands[0]->removeUseAsUsee(lhs_use);
        if (lhs != nullptr)
            lhs->addUseAsUsee(lhs_use);
        _operands[0] = std::move(lhs);
        return true;
    }
    bool BinarySSA::set_rhs(owned<Value> rhs)
    {
        if (_operands[0] == rhs)
            return true;

        ReflectItem &item = binary_ssa_check::operand_map[_opcode];
        if (!item.check_rhs(this, rhs)) {
            throw TypeMismatchException {
                rhs->get_value_type(),
                std::string(item.rhs_err_msg),
                CURRENT_SRCLOC_F
            };
        }
        Use *rhs_use = list_as_user().at(1);
        if (_operands[1] != nullptr)
            _operands[1]->removeUseAsUsee(rhs_use);
        if (rhs != nullptr)
            rhs->addUseAsUsee(rhs_use);
        _operands[1] = std::move(rhs);
        return true;
    }

/* ================ [BinarySSA::override parent] ================ */
    void BinarySSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }
    Value *BinarySSA::operator[](size_t index) {
        return index >= 2 ? nullptr: _operands[index].get();
    }
    void BinarySSA::traverseOperands(ValueAccessFunc fn)
    {
        if (fn(*_operands[0])) return;
        if (fn(*_operands[1])) return;
    }
/* ================ [BinarySSA::override signal] ================ */
    void BinarySSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = ConnectStatus::CONNECTED;
    }
    void BinarySSA::on_parent_finalize()
    {
        if (_connect_status == ConnectStatus::FINALIZED)
            return;

        /* 被清除的仅仅是自己所在的基本块, 不能一次清空所有的 use 关系,
         * 所以只能查找并释放 use-def 关系 */
        auto use_iter = _list_as_user.begin();
        Use *lhs_use = use_iter.get();
        ++use_iter;
        Use *rhs_use = use_iter.get();

        if (_operands[0] != nullptr) {
            _operands[0]->removeUseAsUsee(lhs_use);
            _operands[0].reset();
        }
        if (_operands[1] != nullptr) {
            _operands[1]->removeUseAsUsee(rhs_use);
            _operands[1].reset();
        }
        _connect_status = ConnectStatus::FINALIZED;
    }
    void BinarySSA::on_function_finalize()
    {
        _operands[0].reset();
        _operands[1].reset();
        _connect_status = ConnectStatus::FINALIZED;
    }

    std::string_view BinarySSA::SignFlagGetString(SignFlag flag)
    {
        switch (flag) {
        case SignFlag::NSW:
            return "nsw";
        case SignFlag::NUW:
            return "nuw";
        default:
            return "";
        }
    }
} // namespace MYGL::IR

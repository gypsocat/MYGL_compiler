#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include "ir-instruction-shared.private.hxx"
#include "irbase-capatibility.private.hxx"
#include <initializer_list>
#include <string>

namespace MYGL::IR {
/* ================= [class CastSSA] ================ */
    inline namespace cast_ssa_impl {
        using RefT = owned<CastSSA>;

        static inline constexpr std::string_view
        boolean_stringfy(bool value) {
            using namespace std::string_view_literals;
            return value ? "true"sv: "false"sv;
        }

        static inline void
        check_throw_if_type_null(Type *target, SourceLocation &&src_loc)
        {
            MTB_UNLIKELY_IF(target == nullptr) {
                throw NullException {
                    "CastSSA.target_type",
                    "CastSSA target type cannot be null in constructing",
                    src_loc
                };
            }
        }

        static inline void
        check_throw_if_operand_null(Value *operand, SourceLocation &&src_loc)
        {
            MTB_UNLIKELY_IF(operand == nullptr) {
                throw NullException {
                    "CastSSA.operand",
                    "CastSSA operand cannot be null in constructing",
                    src_loc
                };
            }
        }

        static inline size_t
        type_get_binary_bits(Type *type)
        {
            constexpr size_t iso_bit_per_byte = 8;
            if (type->is_value_type())
                return static_cast<ValueType*>(type)->get_binary_bits();
            else
                return type->get_instance_size() * iso_bit_per_byte;
        }

        static inline std::string
        auto_create_value_cast_get_reason(Type *operand_type,
                                          Type *target,
                                          bool keeps_value)
        {
            using namespace capatibility;
            return osb_fmt(
                "Operand type (", operand_type->toString(),
                ") and target type (", target->toString(),
                ") should follow at least one of the next rules:\n"
                "- has the same binary bits\n"
                "- are all integers or floats\n"
                "also, the `bitcast` requires `keeps_value == false`.\n"
                "(current keeps_value is", boolean_stringfy(keeps_value), ")\n"
            );
        }

        static RefT new_cast(Instruction::OpCode opcode, Type *target_type, owned<Value> operand) {
            RETURN_MTB_OWN(CastSSA, opcode, target_type, std::move(operand));
        }
    }
    namespace cast_ssa_check {        
        using OperandCheckFunc = bool(*)(CastSSA* self, Value* operand, Type *target);

        struct CastOpCodePropertyMap {
            struct InitTuple {
                OpCodeT::self_t  opcode  = OpCodeT::NONE;
                bool        keeps_value  = false;
                bool        keeps_binary = false;
                bool        keeps_sign   = false;
                OperandCheckFunc check_func = nullptr;

                bool operator==(InitTuple const& rhs) const noexcept {
                    return opcode       == rhs.opcode       &&
                           keeps_value  == rhs.keeps_value  &&
                           keeps_binary == rhs.keeps_binary &&
                           keeps_sign   == rhs.keeps_sign   &&
                           check_func   == rhs.check_func;
                }
                bool operator!=(InitTuple const& rhs) const noexcept = default;
            }; // struct InitPair
            using InitListT = std::initializer_list<InitTuple>;

        public:
            constexpr CastOpCodePropertyMap(InitListT &&init_list) noexcept
                : check_func_array{} {
                for (InitTuple const& pair: init_list) {
                    size_t offset = pair.opcode - OpCodeT::ITOF;
                    check_func_array[offset] = std::move(pair);
                }
            }

            InitTuple const& operator[](OpCodeT opcode) const
            {
                size_t offset = opcode - OpCodeT::ITOF;
                return check_func_array[offset];
            }
            InitTuple& operator[](OpCodeT opcode)
            {
                size_t offset = opcode - OpCodeT::ITOF;
                return check_func_array[offset];
            }

            size_t size() const { return map_length; }

            bool contains(OpCodeT opcode) const {
                constexpr InitTuple empty = {};
                return opcode >= __begin && opcode <= __end &&
                       (*this)[opcode] != empty;
            }
        private:
            static constexpr OpCodeT::self_t __begin = OpCodeT::ITOF;
            static constexpr OpCodeT::self_t __end   = OpCodeT::FPTRUNC;
            static constexpr size_t map_length = __end - __begin + 8;
            InitTuple check_func_array[map_length];
        }; // OperandCheckMap

        static bool opcheck_same_type(CastSSA* self, Value* operand, Type *target) {
            Type *operand_type = operand->get_value_type();
            return target       == self->get_new_type() &&
                   operand_type->equals(operand->get_value_type());
        }
        static bool opcheck_itof(CastSSA *self, Value *operand, Type *target) {
            Type *operand_type = operand->get_value_type();
            return operand_type->is_integer_type() &&
                   target->is_float_type();
        }
        static bool opcheck_ftoi(CastSSA *self, Value *operand, Type *target) {
            Type *operand_type = operand->get_value_type();
            return operand_type->is_float_type() &&
                   target->is_integer_type();
        }
        static bool opcheck_same_bits(CastSSA *self, Value *operand, Type *target)
        {
            Type *operand_type = operand->get_value_type();
            return type_get_binary_bits(operand_type) == type_get_binary_bits(target);
        }
        static bool opcheck_iext(CastSSA *self, Value *operand, Type *target)
        {
            Type *operand_type = operand->get_value_type();
            return operand_type->is_integer_type() &&
                   target->is_integer_type() &&
                   type_get_binary_bits(operand_type) < type_get_binary_bits(target);
        }
        static bool opcheck_fpext(CastSSA *self, Value *operand, Type *target)
        {
            Type *operand_type = operand->get_value_type();
            return operand_type->is_float_type() &&
                   target->is_float_type() &&
                   type_get_binary_bits(operand_type) < type_get_binary_bits(target);
        }
        static bool opcheck_itrunc(CastSSA *self, Value *operand, Type *target)
        {
            Type *operand_type = operand->get_value_type();
            return operand_type->is_integer_type() &&
                   target->is_integer_type() &&
                   type_get_binary_bits(operand_type) > type_get_binary_bits(target);
        }
        static bool opcheck_fptrunc(CastSSA *self, Value *operand, Type *target)
        {
            Type *operand_type = operand->get_value_type();
            return operand_type->is_float_type() &&
                   target->is_float_type() &&
                   type_get_binary_bits(operand_type) > type_get_binary_bits(target);
        }

        static CastOpCodePropertyMap operand_check_map {
            {OpCodeT::ITOF, true,  false, true,  opcheck_itof},
            {OpCodeT::UTOF, true,  false, true,  opcheck_itof},
            {OpCodeT::FTOI, true,  false, true,  opcheck_ftoi},
            {OpCodeT::ZEXT, false, true,  false, opcheck_iext},
            {OpCodeT::SEXT, true,  true,  true,  opcheck_iext},
            {OpCodeT::BITCAST, false, true,  true,  opcheck_same_bits},
            {OpCodeT::TRUNC,   false, false, false, opcheck_itrunc},
            {OpCodeT::FPEXT,   true,  false, true,  opcheck_fpext},
            {OpCodeT::FPTRUNC, true,  false, true,  opcheck_fptrunc}
        }; // static OperandCheckMap operand_check_map

        static FloatType* target_float_or_throw(Type       *target,
                                                std::string mismatch_reason,
                                                SourceLocation &&srcloc)
        {
            if (!target->is_float_type()) {
                throw TypeMismatchException {
                    target,
                    std::move(mismatch_reason),
                    srcloc
                };
            }
            return static_cast<FloatType*>(target);
        }
        static IntType* target_int_or_throw(Type       *target,
                                            std::string mismatch_reason,
                                            SourceLocation &&srcloc)
        {
            if (!target->is_integer_type()) {
                throw TypeMismatchException {
                    target,
                    std::move(mismatch_reason),
                    srcloc
                };
            }
            return static_cast<IntType*>(target);
        }
#       define target_float_or_throw(target, reason) \
            cast_ssa_check::target_float_or_throw(target, reason, CURRENT_SRCLOC_F)
#       define target_int_or_throw(target, reason) \
            cast_ssa_check::target_int_or_throw(target, reason, CURRENT_SRCLOC_F)
    } // namespace cast_ssa_check

/* ============= [static class CastSSA] ============= */
    RefT CastSSA::CreateIToFCast(FloatType *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type *operand_type = operand->get_value_type();
        if (!operand_type->is_integer_type()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[ITOF] operand type should be integer type",
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::ITOF, target, std::move(operand));
    }

    RefT CastSSA::CreateUToFCast(FloatType *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type *operand_type = operand->get_value_type();
        if (!operand_type->is_integer_type()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[UTOF] operand type should be integer type",
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::UTOF, target, std::move(operand));
    }

    RefT CastSSA::CreateFToICast(IntType *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type *operand_type = operand->get_value_type();
        if (!operand_type->is_float_type()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[FTOI] operand type should be float type",
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::FTOI, target, std::move(operand));
    }

    RefT CastSSA::CreateZExtCast(IntType *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type *operand_type = operand->get_value_type();
        if (!operand_type->is_integer_type()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[ZEXT] operand type should be integer type",
                CURRENT_SRCLOC_F
            };
        }
        IntType *operand_ity = static_cast<IntType*>(operand_type);
        if (target->get_binary_bits() < operand_ity->get_binary_bits()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[ZEXT] operand integer type should be smaller than target type in binary bits",
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::ZEXT, target, std::move(operand));
    }
    RefT CastSSA::CreateSExtCast(IntType *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type *operand_type = operand->get_value_type();
        if (!operand_type->is_integer_type()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[SEXT] operand type should be integer type",
                CURRENT_SRCLOC_F
            };
        }
        IntType *operand_ity = static_cast<IntType*>(operand_type);
        if (target->get_binary_bits() < operand_ity->get_binary_bits()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[SEXT] operand integer type should be smaller than target type in binary bits",
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::SEXT, target, std::move(operand));
    }
    RefT CastSSA::CreateTruncCast(IntType *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type *operand_type = operand->get_value_type();
        if (!operand_type->is_integer_type()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[TRUNC] operand type should be integer type",
                CURRENT_SRCLOC_F
            };
        }
        IntType *operand_ity = static_cast<IntType*>(operand_type);
        if (target->get_binary_bits() > operand_ity->get_binary_bits()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[TRUNC] operand integer type should be larger than target type in binary bits",
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::TRUNC, target, std::move(operand));
    }
    RefT CastSSA::CreateBitCast(Type *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;

        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type  *operand_type = operand->get_value_type();
        size_t operand_bits = type_get_binary_bits(operand_type);
        size_t target_bits  = type_get_binary_bits(target);

        if (operand_bits != target_bits) {
            throw TypeMismatchException {
                operand_type,
                capatibility::osb_fmt(
                    "Operand type (", operand_bits,
                    " bits) should have the same binary bits with "
                    "target type (", target->toString(), ", ", target_bits, " bits)"),
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::BITCAST, target, std::move(operand));
    }
    RefT CastSSA::CreateFpExtCast(FloatType *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type *operand_type = operand->get_value_type();
        if (!operand_type->is_float_type()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[fpext] operand type should be float type",
                CURRENT_SRCLOC_F
            };
        }
        FloatType *operand_ity = static_cast<FloatType*>(operand_type);
        if (target->get_binary_bits() < operand_ity->get_binary_bits()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[fpext] operand float type should"
                " be smaller than target type in binary bits",
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::FPEXT, target, std::move(operand));
    }
    RefT CastSSA::CreateFpTruncCast(FloatType *target, owned<Value> operand)
    {
        using namespace cast_ssa_impl;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        Type *operand_type = operand->get_value_type();
        if (!operand_type->is_float_type()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[fptrunc] operand type should be float type",
                CURRENT_SRCLOC_F
            };
        }
        FloatType *operand_ity = static_cast<FloatType*>(operand_type);
        if (target->get_binary_bits() > operand_ity->get_binary_bits()) {
            throw TypeMismatchException {
                operand_type,
                "CastSSA[fptrunc] operand float type should"
                " be smaller than target type in binary bits",
                CURRENT_SRCLOC_F
            };
        }
        return new_cast(OpCode::FPTRUNC, target, std::move(operand));
    }

/* =============== [ CastSSA::static collection creator ] =============== */

    RefT CastSSA::CreateTruncOrExtCast(Type *target, owned<Value> operand, bool keeps_value)
    {
        using namespace capatibility;
        using namespace cast_ssa_impl;

        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);
        /* 类型相等, 无需转换, 返回 null */
        if (target->equals(operand->get_value_type()))
            return nullptr;

        Type *operand_type  = operand->get_value_type();
        size_t operand_bits = type_get_binary_bits(operand_type);
        size_t target_bits  = type_get_binary_bits(target);

        /* Integer type cast */
        if (operand_type->is_integer_type()) {
            if (!target->is_integer_type()) {
                if (operand_bits == target_bits && !keeps_value)
                    return CreateBitCast(target, std::move(operand));

                throw TypeMismatchException {
                    operand_type,
                    auto_create_value_cast_get_reason(operand_type, target, keeps_value),
                    CURRENT_SRCLOC_F
                };
            }

            /* 目标类型和源类型都是整数, 执行 zext/sext/trunc */
            IntType *target_ity = static_cast<IntType*>(target);
            if (operand_bits > target_bits)
                return CreateTruncCast(target_ity, std::move(operand));
            if (keeps_value)
                return CreateSExtCast(target_ity, std::move(operand));
            return CreateZExtCast(target_ity, std::move(operand));
        }
        /* Float type cast */
        if (operand_type->is_float_type()) {
            if (!target->is_float_type() && !keeps_value) {
                if (operand_bits == target_bits)
                    return CreateBitCast(target, std::move(operand));

                throw TypeMismatchException {
                    operand_type,
                    auto_create_value_cast_get_reason(operand_type, target, keeps_value),
                    CURRENT_SRCLOC_F
                };
            }

            /* 目标类型和源类型都是浮点，执行 fpext/fptrunc */
            FloatType *target_fty = static_cast<FloatType*>(target);
            if (operand_bits > target_bits)
                return CreateFpTruncCast(target_fty, std::move(operand));
            return CreateFpExtCast(target_fty, std::move(operand));
        }

        if (operand_bits == target_bits)
            return CreateBitCast(target, std::move(operand));

        throw TypeMismatchException {
            operand_type,
            auto_create_value_cast_get_reason(operand_type, target, keeps_value),
            CURRENT_SRCLOC_F
        };
    }

    RefT CastSSA::CreateCheckedCast(OpCode opcode, Type *target, owned<Value> operand)
    {
        using namespace cast_ssa_check;
        check_throw_if_operand_null(operand, CURRENT_SRCLOC_F);
        check_throw_if_type_null(target, CURRENT_SRCLOC_F);

        switch (opcode.get()) {
        case OpCodeT::ITOF: {
            FloatType *target_fty = target_float_or_throw(target,
                "`itof` cast requires target type be float");
            return CreateIToFCast(target_fty, std::move(operand));
        }
        case OpCodeT::FTOI: {
            IntType *target_ity = target_int_or_throw(target,
                "`ftoi` cast requires target type be int");
            return CreateFToICast(target_ity, std::move(operand));
        }
        case OpCodeT::ZEXT: {
            IntType *target_ity = target_int_or_throw(target,
                "`zext` cast requires target type be int");
            return CreateZExtCast(target_ity, std::move(operand));
        }
        case OpCodeT::SEXT: {
            IntType *target_ity = target_int_or_throw(target,
                "`sext` cast requires target type be int");
            return CreateSExtCast(target_ity, std::move(operand));
        }
        case OpCodeT::TRUNC: {
            IntType *target_ity = target_int_or_throw(target,
                "`trunc` cast requires target type be int");
            return CreateTruncCast(target_ity, std::move(operand));
        }
        case OpCodeT::BITCAST: {
            return CreateBitCast(target, std::move(operand));
        }
        case OpCodeT::FPEXT: {
            FloatType *target_fty = target_float_or_throw(target,
                "`fpext` cast requires target type be float");
            return CreateFpExtCast(target_fty, std::move(operand));
        }
        case OpCodeT::FPTRUNC: {
            FloatType *target_fty = target_float_or_throw(target,
                "`fptrunc` cast requires target type be float");
            return CreateFpTruncCast(target_fty, std::move(operand));
        }
        default:
            throw OpCodeMismatchException {
                opcode,
                "opcode isn't in area [OpCode::ITOF, OpCode::FPTRUNC]",
                "", CURRENT_SRCLOC_F
            };
        }
    }

/* ============= [public class CastSSA] ============= */
    CastSSA::CastSSA(OpCode opcode, Type *target_type, owned<Value> operand)
        : UnarySSA(ValueTID::CAST_SSA, target_type, opcode, std::move(operand)){
    }

    bool CastSSA::keeps_value() const {
        return cast_ssa_check::operand_check_map[_opcode].keeps_value;
    }
    bool CastSSA::keeps_binary() const {
        return cast_ssa_check::operand_check_map[_opcode].keeps_binary;
    }
    bool CastSSA::keeps_sign() const {
        return cast_ssa_check::operand_check_map[_opcode].keeps_sign;
    }
    CastMode CastSSA::get_cast_mode() const {
        CastMode ret = CastMode::CAST_NONE;
        if (keeps_value())
            ret = CastMode(size_t(ret) | size_t(CastMode::STATIC_CAST));
        if (keeps_binary())
            ret = CastMode(size_t(ret) | size_t(CastMode::REINTERPRET_CAST));
        return ret;
    }
    /*private*/ bool CastSSA::checkOperand(Value *operand)
    {
        return cast_ssa_check::operand_check_map[_opcode]
              .check_func(this, operand, get_value_type());
    }
/* ============== [CastSSA::overrides] ============== */
    void CastSSA::accept(IValueVisitor &visitor)
    {
        visitor.visit(this);
    }
/* =============== [CastSSA::signals] =============== */
    void CastSSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void CastSSA::on_parent_finalize()
    {
        _operand.reset();
    }
    void CastSSA::on_function_finalize()
    {
        return on_parent_finalize();
    }
/* =============== [end class CastSSA] ============== */

} // namespace MYGL::IR

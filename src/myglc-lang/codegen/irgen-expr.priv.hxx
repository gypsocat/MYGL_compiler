#pragma once

#include "base/mtb-base.hxx"
#include "mygl-ir/irbase-type.hxx"
#include "myglc-lang/codegen/irgen-exprgen.hxx"

#define THROW_WRONG_OP(op) do {\
        char   __buffer__[64]; ((size_t&)*__buffer__) = 0;\
        size_t __buflen__ = snprintf(__buffer__, 64, "token %d",\
                                     int(op));\
        throw EvalException {\
            EvalException::Code::WRONG_OPCODE,\
            {__buffer__, __buflen__},\
            CURRENT_SRCLOC_F \
        };\
    } while (false)

namespace MYGL::IRGen::ExprGen {

struct ExprResult;
using namespace MTB;
using namespace MTB::Compatibility;

using IrValue    = IRBase::Value;
using OpCode     = struct IR::Instruction::OpCode;
using Operator   = enum Ast::Expression::Operator;

struct ExprResult {
    Ast::Expression *expr;  // 对应的表达式
    owned<IrValue> result;  // 结果 Value
    uint32_t        vcost;  // 此表达式与子表达式一共存在的消耗
    bool  has_side_effect;  // 是否存在副作用 (变量改变、输入输出等)
public:
    IRBase::Type *get_ir_type() {
        return result == nullptr? nullptr: result->get_value_type();
    }
}; // struct ExprResult


/** 二元运算表达式生成器, 兼容下面的 */
struct BinGenerator {

    BinGenerator() = default;

    BinGenerator(ExprGenerator *parent, IR::BasicBlock *current, bool use_gen_opt = false)
        : parent(parent), current(current),
          use_generator_optimize(use_gen_opt) {}
public:
    ExprGenerator  *parent;
    ExprResult     l, r;
    ExprResult     result;
    Operator       ast_op;
    IR::BasicBlock *lb, *rb, *current;
    bool           use_generator_optimize;

    /** 阅读计算/比较表达式, 生成相应指令. 生成的指令应当都在同一组基本块内. */
    void generateCalcCmp();

    /** 阅读逻辑表达式, 生成相应指令. 生成的指令可能会在同一基本块内, 也可能会
     *  分成几个基本块. */
    void generateLogical(Ast::Expression *lhs, Ast::Expression *rhs);
}; // struct BinaryGenContext

/** @struct RuntimeShared
 *  @brief 运行时信息, 离开内部方法后就会消失 */
struct ExprGenerator::Runtime {
public:
    ExprGenerator  *parent;
    owned<IrValue>  retval;
    ExprResult      prev_result;
    IR::BasicBlock *current_block;
    IRBase::IntType *i32ty, *boolty;
    IRBase::FloatType *f32ty;
}; // struct ExprGenerator::RuntimeShared

namespace shared_impl {
    using namespace IR;

    class EvalException: public MTB::Exception {
    public:
        enum Code: uint8_t {
            NONE,         // 找不到相关错误
            ZERO_DIV_RHS, // 以 0 作为除数
            ZERO_REM_RHS, // 以 0 作为求余除数
            WRONG_OPCODE, // 运算符错误
        }; // enum Code
        static inline char const* code_make_string(Code code)
        {
            static const char map[][16] = {
                "NONE",
                "ZERO_DIV_RHS", "ZERO_REM_RHS",
                "WRONG_OPCODE"
            };
            return map[code];
        }
        static inline char const* code_make_reason(Code code)
        {
            static const char *const map[] = {
                "Unknown reason",
                "Use constant 0 as divident of a `divide(/)` expression",
                "Use constant 0 as divident of a `mod(%)` expression",
                "Wrong Opcode or Opeator",
            };
            return map[code];
        }
        static inline void stringfy(std::string &out_str, Code code,
                                    std::string_view tips)
        {
            if (tips.empty()) {
                out_str = rt_fmt("EvalException because of [%s]",
                    code_make_reason(code));
            } else {
                out_str = rt_fmt("EvalException because of [%s]: %*s",
                    code_make_reason(code),
                    int(tips.length()), tips.begin());
            }
        }
    public:
        EvalException(Code code, std::string_view tips, SourceLocation cur)
            : MTB::Exception{
                ErrorLevel::CRITICAL, std::string_view{}, cur
            }, code(code) {
            stringfy(msg, code, tips);
        }

        Code code;
    }; // EvalException


    MTB_NONNULL(1) static inline bool is_data_const(Value const* v)
    {
        ValueTID t = v->get_type_id();
        return (t == ValueTID::INT_CONST) ||
               (t == ValueTID::FLOAT_CONST) ||
               (t == ValueTID::ZERO_CONST);
    }
    MTB_NONNULL(1) static inline bool is_boolty(Type const* t)
    {
        if (t->get_type_id() != TypeTID::INT_TYPE)
            return false;
        auto ity = static_cast<IntType const*>(t);
        return ity->is_bool_type();
    }
    MTB_NONNULL(1) static inline IntType *get_boolty(Type *known)
    {
        if (is_boolty(known))
            return static_cast<IntType*>(known);
        return known->get_type_context()->getIntType(1);
    }

} // namespace binary_shared_impl

}
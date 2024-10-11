#pragma once

#include "base/mtb-object.hxx"
#include "mygl-ir/ir-basic-value.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-module.hxx"
#include "mygl-ir/irbase-type.hxx"
#include "mygl-ir/irbase-use-def.hxx"
#include "mygl-ir/ir-builder.hxx"
#include "myglc-lang/ast-node.hxx"
#include "myglc-lang/ast-code-visitor.hxx"
#include "myglc-lang/code-visitors/expr-checker.hxx"
#include "myglc-lang/codegen/irgen-type-forwarding.hxx"
#include "myglc-lang/codegen/irgen-symbol-mapping.hxx"
#include <cstdint>
#include <deque>
#include <memory>

namespace MYGL::IRGen {
    using namespace Ast;
    using namespace MTB;
    using namespace GenUtil;

    class Generator: public Object,
                     public CodeVisitor {
    public:
        using PtrT        = std::shared_ptr<Generator>;
        using UnownedPtrT = std::weak_ptr<Generator>;

        using TypePtrT     = unowned<IRBase::Type>;
        using ValuePtrT    = owned<IRBase::Value>;
        using UserPtrT     = owned<IRBase::User>;
        using UsePtrT      = owned<IRBase::Use>;
        using InstructPtrT = owned<IR::Instruction>;
        using ModulePtrT   = owned<IR::Module>;
        using FuncPtrT     = owned<IR::Function>;
        using BlockPtrT    = owned<IR::BasicBlock>;

        using TypeListT     = std::deque<TypePtrT>;

        using ConditionT    = int64_t;
        using OperatorT     = Expression::Operator;

        using SymbolInfoManager = FunctionLocal::SymbolInfoManager;
    public:
        Generator(CodeContext &ctx);

        owned<IR::Builder> generate();

        bool visit(UnaryExpr::UnownedPtrT node) override;
        bool visit(BinaryExpr::UnownedPtrT node) override;
        bool visit(CallParam::UnownedPtrT node) override;
        bool visit(CallExpr::UnownedPtrT node) override;
        bool visit(InitList::UnownedPtrT node) override;
        bool visit(IndexExpr::UnownedPtrT node) override;
        bool visit(Identifier::UnownedPtrT node) override;
        bool visit(IntValue::UnownedPtrT node) override;
        bool visit(FloatValue::UnownedPtrT node) override;
        bool visit(StringValue::UnownedPtrT node) override;
        bool visit(AssignExpr::UnownedPtrT node) override;

        /* Statements */
        bool visit(Block::UnownedPtrT node) override;
        bool visit(IfStmt::UnownedPtrT node) override;
        bool visit(WhileStmt::UnownedPtrT node) override;
        bool visit(EmptyStmt::UnownedPtrT node) override;
        bool visit(ReturnStmt::UnownedPtrT node) override;
        bool visit(BreakStmt::UnownedPtrT node) override;
        bool visit(ContinueStmt::UnownedPtrT node) override;
        bool visit(ExprStmt::UnownedPtrT node) override;
        bool visit(ConstDecl::UnownedPtrT node) override;
        bool visit(VarDecl::UnownedPtrT node) override;

        bool visit(Function::UnownedPtrT node) override;
        bool visit(Variable::UnownedPtrT node) override;
        bool visit(Type::UnownedPtrT node) override { return true; }
        bool visit(CompUnit::UnownedPtrT node) override;
        bool visit(FuncParam::UnownedPtrT node) override;
        bool visit(ArrayInfo::UnownedPtrT node) override;
    private:
        void _generate_constant_globl(Variable *constant);
        void _generate_variable_globl(Variable *variable);

    /* ============ [Local Variable Management] ============ */
        void _generate_constant_nested(Variable *constant);
        void _generate_variable_nested(Variable *variable);
        void _visit_current_scope();

        struct RuntimeData;

        RuntimeData &_get_runtime_data();
    private:
        CodeContext        &ctx;

        ExprChecker        _checker;
        TypeMapper         _mapper;
        SymbolInfoManager  _symbol_info_manager;

        owned<IR::Builder> _builder;
        IR::Module        *_module;

        /** 运行时栈分配，生成结束后直接销毁 */
        RuntimeData       *_runtime_data;
    }; // class Generator
} // namespace MYGL::IR

#pragma once

#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction-base.hxx"
#include "myglc-lang/ast-code-visitor.hxx"
#include "irgen-symbol-mapping.hxx"

namespace MYGL::IRGen::ExprGen {

    class ExprGenerator: public MTB::Object,
                         public Ast::CodeVisitor {
    public:
        using FuncLocalMap  = SymbolMapping::FunctionLocalMap;
        incomplete struct Runtime;

        ExprGenerator(IR::BasicBlock *working_block, FuncLocalMap *symbol_map);
        ~ExprGenerator();
    public:
        /** 生成合适的 Value. 绝大多数情况下生成的都是指令，但是有一部分情况下生成的是
         *  常量 */
        MTB::owned<IR::Value> generate(Ast::Expression::UnownedPtrT expr);

        struct Runtime *unsafe_get_runtime() const { return _runtime; }
    private:
        FuncLocalMap   *_local_map;
        IR::BasicBlock *_startup_block;
        IRBase::TypeContext *_type_ctx;

        /* 离开内部方法后就无效, 不可暴露 */
        struct Runtime *_runtime;
    public:
        /* 处理运算:
         * - LOGIC::NOT(!)
         * - PLUS(+) NEG(-) */
        bool visit(Ast::UnaryExpr::UnownedPtrT  node) override;

        /* 处理运算:
         * - ADD(+) SUB(-) MUL(*) DIV(/) REM(%)
         * - LT(<) EQ(==) GT(>) LE(<=) NE(!=) GE(>=)
         * 处理错误的语法:
         * - LOGIC::AND(&&)
         * - LOGIC::ORR(||) */
        bool visit(Ast::BinaryExpr::UnownedPtrT node) override;

        /* 处理函数关系. CallParam 结点因为没有任何多态性, 因此不经过
         * `CallParam.accept()`，直接翻译.
         *
         * 处理正确的语法: identifier(Exp, ...) */
        bool visit(Ast::CallExpr::UnownedPtrT   node) override;

        /* 这里需要注意讨论初始化列表位于常量/变量等定义的情况
         * 处理正确的语法: {Exp, {Exp, ...}} */
        bool visit(Ast::InitList::UnownedPtrT   node) override;

        /* 这里需要注意讨论: 等待索引的 Value 是值语义的, 还是引用语义的?
         * 处理正确的语法: identifier[Exp]... */
        bool visit(Ast::IndexExpr::UnownedPtrT  node) override;

        /* 需要加一个作用域说明, 否则遇到 int a = a + 1 这样的语句会出问题 */
        bool visit(Ast::Identifier::UnownedPtrT node) override;

        bool visit(Ast::IntValue::UnownedPtrT   node) override;
        bool visit(Ast::FloatValue::UnownedPtrT node) override;
        bool visit(Ast::StringValue::UnownedPtrT node) override;
        bool visit(Ast::AssignExpr::UnownedPtrT  node) override;
    }; // class ExprGenerator
} // namespace MYGL::IRGen::ExprGen
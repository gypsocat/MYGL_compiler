/** @file ast-printer.hxx MYGL AST Printer */
#pragma once

#include "../ast-code-visitor.hxx"
#include "../ast-code-context.hxx"
#include <format>
#include <iostream>

namespace MYGL::Ast {
    /** @class Printer
     * @brief 语法树打印机 */
    class Printer: public CodeVisitor {
    public:
        Printer(CodeContext &ctx, std::ostream &outfile = std::cout)
            : CodeVisitor(), _ctx(ctx), _outfile(outfile), _indent(0) {}
        
        bool print()
        {
            _outfile << std::format("<!--SysY source `{}`-->", _ctx.get_filename());
            wrap_indent();
            return _ctx.root()->accept(this);
        }
        bool println() {
            bool ret = print();
            _outfile << std::endl;
            return ret;
        }

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
        bool visit(IfStmt::UnownedPtrT node) override;
        bool visit(WhileStmt::UnownedPtrT node) override;
        bool visit(EmptyStmt::UnownedPtrT node) override;
        bool visit(ReturnStmt::UnownedPtrT node) override;
        bool visit(BreakStmt::UnownedPtrT node) override;
        bool visit(ContinueStmt::UnownedPtrT node) override;
        bool visit(Block::UnownedPtrT node) override;
        bool visit(ExprStmt::UnownedPtrT node) override;
        bool visit(ConstDecl::UnownedPtrT node) override;
        bool visit(VarDecl::UnownedPtrT node) override;
        bool visit(Function::UnownedPtrT node) override;
        bool visit(Variable::UnownedPtrT node) override;
        bool visit(Type::UnownedPtrT node) override;
        bool visit(CompUnit::UnownedPtrT node) override;
        bool visit(FuncParam::UnownedPtrT node) override;
        bool visit(ArrayInfo::UnownedPtrT node) override;
    public:
        std::string indent_str = "  ";
    protected:
        int _indent;
        std::ostream &_outfile;
        CodeContext  &_ctx;

        inline void indent(std::string_view space)
        {
            std::string indent_str;
            for (int i = 0; i < _indent; i++)
                indent_str += space;
            _outfile << indent_str;
        }
        inline void indent() { indent(indent_str); }
        void wrap_indent(std::string_view space)
        {
            _outfile << std::endl;
            indent(space);
        }
        inline void wrap_indent() { wrap_indent(indent_str); }
        inline void indent_inc() { _indent++; }
        inline void indent_dec() { _indent--; }
    }; // class Printer
} // namespace MYGL::Ast

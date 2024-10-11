#pragma once

#include "base.hxx"
#include "ast-scope.hxx"
#include "ast-node.hxx"

namespace MYGL::Ast {
    interface CodeVisitor {
    public:
        Scope *current_scope() const { return _current_scope; }
        abstract bool visit(UnaryExpr::UnownedPtrT node) { return false; }
        abstract bool visit(BinaryExpr::UnownedPtrT node) { return false; }
        abstract bool visit(CallParam::UnownedPtrT node) { return false; }
        abstract bool visit(CallExpr::UnownedPtrT node)  { return false; }
        abstract bool visit(InitList::UnownedPtrT node)  { return false; }
        abstract bool visit(IndexExpr::UnownedPtrT node) { return false; }
        abstract bool visit(Identifier::UnownedPtrT node) { return false; }
        abstract bool visit(IntValue::UnownedPtrT node)  { return false; }
        abstract bool visit(FloatValue::UnownedPtrT node) { return false; }
        abstract bool visit(StringValue::UnownedPtrT node) { return false; }
        abstract bool visit(AssignExpr::UnownedPtrT node) { return false; }
        abstract bool visit(IfStmt::UnownedPtrT node)    { return false; }
        abstract bool visit(WhileStmt::UnownedPtrT node) { return false; }
        abstract bool visit(EmptyStmt::UnownedPtrT node) { return false; }
        abstract bool visit(ReturnStmt::UnownedPtrT node) { return false; }
        abstract bool visit(BreakStmt::UnownedPtrT node) { return false; }
        abstract bool visit(ContinueStmt::UnownedPtrT node) { return false; }
        abstract bool visit(Block::UnownedPtrT node)     { return false; }
        abstract bool visit(ExprStmt::UnownedPtrT node)  { return false; }
        abstract bool visit(ConstDecl::UnownedPtrT node) { return false; }
        abstract bool visit(VarDecl::UnownedPtrT node)   { return false; }
        abstract bool visit(Function::UnownedPtrT node)  { return false; }
        abstract bool visit(Variable::UnownedPtrT node)  { return false; }
        abstract bool visit(Type::UnownedPtrT node)      { return false; }
        abstract bool visit(CompUnit::UnownedPtrT node)  { return false; }
        abstract bool visit(FuncParam::UnownedPtrT node) { return false; }
        abstract bool visit(ArrayInfo::UnownedPtrT node) { return false; }
    protected:
        CodeVisitor() noexcept;
    protected:
        Scope *_current_scope;
    }; // class CodeVisitor
} // namespace MYGL::Ast

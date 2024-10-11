#ifndef __MYGL_LANG_AST_DECL_H__
#define __MYGL_LANG_AST_DECL_H__ 1L
#include "base.hxx"

namespace MYGL::Ast {
/** nodes */
    imcomplete class Node;

    imcomplete class Statement;
    imcomplete class Definition;
    imcomplete class Expression;

    /** statements */
    imcomplete class Declaration;
    class IfStmt;
    class WhileStmt;
    class ReturnStmt;
    class BreakStmt;
    class ContinueStmt;
    class EmptyStmt;
    class Block;
    class CompUnit;
    class ExprStmt;

    /** declarations */
    class ConstDecl;
    class VarDecl;

    /** definitions */
    class Function;
    class Variable;
    class FuncParam;
    class Type;
    class VarType;
    class ArrayInfo;

    /** expressions */
    imcomplete class Value;
    class IntValue;
    class FloatValue;
    class StringValue;
    class Identifier;
    class AssignExpr;
    class UnaryExpr;
    class BinaryExpr;
    class CallExpr;
    class InitList;
    class IndexExpr;

/** non-ast classes */
    class Scope;
    interface ScopeContainer;
    imcomplete class CodeVisitor;
    class Sema;
    class IRGenerator;

/** CodeContext utils */
    class CodeContext;
    struct SourceLocation;
    struct SourceRange;
} // namespace MYGL::Ast

#endif
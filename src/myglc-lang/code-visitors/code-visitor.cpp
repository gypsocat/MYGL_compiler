#include "../../include/myglc-lang/ast-code-visitor.hxx"

using namespace MYGL::Ast;

CodeVisitor::CodeVisitor() noexcept
    : _current_scope(nullptr) {
}
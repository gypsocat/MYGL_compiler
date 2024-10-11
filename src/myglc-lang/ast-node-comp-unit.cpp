#include "myglc-lang/ast-node-decl.hxx"
#include "myglc-lang/ast-node.hxx"
#include "myglc-lang/ast-code-visitor.hxx"
#include "myglc-lang/util.hxx"
#include <memory>
#include <string_view>
#include <iostream>


namespace MYGL::Ast {
/* public class CompUnit */
    // variable
    CompUnit::PtrT CompUnit::OwnedPredefined = std::make_unique<CompUnit>(SourceRange{});
    CompUnit &CompUnit::Predefined = *OwnedPredefined;

    // constructor
    CompUnit::CompUnit(SourceRange const& range)
        : Node(range, NodeType::COMP_UNIT, &CompUnit::Predefined),
          _scope_self(std::make_unique<Scope>(nullptr, nullptr)) {
        owner_instance = this;
    }

    // method
    bool CompUnit::accept(CodeVisitor *visitor) {
        return visitor->visit(this);
    }
    bool CompUnit::accept_children(CodeVisitor *visitor) {
        bool ret = true;
        for (auto decl: _decls) {
            ret = decl->accept(visitor);
            if (!ret) return false;
        }
        for (auto func: _funcdefs){ 
            ret = func->accept(visitor);
            if (!ret) return false;
        }
        return true;
    }
    bool CompUnit::register_this(Scope *owner_scope)
    {
        if (owner_scope == nullptr)
            owner_scope = CompUnit::Predefined.scope_self();
        _scope = owner_scope;
        for (auto &i: _decls) {
            std::clog << std::format("got decl type {}", uint64_t(i->node_type())) << std::endl;
            i->register_this(_scope_self.get());
        }
        for (auto &i: _funcdefs)
            i->register_this(_scope_self.get());
        return true;
    }
    void CompUnit::append(Declaration::PtrT &&decl) {
        decl->set_parent(this);
        decl->set_scope(_scope_self.get());
        decls().push_back(decl);
    }
    bool CompUnit::append(Function::PtrT &&fndef)
    {
        if (_scope_self->add(fndef.get()) == false)
            return false;
        fndef->set_parent(this);
        fndef->set_scope(_scope_self.get());
        fndef->register_this();
        funcdefs().push_back(fndef);
        return true;
    }
    void CompUnit::prepend(Declaration::PtrT &&decl) {
        decl->set_parent(this);
        decl->set_scope(_scope_self.get());
        decls().push_front(decl);
    }
    bool CompUnit::prepend(Function::PtrT &&fndef)
    {
        if (_scope_self->add(fndef.get()) == false)
            return false;
        fndef->set_parent(this);
        fndef->set_scope(_scope_self.get());
        fndef->register_this();
        funcdefs().push_front(fndef);
        return true;
    }

    Function *CompUnit::get_function(std::string_view name)
    {
        if (!_scope_self->has_function(name))
            return nullptr;
        return _scope_self->get_function(name);
    }
    Variable *CompUnit::get_variable(std::string_view name)
    {
        return _scope_self->get_constant_or_variable(name);
    }
} // namespace MYGL::Ast

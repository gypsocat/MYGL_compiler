#include "base/mtb-object.hxx"
#include "mygl-ir/ir-builder.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/irbase-type.hxx"
#include "myglc-lang/ast-code-context.hxx"
#include "myglc-lang/ast-node-decl.hxx"
#include "myglc-lang/ast-node.hxx"
#include "myglc-lang/ast-scope.hxx"
#include "myglc-lang/codegen/irgen-type-forwarding.hxx"
#include "myglc-lang/codegen/irgen-generator.hxx"
#include <cstdio>
#include <memory>
#include <string_view>

#define scope_ctx_save(scope) \
    Ast::Scope *__scope_save__ = _current_scope;\
    _current_scope = scope
#define scope_restore_return(val) \
    _current_scope = __scope_save__;\
    return val

namespace MYGL::IRGen {
using namespace MTB;

using std::string_view;
using std::static_pointer_cast;
using AType = Ast::Type;
using IType = IRBase::Type;
using IFuncType = IRBase::FunctionType;
using ATypeUPtr = Ast::Type::UnownedPtrT;
using ITypeUPtr = IRBase::Type*;

inline namespace generator_impl {
    static TypeMapper::ATypeListT function_make_argument_typelist(Ast::Function *fn)
    {
        using AFuncT        = Ast::Function;
        using AFuncParamPtr = Ast::FuncParam::UnownedPtrT;
        using ATypeListT    = TypeMapper::ATypeListT;

        AFuncParamPtr func_params = fn->get_func_params();
        ATypeListT    param_type_list;
        for (auto &i: func_params->get_param_list()) {
            AType *real_type = i->base_type();
            param_type_list.push_back(real_type);
        }
        return param_type_list;
    }

    static Function *scope_get_afunc_node(Scope *scope)
    {
        while (scope != nullptr) {
            Node *node = scope->container()->owner_instance;
            if (node->node_type() == NodeType::FUNC_DEF)
                return static_cast<Function*>(node);
            scope = scope->parent();
        }
        return nullptr;
    }
} // inline namespace generator_impl

struct Generator::RuntimeData {
    using WhileStmtStackT = std::stack<Ast::WhileStmt::UnownedPtrT>;
    using ControlStackT   = std::stack<Ast::Statement::UnownedPtrT>;
public:
    WhileStmt *get_current_while() const noexcept {
        MTB_UNLIKELY_IF (while_stmt_stack.empty()) {
            fputs("While statement underflow: Unwind your calling stack to find what you'd done\n",
                  stderr);
            fputs("While栈为空: 请使用调试工具进行栈回溯. "
                  "可能是没注册while语句, 也可能是在函数根结点遇到了break语句.\n",
                  stderr);
            fflush(stderr);
            abort();
        }
        return while_stmt_stack.top();
    }
    Statement *get_current_ctrl() const noexcept {
        return ctrl_stmt_stack.top();
    }
public:
    Ast::Function  *current_funtion;
    WhileStmtStackT while_stmt_stack;
    ControlStackT   ctrl_stmt_stack;
}; // struct Generator::RuntimeData

/** @class Generator */

Generator::Generator(CodeContext &ctx)
    : ctx(ctx),
      _runtime_data(nullptr) {
}

owned<IR::Builder> Generator::generate()
{
    // visit root compile unit
    // initialize some basic function definitions
    // visit this compile unit
    RuntimeData data {nullptr,{},{}};
    _runtime_data = &data;
    visit(ctx.root().get());
    // clear module context
    _module       = nullptr;
    _runtime_data = nullptr;
    return _builder;
}

bool Generator::visit(Ast::CompUnit::UnownedPtrT comp_unit)
{
    Scope *scope     = comp_unit->scope_self();
    auto  &constants = scope->constants();
    for (auto &i: constants) {
        string_view name     = i.first;
        Variable   *constdef = i.second;
        _generate_constant_globl(constdef);
    }

    auto &variables = scope->variables();
    for (auto &i: variables) {
        string_view name   = i.first;
        Variable   *vardef = i.second;
        _generate_variable_globl(vardef);
    }

    auto &functions = scope->functions();
    for (auto &i: functions) {
        string_view name    = i.first;
        Function   *funcdef = i.second;
        visit(funcdef);
    }
    return true;
}

bool Generator::visit(Ast::Function::UnownedPtrT afunc)
{
    using namespace FunctionLocal;

    TypeMapper::ATypeListT param_type_list = function_make_argument_typelist(afunc);
    ATypeUPtr   ureturn_type = afunc->get_return_type();
    AType::PtrT oreturn_type = static_pointer_cast<AType>(ureturn_type->shared_from_this());
    IFuncType  *ifunc_ty     = _mapper.make_function_type(oreturn_type,
                                                          param_type_list);
    if (afunc->is_extern()) {
        _builder->declareFunction(afunc->name(), ifunc_ty);
    } else {
        IR::Function *ifunc = _builder->defineFunction(afunc->name(), ifunc_ty);
        _builder->selectFunction(ifunc);
        _current_scope = afunc->get_scope();

        SymbolInfoMapper &info = _symbol_info_manager.register_get_function(afunc, ifunc);
        _symbol_info_manager.current_symbol_info_map = &info;
        visit(afunc->get_func_body().get());
    }
    return true;
}

bool Generator::visit(Ast::Block::UnownedPtrT basic_block)
{
    scope_ctx_save(basic_block->get_scope());
    _visit_current_scope();
    /* all children nodes */
    for (Statement::PtrT &i: *basic_block)
        i->accept(this);
    scope_restore_return(true);
}

/* private class Generator */

void Generator::_generate_constant_globl(Variable *constant)
{
    Ast::Type::PtrT constant_type = constant->get_real_type();
    IR::Type *ir_type = _mapper.make_ir_type(constant_type);
    owned<IR::Constant> init_expr;
    if (constant->get_init_expr() == nullptr) {
        if (ir_type->is_array_type())
            init_expr = IR::ArrayExpr::CreateEmpty(ir_type);
        else
            init_expr = IR::Constant::CreateZeroOrUndefined(ir_type);
    } else {
    }
    _builder->defineGlobalConstant(constant->name(), ir_type, init_expr);
}
void Generator::_generate_variable_globl(Variable *variable)
{
    Ast::Type::PtrT vartype = variable->get_real_type();
    IR::Type *ir_type = _mapper.make_ir_type(vartype);
    _builder->defineGlobalVariable(variable->name(), ir_type, );
}

void Generator::_visit_current_scope()
{
    Scope *curr = _current_scope;
    for (auto &[name, cons]: curr->constants())
        _generate_constant_nested(cons);

    for (auto &[name, vari]: curr->variables())
        _generate_variable_nested(vari);
}

/* end class Generator */

} // namespace MYGL::IRGen
/** @file ast-printer.cpp MYGL AST Printer */
#include "../../include/myglc-lang/code-visitors/ast-printer.hxx"
#include <string_view>

using namespace MYGL::Ast;

#define out _outfile
#define outf(fmt, ...) \
    out << std::format(fmt, ##__VA_ARGS__)
#define try_accept(node) {\
    wrap_indent();\
    if ((node)->accept(this) == false) \
        return false;\
}

std::string_view operator_get_string(Expression::Operator op)
{
    static cstring operators[256] = {
        "+", "-", "*", "/", "%",
        "&&", "||", "!",
        "<", "<=", ">", ">=", "==", "!=",
        "=", "+=", "-=", "*=", "/=", "%=",
        nullptr
    };
    if (op == MYGL::Ast::Expression::Operator::NONE)
        return "(none)";
    if (op < MYGL::Ast::Expression::Operator::PLUS ||
        op > MYGL::Ast::Expression::Operator::MOD_ASSIGN)
        return "(ERROR)";
    return std::string_view(operators[int(op) - 256]);
}
std::string_view boolean_get_string(bool self)
{
    cstring BooleanString[] = {"false", "true"};
    return BooleanString[self];
}

bool Printer::visit(UnaryExpr::UnownedPtrT node)
{
    outf("[UnaryExpr (operator = `{}`)]", operator_get_string(node->xoperator()));
    indent_inc(); {
        wrap_indent();
        auto expr = node->get_expression();
        if (expr == nullptr || expr->accept(this) == false)
            return false;
    } indent_dec();
    return true;
}

bool Printer::visit(BinaryExpr::UnownedPtrT node)
{
    outf("[BinaryExpr (operator = `{}`)]",
        operator_get_string(node->get_operator()));
    indent_inc(); {
        auto lhs = node->get_lhs();
        auto rhs = node->get_rhs();
        if (lhs == nullptr || rhs == nullptr)
            return false;

        wrap_indent();
        if (lhs->accept(this) == false)
            return false;
        wrap_indent();
        if (rhs->accept(this) == false)
            return false;
    } indent_dec();
    return true;
}

bool Printer::visit(CallParam::UnownedPtrT node)
{
    outf("[CallParam (length = {})]",
        node->get_expression().size());
    indent_inc();
    for (auto &i: node->get_expression()) {
        wrap_indent();
        if (i->accept(this) == false)
            return false;
    }
    indent_dec();
    return true;
}

bool Printer::visit(CallExpr::UnownedPtrT node)
{
    outf("[CallExpr (function = `{}`)]",
         node->get_name()->name());
    indent_inc(); {
        wrap_indent();
        if (node->get_param()->accept(this) == false)
            return false;
    } indent_dec();
    return true;
}

bool Printer::visit(InitList::UnownedPtrT node)
{
    outf("[InitList (size = {})]", node->get_expr_list().size());
    indent_inc();
    for (auto &i: node->get_expr_list()) {
        wrap_indent();
        if (i->accept(this) == false)
            return false;
    }
    indent_dec();
    return true;
}

bool Printer::visit(IndexExpr::UnownedPtrT node)
{
    outf("[IndexExpr (dimension = {})]",
        node->get_index_list().size());
    indent_inc();
    for (auto &i: node->get_index_list())
        try_accept(i);
    indent_dec();
    return true;
}

bool Printer::visit(Identifier::UnownedPtrT node)
{
    std::string base_type;
    void     *base_typeid;
    auto definition = node->get_definition();
    if (definition == nullptr) {
        base_type = "(undefined)";
        base_typeid = nullptr;
    } else {
        base_type = definition->base_type()->type_name();
        base_typeid = definition->base_type();
    }
    outf("[Identifier (name = `{}`, base_type = `{}`, base_typeid = `{}`)]",
         node->name(), base_type, base_typeid);
    return true;
}
bool Printer::visit(IntValue::UnownedPtrT node)
{
    outf("[IntValue (value = {})]", node->value());
    return true;
}
bool Printer::visit(FloatValue::UnownedPtrT node)
{
    outf("[FloatValue (value = {})]", node->value());
    return true;
}
bool Printer::visit(StringValue::UnownedPtrT node)
{
    outf("[StringValue (value = \"{}\")]", node->value_string());
    return true;
}
bool Printer::visit(AssignExpr::UnownedPtrT node)
{
    outf("[AssignExpr (operator = `{}`)]",
        operator_get_string(node->get_operator()));
    indent_inc();
    wrap_indent(); outf("<destination>");
    try_accept(node->get_destination());
    wrap_indent(); outf("<source>");
    try_accept(node->get_source());
    indent_dec();
    return true;
}
bool Printer::visit(IfStmt::UnownedPtrT node)
{
    outf("[IfStmt (has_else = {})]",
        node->get_false_stmt() == nullptr ? "false": "true");
    indent_inc();
    wrap_indent(); outf("<condition>");
    try_accept(node->get_condition());
    wrap_indent(); outf("<true statement>");
    try_accept(node->get_true_stmt());
    if (node->get_false_stmt() != nullptr) {
        wrap_indent(); outf("<false statment>");
        try_accept(node->get_false_stmt());
    }
    indent_dec();
    return true;
}
bool Printer::visit(WhileStmt::UnownedPtrT node)
{
    outf("[WhileStmt]");
    indent_inc();
    wrap_indent(); outf("<condition>");
    try_accept(node->get_condition());
    wrap_indent(); outf("<true statement>");
    try_accept(node->get_true_stmt());
    indent_dec();
    return true;
}
bool Printer::visit(EmptyStmt::UnownedPtrT node)
{
    outf("[EmptyStmt]");
    return true;
}
bool Printer::visit(ReturnStmt::UnownedPtrT node)
{
    outf("[ReturnStmt]");
    indent_inc();
    if (node->get_expression() != nullptr)
        try_accept(node->get_expression());
    indent_dec();
    return true;
}
bool Printer::visit(BreakStmt::UnownedPtrT node)
{
    outf("[BreakStmt]");
    return true;
}
bool Printer::visit(ContinueStmt::UnownedPtrT node)
{
    outf("[ContinueStmt]");
    return true;
}
bool Printer::visit(Block::UnownedPtrT node)
{
    outf("[Block (size = {})]", node->get_statements().size());
    indent_inc();
    for (auto &i: node->get_statements()) {
        // wrap_indent();
        // fprintf(stderr, "block:scope %p\n", i->get_scope());
        try_accept(i);
    }
    indent_dec();
    return true;
}
bool Printer::visit(ExprStmt::UnownedPtrT node)
{
    outf("[ExprStmt]");
    indent_inc();
    try_accept(node->get_expression());
    indent_dec();
    return true;
}
bool Printer::visit(ConstDecl::UnownedPtrT node)
{
    outf("[ConstDecl (base_type = `{}`)]",
        node->get_base_type()->type_name());
    indent_inc();
    for (auto &i: node->get_variables())
        try_accept(i);
    indent_dec();
    return true;
}
bool Printer::visit(VarDecl::UnownedPtrT node)
{
    outf("[VarDecl (base_type = {})]",
        node->get_base_type()->type_name());
    indent_inc();
    for (auto &i: node->get_variables())
        try_accept(i);
    indent_dec();
    return true;
}
bool Printer::visit(Function::UnownedPtrT node)
{
    outf("[Function (return_type = `{}`, name = `{}`)]",
        node->base_type()->to_string(),
        node->name());
    indent_inc(); {
        try_accept(node->get_func_params());
        try_accept(node->get_func_body());
    } indent_dec();
    return true;
}
bool Printer::visit(Variable::UnownedPtrT node)
{
    std::string_view name = node->is_constant()? "Constant": "Variable";
    outf("[{} (name = `{}`, basetype = `{}`)]",
         name, node->name(), node->base_type()->type_name());
    indent_inc();
    if (node->get_array_info() != nullptr)
        try_accept(node->get_array_info());
    if (node->get_init_expr() != nullptr)
        try_accept(node->get_init_expr());
    indent_dec();
    return true;
}
bool Printer::visit(Type::UnownedPtrT node)
{
    std::string_view base_typename = "(null)";
    if (node->base_type() != nullptr)
        base_typename = node->base_type()->type_name();
    outf("[Type (name = `{}`, base = `{}`)]",
        node->type_name(), base_typename);
    return true;
}

bool Printer::visit(CompUnit::UnownedPtrT node)
{
    outf("[CompUnit (decls = {}, funcs = {})]",
        node->decls().size(), node->funcdefs().size());
    indent_inc();
    for (auto &i: node->decls())
        try_accept(i);
    for (auto &i: node->funcdefs())
        try_accept(i);
    indent_dec();
    return true;
}
bool Printer::visit(FuncParam::UnownedPtrT node)
{
    outf("[FuncParam (size = {})]",
        node->get_param_list().size());
    indent_inc();
    for (auto &i : node->get_param_list())
        try_accept(i);
    indent_dec();
    return true;
}
bool Printer::visit(ArrayInfo::UnownedPtrT node)
{
    outf("[ArrayInfo (dimension = {})]", node->array_size());
    indent_inc();
    for (auto &i: node->get_array_info())
        try_accept(i);
    indent_dec();
    return true;
}

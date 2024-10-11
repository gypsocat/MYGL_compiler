#include "myglc-lang/code-visitors/expr-checker.hxx"
#include "myglc-lang/util/expr-indexer.hxx"
#include <cstdint>
#include <iostream>
#include <vector>

namespace MYGL::GenUtil {

using namespace Ast;

static SourceRange anomymous_range = {
    "<anomymous>",
    {nullptr, 0, 0},
    {nullptr, 0, 0}
};

static double value_ptr_get_float(Value *val)
{
    if (val->node_type() == NodeType::FLOAT_VALUE)
        return static_cast<FloatValue*>(val)->get_value();
    else if (val->node_type() == NodeType::INT_VALUE)
        return static_cast<IntValue*>(val)->get_value();
    else
        return 0.0;
}
static int64_t value_ptr_get_int(Value *val)
{
    if (val->node_type() == NodeType::FLOAT_VALUE)
        return static_cast<FloatValue*>(val)->get_value();
    else if (val->node_type() == NodeType::INT_VALUE)
        return static_cast<IntValue*>(val)->get_value();
    else
        return 0;
}

static void calc_do_unary_neg(Value *val)
{
    if (IntValue *ival = dynamic_cast<IntValue*>(val);ival != nullptr) {
        ival->value() = -ival->value();
    } else if (auto fval = dynamic_cast<FloatValue*>(val); fval != nullptr) {
        fval->value() = -fval->value();
    }
}
static void calc_do_unary_not(Value::PtrT &val)
{
    if (IntValue *ival = dynamic_cast<IntValue*>(val.get());ival != nullptr) {
        ival->value() = !ival->value();
    } else if (auto fval = dynamic_cast<FloatValue*>(val.get()); fval != nullptr) {
        int fnot = !fval->value();
        val = std::make_shared<IntValue>(fnot);
    }
}
static Value::PtrT calc_do_bin_add(Value::PtrT &lhs, Value::PtrT &rhs)
{
    if (lhs->node_type() == NodeType::STRING_VALUE ||
        rhs->node_type() == NodeType::STRING_VALUE) {
        return nullptr;
    } else if (lhs->node_type() == NodeType::FLOAT_VALUE ||
               rhs->node_type() == NodeType::FLOAT_VALUE) {
        return std::make_shared<FloatValue>(
            value_ptr_get_float(lhs.get()) + value_ptr_get_float(rhs.get()));
    } else {
        return std::make_shared<IntValue>(
            dynamic_cast<IntValue*>(lhs.get())->value() +
            dynamic_cast<IntValue*>(rhs.get())->value()
        );
    }
}
static Value::PtrT calc_do_bin_sub(Value::PtrT &lhs, Value::PtrT &rhs)
{
    if (lhs->node_type() == NodeType::STRING_VALUE ||
        rhs->node_type() == NodeType::STRING_VALUE) {
        return nullptr;
    } else if (lhs->node_type() == NodeType::FLOAT_VALUE ||
               rhs->node_type() == NodeType::FLOAT_VALUE) {
        return std::make_shared<FloatValue>(
            value_ptr_get_float(lhs.get()) - value_ptr_get_float(rhs.get()));
    } else {
        return std::make_shared<IntValue>(
            dynamic_cast<IntValue*>(lhs.get())->value() -
            dynamic_cast<IntValue*>(rhs.get())->value()
        );
    }
}
static Value::PtrT calc_do_bin_mul(Value::PtrT &lhs, Value::PtrT &rhs)
{
    if (lhs->node_type() == NodeType::STRING_VALUE ||
        rhs->node_type() == NodeType::STRING_VALUE) {
        return nullptr;
    } else if (lhs->node_type() == NodeType::FLOAT_VALUE ||
               rhs->node_type() == NodeType::FLOAT_VALUE) {
        return std::make_shared<FloatValue>(
            value_ptr_get_float(lhs.get()) * value_ptr_get_float(rhs.get()));
    } else {
        return std::make_shared<IntValue>(
            dynamic_cast<IntValue*>(lhs.get())->value() *
            dynamic_cast<IntValue*>(rhs.get())->value()
        );
    }
}
static Value::PtrT calc_do_bin_div(Value::PtrT &lhs, Value::PtrT &rhs)
{
    if (lhs->node_type() == NodeType::STRING_VALUE ||
        rhs->node_type() == NodeType::STRING_VALUE) {
        return nullptr;
    } else if (lhs->node_type() == NodeType::FLOAT_VALUE ||
               rhs->node_type() == NodeType::FLOAT_VALUE) {
        return std::make_shared<FloatValue>(
            value_ptr_get_float(lhs.get()) / value_ptr_get_float(rhs.get()));
    } else {
        int64_t rhsv = dynamic_cast<IntValue*>(rhs.get())->value();
        if (rhsv == 0) {
            std::clog << "Divided by zero!" << std::endl;
            return nullptr;
        }
        return std::make_shared<IntValue>(
            dynamic_cast<IntValue*>(lhs.get())->value() / rhsv
        );
    }
}
static Value::PtrT calc_do_bin_mod(Value::PtrT &lhs, Value::PtrT &rhs)
{
    if (lhs->node_type() == NodeType::STRING_VALUE ||
        rhs->node_type() == NodeType::STRING_VALUE) {
        return nullptr;
    } else if (lhs->node_type() == NodeType::FLOAT_VALUE ||
               rhs->node_type() == NodeType::FLOAT_VALUE) {
        std::clog << "Float value has nothing like \"mod\"" << std::endl;
        return nullptr;
    } else {
        int64_t rhsv = dynamic_cast<IntValue*>(rhs.get())->value();
        if (rhsv == 0) {
            std::clog << "Divided by zero!" << std::endl;
            return nullptr;
        }
        return std::make_shared<IntValue>(
            dynamic_cast<IntValue*>(lhs.get())->value() % rhsv
        );
    }
}
static Value::PtrT calc_do_bin_and(Value::PtrT &lhs, Value::PtrT &rhs)
{
    if (lhs->node_type() == NodeType::STRING_VALUE ||
        rhs->node_type() == NodeType::STRING_VALUE) {
        return nullptr;
    }
    bool result = true;
    if (lhs->node_type() == NodeType::INT_VALUE)
        result = (dynamic_cast<IntValue*>(lhs.get())->get_value() != 0);
    else
        result = (dynamic_cast<FloatValue*>(lhs.get())->get_value() != 0);

    if (rhs->node_type() == NodeType::INT_VALUE)
        result &= (dynamic_cast<IntValue*>(rhs.get())->get_value() != 0);
    else
        result &= (dynamic_cast<FloatValue*>(rhs.get())->get_value() != 0);

    return std::make_shared<IntValue>(result);
}
static Value::PtrT calc_do_bin_or(Value::PtrT &lhs, Value::PtrT &rhs)
{
    if (lhs->node_type() == NodeType::STRING_VALUE ||
        rhs->node_type() == NodeType::STRING_VALUE) {
        return nullptr;
    }
    bool result = false;
    if (lhs->node_type() == NodeType::INT_VALUE)
        result = (dynamic_cast<IntValue*>(lhs.get())->get_value() != 0);
    else
        result = (dynamic_cast<FloatValue*>(lhs.get())->get_value() != 0);

    if (rhs->node_type() == NodeType::INT_VALUE)
        result |= (dynamic_cast<IntValue*>(rhs.get())->get_value() != 0);
    else
        result |= (dynamic_cast<FloatValue*>(rhs.get())->get_value() != 0);

    return std::make_shared<IntValue>(result);
}

static Expression::PtrT
calc_do_index_initlist(InitList::UnownedPtrT init_list,
                       std::vector<int> const &dimension_list,
                       std::vector<int> const &index_list)
{
    auto ret = Utility::AstIndexer {
        init_list, dimension_list, index_list
    }()->shared_from_this();
    return std::dynamic_pointer_cast<Expression>(ret);
}

#define CHECK_FAIL() {\
    return false;\
}

#define CHECK_ERROR() {\
    std::clog << "No implement of this visit() function" << std::endl; \
    return false;\
}

ExprChecker::ExprChecker() = default;

Value::PtrT ExprChecker::do_try_calculate(Expression::UnownedPtrT expr)
{
    if (expr->accept(this) == false)
        return nullptr;
    else
        return _cur_value;
}
bool ExprChecker::list_is_constant(InitList::UnownedPtrT init_list)
{
    for (auto &i: init_list->get_expr_list()) {
        if (auto list = dynamic_cast<InitList*>(i.get());
            (list != nullptr) && (!list_is_constant(list))) {
            return false;
        } else if (!value_is_constant(i.get())) {
            return false;
        }
    }
    return true;
}

bool ExprChecker::visit(UnaryExpr::UnownedPtrT unary_expr)
{
    if (unary_expr->get_expression()->accept(this) == false)
        return false;
    switch (unary_expr->xoperator()) {
    case Ast::Expression::Operator::PLUS:
        break;
    case Ast::Expression::Operator::SUB:
        calc_do_unary_neg(_cur_value.get());
        break;
    case Ast::Expression::Operator::NOT:
        calc_do_unary_not(_cur_value);
        break;
    default:
        return false;
    }
    return true;
}

/* 对于AND/OR运算而言，没有做短路计算的原因是已经判断两边是否常量。
 * 倘若出现变量/函数调用，会被判false的。 */
bool ExprChecker::visit(BinaryExpr::UnownedPtrT node)
{
    auto lhs = do_try_calculate(node->get_lhs().get());
    if (lhs == nullptr)
        return false;
    auto rhs = do_try_calculate(node->get_rhs().get());
    if (rhs == nullptr)
        return false;
    switch (node->get_operator()) {
    case Ast::Expression::Operator::PLUS:
        _cur_value = calc_do_bin_add(lhs, rhs);
        break;
    case Ast::Expression::Operator::SUB:
        _cur_value = calc_do_bin_sub(lhs, rhs);
        break;
    case Ast::Expression::Operator::STAR:
        _cur_value = calc_do_bin_mul(lhs, rhs);
        break;
    case Ast::Expression::Operator::SLASH:
        _cur_value = calc_do_bin_div(lhs, rhs);
        if (_cur_value == nullptr)
            return false;
        break;
    case Ast::Expression::Operator::PERCENT:
        _cur_value = calc_do_bin_mod(lhs, rhs);
        if (_cur_value == nullptr)
            return false;
        break;
    case Ast::Expression::Operator::AND:
        _cur_value = calc_do_bin_and(lhs, rhs);
        break;
    case Ast::Expression::Operator::OR:
        _cur_value = calc_do_bin_or(lhs, rhs);
        break;
    default:
        return false;
    }
    return true;
}

// 检查InitList是否为常量表达式
bool ExprChecker::visit(InitList::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(CallParam::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(CallExpr::UnownedPtrT node) CHECK_FAIL()

/* 试着写了一下数组取索引的函数，不知道对不对。*/
bool ExprChecker::visit(IndexExpr::UnownedPtrT node)
{
    Identifier::UnownedPtrT name = node->get_name().get();
    auto def = name->get_definition();
    /* 查找是否为函数/预定义函数(def == nullptr时就是预定义函数) */
    if (def == nullptr || def->node_type() == NodeType::FUNC_DEF)
        return false;
    auto var_def = dynamic_cast<Variable*>(def);
    /* 保险用的，指不定哪天就出现typedef了 */
    if (var_def == nullptr)
        return false;
    /* 验证是否为常量数组类型 */
    if (!var_def->is_constant() ||
        !var_def->is_array_type())
        return false;

    int def_dimension = var_def->get_array_info()->get_dimension();
    int index_dimension = node->get_index_list().size();

    // 懒得写一个异常类了。这种语法错误到处都要用，不如向负责IR的白嫖一个
    if (def_dimension != index_dimension) {
        std::clog << std::format(
                "Syntax error at {}: array requires {} dimensions, but got {}\n",
                node->range().to_string(),
                def_dimension, index_dimension)
            << std::format("Content: \n```\n{}\n```", node->range().get_content())
            << std::endl;
        return false;
    }

    // 如果没有init_expr, 就是空初始化，一律填0.
    auto init_expr = var_def->get_init_expr().get();
    if (init_expr == nullptr) {
        _cur_value = std::make_shared<IntValue>(0);
        return true;
    }
    // init_expr不是InitList类型的，相当于用标量初始化向量，报错
    auto init_list = dynamic_cast<InitList*>(init_expr);
    if (init_expr == nullptr) {
        std::clog << std::format(
                "Syntax error at {}: do not use value to initialize array",
                var_def->range().to_string());
        return false;
    }

    // 索引列表，对应`putint(a[1][2][3])`里的{1, 2, 3}
    std::vector<int> index_list(index_dimension);
    // 维度列表，对应`int a[4][5][6]`里的{4, 5, 6}
    std::vector<int> dimension_list(def_dimension);
    for (int index = 0;
         auto &i: var_def->get_array_info()->get_array_info()) {
        if (i->accept(this) == false ||
            _cur_value->node_type() != NodeType::INT_VALUE)
            return false;
        dimension_list[index] = value_ptr_get_int(_cur_value.get());
        index++;
    }
    for (int index = 0;
         auto &i: node->get_index_list()) {
        if (i->accept(this) == false ||
            _cur_value->node_type() != NodeType::INT_VALUE)
            return false;
        index_list[index] = value_ptr_get_int(_cur_value.get());
        index++;
    }
    // 上面的代码都是检查与初始化用的。
    // 初始化列表的索引非常复杂，请参见类CalcDoIndexList::Calculator的实现。
    Utility::AstIndexer index {
        init_list, dimension_list, index_list
    };
    auto index_pointer = index();
    if (index_pointer == nullptr)
        return false;
    bool ret = index_pointer->accept(this);
    return ret;
}
bool ExprChecker::visit(Identifier::UnownedPtrT node)
{
    auto def = node->get_definition();
    /* 查找是否为函数/预定义函数(def == nullptr时就是预定义函数) */
    if (def == nullptr || def->node_type() == NodeType::FUNC_DEF)
        return false;
    auto var_def = dynamic_cast<Variable*>(def);
    /* 保险用的，指不定哪天就出现typedef了 */
    if (var_def == nullptr)
        return false;
    if (!var_def->is_constant())
        return false;
    return var_def->get_init_expr()->accept(this);
}
bool ExprChecker::visit(IntValue::UnownedPtrT node)
{
    _cur_value = std::make_shared<IntValue>(node->get_value());
    return true;
}
bool ExprChecker::visit(FloatValue::UnownedPtrT node)
{
    _cur_value = std::make_shared<FloatValue>(node->get_value());
    return true;
}
bool ExprChecker::visit(StringValue::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(AssignExpr::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(IfStmt::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(WhileStmt::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(EmptyStmt::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(ReturnStmt::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(BreakStmt::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(ContinueStmt::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(Block::UnownedPtrT node) CHECK_ERROR()
bool ExprChecker::visit(ExprStmt::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(ConstDecl::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(VarDecl::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(Function::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(Variable::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(Type::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(CompUnit::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(FuncParam::UnownedPtrT node) CHECK_FAIL()
bool ExprChecker::visit(ArrayInfo::UnownedPtrT node) CHECK_FAIL()

}

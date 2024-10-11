#include "myglc-lang/util/expr-indexer.hxx"
#include "myglc-lang/code-visitors/expr-checker.hxx"
#include <format>
#include <vector>

using namespace MYGL::Utility;
using namespace MYGL::Ast;
using namespace MYGL;

AstIndexerContext::AstIndexerContext(InitList::UnownedPtrT init_list,
                               std::vector<int> const &dimension_list,
                               std::vector<int> const &index_list)
    : init_list(init_list),
      dimension_list(dimension_list),
      index_list(index_list),
      step_list(dimension_list.size())  {
    int _total = 1;
    for (int i = dimension_list.size() - 1; i >= 0; i--) {
        step_list[i] = _total;
        int _i = dimension_list[i];
        _total *= _i;
    }
    total = _total;
    for (int i = 0; i < index_list.size(); i++) {
        max_step += index_list[i] * step_list[i];
    }
}

AstIndexerContext::Dependency
AstIndexerContext::createDependency(Variable::UnownedPtrT array)
{
    if (array == nullptr || !array->is_array_type())
        return {};
    auto arr_info = array->get_array_info().get();
    auto init_expr = array->get_init_expr().get();
    auto init_list = dynamic_cast<InitList*>(init_expr);
    size_t dimension = arr_info->get_dimension();
    GenUtil::ExprChecker checker;
    Dependency ret {
        init_list,
        std::vector<int>(dimension),
        std::vector<int>(dimension),
        array
    };
    for (int step = 0;
         auto &i: arr_info->get_array_info()) {
        auto result = checker.do_try_calculate(i.get());
        if ((result == nullptr) ||
            (result->node_type() != Ast::NodeType::INT_VALUE))
            return {};
        /** 已经是IntValue了，不使用RTTI */
        auto iresult = reinterpret_cast<IntValue*>(result.get());
        if (iresult->value() == 0 && step != 0) {
            throw BrokenListException(
                init_list,
                std::format(
                    "parent array definition `{}` has a dimension of length 0\nContent: {}",
                    array->name(), array->range().get_content()
                ));
        }
        ret.dimension_list[step] = iresult->value();
        ret.index_list[step] = 0;
        step++;
    }
    return ret;
}

AstIndexer::AstIndexer(InitList::UnownedPtrT init_list,
                std::vector<int> const &dimension_list,
                std::vector<int> const &index_list)
    : AstIndexerContext(init_list, dimension_list, index_list) {
}

Expression::UnownedPtrT AstIndexer::do_calculate(InitList *node, int depth)
{
    int ilist_step = step_list[depth];

    for (auto &i: node->get_expr_list()) {
        InitList *ilist = dynamic_cast<InitList*>(i.get());
        if (ilist == nullptr) {
            if (cur_step == max_step)
                return i.get();
            cur_step++;
            continue;
        }
        if (cur_step % ilist_step != 0) {
            throw BrokenListException(
                init_list,
                std::format(
                    "requires a multiple of {} steps, but got {}\n",
                    ilist_step, cur_step
            ));
        }
        if (cur_step + ilist_step < max_step) {
            cur_step += ilist_step;
            continue;
        }
        return do_calculate(ilist, depth + 1);
    }
    result = std::make_shared<IntValue>(0);
    return result.get();
}

void AstArrayFiller::do_init() {
    result = std::make_shared<IntValue>(0);

    if (dimension_list[0] != 0)
        return;

    int ilist_step = step_list[0];
    total = 0;
    for (auto &i: init_list->get_expr_list()) {
        InitList *ilist = dynamic_cast<InitList*>(i.get());
        if (ilist == nullptr) {
            total++;
            continue;
        }
        if (cur_step % ilist_step != 0) {
            throw BrokenListException(
                init_list,
                std::format(
                    "requires a multiple of {} steps, but got {}\n",
                    ilist_step, cur_step
            ));
        }
        total += ilist_step;
    }
    auto &first_dimension = _array->get_array_info()->array_info()[0];
    auto first_dimension_value = dynamic_cast<IntValue*>(first_dimension.get());
    if (first_dimension_value != nullptr) {
        first_dimension_value->value() = total / ilist_step;
    }
}

void AstArrayFiller::do_fill(InitList *current, int depth)
{
    if (depth >= step_list.size()) {
        throw BrokenListException(init_list,
            std::format(""));
    }

    int ilist_step = step_list[depth];
    int ldstep = 0; // level-dependent step
    int ldstep_max = (depth == 0) ? (total) : (step_list[depth - 1]);
    auto &array_info = _array->get_array_info()->array_info();
    for (auto &i: current->get_expr_list()) {
        if (ldstep >= ilist_step) {
            throw BrokenListException(
                init_list,
                std::format(
                    "this dimension (at {}) overflow: \n```\n{}\n```\n",
                    current->range().to_string(),
                    current->range().get_content())
            );
        }
        InitList *ilist = dynamic_cast<InitList*>(i.get());
        if (ilist == nullptr) {
            expr_list.push_back(i);
            ldstep++;
            continue;
        }
        if (ldstep % ilist_step != 0) {
            throw BrokenListException(
                init_list,
                std::format(
                    "requires a mutiple of {} steps, but got {}\n",
                    ilist_step, cur_step));
        }
        do_fill(ilist, depth + 1);
        ldstep += ilist_step;
    }

    while (ldstep < ilist_step - 1) {
        array_info[cur_step + ldstep] = result;
        ldstep++;
    }
}

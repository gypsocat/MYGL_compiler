#pragma once
#include "../ir-utils.hxx"
#include "myglc-lang/code-visitors/expr-checker.hxx"
#include <memory>
#include <vector>

namespace MYGL::Utility {
    using namespace Ast;

    struct AstIndexerContext {
    public:
        class BrokenListException: public std::exception {
        public:
            BrokenListException(InitList::UnownedPtrT list, std::string_view msg)
                : msg(std::format("BrokenList `{}`: {}",
                      list->range().get_content(),
                      msg)),
                  list(list) {
            }

            cstring what() const noexcept override {
                return msg.c_str();
            }
            std::string           msg;
            InitList::UnownedPtrT list;
        }; // class BrokenListException

        struct Dependency {
            InitList::UnownedPtrT init_list;
            std::vector<int> dimension_list;
            std::vector<int>     index_list;
            Variable::UnownedPtrT     array;
        }; // struct Dependency
    public:
        AstIndexerContext(InitList::UnownedPtrT init_list,
                       std::vector<int> const &dimension_list,
                       std::vector<int> const &index_list);
        AstIndexerContext(Dependency const &dep)
            : AstIndexerContext(dep.init_list,
                             dep.dimension_list,
                             dep.index_list){}
        
        /** @brief 从数组定义创建一个初始化列表展开上下文。这个是给AstArrayFiller使用的。 */
        static Dependency createDependency(Variable::UnownedPtrT array);

        InitList::UnownedPtrT    init_list;
        std::vector<int> const &dimension_list;
        std::vector<int> const &index_list;
        std::vector<int> step_list;         /// 每个维度对应的步数列表。
        int deepest = 0;
        int total, cur_step = 0, max_step = 0;
        IntValue::PtrT result;
    }; // struct IndexerContext

    class AstIndexer: protected AstIndexerContext {
    public:
        class BrokenListException: public std::exception {
        public:
            BrokenListException(InitList::UnownedPtrT list, std::string_view msg)
                : msg(std::format("BrokenList `{}`: {}",
                      list->range().get_content(),
                      msg)),
                  list(list) {
            }

            cstring what() const noexcept override {
                return msg.c_str();
            }
            std::string           msg;
            InitList::UnownedPtrT list;
        }; // class BrokenListException
    public:
        AstIndexer(InitList::UnownedPtrT init_list,
                std::vector<int> const &dimension_list,
                std::vector<int> const &index_list);

        inline Expression::UnownedPtrT operator() () {
            Expression::UnownedPtrT ret;
            try {
                ret = do_calculate(init_list, 0);
            } catch (BrokenListException &e) {
                std::clog << e.what() << std::endl;
                throw e;
            }
            return ret;
        }
    private:
        Expression::UnownedPtrT do_calculate(InitList *node, int depth);
    }; // class Indexer

    class AstArrayFiller: public AstIndexerContext {
    public:
        using PtrT = std::shared_ptr<AstArrayFiller>;
        using ExprListT = std::vector<Expression::PtrT>;
    public:
        AstArrayFiller(Dependency const &dep)
            : AstIndexerContext(dep),
              _array(dep.array) {
            do_init();
        }

        void operator()() {
            do_fill(init_list, 0);
        }
        ExprListT expr_list;
    private:
        Variable::UnownedPtrT _array;
        GenUtil::ExprChecker  _checker;

        void do_init();
        void do_fill(InitList::UnownedPtrT current, int depth);
    }; // class ArrayFiller
} // namespace MYGL::Utility

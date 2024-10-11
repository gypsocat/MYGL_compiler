#include <cmath>
#ifndef __MYGL_LANG_AST_H__
#define __MYGL_LANG_AST_H__ 1L

#include <cstdint>
#include <string>
#include <cstring>
#include <deque>
#include <functional>
#include <string_view>
#include <memory>
#include <map>
#include "base.hxx"
#include "ast-node-decl.hxx"
#include "ast-scope.hxx"
#include "util.hxx"

namespace MYGL::Ast {
    /** @enum NodeType
     * @brief 结点类型枚举，用于反射.
     * @fn GetString() 结点类型对应的字符串 */
    enum class NodeType {
        BASE = 1,
        /** Statements */
        STMT, IF_STMT, WHILE_STMT, EXPR_STMT, EMPTY_STMT,
        BREAK_STMT, CONTINUE_STMT, RETURN_STMT, BLOCK_STMT,
        DECL, CONST_DECL, VAR_DECL,
        /** Definition */
        DEF, TYPE_DEF, VAR_DEF, FUNC_DEF,
        /** Expression */
        EXPR, UNARY_EXPR, BINARY_EXPR, CALL_EXPR, INIT_LIST,
        INDEX_EXPR, IDENT,
        VALUE, INT_VALUE, FLOAT_VALUE, STRING_VALUE,
        ASSIGN_EXPR,
        /** Other */
        COMP_UNIT, FUNC_PARAM, CALL_PARAM, ARRAY_DECL,
    }; // enum class Type
    cstring NodeTypeGetString(NodeType self);
    /** @class Node abstract
     * @brief 语法树结点，所有其他结点类的父类
     *
     * @var scope{get; set;}
     * @var range{access;}
     * @var node_type{get;}
     * @var parent{get; set;}
     *
     * @fn Node(SourceRange const& range,
     *          NodeType node_type = NodeType::BASE,
     *          Node *parent_node = nullptr,
     *          Scope *scope = nullptr)
     * @fn accept(CodeVisitor*) abstract
     * @fn accept_children(CodeVisitor*) abstract */
    imcomplete class Node: public std::enable_shared_from_this<Node> {
    public:
        /** @brief 指针定义: 每个Node都要定义一个带所有权和不带所有权的指针类型 */
        using PtrT = std::shared_ptr<Node>;
        using UnownedPtrT = Node*;
    public:
        Scope *get_scope() const { return _scope; }
        void set_scope(Scope *owner_scope) { _scope = owner_scope; }
        SourceRange &range() { return _range; }
        NodeType node_type() const { return _node_type; }
        Node *parent() const { return _parent_node; }
        void set_parent(Node *parent) { _parent_node = parent; }
        /* 虚函数与抽象函数 */
        /** @fn accept(CodeVisitor*)
         * @brief 让CodeVisitor扫描自己 */
        abstract bool accept(CodeVisitor *visitor) = 0;
        /** @fn accept_children(CodeVisitor*)
         * @brief 让CodeVisitor扫描自己的子结点 */
        abstract bool accept_children(CodeVisitor *visitor) = 0;
        /** @fn register_this(Scope *owner_scope, Node *parent)
         * @brief 让父作用域注册自己 */
        abstract bool register_this(Scope *owner_scope) = 0;
    protected:
        Scope      *_scope = nullptr;
        SourceRange _range;
        NodeType    _node_type;
        Node       *_parent_node;
    protected:
        Node(SourceRange const& range,
             NodeType node_type = NodeType::BASE,
             Node *parent_node = nullptr,
             Scope *scope = nullptr)
            : _range(range), _node_type(node_type),
              _scope(scope), _parent_node(parent_node){}
        virtual ~Node() = default;
    }; // class Node
    /** @class Statement abstract
     * @brief 语句结点，包括代码块与代码块里的所有结点。语句本身会串成一个链表，
     *        但是语句之间没有所有权。
     *
     * @var next{get; set; default = null} unowned 后继结点
     * @var prev protected unowned 前驱结点 */
    imcomplete class Statement: public Node {
    public:
        using PtrT = std::shared_ptr<Statement>;
        using UnownedPtrT = Statement*;
        inline Statement *get_next() { return _next; }
        inline void set_next(Statement *next) {
            if (_next != nullptr)
                _next->_prev = nullptr;
            _next = next;
            if (next != nullptr)
                next->_prev = this;
        }
        inline Statement *clean_next() {
            Statement *ret = _next;
            _next = nullptr;
            if (ret != nullptr)
                ret->_prev = nullptr;
            return ret;
        }
    protected:
        Statement(SourceRange const &range,
                  NodeType type = NodeType::STMT,
                  Node *parent = nullptr,
                  Scope *scope = nullptr)
            : Node(range, type, parent, scope) {}
        virtual ~Statement() override = default;
        Statement *_next = nullptr, *_prev = nullptr;
    }; // class Statement
    /** @class Expression abstract
     * @brief 表达式基类
     *
     * @fn is_lvalue() const -> bool 是否为左值表达式
     * @fn get_variable_list() abstract -> std::shared_ptr<VarListT>
     *     获取被使用的变量列表。
     * @fn register_this() abstract unused -> bool
     *     让父作用域注册自己，实际上是给自己的所有标识符找定义。 */
    imcomplete class Expression: public Node {
    public:
        using PtrT = std::shared_ptr<Expression>;
        using UnownedPtrT = Expression*;
        using VarListT = std::map<std::string_view, Variable*>;
        enum class Operator {
            NONE,
            PLUS = 0x100, SUB, STAR, SLASH, PERCENT,
            AND, OR, NOT,
            LT, LE, GT, GE, EQ, NE,
            ASSIGN, PLUS_ASSIGN, SUB_ASSIGN, MUL_ASSIGN, DIV_ASSIGN, MOD_ASSIGN
        }; // enum Operator
        static Operator OperatorFromStr(std::string_view op);
    public:
        bool is_lvalue() const { return _is_lvalue; }
        abstract std::shared_ptr<VarListT> get_variable_list() = 0;
    protected:
        Expression(SourceRange const &range,
                   Node *parent = nullptr,
                   Scope *owner_scope = nullptr,
                   NodeType type = NodeType::EXPR,
                   bool is_lvalue = false);
        virtual ~Expression() override = default;
        bool _is_lvalue;
    }; // class Expression
    /** @class Definition abstract
     * @brief 定义结点基类，从一个基础类型定义成员或类型的语句
     *
     * @var base_type{get; protected set;} 基础类型
     * @var name{get; protected set;} 名称，在不同子类里含义不同
     * @var base_type_string{access;}  基类型的字符串
     *
     * @fn register_this() abstract -> bool 让父作用域注册自己 */
    imcomplete class Definition: public Node {
    public:
        using PtrT = std::shared_ptr<Definition>;
        using UnownedPtrT = Definition*;
    public:
        Type *base_type() const { return _base_type; }
        void set_base_type(Type *type) {
            _base_type = type;
        }
        std::string_view name() const { return _name; }
        abstract bool register_this() = 0;
    protected:
        Ast::Type       *_base_type;
        std::string      _name;
        /** @brief constructor for derived definitions for derived Definition
         * @param range     Definition所在的源码范围
         * @param base_type 定义的基础类型
         * @param name      定义单元的名称
         * @param parent    父结点 */
        Definition(SourceRange const &range,
                   Type *base_type, std::string_view name,
                   Node *parent, Scope *parent_scope)
            : Node(range, NodeType::DEF, parent, parent_scope),
              _name(name) {
            _base_type = base_type;
        }
    }; // class Definition
/**
 * ========================================================== *
 * ====                Statement Node                    ==== *
 * ========================================================== *
 * 
 * @brief 代码块(Block)，以及可以出现在代码块里的结点。
 *
 * @class Statement abstract 语句结点的基类
 * @class Block final 代码块、空代码块
 * @class EmptyStmt final 空语句
 * @class Declaration abstract 定义语句
 * @class ConstDecl final(没有新成员) 常量定义语句
 * @class VarDecl final(没有新成员) 变量定义语句
 * @class IfStmt final if语句、if-else语句
 * @class WhileStmt final while语句
 * @class BreakStmt final break语句
 * @class ContinueStmt final continue语句
 * @class ReturnStmt final return语句
 */
    /** @class Block final 
     * @brief 代码块，可以形象地写成{Block}.
     *
     * @fn Block(SourceRange const &range, Node *parent, Scope *parent_scope)
     * @fn statements() 读写 getter & setter
     * @fn get_statements() 只读getter
     * @fn append((owned) Statement)
     *     注意，append与prepend如果涉及智能指针，那必须使用右值引用。否则会导致内存泄漏。
     * @fn prepend((owned) Statement)
     * @fn begin()
     * @fn end() */
    class Block: public Statement, public virtual ScopeContainer {
    public:
        using PtrT = std::unique_ptr<Block>;
        using UnownedPtrT = Block*;
        using StmtListT = std::deque<Statement::PtrT>;
    public:
        Block(SourceRange const &range, Node *parent = nullptr, Scope *parent_scope = nullptr)
            : Statement(range, NodeType::BLOCK_STMT),
            _scope_self(nullptr) {
            this->owner_instance = this;
        }

        /* implements ScopeContainer */
        Scope *scope_self() override { return _scope_self.get(); }

        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner_scope) override;

        /** @property statements{get;access;} */
        StmtListT const &get_statements() const { return _statements; }
        StmtListT &      statements()           { return _statements; }

        /** @property is_function_root{get;} bool */
        bool is_function_root() const;

        bool append(Statement::PtrT &&stmt) {
            if (stmt == nullptr)
                return false;
            stmt->set_parent(this);
            Statement::PtrT last = _statements.back();
            last->set_next(stmt.get());
            _statements.push_back(std::move(stmt));
            return true;
        }
        bool prepend(Statement::PtrT &&stmt) {
            if (stmt == nullptr)
                return false;
            stmt->set_parent(this);
            if (!_statements.empty()) {
                Statement::PtrT first = _statements.front();
                stmt->set_next(first.get());
            }
            _statements.push_front(std::move(stmt));
            return true;
        }
        StmtListT::iterator begin() { return _statements.begin(); }
        StmtListT::iterator end()   { return _statements.end(); }
    private:
        StmtListT   _statements;
        Scope::PtrT _scope_self;
    };
    /** @class EmptyStmt
     * @brief 空语句，只有一个分号。(其实我想把{}做成空语句来着)
     * @fn EmptyStmt(SourceRange) */
    class EmptyStmt final: public Statement {
    public:
        using PtrT = std::shared_ptr<EmptyStmt>;
        using UnownedPtrT = EmptyStmt*;
    public:
        EmptyStmt(SourceRange const &range)
            : Statement(range, NodeType::EMPTY_STMT){}
        /* extends Node */
        /** @brief 让visitor直接跳过这段语句. */
        bool accept(CodeVisitor *visitor) override {
            (void)visitor;
            return true;
        }
        bool accept_children(CodeVisitor *visitor) override {
            (void)visitor;
            return true;
        }
        bool register_this(Scope *owner_scope) override {
            return true;
        }
    private:
    }; // class EmptyStmt
    /**   
     * @brief if 分支语句，有符合条件分支与不合条件分支。
     *   
     * @fn IfStmt(SourceRange const &range,  
     *            Node *parent, Scope *parent_scope,  
     *            Statement::PtrT &&true_stmt,  
     *            Statement::PtrT &&false_stmt)
     * @fn get_true_stmt() true分支的getter方法。
     * @fn set_true_stmt(Statement::PtrT const &) true分支的拷贝setter方法。
     * @fn set_true_stmt(Statement::PtrT &&) true分支的移动setter方法。
     *   
     * @fn get_false_stmt() false分支的getter方法。
     * @fn set_false_stmt(Statement::PtrT const &) false分支的拷贝setter方法。
     * @fn set_false_stmt(Statement::PtrT &&) false分支的移动setter方法。
     *
     * @fn get_condition()
     * @fn set_condition(Expression::PtrT const &)
     * @fn set_condition(Expression::PtrT &&)
     */
    class IfStmt final: public Statement {
    public:
        using PtrT = std::shared_ptr<IfStmt>;
        using UnownedPtrT = IfStmt*;
    public:
        IfStmt(SourceRange const &range,
               Expression::PtrT &&condition,
               Statement::PtrT &&true_stmt,
               Statement::PtrT &&false_stmt = nullptr,
               Node *parent = nullptr,
               Scope *parent_scope = nullptr);
        
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner_scope) override;
        
        Statement::PtrT const &get_true_stmt() const { return _true_stmt; }
        void set_true_stmt(Statement::PtrT const &true_stmt);
        void set_true_stmt(Statement::PtrT &&rrtrue_stmt);
        bool has_else() const { return _false_stmt != nullptr; }
        Statement::PtrT const &get_false_stmt() const { return _false_stmt; }
        void set_false_stmt(Statement::PtrT const &false_stmt); 
        void set_false_stmt(Statement::PtrT &&rrfalse_stmt);
        Expression::PtrT const &get_condition() const { return _condition; }
        void set_condition(Expression::PtrT const &condition);
        void set_condition(Expression::PtrT &&rrcondition);
    private:
        Statement::PtrT _true_stmt;
        Statement::PtrT _false_stmt;
        Expression::PtrT _condition;
    }; // class IfStmt
    /** @class WhileStmt final
     * @brief While循环语句
     *
     * @fn WhileStmt(SourceRange const &range,
     *               Node *parent, Scope *parent_scope,
     *               Statement::PtrT true_stmt)
     *
     * @fn get_true_stmt() true分支的getter方法。
     * @fn set_true_stmt(Statement::PtrT const &) true分支的拷贝setter方法。
     * @fn set_true_stmt(Statement::PtrT &&) true分支的移动setter方法。
     *
     * @fn get_condition() 条件表达式的getter方法。
     * @fn set_condition(Expression::PtrT const &)
     * @fn set_condition(Expression::PtrT &&) */
    class WhileStmt final: public Statement {
    public:
        using PtrT = std::shared_ptr<WhileStmt>;
        using UnownedPtrT = WhileStmt*;
    public:
        WhileStmt(SourceRange const &range,
                  Expression::PtrT &&condition,
                  Statement::PtrT &&true_stmt,
                  Node *parent = nullptr,
                  Scope *parent_scope = nullptr);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner) override;
        Statement::PtrT const &get_true_stmt() const { return _true_stmt; }
        void set_true_stmt(Statement::PtrT const &true_stmt);
        void set_true_stmt(Statement::PtrT &&rrtrue_stmt);
        Expression::PtrT const &get_condition() const { return _condition; }
        void set_condition(Expression::PtrT const &condition);
        void set_condition(Expression::PtrT &&rrcondition);
    private:
        Statement::PtrT _true_stmt;
        Expression::PtrT _condition;
    }; // class WhileStmt
    /** @class ReturnStmt final
     * @brief 函数的返回语句
     *
     * @fn ReturnStmt(SourceRange const &range,
                      FunctionPtrT parent,
                      Expression::PtrT return_expression);
     * @fn get_expression()
     * @fn set_expression(Expression::PtrT const &)
     * @fn set_expression(Expression::PtrT &&)
     *
     * @fn get_function()
     * @fn set_function(FunctionPtrT) */
    class ReturnStmt final: public Statement {
    public:
        using PtrT = std::shared_ptr<ReturnStmt>;
        using UnownedPtrT = ReturnStmt*;
    public:
        ReturnStmt(SourceRange const &range,
                   Expression::PtrT &&return_expression = nullptr);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner) override;
        Expression::PtrT const &get_expression() const { return _expression; }
        void set_expression(Expression::PtrT const &return_expression);
        void set_expression(Expression::PtrT &&rreturn_expression);
    private:
        Expression::PtrT _expression;
    }; // @class ReturnStmt
    /** @class BreakStmt final
     * @brief 条件语句/While语句的退出语句.注意，在注册时不会初始化control_node属性。
     *
     * @fn BreakStmt()
     * @fn control_node() getter & setter */
    class BreakStmt final: public Statement {
    public:
        using PtrT = std::shared_ptr<BreakStmt>;
        using UnownedPtrT = BreakStmt*;
        using OwnerPtrT = Statement*;
    public:
        BreakStmt(SourceRange const &range);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner) override;
        OwnerPtrT &control_node() { return _control_node; }
        OwnerPtrT get_control_node() const { return _control_node; }
        bool register_control_node();
    private:
        OwnerPtrT _control_node;
    }; // class BreakStmt
    /** @class ContinueStmt final
     * @brief 条件语句/While语句的退出语句
     *
     * @fn ContinueStmt()
     * @fn control_node() getter & setter */
    class ContinueStmt final: public Statement {
    public:
        using PtrT = std::shared_ptr<ContinueStmt>;
        using UnownedPtrT = ContinueStmt*;
        using OwnerPtrT = WhileStmt*;
    public:
        ContinueStmt(SourceRange const &range);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner) override;
        OwnerPtrT &control_node() { return _control_node; }
        OwnerPtrT get_control_node() const { return _control_node; }
        bool register_control_node();
    private:
        OwnerPtrT _control_node;
    }; // class ContinueStmt
    /** @class ExprStmt final
     * @brief 表达式的包装语句
     *
     * @fn ExprStmt()
     * @fn get_expression()
     * @fn set_expression(Expression::PtrT const&)
     * @fn set_expression(Expression::PtrT &&) */
    class ExprStmt final: public Statement {
    public:
        using PtrT = std::shared_ptr<ExprStmt*>;
        using UnownedPtrT = ExprStmt*;
    public:
        ExprStmt(SourceRange const &range,
                 Expression::PtrT &&expression,
                 Node *parent = nullptr,
                 Scope *owner_scope = nullptr);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner) override;
        /* owned Expression expression {get; set; default = null}; */
        Expression::PtrT const &get_expression() const { return _expression; }
        void set_expression(Expression::PtrT const &value);
        void set_expression(Expression::PtrT &&rrvalue);
    private:
        Expression::PtrT _expression;
    }; // class ExprStmt
/**
 * ========================================================== *
 * ====               Expression Node                    ==== *
 * ========================================================== *
 *
 * @brief 表达式结点。包括表达式、值、变量、初始化列表。
 *
 * @class Expression
 * @class UnaryExpr  一元表达式
 * @class BinaryExpr 二元表达式
 * @class CallExpr   函数调用表达式
 * @class AssignExpr 赋值表达式，左操作数为左值，有时需要被读取
 * @class IndexExpr  数组索引表达式，左操作数可以被解引用
 * @class Value abstract 值表达式，都是右值
 * @class IntValue final 整数值表达式
 * @class FloatValue final 浮点值表达式
 * @class StringValue final (备用)字符串值表达式
 * @class Identifier 标识符表达式，可以标识变量、类型与函数.
 *
 * @fn register_this() 通过自己所处的作用域给标识符匹配合适的定义
 */
    /** @class UnaryExpr
     * @brief 一元表达式
     *
     * @fn UnaryExpr(SourceRange const &range,
                     Operator xoperator,
                     Expression::PtrT const &expr,
                     Node *parent)
     * @fn operator{get = xoperator(); set;}
     * @fn expression{get; set;} */
    class UnaryExpr final: public Expression {
    public:
        using PtrT = std::shared_ptr<UnaryExpr>;
        using UnownedPtrT = UnaryExpr*;
    public:
        UnaryExpr(SourceRange const &range,
                  Operator xoperator,
                  Expression::PtrT &&expr,
                  Node *parent = nullptr);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner_scope) override;
        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override;
        Operator xoperator() const { return _xoperator; }
        Operator get_operator() const { return _xoperator; }
        void set_operator(Operator xoperator) {
            _xoperator = xoperator;
        }
        Expression::PtrT const &get_expression() const {
            return _expression;
        }
        void set_expression(Expression::PtrT const &expr);
        void set_expression(Expression::PtrT &&expr);
    private:
        Operator _xoperator;
        Expression::PtrT _expression;
    }; // class UnaryExpr


    /** @class BinaryExpr
     * @brief 二元表达式。注意，在语法分析器里只能通过移动语义设置左右操作数。
     *
     * @fn BinaryExpr(SourceRange const &range,
                      Operator xoperator,
                      Expression::PtrT lhs,
                      Expression::PtrT rhs,
                      Node *parent)
     * @fn get_operator()
     * @fn set_operator(const Operator)
     *
     * @fn get_lhs()
     * @fn set_lhs(Expression::PtrT const &)
     * @fn set_lhs(Expression::PtrT &&)
     *
     * @fn get_rhs()
     * @fn set_rhs(Expression::PtrT const &)
     * @fn set_rhs(Expression::PtrT &&) */
    class BinaryExpr final: public Expression {
    public:
        using PtrT = std::shared_ptr<BinaryExpr>;
        using UnownedPtrT = BinaryExpr*;
    public:
        BinaryExpr(SourceRange const &range,
                   Operator xoperator,
                   Expression::PtrT &&lhs,
                   Expression::PtrT &&rhs,
                   Node *parent = nullptr);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner_scope) override;

        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override;

        /** @property operator{get;set;} */
        Operator get_operator() const { return _xoperator; }
        void set_operator(const Operator xoperator) {
            _xoperator = xoperator;
        }

        /** @property lhs{get;set;} */
        Expression::PtrT const &get_lhs() const { return _lhs; }
        void set_lhs(Expression::PtrT const &lhs);
        void set_lhs(Expression::PtrT &&rlhs);

        /** @property rhs{get;set;} */
        Expression::PtrT const &get_rhs() const { return _rhs; }
        void set_rhs(Expression::PtrT const &rhs);
        void set_rhs(Expression::PtrT &&rrhs);
    private:
        Operator _xoperator;
        Expression::PtrT _lhs, _rhs;
    }; // class BinaryExpr
    /** @class CallParam
     * @brief 调用表达式的参数列表。注意，在语法分析器里只能使用移动语义的append。
     *
     * @var expression{get; access;} 表达式列表
     * @fn append(Expression::PtrT &&)
     * @fn prepend(Expression::PtrT &&)
     * @fn operator[](int index) */
    class CallParam final: public Expression {
    public:
        using PtrT = std::shared_ptr<CallParam>;
        using UnownedPtrT = CallParam*;
        using ExprListT = std::deque<Expression::PtrT>;
    public:
        CallParam(SourceRange const &range);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope::UnownedPtrT scope) override;
        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override;
        ExprListT &expression() { return _expression; }
        ExprListT const &get_expression() const { return _expression; }
        bool append(Expression::PtrT const &expr);
        bool append(Expression::PtrT &&expr);
        bool prepend(Expression::PtrT const &expr);
        bool prepend(Expression::PtrT &&expr);
        Expression::PtrT &operator[](int index) {
            return _expression[index];
        }
    private:
        ExprListT _expression;
    }; // class CallParam
    class Identifier final: public Expression {
    public:
        using PtrT = std::shared_ptr<Identifier>;
        using UnownedPtrT = Identifier*;
    public:
        Identifier(SourceRange const &range);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope::UnownedPtrT scope) override;
        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override;
        std::string_view name() { return range().get_content(); }
        Definition::UnownedPtrT get_definition() { return _definition; }
        void set_definition(Definition::UnownedPtrT definition) {
            _definition = definition;
        }
    private:
        Definition::UnownedPtrT _definition;
    }; // class Identifier
    /** @class CallExpr
     * @brief 函数调用表达式
     *
     * @var name{get; set;} 被调用的函数名称
     * @var param{get; set;} 参数列表 */
    class CallExpr final: public Expression {
    public:
        using PtrT = std::shared_ptr<CallExpr>;
        using UnownedPtrT = CallExpr*;
    public:
        CallExpr(SourceRange const &range,
                 Identifier::PtrT &&name,
                 CallParam::PtrT  &&param);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner_scope) override;
        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override;
        Identifier::PtrT const &get_name() const { return _name; }
        void set_name(Identifier::PtrT const &name);
        void set_name(Identifier::PtrT &&name);
        CallParam::PtrT const &get_param() const { return _param; }
        void set_param(CallParam::PtrT const &param);
        void set_param(CallParam::PtrT &&param);
    private:
        Identifier::PtrT _name;
        CallParam::PtrT _param;
    }; // class CallExpr

    class InitList final: public Expression {
    public:
        using PtrT = std::shared_ptr<InitList>;
        using UnownedPtrT = InitList*;
        using ExprListT = std::deque<Expression::PtrT>;
    public:
        InitList(SourceRange const &range);
        
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope::UnownedPtrT scope) override;
        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override;
        ExprListT const &get_expr_list() { return _expr_list; }
        ExprListT &expr_list() { return _expr_list; }
        bool append(Expression::PtrT const &item);
        bool append(Expression::PtrT &&item);
        bool prepend(Expression::PtrT const &item);
        bool prepend(Expression::PtrT &&item);
    private:
        ExprListT _expr_list;
    }; // class InitList

    /** @class IndexExpr final
     * @brief 数组索引表达式，包括变量引用+一串方括号表达式
     *
     * @var name{get; set;} 被索引的变量名
     * @var index_list{get; access;} 下标表达式列表
     *
     * @fn append(Expression::PtrT const &) 在末尾加一个索引
     * @fn append(Expression::PtrT &&) 把游离的索引结点移动到列表末尾
     * @fn prepend(Expression::PtrT const &) 在开头加一个索引
     * @fn prepend(Expression::PtrT &&) 把游离的索引结点移动到列表开头 */
    class IndexExpr final: public Expression {
    public:
        using PtrT = std::shared_ptr<IndexExpr>;
        using UnownedPtrT = IndexExpr*;
        using ExprListT = std::deque<Expression::PtrT>;
    public:
        IndexExpr(SourceRange const &range, Identifier::PtrT &&name);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope::UnownedPtrT scope) override;
        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override;

        /** @property name{get;set;} shared
         *  @brief 索引表达式的名称, 实际上是Identifier. */
        Identifier::PtrT const &get_name() const { return _name; }
        void set_name(Identifier::PtrT const &name);
        void set_name(Identifier::PtrT &&name);
        
        /** @property index_list{get;access;} */
        ExprListT const &get_index_list() const { return _index_list; }
        ExprListT &index_list() { return _index_list; }

        bool append(Expression::PtrT const &index);
        bool append(Expression::PtrT &&index);
        bool prepend(Expression::PtrT const &index);
        bool prepend(Expression::PtrT &&index);
    private:
        Identifier::PtrT _name;
        ExprListT  _index_list;
    }; // class IndexExpr

    /** @class Value abstract*/
    imcomplete class Value: public Expression {
    public:
        using PtrT = std::shared_ptr<Value>;
        using UnownedPtrT = Value*;

        struct ivalue_agent {
            int64_t ivalue:63;
            bool   success:1;
        public:
            operator int64_t() const { return ivalue;  }
            bool get_success() const { return success; }
        }; // struct ivalue_agent

        struct fvalue_agent {
            double   fvalue;
        public:
            operator double() const { return fvalue; }
            bool    success() const {
                return std::isnan(fvalue);
            }
        }; // struct fvalue_agent
    public:
        Value(SourceRange const &range, NodeType type = NodeType::VALUE);

        /* extends Node */
        bool accept_children(CodeVisitor *visitor) override {
            (void)visitor;
            return true;
        }
        bool register_this(Scope::UnownedPtrT scope) override {
            set_scope(scope);
            return true;
        }
        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override {
            return std::make_shared<VarListT>();
        }

        /* 与一般常量的交互 */
        abstract std::string value_string() {
            return std::string(range().get_content());
        }
        /** @property int_value{get;set;} */
        abstract ivalue_agent get_int_value() const = 0;

        /** @property float_value{get;set;} */
        abstract fvalue_agent get_float_value() const = 0;
    private:
    }; // class Value

    class StringValue final: public Value {
    public:
        StringValue(SourceRange const &range);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        ivalue_agent get_int_value() const final { return {0, false}; }
        fvalue_agent get_float_value() const final { return {NAN}; }
    }; // class StringValue
    class IntValue final: public Value {
    public:
        using PtrT = std::shared_ptr<IntValue>;
        using UnownedPtrT = IntValue*;
    public:
        IntValue(SourceRange const &range);
        IntValue(int64_t ival)
            : Value(SourceRange{}, NodeType::INT_VALUE),
            _value(ival) {
        }
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        /* extends Value */
        std::string value_string() override;
        ivalue_agent get_int_value() const final { return {get_value(), true}; }
        fvalue_agent get_float_value() const final {
            return {double(get_value())};
        }

        int64_t &value() { return _value; }
        int64_t get_value() const { return _value; }
    private:
        int64_t _value;
    }; // class IntValue

    class FloatValue final: public Value {
    public:
        using PtrT = std::shared_ptr<FloatValue>;
        using UnownedPtrT = FloatValue*;
    public:
        FloatValue(SourceRange const &range);
        FloatValue(double value)
            : Value(SourceRange{}, NodeType::FLOAT_VALUE) {
            _value = value;
        }
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        /* extends Value */
        std::string value_string() override;
        ivalue_agent get_int_value() const final {
            return {int64_t(get_value()), true};
        }
        fvalue_agent get_float_value() const final {
            return {get_value()};
        }

        double &value() { return _value; }
        double get_value() const { return _value; }
    private:
        double _value;
    }; // class FloatValue

    /** @class AssignExpr
     * @brief 赋值表达式。考虑到以后可能会出现`+=`、连等之类的语法，所以没有列成
     *    Statement.
     * @*/
    class AssignExpr final: public Expression {
    public:
        using PtrT = std::shared_ptr<AssignExpr>;
        using UnownedPtrT = AssignExpr*;
    public:
        AssignExpr(SourceRange const &range,
                   Expression::PtrT &&source,
                   Expression::PtrT &&destination);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope::UnownedPtrT scope) override;
        /* extends Expression */
        std::shared_ptr<VarListT> get_variable_list() override;
        Operator &xoperator() { return _xoperator; }
        Operator get_operator() const { return _xoperator;}
        /** @brief destination属性的getter. */
        Expression::PtrT const &get_destination() const noexcept {
            return _destination;
        }
        /** @brief destination属性的拷贝setter. */
        void set_destination(Expression::PtrT const &value);
        /** @brief destination属性的移动setter. */
        void set_destination(Expression::PtrT &&value);
        /** @var source 源操作数 */
        /** @brief source属性: getter */
        Expression::PtrT const &get_source() const noexcept {
            return _source;
        }
        /** @brief source属性: 拷贝setter */
        void set_source(Expression::PtrT const &source);
        /** @brief source属性: 移动setter */
        void set_source(Expression::PtrT &&source);
    private:
        Operator _xoperator;
        Expression::PtrT _destination;
        Expression::PtrT _source;
    }; // class AssignExpr

/**
 * ========================================================== *
 * ====               Definition Node                    ==== *
 * ========================================================== *
 *
 * extended classes:<br/>
 * @class Type abstract 数据类型
 * @class VarType final 变量类型
 * @class ArrayInfo final 数组类型
 * @class Variable final 变量与常量定义
 * @class Function final 函数定义 */
    class ArrayInfo final: public Node {
    public:
        using PtrT = std::shared_ptr<ArrayInfo>;
        using UnownedPtrT = ArrayInfo*;
        using InfoListT = std::deque<Expression::PtrT>;
    public:
        ArrayInfo(SourceRange const &range)
            : Node(range, NodeType::ARRAY_DECL) {}
        ArrayInfo(ArrayInfo const &another)
            : Node(another._range, NodeType::ARRAY_DECL, another.parent(), another.get_scope()),
              _array_info(another._array_info) {
        }
        ArrayInfo(ArrayInfo::PtrT const &another)
            : ArrayInfo(*another){}
    public:
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override {
            (void)visitor;
            return false;
        }
        bool register_this(Scope *owner_scope) override;

        InfoListT &array_info() { return _array_info; }
        InfoListT const &get_array_info() const { return _array_info; }

        int array_size() const { return _array_info.size(); }

        bool append(Expression::PtrT const &next);
        bool append(Expression::PtrT &&next);
        bool prepend(Expression::PtrT const &next);
        bool prepend(Expression::PtrT &&next);

        std::string to_string() const;

        int get_dimension() const { return _array_info.size(); }
        bool requires_initlist() const {
            return _array_info[0] == 0;
        }

        InfoListT::value_type &operator[](int index) {
            return _array_info[index];
        }
        InfoListT::iterator begin() { return _array_info.begin(); }
        InfoListT::iterator end()   { return _array_info.end();   }

        bool equals(ArrayInfo *rhs);
    private:
        InfoListT _array_info;
    }; // class ArrayDecl

    /** @class Type
     * @brief 类型定义. 
     * @warning 设计失误：由于SysY只有基础类型与数组类型，所以这么设计没有大问题。但是如果出现struct,
     *   typedef之类的自定义类型，那么这个类必须重构。
     *     实际上我是打算这么设计Type类型的：
     *   * public abstract Type作为base class.
     *   * public final VarType/ArrayType/FunctionType/Structure/TypeAlias作为子类型
     *   * 通过full_type_name(), traverse_type()等等虚函数作为访问接口.
     *   可惜时间紧，我没法这么做了。
     *
     * @fn type_name() 类型名称
     * @fn full_type_name() 获取完整类型名称
     * @fn is_array_type() 是否为数组类型
     * @fn array_info() 读/写数组信息 */
    class Type: public Definition {
    public:
        using PtrT = std::shared_ptr<Type>;
        using UnownedPtrT = Type*;
    public:
        Type(SourceRange const &range,
             Type *base_type, std::string_view type_name,
             Node *parent, Scope *owner_scope)
            : Definition(range, base_type, type_name, parent, owner_scope),
              _array_info(nullptr) {
            _node_type = NodeType::TYPE_DEF;
        }
        Type(SourceRange const &range,
             Type *base_type, std::string_view type_name,
             ArrayInfo::PtrT &&arr_info = nullptr)
            : Definition(range, base_type, type_name, nullptr, nullptr),
              _array_info(std::move(arr_info)) {
            _node_type = NodeType::TYPE_DEF;
        }
        Type(Type const &another);
        Type(Type &&rranother);
        /* extends Definition */
        /** @brief override 在父结点中注册自己
         * @warning 由于SysY没有typedef这样的语句，所以该函数是不可使用的。
         *     就算调用了该函数，也会直接拒绝注册、返回false. */
        bool register_this() override { return false; }
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override {
            (void)visitor;
            return false;
        }
        bool register_this(Scope *owner_scope) override {
            set_scope(owner_scope);
            if (_array_info != nullptr)
                _array_info->register_this(owner_scope);
            return true;
        }
        inline std::string_view type_name() const { return name(); }
        std::string full_name() { return to_string(); }
        std::string to_string();
        bool is_array_type() const { return (_array_info != nullptr); }
        ArrayInfo::PtrT const &get_array_info() { return _array_info; }
        void set_array_info(ArrayInfo::PtrT const &arr_info) {
            if (arr_info == nullptr || arr_info == _array_info)
                return;
            arr_info->set_parent(this);
            _array_info = arr_info;
        }
        void set_array_info(ArrayInfo::PtrT &&arr_info) {
            if (arr_info == nullptr || arr_info == _array_info)
                return;
            arr_info->set_parent(this);
            _array_info = std::move(arr_info);
        }

        bool equals(Type::UnownedPtrT rhs);
    protected:
        ArrayInfo::PtrT _array_info;
    public:
        /** @brief IntType, FloatType, VoidType
         * 这些是预定义的类型，作用域属于预定义全局作用域(CompUnit::Predefined). */
        static Type &IntType;   /// int类型
        static Type &FloatType; /// float类型
        static Type &VoidType;  /// void类型
        static std::shared_ptr<Type> OwnedIntType;
        static std::shared_ptr<Type> OwnedFloatType;
        static std::shared_ptr<Type> OwnedVoidType;
    }; // class Type
    class VarType final: public Type {};

    /** @class Variable
     * @brief 变量与常量定义
     * @fn is_constant() 是否为常量
     * @fn array_type() 返回变量的数组属性指针，没有的话就是null
     * @fn variable_name() 返回变量的名称 */
    class Variable final: public Definition {
    public:
        using PtrT = std::shared_ptr<Variable>;
        using UnownedPtrT = Variable*;
    public:
        Variable(SourceRange const &range,
                 bool is_constant,
                 Type *base_type, std::string_view name,
                 ArrayInfo::PtrT &&arr_info = nullptr,
                 Expression::PtrT &&init_expr = nullptr)
            : Definition(range, base_type, name, nullptr, nullptr),
              _is_constant(is_constant),
              _array_info(std::move(arr_info)),
              _init_expr(std::move(init_expr)) {
            _node_type = NodeType::VAR_DEF;
            if (_array_info != nullptr)
                _array_info->set_parent(this);
            if (_init_expr != nullptr)
                _init_expr->set_parent(this);
        }

        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        // children: Type, Expression
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner_scope) override;
        /* extends Definition */
        bool register_this() override { return register_this(_scope); }

        /** @property is_constant{get;set;} bool
         *  @brief 获取变量声明的完整类型属性，包括基础类型、数组类型 */
        bool is_constant() const noexcept { return _is_constant; }
        void set_constant(bool is_constant) { _is_constant = is_constant; }

        /** @property real_type{get;} shared */
        Type::PtrT get_real_type() const;
        bool is_array_type() const { return get_array_info() != nullptr; }

        /** @property array_info{get;set;} */
        ArrayInfo::PtrT const &get_array_info() const { return _array_info; }
        void set_array_info(ArrayInfo::PtrT const &array_info) {
            array_info->set_parent(this);
            _array_info = array_info;
        }
        void set_array_info(ArrayInfo::PtrT &&array_info) {
            array_info->set_parent(this);
            _array_info = std::move(array_info);
        }

        /** @property init_expr{get;set;} */
        Expression::PtrT const &get_init_expr() { return _init_expr; }
        void set_init_expr(Expression::PtrT const &value);
        void set_init_expr(Expression::PtrT &&value);

        /** @property is_func_param{get;} bool */
        bool is_func_param() const;
    private:
        bool _is_constant = false;
        ArrayInfo::PtrT _array_info;
        Expression::PtrT _init_expr;
    }; // class Variable
    /** @class FuncParam final
     * @brief 函数参数列表，本质上是Variable声明
     * @fn FuncParam(SourceRange const &range)
     * @fn get_parent_function() 父结点是函数
     * @fn set_parent_function()
     * @fn  */
    class FuncParam final: public Node {
    public:
        using PtrT = std::unique_ptr<FuncParam>;
        using UnownedPtrT = FuncParam*;
        using ParamListT = std::deque<Variable::PtrT>;
    public:
        FuncParam(SourceRange const &range);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owenr) override;
        Function *get_function() const;
        void set_function(Function *parent);
        ParamListT const& get_param_list() const { return _param_list; }
        bool append(Variable::PtrT &&param);
        bool prepend(Variable::PtrT &&param);
        bool append_param(SourceRange const& range,
                          Type *base_type, std::string_view name,
                          ArrayInfo::PtrT &&array_info = nullptr);
        bool prepend_param(SourceRange const& range,
                           Type *base_type, std::string_view name,
                           ArrayInfo::PtrT &&array_info = nullptr);
    private:
        ParamListT _param_list;
    };
    class Function final: public Definition, public ScopeContainer {
    public:
        using PtrT = std::shared_ptr<Function>;
        using UnownedPtrT = Function*;
    public:
        Function(SourceRange const &range,
                 Type *return_type, std::string_view func_name,
                 FuncParam::PtrT &&func_param, Block::PtrT &&func_body);

        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owenr) override;

        /* extends Definition */
        bool register_this() override { return get_scope()->add(this); }

        /* implements ScopeContainer */
        Scope *scope_self() override { return _scope_self.get(); }

        /** @property return_type{get;set;} */
        Type::UnownedPtrT get_return_type() const { return _base_type; }

        /** @property func_params{get;set;}
         *  @brief 函数参数 */
        FuncParam::UnownedPtrT get_func_params() const { return _func_params.get(); }
        void set_func_params(FuncParam::PtrT &&fn_params);

        /** @property func_body{get;access;}
         *  @brief    函数体, 本质上是一个Block */
        Block::PtrT const &get_func_body() const { return _func_body; }
        Block::PtrT &      func_body()           { return _func_body; }

        /** @property is_extern{get;} bool
         *  @brief 指示该函数是否只是声明而非定义 */
        bool is_extern() const { return _func_body == nullptr; }
    private:
        Scope::PtrT     _scope_self;
        Block::PtrT     _func_body;
        FuncParam::PtrT _func_params;
    };
    /** @class Declaration abstract
     * @brief 声明语句，一种Statement. 在Sema扫描自己的时候，声明里的所有变量
     *   都会注册到自己所在的作用域里。
     *     在我最初的设计里，Declaration除了没有CodeVisitor的访问函数以外，和
     *   它的子类一模一样。
     *     注意，在语法分析器里只能使用移动语义的append/prepend函数。
     *
     * @fn Declaration()
     * @fn is_constant()
     * @fn set_constant(bool)
     *
     * @fn variables()
     * @fn get_variables()
     *
     * @fn append(VarPtrT const &)
     * @fn append(VarPtrT &&)
     * @fn prepend(VarPtrT const &)
     * @fn prepend(VarPtrT &&)
     *
     * @fn get_variable(std::string_view)
     * @fn register_this() */
    imcomplete class Declaration: public Statement {
    public:
        using PtrT = std::shared_ptr<Declaration>;
        using UnownedPtrT = Declaration*;
        using VarPtrT    = Variable::PtrT;
        using VarUnowned = Variable::UnownedPtrT;
        using VarListT   = std::deque<VarPtrT>;
        using VarListPtrT = std::shared_ptr<VarListT>;
    public:
        bool is_constant() const { return _is_constant; }
        void set_constant(bool is_constant) { _is_constant = is_constant; }
        VarListT &variables() { return _variables; }
        VarListT const &get_variables() const { return _variables; }
        bool append(VarPtrT const &variable);
        bool append(VarPtrT &&rrvariable);
        bool prepend(VarPtrT const &variable);
        bool prepend(VarPtrT &&rrvariable);
        bool append(VarListPtrT &&variable_list) {
            bool ret = true;
            for (auto &i: *variable_list) {
                ret = append(std::move(i));
                if (!ret) return false;
            }
            return true;
        }
        bool prepend(VarListPtrT &&variable_list) {
            bool ret = true;
            for (auto &i: *variable_list) {
                ret = prepend(std::move(i));
                if (!ret) return false;
            }
            return true;
        }
        VarUnowned get_variable(std::string_view name);
        Type *get_base_type() { return _base_type; }
        void set_base_type(Type *base_type) {
            _base_type = base_type;
            for (auto &i : _variables)
                i->set_base_type(base_type);
        }
    protected:
        Declaration(SourceRange const &range,
                    Type *base_type, bool is_constant = false,
                    Node *parent = nullptr,
                    Scope *owner_scope = nullptr);
        VarListT _variables;
        Type    *_base_type;
        bool   _is_constant;
    }; // class Declaration
    class VarDecl final: public Declaration {
    public:
        using PtrT = std::shared_ptr<VarDecl>;
        using UnownedPtrT = VarDecl*;
    public:
        VarDecl(SourceRange const &range,
                Type *base_type,
                Node *parent = nullptr, Scope *owner_scope = nullptr);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner) override;
    }; // class VarDecl
    class ConstDecl final: public Declaration {
    public:
        using PtrT = std::shared_ptr<ConstDecl>;
        using UnownedPtrT = ConstDecl*;
    public:
        ConstDecl(SourceRange const &range,
                Type *base_type,
                Node *parent = nullptr, Scope *owner_scope = nullptr);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owenr) override;
    }; // class ConstDecl
    class CompUnit final: public Node, public virtual ScopeContainer {
    public:
        using PtrT = std::unique_ptr<CompUnit>;
        using UnownedPtrT = CompUnit*;
        using DeclListT = std::deque<Declaration::PtrT>;
        using FuncListT = std::deque<Function::PtrT>;
        using FuncTraverseFunc = std::function<bool (Function &)>;
        using DeclTraverseFunc = std::function<bool (Declaration &)>;
    public:
        CompUnit(SourceRange const &range);
        /* extends Node */
        bool accept(CodeVisitor *visitor) override;
        bool accept_children(CodeVisitor *visitor) override;
        bool register_this(Scope *owner_scope) override;
        /* implements ScopeContainer */
        Scope *scope_self() override { return _scope_self.get(); }
        DeclListT &decls() { return _decls; }
        FuncListT &funcdefs() { return _funcdefs; }
        DeclListT const &get_decls() const { return _decls; }
        FuncListT const &get_funcdefs() const { return _funcdefs; }
        void append(Declaration::PtrT  &&decl);
        bool append(Function::PtrT     &&func);
        void prepend(Declaration::PtrT &&decl);
        bool prepend(Function::PtrT    &&func);
        Function *get_function(std::string_view name);
        Function *get_function(cstring name) {
            return get_function({name, strlen(name)});
        }
        Variable *get_variable(std::string_view name);
        Variable *get_variable(cstring name) {
            return get_variable({name, strlen(name)});
        }
        int traverse(FuncTraverseFunc fn_fntraverse, DeclTraverseFunc fn_decltraverse)
        {
            int cnt = 0;
            for (auto i : funcdefs()) {
                cnt++;
                if (!fn_fntraverse(*i))
                    return cnt;
            }
            cnt = 0;
            for (auto i: decls()) {
                cnt--;
                if (!fn_decltraverse)
                    return cnt;
            }
            return 0;
        }
        /** @brief 预定义全局作用域，所有作用域的根作用域，每个源文件都一样。
         * 全局只读，存放编译器默认的变量、builtin函数、基本类型等。 */
        static CompUnit &Predefined;
        static CompUnit::PtrT OwnedPredefined;
    private:
        Scope::PtrT _scope_self;
        DeclListT   _decls;
        FuncListT   _funcdefs;
    }; // CompUnit

} // namespace MYGL::Ast

#endif

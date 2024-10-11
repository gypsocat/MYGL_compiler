%require "3.2"
%language "c++"
%header "../include/myglc-lang/ast-parser.tab.hxx"
%code requires {
#include "ast-node.hxx"
#include "ast-lexer.hxx"
#include "ast-code-context.hxx"
#include "ast-exception.hxx"
using namespace MYGL::Ast;
using Token = Lexer::Token;
}
%code top {
#include "../include/myglc-lang/ast-parser.tab.hxx"
#include "base/mtb-exception.hxx"
static auto yylex(parser::value_type *val_type,
                  parser::location_type *location,
                  Lexer *lexer) {
    Token::PtrT token;
    auto ret = lexer->lex(token);
//     token->println();
    val_type->as<Token::PtrT>() = token;
    *location = lexer->src_range;
    return ret;
}
}

%define api.namespace {MYGL::Ast}
/* 我用智能指针当语法单元/词法单元，为了避免智能指针类型不同导致的内存错误，所以只能使用variant了 */
%define api.value.type variant
%define api.location.type {SourceRange}

%lex-param {Lexer *lexer}

%parse-param { CodeContext &ctx };
%parse-param { Lexer *lexer };
%initial-action {
}
%locations

%{
// template<typename Tp, typename... Args>
// inline auto news(Args... args) -> std::shared_ptr<Tp> {
//     return std::make_shared<Tp>(args...);
// }
// template<typename Tp, typename... Args>
// inline auto newu(Args... args) -> std::unique_ptr<Tp> {
//     return std::make_unique<Tp>(args...);
// }
#define news std::make_shared
#define newu std::make_unique

template<typename T1, typename T2>
SourceRange merge_range(T1 const &s1, T2 const &s2) {
    SourceRange ret = s1->range();
    ret.end = s2->range().end;
    return ret;
}

void parser::error(parser::location_type const &location, std::string const &msg)
{
    using namespace MTB::Compatibility;
    MTB::crash_with_stacktrace(
        true, CURRENT_SRCLOC_F,
        rt_fmt("location %s: %s",
               location.to_string().c_str(),
               msg.c_str())
    );
}

%}

%type <CompUnit::PtrT>    CompUnit MYGLProgram
%type <Declaration::PtrT> Decl
%type <ConstDecl::PtrT>   ConstDecl
%type <Declaration::VarListPtrT> ConstDefs VarDecls
%type <VarDecl::PtrT>     VarDecl
%type <Variable::PtrT>    ConstDef  VarDef
%type <ArrayInfo::PtrT>   ConstExps
%type <Expression::PtrT>  ConstInitVal  InitVal
%type <InitList::PtrT>    ConstInitVals InitVals
%type <Function::PtrT>    FuncDef
%type <FuncParam::PtrT>   FuncParams
%type <CallParam::PtrT>   CallParams
%type <Variable::PtrT>    FuncParam
%type <Block::PtrT>       Block BlockItems
%type <Statement::PtrT>   BlockItem Stmt
%type <Expression::PtrT>  Exp ConstExp LVal PrimaryExp UnaryExp
%type <Expression::PtrT>  MulExp AddExp RelExp EqExp LAndExp LOrExp
%type <Type::PtrT>        Type
%type <ArrayInfo::PtrT>   ArraySubscripts

%token <Token::PtrT> T_BASE
%token <Token::PtrT> T_SINGLE_LINE_COMMENT
%token <Token::PtrT> T_COMMENT_OPEN
%token <Token::PtrT> T_COMMENT_CLOSE
%token <Token::PtrT> T_SEMICOLON
%token <Token::PtrT> T_COLON
%token <Token::PtrT> T_COMMA
%token <Token::PtrT> T_QUESTION
%token <Token::PtrT> T_INT_CONST
%token <Token::PtrT> T_FLOAT_CONST
%token <Token::PtrT> T_STRING_LITERAL
%token <Token::PtrT> T_VOID
%token <Token::PtrT> T_INT
%token <Token::PtrT> T_FLOAT
%token <Token::PtrT> T_CONST
%token <Token::PtrT> T_IDENT
%token <Token::PtrT> T_OP_LBRACE
%token <Token::PtrT> T_OP_RBRACE
%token <Token::PtrT> T_OP_LQUOTE
%token <Token::PtrT> T_OP_RQUOTE
%token <Token::PtrT> T_OP_LBRACKET
%token <Token::PtrT> T_OP_RBRACKET
%token <Token::PtrT> T_OP_ASSIGN
%token <Token::PtrT> T_OP_SHL
%token <Token::PtrT> T_OP_SHR
%token <Token::PtrT> T_OP_AND
%token <Token::PtrT> T_OP_OR
%token <Token::PtrT> T_OP_PLUS
%token <Token::PtrT> T_OP_SUB
%token <Token::PtrT> T_OP_STAR
%token <Token::PtrT> T_OP_SLASH
%token <Token::PtrT> T_OP_PERCENT
%token <Token::PtrT> T_OP_LT
%token <Token::PtrT> T_OP_GT
%token <Token::PtrT> T_OP_LE
%token <Token::PtrT> T_OP_GE
%token <Token::PtrT> T_OP_EQ
%token <Token::PtrT> T_OP_NE
%token <Token::PtrT> T_REL_AND
%token <Token::PtrT> T_REL_OR
%token <Token::PtrT> T_REL_NOT
%token <Token::PtrT> T_OP_DOT
%token <Token::PtrT> T_OP_SARROW
%token <Token::PtrT> T_OP_DARROW
%token <Token::PtrT> T_OP_SIZEOF
%token <Token::PtrT> T_IF
%token <Token::PtrT> T_ELSE
%token <Token::PtrT> T_WHILE
%token <Token::PtrT> T_FOR
%token <Token::PtrT> T_CONTINUE
%token <Token::PtrT> T_BREAK
%token <Token::PtrT> T_SWITCH
%token <Token::PtrT> T_CASE
%token <Token::PtrT> T_RETURN
%token <Token::PtrT> T_EOI
%token <Token::PtrT> T_BIGGEST
%token <Token::PtrT> T_SPACE
%token <Token::PtrT> T_COMMENT
%token <Token::PtrT> T_STRUCT
%token <Token::PtrT> T_CLASS
%token <Token::PtrT> T_AUTO
%token <Token::PtrT> T_PUBLIC
%token <Token::PtrT> T_PRIVATE
%token <Token::PtrT> T_PROTECTED
%token <Token::PtrT> 

%%
MYGLProgram: CompUnit {
                        $$ = std::move($1);
                        if ($$ == nullptr) {
                            throw NullException(nullptr, ErrorLevel::FATAL, "Bad CompUnit\n");
                        }
                        $$->register_this(CompUnit::Predefined.scope_self());
                        ctx.sync_lexer();
                        ctx.root() = std::move($$);
                        YYACCEPT;
                    }
           ;

CompUnit: Decl CompUnit {
                        $2->prepend(std::move($1));
                        $$ = std::move($2);
                    }
        | FuncDef CompUnit {
                        $2->prepend(std::move($1));
                        $$ = std::move($2);
                    }
        | Decl {        auto mccv = std::make_unique<CompUnit>($1->range());
                        mccv->append(std::move($1));
                        $$ = std::move(mccv);
               }
        | FuncDef {     auto mccv = std::make_unique<CompUnit>($1->range());
                        mccv->append(std::move($1));
                        $$ = std::move(mccv);
               }
        ;

Decl: ConstDecl {   $$ = std::move($1); }
    | VarDecl   {   $$ = std::move($1); }
    ;

ConstDecl: T_CONST Type ConstDef T_SEMICOLON {
                    SourceRange new_range = $1->range();
                    new_range.end = $4->range().end;
                    $$ = news<ConstDecl>(new_range, $2.get(), nullptr, nullptr);
                    $$->append(std::move($3));
                }
         | T_CONST Type ConstDefs T_SEMICOLON {
                    SourceRange new_range = $1->range();
                    new_range.end = $4->range().end;
                    $$ = news<ConstDecl>(new_range, $2.get(), nullptr, nullptr);
                    $$->append(std::move($3));
                    $3.reset();
                }
         ;

ConstDefs: ConstDef T_COMMA ConstDef {
                    $$ = news<Declaration::VarListT>();
                    $$->push_back(std::move($1));
                    $$->push_back(std::move($3));
                }
         | ConstDefs T_COMMA ConstDef {
                    $$ = std::move($1);
                    $$->push_back(std::move($3));
                }
         ;

ConstDef: T_IDENT T_OP_ASSIGN ConstInitVal {
                    SourceRange new_range = $1->range();
                    new_range.end = $3->range().end;
                    auto &void_type = Type::VoidType;
                    $$ = news<Variable>(
                        new_range, true,
                        &void_type, $1->range().get_content(),
                        nullptr, std::move($3)
                    );
                }
        | T_IDENT ConstExps T_OP_ASSIGN ConstInitVal {
                    SourceRange new_range = $1->range();
                    new_range.end = $4->range().end;
                    $$ = news<Variable>(
                        new_range, true,
                        &Type::VoidType,
                        $1->range().get_content(),
                        std::move($2),
                        std::move($4)
                    );
                }
        ;


ConstExps: T_OP_LBRACKET ConstExp T_OP_RBRACKET {
                    $$ = news<ArrayInfo>(merge_range($1, $3));
                    $$->prepend(std::move($2));
                }
         | T_OP_LBRACKET ConstExp T_OP_RBRACKET ConstExps {
                    $$ = std::move($4);
                    $$->prepend(std::move($2));
                    $$->range() = merge_range($1, $$);
                }
         ;

ConstInitVal: ConstExp {
                    $$ = std::move($1);
                }
            | T_OP_LBRACE T_OP_RBRACE {
                    $$ = news<InitList>(merge_range($1, $2));
                }
            | T_OP_LBRACE ConstInitVal T_OP_RBRACE {
                    auto mccv = news<InitList>(merge_range($1, $3));
                    mccv->append(std::move($2));
                    $$ = std::move(mccv);
                }
            | T_OP_LBRACE ConstInitVal ConstInitVals T_OP_RBRACE {
                    $3->prepend(std::move($2));
                    $3->range() = merge_range($1, $4);
                    $$ = std::move($3);
                }
            ;

ConstInitVals: T_COMMA ConstInitVal {
                    $$ = news<InitList>(merge_range($1, $2));
                    $$->prepend(std::move($2));
                }
             | T_COMMA ConstInitVal ConstInitVals {
                    $$ = std::move($3);
                    $$->range() = merge_range($1, $$);
                    $$->prepend(std::move($2));
                }
             ;

VarDecl: Type VarDef T_SEMICOLON {
                    $$ = news<VarDecl>(merge_range($1, $3), $1.get());
                    $$->append(std::move($2));
                    $1.reset();
                }
       | Type VarDef VarDecls T_SEMICOLON {
                    $$ = news<VarDecl>(merge_range($1, $4), $1.get());
                    $$->append(std::move($2));
                    $$->append(std::move($3));
                }
       ;

VarDecls: T_COMMA VarDef {
                    $$ = news<Declaration::VarListT>();
                    $$->push_front(std::move($2));
                }
        | T_COMMA VarDef VarDecls {
                    $$ = std::move($3);
                    $$->push_front(std::move($2));
                }
        ;

VarDef: T_IDENT {   $$ = news<Variable>(
                        $1->range(), false,
                        &Type::VoidType, $1->range().get_content()
                    );
                }
      | T_IDENT T_OP_ASSIGN InitVal {
                    $$ = news<Variable>(
                        merge_range($1, $3), false,
                        &Type::VoidType, $1->range().get_content(),
                        nullptr, std::move($3)
                    );
                }
      | T_IDENT ConstExps {
                    $$ = news<Variable>(
                        merge_range($1, $2), false,
                        &Type::VoidType, $1->range().get_content(),
                        std::move($2)
                    );
                }
      | T_IDENT ConstExps T_OP_ASSIGN InitVal {
                    $$ = news<Variable>(
                        merge_range($1, $4), false,
                        &Type::VoidType, $1->range().get_content(),
                        std::move($2), std::move($4)
                    );
                }
      ;


InitVal: Exp {      $$ = std::move($1); }
       | T_OP_LBRACE T_OP_RBRACE {
                    $$ = news<InitList>(merge_range($1, $2));
                }
       | T_OP_LBRACE InitVal T_OP_RBRACE {
                    auto mccv = news<InitList>(merge_range($1, $3));
                    mccv->append(std::move($2));
                    $$ = std::move(mccv);
                }
       | T_OP_LBRACE InitVal InitVals T_OP_RBRACE {
                    $3->range() = merge_range($1, $4);
                    $3->prepend(std::move($2));
                    $$ = std::move($3);
                }
       ;

InitVals: T_COMMA InitVal {
                    $$ = news<InitList>(merge_range($1, $2));
                    $$->prepend(std::move($2));
                }
        | T_COMMA InitVal InitVals {
                    $$ = std::move($3);
                    $$->range() = merge_range($1, $$);
                    $$->prepend(std::move($2));
                }
        ;

FuncDef: Type T_IDENT T_OP_LQUOTE T_OP_RQUOTE Block {
                    $$ = news<Function>(
                        merge_range($1, $5),
                        $1.get(), $2->range().get_content(),
                        newu<FuncParam>(merge_range($3, $4)),
                        std::move($5)
                    );
                    $1.reset();
                }
       | Type T_IDENT T_OP_LQUOTE FuncParams T_OP_RQUOTE Block {
                    $$ = news<Function>(
                        merge_range($1, $6),
                        $1.get(), $2->range().get_content(),
                        std::move($4),
                        std::move($6)
                    );
                    $1.reset();
                }
       ;

FuncParams: FuncParam {
                    $$ = newu<FuncParam>($1->range());
                    $$->append(std::move($1));
                }
          | FuncParams T_COMMA FuncParam {
                    $1->range() = merge_range($1, $3);
                    $$ = std::move($1);
                    $$->append(std::move($3));
                }
          ;

FuncParam: Type T_IDENT {
                    $$ = news<Variable>(
                        merge_range($1, $2), true,
                        $1.get(), $2->range().get_content());
                }
         | Type T_IDENT T_OP_LBRACKET T_OP_RBRACKET {
                    auto empty_array = news<ArrayInfo>(merge_range($3, $4));
                    auto empty_value = news<IntValue>(0);
                    empty_value->range() = merge_range($3, $4);
                    empty_array->append(std::move(empty_value));
                    $$ = news<Variable>(
                        merge_range($1, $4), true,
                        $1.get(), $2->range().get_content(),
                        std::move(empty_array)
                    );
                }
         | Type T_IDENT ArraySubscripts {
                    $$ = news<Variable>(
                        merge_range($1, $3), true,
                        $1.get(), $2->range().get_content(),
                        std::move($3)
                    );
                }
         | Type T_IDENT T_OP_LBRACKET T_OP_RBRACKET ArraySubscripts {
                    auto empty_value = news<IntValue>(0);
                    empty_value->range() = merge_range($3, $4);
                    $5->prepend(std::move(empty_value));
                    $$ = news<Variable>(
                        merge_range($1, $5), true,
                        $1.get(), $2->range().get_content(),
                        std::move($5)
                    );
                }
         ;

Block: T_OP_LBRACE BlockItems T_OP_RBRACE {
                    $$ = std::move($2);
                    $$->range() = merge_range($1, $3);
                }
     | T_OP_LBRACE T_OP_RBRACE {
                    $$ = newu<Block>(merge_range($1, $2));
                }
     ;


BlockItems: BlockItem {
                    $$ = newu<Block>($1->range());
                    $$->prepend(std::move($1));
                }
          | BlockItem BlockItems {
                    $2->range() = merge_range($1, $2);
                    $$ = std::move($2);
                    $$->prepend(std::move($1));
                }
          ;


BlockItem: Decl {   $$ = std::move($1); }
         | Stmt {   $$ = std::move($1); }
         ;

Stmt: LVal T_OP_ASSIGN Exp T_SEMICOLON {
                    auto assign_exp = news<AssignExpr>(
                        merge_range($1, $3),
                        std::move($3), std::move($1));
                    assign_exp->xoperator() = Expression::Operator::ASSIGN;
                    $$ = news<ExprStmt>(
                        merge_range($1, $4), std::move(assign_exp)
                    );
                }
    | T_SEMICOLON { $$ = news<EmptyStmt>($1->range()); }
    | Exp T_SEMICOLON {
                    $$ = news<ExprStmt>(
                        merge_range($1, $2), std::move($1)
                    );
                }
    | Block     {   $$ = std::move($1); }
    | T_WHILE T_OP_LQUOTE LOrExp T_OP_RQUOTE Stmt {
                    $$ = news<WhileStmt>(
                        merge_range($1, $5),
                        std::move($3), std::move($5));
                }
    | T_IF T_OP_LQUOTE LOrExp T_OP_RQUOTE Stmt {
                    $$ = news<IfStmt>(
                        merge_range($1, $5),
                        std::move($3), std::move($5));
                }
    | T_IF T_OP_LQUOTE LOrExp T_OP_RQUOTE Stmt T_ELSE Stmt {
                    $$ = news<IfStmt>(
                        merge_range($1, $7),
                        std::move($3),
                        std::move($5), std::move($7));
                }
    | T_BREAK T_SEMICOLON {
                    $$ = news<BreakStmt>(merge_range($1, $2)); }
    | T_CONTINUE T_SEMICOLON {
                    $$ = news<ContinueStmt>(merge_range($1, $2)); }
    | T_RETURN Exp T_SEMICOLON {
                    $$ = news<ReturnStmt>(
                        merge_range($1, $2), std::move($2));
                }
    | T_RETURN T_SEMICOLON {
                    $$ = news<ReturnStmt>(merge_range($1, $2));
                }
    ;

Exp: AddExp {   $$ = std::move($1); }
   ;

LVal: T_IDENT { $$ = news<Identifier>($1->range()); }
    | T_IDENT ArraySubscripts {
                    auto name = news<Identifier>($1->range());
                    auto mccv = news<IndexExpr>(
                        merge_range($1, $2),
                        std::move(name));
                    for (auto &exp: $2->array_info()) {
                        mccv->append(std::move(exp));
                    }
                    $$ = std::move(mccv);
                }
    ;

ArraySubscripts: T_OP_LBRACKET Exp T_OP_RBRACKET {
                    $$ = news<ArrayInfo>(merge_range($1, $3));
                    $$->prepend(std::move($2));
                }
               | T_OP_LBRACKET Exp T_OP_RBRACKET ArraySubscripts {
                    $4->range() = merge_range($1, $4);
                    $$ = std::move($4);
                    $$->prepend(std::move($2));
                }
               ;

PrimaryExp: T_OP_LQUOTE Exp T_OP_RQUOTE {
                    $$ = std::move($2);
                }
          | LVal {  $$ = std::move($1); }
          | T_INT_CONST {   $$ = news<IntValue>($1->range()); }
          | T_FLOAT_CONST { $$ = news<FloatValue>($1->range()); }
          ;

UnaryExp: PrimaryExp {  $$ = std::move($1); }
        | T_IDENT T_OP_LQUOTE T_OP_RQUOTE {
                    auto name = news<Identifier>($1->range());
                    $$ = news<CallExpr>(
                        merge_range($1, $3),
                        std::move(name),
                        news<CallParam>(merge_range($2, $3))
                    );
                }
        | T_IDENT T_OP_LQUOTE CallParams T_OP_RQUOTE {
                    auto name = news<Identifier>($1->range());
                    $3->range() = merge_range($2, $4);
                    $$ = news<CallExpr>(
                        merge_range($1, $4),
                        std::move(name),
                        std::move($3)
                    );
                }
        | T_OP_PLUS UnaryExp {
                    $$ = news<UnaryExpr>(
                        merge_range($1, $2),
                        Expression::Operator::PLUS,
                        std::move($2)
                    );
                }
        | T_OP_SUB UnaryExp {
                    $$ = news<UnaryExpr>(
                        merge_range($1, $2),
                        Expression::Operator::SUB,
                        std::move($2)
                    );
                }
        | T_REL_NOT UnaryExp {
                    $$ = news<UnaryExpr>(
                        merge_range($1, $2),
                        Expression::Operator::NOT,
                        std::move($2)
                    );
                }
        ;

CallParams: Exp {   $$ = news<CallParam>($1->range());
                    $$->append(std::move($1));
                }
          | Exp T_COMMA CallParams {
                    $3->range() = merge_range($1, $3);
                    $$ = std::move($3);
                    $$->prepend(std::move($1));
                }
          ;

MulExp: UnaryExp {    $$ = std::move($1); }
      | MulExp T_OP_STAR UnaryExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::STAR,
                        std::move($1), std::move($3)
                    );
                }
      | MulExp T_OP_SLASH UnaryExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::SLASH,
                        std::move($1), std::move($3)
                    );
                }
      | MulExp T_OP_PERCENT UnaryExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::PERCENT,
                        std::move($1), std::move($3)
                    );
                }
      ;

AddExp: MulExp {    $$ = std::move($1); }
      | AddExp T_OP_PLUS MulExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::PLUS,
                        std::move($1), std::move($3)
                    );
                }
      | AddExp T_OP_SUB MulExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::SUB,
                        std::move($1), std::move($3)
                    );
                }
      ;

RelExp: AddExp {     $$ = std::move($1); }
      | AddExp T_OP_LT RelExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::LT,
                        std::move($1), std::move($3)
                    );
                }
      | AddExp T_OP_GT RelExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::GT,
                        std::move($1), std::move($3)
                    );
                }
      | AddExp T_OP_LE RelExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::LE,
                        std::move($1), std::move($3)
                    );
                }
      | AddExp T_OP_GE RelExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::GE,
                        std::move($1), std::move($3)
                    );
                }
      ;

EqExp: RelExp {     $$ = std::move($1); }
     | RelExp T_OP_EQ EqExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::EQ,
                        std::move($1), std::move($3)
                    );
                }
     | RelExp T_OP_NE EqExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::NE,
                        std::move($1), std::move($3)
                    );
                }
     ;

LAndExp: EqExp {    $$ = std::move($1); }
       | EqExp T_REL_AND LAndExp {
                    $$ = news<BinaryExpr>(
                        merge_range($1, $3),
                        Expression::Operator::AND,
                        std::move($1), std::move($3)
                    );
                }
       ;

LOrExp: LAndExp {   $$ = std::move($1); }
      | LAndExp T_REL_OR LOrExp {
                    SourceRange range = $1->range();
                    range.end = $3->range().end;
                    $$ = std::make_shared<BinaryExpr>(
                        range, Expression::Operator::OR,
                        std::move($1), std::move($3));
                }
      ;

ConstExp: AddExp { $$ = std::move($1); }
        ;

Type: T_INT {   $$ = Type::OwnedIntType;   }
    | T_FLOAT { $$ = Type::OwnedFloatType; }
    | T_VOID {  $$ = Type::OwnedVoidType;  }
    ;
%%

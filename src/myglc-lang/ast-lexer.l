%top {
#include <cstdint>
#include <memory>
}

%option c++ yyclass="MYGL::Ast::Lexer" noyywrap
%option yylineno
%option outfile="ast-lexer.yy.cpp"

%{
/** @file ast-lexer.yy.cpp MYGL-C的词法分析器 */

#include "../include/myglc-lang/ast-code-context.hxx"
#include "../include/myglc-lang/ast-lexer.hxx"
#include "ast-parser.tab.hxx"
using namespace MYGL::Ast;
namespace MYGL::Ast {
    Token::Token(Lexer::UnownedPtrT lexer, TypeId type)
        : source_range(lexer->src_range), type(type) {}
} // namespace MYGL::Ast

// header-file="../include/myglc-lang/ast-lexer.yy.hxx"
using TOKEN_ABBR = MYGL::Ast::Lexer::Token;

#define TOKEN(type) (parser::token::type)
%}

MY_SPACE    [\n\t ]
MY_DIGIT    [0-9]
MY_DDIGIT   [1-9]
MY_XDIGIT   [0-9A-Fa-f]
MY_ODIGIT   [0-7]
MY_IDCHAR   [_0-9A-Za-z]
MY_IDENT    [_a-zA-Z][_0-9A-Za-z]*
MY_OCTNUM   0[0-7]*
MY_DECNUM   [1-9][0-9]*
MY_HEXNUM   0x[0-9A-Fa-f]+
MY_STRING_LITERAL   \"([^\"]|(\\\"))*\"
MY_BLOCKED_COMMENT  "/*"((\*+[^/*])|([^*]))*\**"*/"
MY_DECIMAL_FLOAT1   [0-9]+\.[0-9]+
MY_DECIMAL_FLOAT2   [0-9]+\.[0-9]+(E|e)(((\+|-)[0-9]+)|([0-9]+))
MY_DECIMAL_FLOAT3   \.[0-9]+
MY_DECIMAL_FLOAT4   (\.[0-9]+)(E|e)(((\+|-)[0-9]+)|([0-9]+))
MY_DECIMAL_FLOAT5   [0-9]+(E|e)(((\+|-)[0-9]+)|([0-9]+))
MY_HEX_FLOAT1   (0x[0-9A-Fa-f]+)\.([0-9A-Fa-f]+)
MY_HEX_FLOAT2   ((0x[0-9A-Fa-f]+)\.([0-9A-Fa-f]+))(P|p)(((\+|-)[0-9A-Fa-f]+)|([0-9A-Fa-f]+))
MY_HEX_FLOAT3   0x\.[0-9A-Fa-f]+
MY_HEX_FLOAT4   (0x\.[0-9A-Fa-f]+)(P|p)(((\+|-)[0-9A-Fa-f]+)|([0-9A-Fa-f]+))
MY_HEX_FLOAT5   (0x[0-9A-Fa-f]+)(P|p)(((\+|-)[0-9A-Fa-f]+)|([0-9A-Fa-f]+))

%%
{MY_SPACE}          { return TOKEN(T_SPACE); }
\/\/[^\n]*\n        { /*printf("SINGLE LINE COMMENT\n");*/ return TOKEN(T_COMMENT); }
{MY_BLOCKED_COMMENT} { /*printf("BLOCKED COMMENT\n");*/ return TOKEN(T_COMMENT); }
void                { return TOKEN(T_VOID); }
int                 { return TOKEN(T_INT); }
float               { return TOKEN(T_FLOAT); }
const               { return TOKEN(T_CONST); }
sizeof              { return TOKEN(T_OP_SIZEOF); }
if                  { return TOKEN(T_IF); }
else                { return TOKEN(T_ELSE); }
while               { return TOKEN(T_WHILE); }
for                 { return TOKEN(T_FOR); }
continue            { return TOKEN(T_CONTINUE); }
break               { return TOKEN(T_BREAK); }
switch              { return TOKEN(T_SWITCH); }
case                { return TOKEN(T_CASE); }
return              { return TOKEN(T_RETURN); }
{MY_DECIMAL_FLOAT1}|{MY_DECIMAL_FLOAT2}     { return TOKEN(T_FLOAT_CONST); }
{MY_DECIMAL_FLOAT3}|{MY_DECIMAL_FLOAT4}     { return TOKEN(T_FLOAT_CONST); }
{MY_DECIMAL_FLOAT5} { return TOKEN(T_FLOAT_CONST); }
{MY_HEX_FLOAT1}|{MY_HEX_FLOAT2}             { return TOKEN(T_FLOAT_CONST); }
{MY_HEX_FLOAT3}|{MY_HEX_FLOAT4}             { return TOKEN(T_FLOAT_CONST); }
{MY_HEX_FLOAT5}     { return TOKEN(T_FLOAT_CONST); }
{MY_HEXNUM}|{MY_OCTNUM}|{MY_DECNUM} { return TOKEN(T_INT_CONST); }
{MY_IDENT}          { return TOKEN(T_IDENT); }
"("                 { return TOKEN(T_OP_LQUOTE); }
")"                 { return TOKEN(T_OP_RQUOTE); }
"["                 { return TOKEN(T_OP_LBRACKET); }
"]"                 { return TOKEN(T_OP_RBRACKET); }
"{"                 { return TOKEN(T_OP_LBRACE); }
"}"                 { return TOKEN(T_OP_RBRACE); }
"=="                { return TOKEN(T_OP_EQ); }
"!="                { return TOKEN(T_OP_NE); }
">="                { return TOKEN(T_OP_GE); }
"<="                { return TOKEN(T_OP_LE); }
"&&"                { return TOKEN(T_REL_AND); }
"||"                { return TOKEN(T_REL_OR); }
"!"                 { return TOKEN(T_REL_NOT); }
">"                 { return TOKEN(T_OP_GT); }
"<"                 { return TOKEN(T_OP_LT); }
"+"                 { return TOKEN(T_OP_PLUS); }
"-"                 { return TOKEN(T_OP_SUB); }
"*"                 { return TOKEN(T_OP_STAR); }
"/"                 { return TOKEN(T_OP_SLASH); }
"%"                 { return TOKEN(T_OP_PERCENT); }
"="                 { return TOKEN(T_OP_ASSIGN); }
";"                 { return TOKEN(T_SEMICOLON); }
":"                 { return TOKEN(T_COLON); }
"?"                 { return TOKEN(T_QUESTION); }
","                 { return TOKEN(T_COMMA); }
"&"                 { // 整活开始
                        return TOKEN(T_OP_AND);
                    }
"|"                 { return TOKEN(T_OP_OR); }
"."                 { return TOKEN(T_OP_DOT); }
"->"                { return TOKEN(T_OP_SARROW); }
"=>"                { return TOKEN(T_OP_DARROW); /*整活结束*/ }
{MY_STRING_LITERAL} { return TOKEN(T_STRING_LITERAL); }
"\0"                { return TOKEN(T_EOI); }
<<EOF>>             { return TOKEN(T_EOI); }
.                   { return TOKEN(YYerror); }
%%

int yywrap() { return 1; }

Token::TypeId Lexer::lex()
{
    Token::TypeId ret;
    while (true) {
        ret = yylex();

        switch (ret) {
        case parser::token::T_SPACE:
            [[fallthrough]];
        case parser::token::T_COMMENT:
            src_buffer.append(yytext, yyleng);
            src_range.end.advance(yyleng);
            continue;
        case parser::token::T_EOI:
            src_range.begin = src_range.end;
            return Token::TypeId(parser::token::YYEOF);
        default:
            goto mygl_ast_lexer_lex_void_got_token;
        }
    }
mygl_ast_lexer_lex_void_got_token:
//    printf("%%ptr{yytext} = %p\n", yytext);
//    printf("%%str{yytext} = %s\n", yytext);
//    printf("%%int{yyleng} = %d\n", yyleng);
    src_range.begin = src_range.end;
    src_buffer.append(yytext);
    src_range.end.advance(yyleng);
    return ret;
}

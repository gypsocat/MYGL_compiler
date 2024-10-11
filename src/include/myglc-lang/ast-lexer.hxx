/** @file ast-lexer.hxx MYGL Lexer */
#pragma once


#ifndef __FLEX_LEXER_H
#include <FlexLexer.h>
#endif
#include <cstdint>
#include <memory>
#include "util.hxx"

namespace MYGL::Ast {
    class Lexer: public yyFlexLexer/*, public std::enable_shared_from_this<Lexer>*/ {
    public:
        using PtrT = std::shared_ptr<Lexer>;
        using UnownedPtrT = Lexer*;

        class Token {
        public:
            using PtrT = std::shared_ptr<Token>;
            using UnownedPtrT = std::weak_ptr<Token>;
            using TypeId = uint64_t;
        public:
            SourceRange source_range;
            TypeId      type;

            static PtrT from_lexer(Lexer::UnownedPtrT lexer, TypeId type) {
                return std::make_shared<Token>(lexer, type);
            }
            SourceRange const &range() { return source_range; }
            void println() {
                std::cout << std::format("Token{{type:{}, range{:36s}`{}`}}",
                    type, source_range.to_string(), source_range.get_content()
                ) << std::endl;
            }
            Token(Lexer::UnownedPtrT lexer, TypeId type);
        private:
        }; // class Token
    public:
        Lexer(std::string filename, std::istream *ihandle, std::string &src_buffer)
            : yyFlexLexer(ihandle, nullptr),
              filename(filename), src_buffer(src_buffer) {
            // yylex();
            src_range = {
                filename.c_str(),
                {&src_buffer, 0, 1, 0},
                {&src_buffer, 0, 1, 0}
            };
        }
        virtual ~Lexer() = default;

        int yylex() override;
        Token::TypeId lex();
        Token::TypeId lex(Token::PtrT &parser_lval)
        {
            Token::TypeId ret = lex();
            parser_lval = Token::from_lexer(this, ret);
            // parser_lval->println();
            return ret;
        }

        std::string   filename;
        std::string  &src_buffer;
        SourceRange  src_range;
        Token::TypeId cur_type;
    public:
    }; // class Lexer
} // namespace MYGL::Ast

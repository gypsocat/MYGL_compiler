#pragma once

/** @file ast-parser.hxx MYGL-C AST Parser
 * 把 yy::parser 简单封装了一下。
 * @class Parser 封装的Parser类
 * @class parser Bison生成的Parser类 */
#include "ast-parser.tab.hxx"
#include "ast-code-context.hxx"
#include "ast-exception.hxx"
#include <format>

namespace MYGL::Ast {
    class Parser: public parser {
    public:
        Parser(CodeContext &ctx)
            :parser(ctx, ctx.get_lexer()){}

        int try_parse() {
            int ret;
            try {
                ret = parse();
            } catch (NullException &e) {
                std::clog << std::format("MYGL::{} {}\n",
                    ErrorLevelGetString(e.error_level), e.what());
                if (e.error_level >= GlobalErrLimit)
                    exit(1);
            } catch (UndefinedException &e) {
                std::clog << std::format("MYGL::{} {}\n",
                    ErrorLevelGetString(e.error_level), e.what());
                if (e.error_level >= GlobalErrLimit)
                    exit(1);
            } catch (std::exception &e) {
                std::clog << e.what() << std::endl;
                return 1;
            }
            return ret;
        }
    }; // class Parser
} // namespace MYGL::Ast

#pragma once

#include "ast-node.hxx"
#include "ast-lexer.hxx"
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>

namespace MYGL::Ast {
    class CodeContext final/*: std::enable_shared_from_this<CodeContext>*/ {
    public:
        using InputHandle  = std::istream*;
        using OutputHandle = std::ostream*;
        using LexerT  = Lexer;
        using LexPtrT = Lexer::PtrT;
        using PtrT = std::shared_ptr<CodeContext>;
        using UnownedPtrT = std::weak_ptr<CodeContext>;
    public:
        static PtrT fromString(std::string const &str) {
            InputHandle input_handle = new std::stringstream(str);
            PtrT ret = std::make_shared<CodeContext>(input_handle, true);
            ret->_filename = "<anonymous>";
            return ret;
        }
        static PtrT fromFilename(std::string const &filename) {
            InputHandle input_handle = new std::fstream(filename, std::ios_base::in);
            std::istream *ihandle_real = dynamic_cast<std::istream*>(input_handle);
            PtrT ret = std::make_shared<CodeContext>(ihandle_real, true, filename);
            ret->_filename = filename;
            return ret;
        }
        static PtrT fromInputFile(std::istream &stream = std::cin) {
            return std::make_shared<CodeContext>(&stream);
        }
        ~CodeContext() {
            if (_input_owned)
                delete _input_handle;
        }
        LexerT *get_lexer() const { return _lexer.get(); }
        InputHandle get_input_handle() { return _input_handle; }
        std::string const &get_filename() const {
            return _filename;
        }
        std::string &filename() { return _filename; }
        void sync_lexer() {
            // _source_code = std::string_view{_lexer->YYText(), (size_t)_lexer->YYLeng()};
        }
        CompUnit::PtrT &root() {
            return _root;
        }
        CodeContext(std::istream *input_handle, bool input_owned = false, std::string const &filename = "<anonymous>")
            : _input_handle(input_handle),
              _input_owned(input_owned),
              _filename(filename),
              _lexer(std::make_shared<LexerT>(filename, input_handle, _source_code)),
              _source_code() {
        }
            
    private:
        CompUnit::PtrT _comp_unit = nullptr;
        LexPtrT        _lexer;
        std::string    _source_code;
        std::string    _filename;
        InputHandle    _input_handle;
        bool           _input_owned;
        CompUnit::PtrT _root;
    }; // class CodeContext
} // namespace MYGL::Ast

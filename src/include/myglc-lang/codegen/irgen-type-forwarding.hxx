#pragma once
#ifndef __MYGL_IRGEN_TYPE_FORWARDING_H__
#define __MYGL_IRGEN_TYPE_FORWARDING_H__

#include "mygl-ir/irbase-type.hxx"
#include "mygl-ir/irbase-type-context.hxx"
#include "myglc-lang/ast-node.hxx"
#include "myglc-lang/ast-code-context.hxx"
#include <cstddef>
#include <list>
#include <unordered_map>

namespace MYGL::IRGen {

using namespace MTB;

struct AstTypeHasher {
    size_t operator()(Ast::Type *type);
}; // struct AstTypeHasher

struct TypeMapper {
    using TypeContext = IRBase::TypeContext;
    using TypeMapT    = std::unordered_map<Ast::Type::PtrT, IRBase::Type*>;
    using ATypeListT  = std::list<Ast::Type*>;
public:
    TypeMapper() = default;
    TypeMapper(Ast::CodeContext *ast_ctx, TypeContext *ctx)
        : type_ctx(ctx), ast_ctx(ast_ctx){}

    /** @fn make_ir_type
     * @brief 把AST类型映射到IR类型. */
    IRBase::Type *make_ir_type(Ast::Type::PtrT const& ast_type);

    IRBase::ArrayType *make_ir_array_type(Ast::Type::UnownedPtrT    ast_type_base,
                                          Ast::ArrayInfo::UnownedPtrT ast_arrinfo);

    /** @fn make_function_type
     * @brief 把AST函数类型映射到IR函数类型. */
    IRBase::FunctionType *make_function_type(Ast::Type::PtrT const& return_type,
                                             ATypeListT const&    argument_list);
public:
    TypeContext *type_ctx;
    TypeMapT     type_map;

    Ast::CodeContext *ast_ctx;
}; // class TypeMapper

} // namespace MYGL::IRGen

#endif
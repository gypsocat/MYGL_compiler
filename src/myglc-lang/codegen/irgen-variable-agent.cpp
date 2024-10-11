#include "base/mtb-object.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "myglc-lang/ast-node-decl.hxx"
#include "myglc-lang/ast-node.hxx"
#include "myglc-lang/codegen/irgen-function-local.hxx"

namespace MYGL::IRGen::FunctionLocal {

using Ast::Variable;
using IR::Constant;

SymbolInfo &SymbolInfoMapper::
register_local_variable(Variable::UnownedPtrT   avar,
                        IR::Instruction *define_inst)
{
    auto [iter, newly_inserted] = symbol_map.insert_or_assign(
        avar,
        SymbolInfo{avar, define_inst}
    );
    return iter->second;
}

SymbolInfo &SymbolInfoMapper::
find_local_variable(Ast::Variable::UnownedPtrT avar)
{
    using namespace std::string_literals;
    using namespace std::string_view_literals;

    auto iter = symbol_map.find(avar);
    MTB_UNLIKELY_IF (iter == symbol_map.end()) {
        throw MTB::NullException {
            "SymbolInfoMapper("s
                .append(afunc->name())
                .append(")["sv)
                .append(avar->name())
                .append("]"sv),
            "",
            CURRENT_SRCLOC_F
        }; // throw MTB::NullException
    }
    return iter->second;
}

Constant *SymbolInfoMapper::
register_local_constant(AVarUPtr aconst, owned<Constant> data)
{
    if (data == nullptr)
        return nullptr;
    auto [iter, inserted] = const_map.insert_or_assign(aconst, std::move(data));
    return iter->second;
}

Constant *SymbolInfoMapper::
find_local_constant(AVarUPtr aconst)
{
    auto iter = const_map.find(aconst);
    if (iter == const_map.end())
        return nullptr;
    return iter->second;
}

} // namespace MYGL::IRGen::FunctionLocal
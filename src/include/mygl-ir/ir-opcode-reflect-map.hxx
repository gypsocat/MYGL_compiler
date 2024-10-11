#include "base/mtb-object.hxx"
#include "ir-instruction-base.hxx"
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

namespace MYGL::IR::OpCodeImpl {
    using OpCode     = Instruction::OpCode;
    using OpCodeEnum = OpCode::self_t;
    constexpr auto __opcode_end__ = OpCode::OPCODE_RESERVED_FOR_COUNTING - 1;

    template<typename ElemT,
             OpCodeEnum opcode_begin = OpCode::NONE,
             OpCodeEnum opcode_end   = __opcode_end__>
    MTB_REQUIRE((std::is_move_constructible_v<ElemT> &&
                 std::is_move_assignable_v<ElemT>))
    struct StaticReflectMap {
    private:
        static constexpr auto
        _array_length = opcode_end - opcode_begin + 1;

        static_assert(_array_length > 0, "opcode end should larger than"
                                         " or equal to opcode begin");
    public:
        struct InitItem {
            OpCodeEnum  opcode_enum;
            ElemT       element;
        }; // struct InitItem
        using InitListT = std::initializer_list<InitItem>;
    public:
        explicit constexpr
        StaticReflectMap(InitListT init_list) noexcept {
            for (bool &i: _has_item) i = false;

            for (InitItem const& i: init_list) {
                /* 由于 constexpr 不得随意抛异常, 遇到超过界限的值, 直接忽略. */
                if (i.opcode_enum < opcode_begin ||
                    i.opcode_enum > opcode_end)
                    continue;

                unsigned offset = i.opcode_enum - opcode_begin;
                _data[offset]     = std::move(i);
                _has_item[offset] = true;
            }
        }

        size_t size() const noexcept { return _array_length; }

        bool contains(OpCodeEnum opcode) const noexcept {
            return opcode >= opcode_begin &&
                   opcode <= opcode_end   &&
                   _has_item[opcode - opcode_begin];
        }
        ElemT &operator[](OpCodeEnum opcode) noexcept {
            return _data[opcode - opcode_begin].element;
        }
        ElemT const& operator[](OpCodeEnum opcode) const noexcept {
            return _data[opcode - opcode_begin].element;
        }
    public:
        InitItem _data[_array_length];
        bool     _has_item[_array_length];
    }; // struct StaticReflectMap
} // namespace MYGL::IR:OpCodeImpl

namespace MYGL::IR {
    template<typename ElemT,
             OpCodeImpl::OpCodeEnum opcode_begin = Instruction::OpCode::NONE,
             OpCodeImpl::OpCodeEnum opcode_end   = OpCodeImpl::__opcode_end__>
    using OpCodeReflectMap = OpCodeImpl::StaticReflectMap<ElemT, opcode_begin, opcode_end>;
} // namespace MYGL::IR

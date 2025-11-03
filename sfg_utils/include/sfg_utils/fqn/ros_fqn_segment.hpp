#pragma once

#include <cctype>
#include <magic_enum_flags.hpp>

namespace sfg_utils::fqn
{
    enum class RosFqnSegment : std::uint64_t
    {

        Scope = 1 << 0,
        Agent = 1 << 1,
        Component = 1 << 2,
        Stream = 1 << 3,
        Resource = 1 << 4,

        None = 0,
        All = ~None
    };
}

template <>
struct magic_enum::customize::enum_range<sfg_utils::fqn::RosFqnSegment>
{
    static constexpr auto is_flags = true;
};
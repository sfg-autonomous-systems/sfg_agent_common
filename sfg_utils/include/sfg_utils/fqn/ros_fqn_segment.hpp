#pragma once

#include <cctype>
#include <magic_enum_flags.hpp>

namespace sfg_utils::fqn
{
    enum class RosFqnSegment : std::uint64_t
    {
        None = 0,
        Scope = 1 << 0,
        Agent = 1 << 1,
        Component = 1 << 2,
        Stream = 1 << 3,
        Resource = 1 << 4,
        All = Scope | Agent | Component | Stream | Resource
    };
}

template <>
struct magic_enum::customize::enum_range<sfg_utils::fqn::RosFqnSegment>
{
    static constexpr bool is_flags = true;
};
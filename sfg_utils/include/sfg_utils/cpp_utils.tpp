#pragma once

#include "sfg_utils/cpp_utils.hpp"

#include <string_view>

namespace sfg_utils
{
    template <typename T>
    std::string get_type_name()
    {
        // Only works correctly with GCC.
        constexpr std::string_view prefix = "with T = ";
        constexpr std::string_view suffix = ";";
        constexpr std::string_view function = __PRETTY_FUNCTION__;
        const auto start = function.find(prefix) + prefix.size();
        const auto end = function.find(suffix, start);
        return std::string(function.substr(start, end - start));
    }
}
#pragma once

#include "sfg_utils/cpp_utils.hpp"

#include <string_view>
#include <typeinfo>

namespace sfg_utils
{
    template <typename T>
    std::string get_type_name()
    {
#if defined(__clang__)
        constexpr std::string_view prefix = "[T = ";
        constexpr std::string_view suffix = "]";
        constexpr std::string_view function = __PRETTY_FUNCTION__;
        const auto start = function.find(prefix) + prefix.size();
        const auto end = function.find(suffix, start);
        return std::string(function.substr(start, end - start));
#elif defined(__GNUC__)
        constexpr std::string_view prefix = "with T = ";
        constexpr std::string_view suffix = ";";
        constexpr std::string_view function = __PRETTY_FUNCTION__;
        const auto start = function.find(prefix) + prefix.size();
        const auto end = function.find(suffix, start);
        return std::string(function.substr(start, end - start));
#elif defined(_MSC_VER)
        constexpr std::string_view prefix = "get_type_name<";
        constexpr std::string_view suffix = ">(void)";
        constexpr std::string_view function = __FUNCSIG__;
        const auto start = function.find(prefix) + prefix.size();
        const auto end = function.find(suffix, start);
        return std::string(function.substr(start, end - start));
#else
        return typeid(T).name();
#endif
    }
}
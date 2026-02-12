#pragma once

#include <string>
#include <vector>

namespace sfg_utils::cpp_utils
{
    template <typename Type>
    std::string get_type();
    template <typename Type>
    std::string get_type(const Type *instance);

    template <typename TEnum>
    std::string enum_value_to_snake_case_string(TEnum value);

    std::string escape_regex(const std::string &regex);
    std::vector<std::string> split_string(const std::string &str, std::string_view delimiter);
}

#include "sfg_utils/cpp_utils.tpp"
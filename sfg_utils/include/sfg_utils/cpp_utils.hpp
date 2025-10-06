#pragma once

#include <string>
#include <vector>

namespace sfg_utils::cpp_utils
{
    template <typename T>
    std::string get_type_name();

    template <typename TEnum>
    std::string enum_value_to_snake_case_string(TEnum value);

    std::string escape_regex(const std::string &regex);
    std::vector<std::string> split_string(const std::string &str, std::string_view delimiter);
}

#include "sfg_utils/cpp_utils.tpp"
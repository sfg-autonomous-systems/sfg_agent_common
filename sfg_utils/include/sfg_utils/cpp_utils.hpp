#pragma once

#include <string>
#include <vector>

namespace sfg_utils::cpp_utils
{
    template <typename T>
    std::string get_type_name();

    std::string escape_regex(const std::string &regex);
    std::vector<std::string> split_string(const std::string &str, char delimiter);
}

#include "sfg_utils/cpp_utils.tpp"
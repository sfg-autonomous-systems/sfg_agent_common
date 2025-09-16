#pragma once

#include <string>

namespace sfg_utils
{
    template <typename T>
    std::string get_type_name();

    std::string escape_regex(const std::string &regex);
}

#include "sfg_utils/cpp_utils.tpp"
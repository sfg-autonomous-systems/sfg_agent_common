#include "sfg_utils/cpp_utils.hpp"

#include <regex>

namespace sfg_utils
{
    // Implementation based on https://stackoverflow.com/a/40195721.
    std::string escape_regex(const std::string &regex)
    {
        std::regex special_characters{R"([-[\]{}()*+?.,\^$|#\s])"};
        return std::regex_replace(regex, special_characters, R"(\$&)");
    }
}
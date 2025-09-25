#include "sfg_utils/cpp_utils.hpp"

#include <regex>

namespace sfg_utils::cpp_utils
{
    // Implementation based on https://stackoverflow.com/a/40195721.
    std::string escape_regex(const std::string &regex)
    {
        std::regex special_characters{R"([-[\]{}()*+?.,\^$|#\s])"};
        return std::regex_replace(regex, special_characters, R"(\$&)");
    }

    std::vector<std::string> split_string(const std::string &str, std::string_view delimiter)
    {
        std::vector<std::string> tokens;
        std::string_view view(str);
        size_t start = 0;
        size_t end;

        while ((end = view.find(delimiter, start)) != std::string_view::npos)
        {
            tokens.emplace_back(view.substr(start, end - start));
            start = end + delimiter.length();
        }
        tokens.emplace_back(view.substr(start));

        return tokens;
    }
}
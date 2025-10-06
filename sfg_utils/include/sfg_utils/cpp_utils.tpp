#include "sfg_utils/cpp_utils.hpp"

#include <magic_enum.hpp>
#include <sstream>
#include <string_view>

namespace sfg_utils::cpp_utils
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

    template <typename TEnum>
    std::string enum_value_to_snake_case_string(TEnum value)
    {
        auto enum_name = magic_enum::enum_name(value);
        std::stringstream snake_case_stream = std::stringstream();
        snake_case_stream << static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(enum_name[0])));

        for (size_t index = 1; index < enum_name.size(); index++)
        {
            if (std::islower(enum_name[index]))
            {
                snake_case_stream << static_cast<unsigned char>(enum_name[index]);
                continue;
            }

            if (index < enum_name.size() - 1 && std::islower(enum_name[index + 1]))
            {
                snake_case_stream << "_";
            }
            else if (index == enum_name.size() - 1 && std::islower(enum_name[index - 1]))
            {
                snake_case_stream << "_";
            }
            snake_case_stream << static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(enum_name[index])));
        }
        return snake_case_stream.str();
    }
}
#include "sfg_utils/cpp_utils.hpp"

#include <cxxabi.h>
#include <magic_enum.hpp>
#include <memory>
#include <sstream>
#include <string_view>

namespace sfg_utils::cpp_utils
{
    template <typename Type>
    std::string get_type()
    {
        std::int32_t status = -4;
        std::unique_ptr<char, void (*)(void *)> result{abi::__cxa_demangle(typeid(Type).name(), nullptr, nullptr, &status), std::free};
        return (status == 0) ? result.get() : typeid(Type).name();
    }

    template <typename Type>
    std::string get_type(const Type *instance)
    {
        if (!instance)
        {
            return "nullptr";
        }

        std::int32_t status = -4;
        std::unique_ptr<char, void (*)(void *)> result{abi::__cxa_demangle(typeid(*instance).name(), nullptr, nullptr, &status), std::free};
        return (status == 0) ? result.get() : typeid(*instance).name();
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
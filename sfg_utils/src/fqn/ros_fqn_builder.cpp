#include "sfg_utils/fqn/ros_fqn_builder.hpp"

#include <magic_enum.hpp>
#include <rmw/validate_full_topic_name.h>
#include <sstream>
#include <stdexcept>

#include "sfg_utils/agent_utils.hpp"

#define STRINGIFY(x) #x

namespace
{
    template <typename TEnum>
    std::string enum_to_lower_string(TEnum e)
    {
        // magic_enum::enum_name returns a std::string_view, so convert to std::string
        std::string name(magic_enum::enum_name(e));
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        return name;
    }
}

namespace sfg_utils::fqn
{
    RosFQNBuilder &RosFQNBuilder::scope(Scope scope)
    {
        m_scope = enum_to_lower_string(scope);
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::agent(const std::string &name)
    {
        if (!m_scope.has_value())
        {
            throw std::logic_error("Setting an agent requires a scope to be set.");
        }

        m_agent = sanitize_agent_name(name.empty() ? get_agent_name() : name);
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::component(Component component, const std::string &name)
    {
        if (!m_agent.has_value())
        {
            throw std::logic_error("Setting a component requires an agent to be set.");
        }

        if (name.empty())
        {
            throw std::invalid_argument("Parameter " STRINGIFY(name) " is required for a component.");
        }

        if (component == Component::Custom)
        {
            m_component = name;
            return *this;
        }

        m_component = enum_to_lower_string(component) + "_" + name;
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::stream(Stream stream, const std::string &name)
    {
        if (!m_component.has_value())
        {
            throw std::logic_error("Setting a stream requires a component to be set.");
        }

        if (stream == Stream::Custom)
        {
            if (name.empty())
            {
                throw std::invalid_argument("Parameter " STRINGIFY(name) " is required for a custom stream.");
            }

            m_stream = name;
            return *this;
        }

        m_stream = enum_to_lower_string(stream);
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::resource(Resource resource, const std::string &name)
    {
        if (!m_scope.has_value())
        {
            throw std::logic_error("Setting a resource requires a scope to be set.");
        }

        if (resource == Resource::Custom)
        {
            if (name.empty())
            {
                throw std::invalid_argument("Parameter " STRINGIFY(name) " is required for a custom resource.");
            }

            m_resource = name;
            return *this;
        }

        m_resource = enum_to_lower_string(resource);
        return *this;
    }

    [[nodiscard]] std::string RosFQNBuilder::build(bool only_namespace) const
    {
        std::stringstream result_stream;

        if (!m_scope.has_value())
        {
            throw std::logic_error("FQN build failed: Scope was not set.");
        }

        result_stream << "/" << *m_scope;

        if (m_agent.has_value())
        {
            result_stream << "/" << *m_agent;
        }

        if (only_namespace)
        {
            return result_stream.str();
        }

        if (m_component.has_value())
        {
            result_stream << "/" << *m_component;
        }

        if (m_stream.has_value())
        {
            result_stream << "/" << *m_stream;
        }

        if (!m_resource.has_value())
        {
            throw std::logic_error("FQN build failed: Resource was not set.");
        }

        result_stream << "/" << *m_resource;

        auto result_string = result_stream.str();
        int validation_result = RMW_TOPIC_VALID;
        size_t invalid_index = -1;

        if (rmw_validate_full_topic_name(result_string.c_str(), &validation_result, &invalid_index) != RMW_RET_OK || validation_result != RMW_TOPIC_VALID)
        {
            std::stringstream error_stream;
            error_stream << "FQN build failed: Name '" << result_string << "' is invalid. Error at character index " << invalid_index << ". ";

            switch (validation_result)
            {
            case RMW_TOPIC_INVALID_IS_EMPTY_STRING:
                error_stream << "Name is empty.";
                break;
            case RMW_TOPIC_INVALID_NOT_ABSOLUTE:
                error_stream << "Name is not absolute.";
                break;
            case RMW_TOPIC_INVALID_ENDS_WITH_FORWARD_SLASH:
                error_stream << "Name ends with a forward slash.";
                break;
            case RMW_TOPIC_INVALID_CONTAINS_UNALLOWED_CHARACTERS:
                error_stream << "Name contains unallowed characters.";
                break;
            case RMW_TOPIC_INVALID_CONTAINS_REPEATED_FORWARD_SLASH:
                error_stream << "Name contains repeated forward slashes.";
                break;
            case RMW_TOPIC_INVALID_NAME_TOKEN_STARTS_WITH_NUMBER:
                error_stream << "Name token starts with a number.";
                break;
            case RMW_TOPIC_INVALID_TOO_LONG:
                error_stream << "Name is too long.";
                break;
            }

            throw std::invalid_argument(error_stream.str());
        }

        return result_string;
    }
}

#pragma once

#include "sfg_utils/fqn/scope.hpp"
#include "sfg_utils/fqn/endpoint.hpp"
#include "sfg_utils/fqn/component.hpp"

#include <string>
#include <optional>

namespace sfg_utils::fqn
{
    class RosFQNBuilder
    {
    public:
        RosFQNBuilder &scope(Scope scope);
        RosFQNBuilder &agent(const std::string &name = "");
        RosFQNBuilder &component(Component type, const std::string &name);
        RosFQNBuilder &endpoint(Endpoint type, const std::string &stream = "", const std::string &resource = "");
        [[nodiscard]] std::string build(bool only_namespace = false) const;

    private:
        std::optional<std::string> m_scope;
        std::optional<std::string> m_agent;
        std::optional<std::string> m_component;
        std::optional<std::string> m_stream;
        std::optional<std::string> m_resource;
    };
}
#include "sfg_utils/fqn/ros_fqn_builder.hpp"

#include <magic_enum.hpp>
#include <magic_enum_flags.hpp>
#include <rmw/validate_full_topic_name.h>
#include <sstream>
#include <stdexcept>

#include "sfg_utils/agent_utils.hpp"

#define STRINGIFY(x) #x

using namespace magic_enum::bitwise_operators;

namespace
{
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

namespace sfg_utils::fqn
{
    size_t RosFQNBuilder::s_get_index(RosFQNSegment segment)
    {
        if (__builtin_popcount(std::underlying_type_t<RosFQNSegment>(segment)) != 1)
        {
            throw std::invalid_argument("Invalid segment type.");
        }

        // RosFQNSegment is a flag enum, so we use __builtin_ctzll to get the index required to access
        // the corresponding element in s_segment_rules and m_segment_values.
        return __builtin_ctzll(std::underlying_type_t<RosFQNSegment>(segment));
    }

    const std::array<RosFQNBuilder::RosFQNSegmentRule, magic_enum::enum_count<RosFQNSegment>()> RosFQNBuilder::s_segment_rules = {
        RosFQNSegmentRule{
            true,
            RosFQNSegment::Scope,
            RosFQNSegment::None,
            Resource::AgentHeartbeat | Resource::Custom},
        RosFQNSegmentRule{
            false,
            RosFQNSegment::Agent,
            RosFQNSegment::Scope,
            Resource::RobotDescription | Resource::Custom},
        RosFQNSegmentRule{
            false,
            RosFQNSegment::Component,
            RosFQNSegment::Agent,
            Resource::ImageRaw | Resource::ImageCompressed | Resource::CameraInfo | Resource::PointCloud | Resource::IMU | Resource::Custom},
        RosFQNSegmentRule{
            false,
            RosFQNSegment::Stream,
            RosFQNSegment::Component,
            Resource::ImageRaw | Resource::ImageCompressed | Resource::CameraInfo | Resource::PointCloud | Resource::IMU | Resource::Custom},
        RosFQNSegmentRule{
            true,
            RosFQNSegment::Resource,
            RosFQNSegment::Scope,
            Resource::None}};

    RosFQNBuilder::RosFQNBuilder() : m_set_segments(RosFQNSegment::None),
                                     m_segment_values({}),
                                     m_resource(Resource::None) {}

    RosFQNBuilder &RosFQNBuilder::scope(Scope scope)
    {
        m_segment_values[s_get_index(RosFQNSegment::Scope)] = enum_value_to_snake_case_string(scope);
        m_set_segments |= RosFQNSegment::Scope;
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::agent(const std::string &name)
    {
        m_segment_values[s_get_index(RosFQNSegment::Agent)] = sanitize_agent_name(name.empty() ? get_agent_name() : name);
        m_set_segments |= RosFQNSegment::Agent;
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::component(Component component, const std::string &name)
    {
        set_segment(RosFQNSegment::Component, component, name);
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::stream(Stream stream, const std::string &name)
    {
        set_segment(RosFQNSegment::Stream, stream, name);
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::resource(Resource resource, const std::string &name)
    {
        // Because Resource is a flag enum, we need to ensure that only one bit is set.
        // __builtin_popcount is used to count the number of bits set in the underlying type.
        if (__builtin_popcount(std::underlying_type_t<Resource>(resource)) != 1)
        {
            throw std::invalid_argument("Invalid resource type.");
        }

        set_segment(RosFQNSegment::Resource, resource, name);
        m_resource = resource;
        return *this;
    }

    [[nodiscard]] std::string RosFQNBuilder::build(RosFQNSegment begin, RosFQNSegment end) const
    {
        // Because RosFQNSegment is a flag enum, we need to ensure that both begin and end are referring to single segments...
        if (__builtin_popcount(std::underlying_type_t<RosFQNSegment>(begin)) != 1 || __builtin_popcount(std::underlying_type_t<RosFQNSegment>(end)) != 1 ||
            // or if the begin segment is after the end segment.
            begin > end)
        {
            throw std::invalid_argument("Invalid ROS FQN segment range.");
        }

        bool is_absolute_name = begin == RosFQNSegment::Scope;
        // Prepend a slash if the name is absolute.
        auto stream = std::stringstream() << (is_absolute_name ? "/" : "");

        // Depending on the segment range, we need to selectively omit/include certain segment validation rules.
        // We do this by creating a mask that includes only the segments in the specified range.
        auto segment_mask = RosFQNSegment::None;

        for (auto segment : magic_enum::enum_values<RosFQNSegment>())
        {
            if (segment >= begin && segment <= end)
            {
                segment_mask |= segment;
            }
        }

        // Since the preceeding segment determines which resources are allowed,
        // we need to keep track of the allowed resources as we build the FQN.
        auto allowed_resources = s_segment_rules[s_get_index(begin)].m_allowed_resources;
        auto last_segment = RosFQNSegment::None;

        for (size_t index = s_get_index(begin); index <= s_get_index(end); index++)
        {
            const auto &segment_rule = s_segment_rules[index];
            const auto &segment_value = m_segment_values[index];

            if (segment_value.has_value())
            {
                auto masked_required_segments = segment_rule.m_required_segments & segment_mask;

                // Check that the required segments for the current segment are set while taking the segment mask into account.
                if ((m_set_segments & masked_required_segments) != masked_required_segments)
                {
                    throw std::runtime_error(
                        "ROS FQN build failed: Segment '" + std::string(magic_enum::enum_flags_name(segment_rule.m_segment)) +
                        "' requires segments '" + std::string(magic_enum::enum_flags_name(masked_required_segments)) +
                        "' to be set.");
                }

                // We can only perform Resource type checks if the beginning of the build range is not of RosFQNSegment type 'Resource'.
                if (begin != RosFQNSegment::Resource && segment_rule.m_segment == RosFQNSegment::Resource && (allowed_resources & m_resource) != m_resource)
                {
                    throw std::runtime_error(
                        "ROS FQN build failed: Resource '" + std::string(magic_enum::enum_flags_name(m_resource)) +
                        "' is not allowed for the segment '" + std::string(magic_enum::enum_flags_name(last_segment)) + "'. " +
                        "Allowed resources are '" + std::string(magic_enum::enum_flags_name(allowed_resources)) + "'.");
                }

                stream << *segment_value << s_segment_delimiter;
                allowed_resources = segment_rule.m_allowed_resources;
                last_segment = segment_rule.m_segment;
            }
            else if (segment_rule.m_is_required || begin == segment_rule.m_segment || end == segment_rule.m_segment)
            {
                throw std::runtime_error(
                    "ROS FQN build failed: Segment '" + std::string(magic_enum::enum_flags_name(segment_rule.m_segment)) + "' is required but not set.");
            }
        }

        // Remove the trailing slash.
        auto name = stream.str().erase(stream.str().size() - s_segment_delimiter.size());
        int validation_result = RMW_TOPIC_VALID;
        size_t invalid_index = -1;

        if (rmw_validate_full_topic_name(name.c_str(), &validation_result, &invalid_index) != RMW_RET_OK)
        {
            throw std::runtime_error("ROS FQN build failed: An RMW error occurred during validation.");
        }

        // Do not consider RMW_TOPIC_INVALID_NOT_ABSOLUTE to be an error if the name is not absolute.
        if (validation_result != RMW_TOPIC_VALID && (is_absolute_name || validation_result != RMW_TOPIC_INVALID_NOT_ABSOLUTE))
        {
            std::stringstream error_stream;
            error_stream << "ROS FQN build failed: Name '" << name << "' is invalid. ";
            error_stream << "Error at character index " << invalid_index << ": " << rmw_full_topic_name_validation_result_string(validation_result);
            throw std::runtime_error(error_stream.str());
        }

        return name;
    }

    [[nodiscard]] std::string RosFQNBuilder::build(RosFQNSegment segment) const
    {
        return build(segment, segment);
    }

    [[nodiscard]] std::string RosFQNBuilder::build() const
    {
        return build(RosFQNSegment::Scope, RosFQNSegment::Resource);
    }

    RosFQNBuilder &RosFQNBuilder::reset()
    {
        return reset(RosFQNSegment::All);
    }

    RosFQNBuilder &RosFQNBuilder::reset(RosFQNSegment segments)
    {
        for (size_t index = 0; index < m_segment_values.size(); index++)
        {
            if ((segments & s_segment_rules[index].m_segment) == s_segment_rules[index].m_segment)
            {
                m_set_segments &= ~s_segment_rules[index].m_segment;
                m_segment_values[index].reset();
            }
        }

        if ((segments & RosFQNSegment::Resource) == RosFQNSegment::Resource)
        {
            m_resource = Resource::None;
        }
        return *this;
    }

    template <typename TEnum>
    void RosFQNBuilder::set_segment(RosFQNSegment segment, TEnum value, const std::string &name)
    {
        if (value != TEnum::Custom)
        {
            m_segment_values[s_get_index(segment)] = enum_value_to_snake_case_string(value) + (name.empty() ? "" : "_" + name);
            m_set_segments |= segment;
            return;
        }

        if (name.empty())
        {
            throw std::invalid_argument("Parameter " STRINGIFY(name) " is required.");
        }

        m_segment_values[s_get_index(segment)] = name;
        m_set_segments |= segment;
    }
}

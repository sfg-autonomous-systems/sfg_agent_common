#include "sfg_utils/fqn/ros_fqn_builder.hpp"

#include <magic_enum_flags.hpp>
#include <rmw/validate_full_topic_name.h>
#include <stdexcept>

#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/cpp_utils.hpp"

#define STRINGIFY(x) #x

using namespace magic_enum::bitwise_operators;

namespace sfg_utils::fqn
{
    size_t RosFqnBuilder::s_get_index(RosFqnSegment segment)
    {
        if (__builtin_popcount(std::underlying_type_t<RosFqnSegment>(segment)) != 1)
        {
            throw std::invalid_argument("Invalid segment type.");
        }
        // RosFqnSegment is a flag enum, so we use __builtin_ctzll to get the index required to access
        // the corresponding element in s_segment_rules and m_segment_values.
        return __builtin_ctzll(std::underlying_type_t<RosFqnSegment>(segment));
    }

    const std::array<RosFqnBuilder::RosFqnSegmentRule, magic_enum::enum_count<RosFqnSegment>()> RosFqnBuilder::s_segment_rules = {
        RosFqnSegmentRule{
            true,
            RosFqnSegment::Scope,
            RosFqnSegment::None,
            Resource::AgentHeartbeat | Resource::Custom},
        RosFqnSegmentRule{
            false,
            RosFqnSegment::Agent,
            RosFqnSegment::Scope,
            Resource::RobotDescription | Resource::Custom},
        RosFqnSegmentRule{
            false,
            RosFqnSegment::Component,
            RosFqnSegment::Agent,
            Resource::CameraInfo | Resource::CmdVel | Resource::ImageCompressed | Resource::ImageRaw | Resource::Imu | Resource::JointStates | Resource::PointCloud | Resource::Custom},
        RosFqnSegmentRule{
            false,
            RosFqnSegment::Stream,
            RosFqnSegment::Component,
            Resource::CameraInfo | Resource::CmdVel | Resource::ImageCompressed | Resource::ImageRaw | Resource::Imu | Resource::JointStates | Resource::PointCloud | Resource::Custom},
        RosFqnSegmentRule{
            true,
            RosFqnSegment::Resource,
            RosFqnSegment::Scope,
            Resource::None}};

    RosFqnBuilder::RosFqnBuilder()
        : m_set_segments(RosFqnSegment::None),
          m_segment_values({}),
          m_resource(Resource::None) {}

    RosFqnBuilder &RosFqnBuilder::scope(Scope scope)
    {
        m_segment_values[s_get_index(RosFqnSegment::Scope)] = sfg_utils::cpp_utils::enum_value_to_snake_case_string(scope);
        m_set_segments |= RosFqnSegment::Scope;
        return *this;
    }

    RosFqnBuilder &RosFqnBuilder::agent(const std::string &name)
    {
        m_segment_values[s_get_index(RosFqnSegment::Agent)] = agent_utils::sanitize_agent_name(name.empty() ? agent_utils::get_agent_name() : name);
        m_set_segments |= RosFqnSegment::Agent;
        return *this;
    }

    RosFqnBuilder &RosFqnBuilder::component(Component component, const std::string &name)
    {
        set_segment(RosFqnSegment::Component, component, name);
        return *this;
    }

    RosFqnBuilder &RosFqnBuilder::stream(Stream stream, const std::string &name)
    {
        set_segment(RosFqnSegment::Stream, stream, name);
        return *this;
    }

    RosFqnBuilder &RosFqnBuilder::resource(Resource resource, const std::string &name)
    {
        // Because Resource is a flag enum, we need to ensure that only one bit is set.
        // __builtin_popcount is used to count the number of bits set in the underlying type.
        if (__builtin_popcount(std::underlying_type_t<Resource>(resource)) != 1)
        {
            throw std::invalid_argument("Invalid resource type.");
        }

        set_segment(RosFqnSegment::Resource, resource, name);
        m_resource = resource;
        return *this;
    }

    [[nodiscard]] std::string RosFqnBuilder::build(RosFqnSegment begin, RosFqnSegment end) const
    {
        // Because RosFqnSegment is a flag enum, we need to ensure that both begin and end are referring to single segments...
        if (__builtin_popcount(std::underlying_type_t<RosFqnSegment>(begin)) != 1 || __builtin_popcount(std::underlying_type_t<RosFqnSegment>(end)) != 1 ||
            // or if the begin segment is after the end segment.
            begin > end)
        {
            throw std::invalid_argument("Invalid ROS FQN segment range.");
        }

        bool is_absolute_name = begin == RosFqnSegment::Scope;
        // Prepend a slash if the name is absolute.
        auto stream = std::stringstream() << (is_absolute_name ? "/" : "");

        // Depending on the segment range, we need to selectively omit/include certain segment validation rules.
        // We do this by creating a mask that includes only the segments in the specified range.
        auto segment_mask = RosFqnSegment::None;

        for (auto segment : magic_enum::enum_values<RosFqnSegment>())
        {
            if (segment >= begin && segment <= end)
            {
                segment_mask |= segment;
            }
        }

        // Since the preceeding segment determines which resources are allowed,
        // we need to keep track of the allowed resources as we build the FQN.
        auto allowed_resources = s_segment_rules[s_get_index(begin)].m_allowed_resources;
        auto last_segment = RosFqnSegment::None;

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

                // We can only perform Resource type checks if the beginning of the build range is not of RosFqnSegment type 'Resource'.
                if (begin != RosFqnSegment::Resource && segment_rule.m_segment == RosFqnSegment::Resource && (allowed_resources & m_resource) != m_resource)
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

    [[nodiscard]] std::string RosFqnBuilder::build(RosFqnSegment segment) const
    {
        return build(segment, segment);
    }

    [[nodiscard]] std::string RosFqnBuilder::build() const
    {
        return build(RosFqnSegment::Scope, RosFqnSegment::Resource);
    }

    RosFqnBuilder &RosFqnBuilder::reset()
    {
        return reset(RosFqnSegment::All);
    }

    RosFqnBuilder &RosFqnBuilder::reset(RosFqnSegment segments)
    {
        for (size_t index = 0; index < m_segment_values.size(); index++)
        {
            if ((segments & s_segment_rules[index].m_segment) == s_segment_rules[index].m_segment)
            {
                m_set_segments &= ~s_segment_rules[index].m_segment;
                m_segment_values[index].reset();
            }
        }

        if ((segments & RosFqnSegment::Resource) == RosFqnSegment::Resource)
        {
            m_resource = Resource::None;
        }
        return *this;
    }

    template <typename TEnum>
    void RosFqnBuilder::set_segment(RosFqnSegment segment, TEnum value, const std::string &name)
    {
        if (value != TEnum::Custom)
        {
            m_segment_values[s_get_index(segment)] = sfg_utils::cpp_utils::enum_value_to_snake_case_string(value) + (name.empty() ? "" : "_" + name);
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

#undef STRINGIFY
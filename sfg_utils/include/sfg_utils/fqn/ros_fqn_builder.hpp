#pragma once

#include "sfg_utils/fqn/component.hpp"
#include "sfg_utils/fqn/resource.hpp"
#include "sfg_utils/fqn/ros_fqn_segment.hpp"
#include "sfg_utils/fqn/scope.hpp"
#include "sfg_utils/fqn/stream.hpp"

#include <optional>
#include <string>

namespace sfg_utils::fqn
{
    class RosFqnBuilder
    {
    private:
        static constexpr std::string_view s_segment_delimiter = "/";

    public:
        RosFqnBuilder();

        RosFqnBuilder scope(Scope scope) const;
        RosFqnBuilder agent(const std::string &name = "") const;
        RosFqnBuilder component(Component component, const std::string &name = "") const;
        RosFqnBuilder stream(Stream stream, const std::string &name = "") const;
        RosFqnBuilder resource(Resource resource, const std::string &name = "") const;
        RosFqnBuilder reset() const;
        RosFqnBuilder reset(RosFqnSegment segments) const;
        [[nodiscard]] std::string build(RosFqnSegment begin, RosFqnSegment end) const;
        [[nodiscard]] std::string build(RosFqnSegment segment) const;
        [[nodiscard]] std::string build() const;

    private:
        // Our ROS FQN naming convention follows the segmented structure below:
        //
        // /<scope>/<?agent>/<?component>/<?stream>/<resource>
        //
        // Additionally, the type of resource is constrained by the immediately preceding segment.
        // In order to capture these rules, we define a set of validation rules for each segment
        // using the following structure:
        struct RosFqnSegmentRule
        {
            bool m_is_required;
            RosFqnSegment m_segment;
            RosFqnSegment m_required_segments;
            Resource m_allowed_resources;
        };

        static size_t s_get_index(RosFqnSegment segment);

        // This array defines the validation rules for each segment. The order of the
        // rules is important and must match the order in which they are defined in RosFqnSegment.
        static const std::array<RosFqnSegmentRule, magic_enum::enum_count<RosFqnSegment>()> s_segment_rules;

        template <typename TEnum>
        RosFqnBuilder set_segment(RosFqnSegment segment, TEnum value, const std::string &name = "") const;

        RosFqnSegment m_set_segments;

        // This array holds the values for each segment. The order of the  segments is
        // important and must match the order in which they are defined in RosFqnSegment.
        std::array<std::optional<std::string>, magic_enum::enum_count<RosFqnSegment>()> m_segment_values;

        // The resource that was set. We need to keep track of this because
        // we only allow certain resources, depending on the preceding segment.
        Resource m_resource;
    };
}
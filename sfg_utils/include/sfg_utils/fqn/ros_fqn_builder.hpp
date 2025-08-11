#pragma once

#include "sfg_utils/fqn/component.hpp"
#include "sfg_utils/fqn/resource.hpp"
#include "sfg_utils/fqn/ros_fqn_segment.hpp"
#include "sfg_utils/fqn/scope.hpp"
#include "sfg_utils/fqn/stream.hpp"

#include <string>
#include <optional>

namespace sfg_utils::fqn
{
    class RosFQNBuilder
    {
    public:
        RosFQNBuilder();

        RosFQNBuilder &scope(Scope scope);
        RosFQNBuilder &agent(const std::string &name = "");
        RosFQNBuilder &component(Component component, const std::string &name = "");
        RosFQNBuilder &stream(Stream stream, const std::string &name = "");
        RosFQNBuilder &resource(Resource resource, const std::string &name = "");
        [[nodiscard]] std::string build(RosFQNSegment begin = RosFQNSegment::Scope, RosFQNSegment end = RosFQNSegment::Resource) const;
        RosFQNBuilder &reset();
        RosFQNBuilder &reset(RosFQNSegment segments);

    private:
        // Our ROS FQN naming convention follows the segmented structure below:
        //
        // /<scope>/<?agent>/<?component>/<?stream>/<resource>
        //
        // Additionally, the type of resource is constrained by the immediately preceding segment.
        // In order to capture these rules, we define a set of validation rules for each segment
        // using the following structure:
        struct RosFQNSegmentRule
        {
            bool m_is_required;
            RosFQNSegment m_segment;
            RosFQNSegment m_required_segments;
            Resource m_allowed_resources;
        };

        static size_t s_get_index(RosFQNSegment segment);

        // This array defines the validation rules for each segment. The order of the
        // rules is important and must match the order in which they are defined in RosFQNSegment.
        static const std::array<RosFQNSegmentRule, magic_enum::enum_count<RosFQNSegment>()> s_segment_rules;

        template <typename TEnum>
        void set_segment(RosFQNSegment segment, TEnum value, const std::string &name);

        RosFQNSegment m_set_segments;

        // This array holds the values for each segment. The order of the  segments is
        // important and must match the order in which they are defined in RosFQNSegment.
        std::array<std::optional<std::string>, magic_enum::enum_count<RosFQNSegment>()> m_segment_values;

        // The resource that was set. We need to keep track of this because
        // we only allow certain resources, depending on the preceding segment.
        Resource m_resource;
    };
}
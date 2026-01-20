import launch
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from rospkg import get_package_name
from sfg_utils.fqn import RosFqnBuilder, RosFqnSegment, Scope

package_name = get_package_name(__file__)
local_namespace = RosFqnBuilder().scope(Scope.Local).agent()
global_namespace = RosFqnBuilder().scope(Scope.Global).agent()


def generate_launch_description() -> launch.LaunchDescription:
    metadata_filepath_argument = DeclareLaunchArgument(
        "metadata_filepath", default_value=""
    )

    agent_status_provider_node = ComposableNode(
        package=package_name,
        plugin="sfg_agent::AgentStatusProvider",
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
        name="agent_status_provider",
        parameters=[
            {
                metadata_filepath_argument.name: LaunchConfiguration(
                    metadata_filepath_argument.name
                )
            },
        ],
        extra_arguments=[{"use_intra_process_comms": True}],
    )

    return launch.LaunchDescription(
        [
            metadata_filepath_argument,
            ComposableNodeContainer(
                package="rclcpp_components",
                executable="component_container_mt",
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
                name="agent_container",
                output="screen",
                composable_node_descriptions=[
                    agent_status_provider_node,
                ],
            ),
        ]
    )

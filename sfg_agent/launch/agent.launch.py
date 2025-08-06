import launch
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from rospkg import get_package_name
from sfg_utils.fqn import RosFQNBuilder, Scope

package_name = get_package_name(__file__)
local_namespace, global_namespace = (
    RosFQNBuilder().scope(Scope.Local).agent().build(only_namespace=True),
    RosFQNBuilder().scope(Scope.Global).agent().build(only_namespace=True),
)


def generate_launch_description():
    agent_container = ComposableNodeContainer(
        package="rclcpp_components",
        executable="component_container_mt",
        namespace=local_namespace,
        name=f"{package_name}_agent_container",
        output="screen",
        composable_node_descriptions=(
            ComposableNode(
                package=package_name,
                plugin="sfg_agent::AgentStatusProvider",
                namespace=local_namespace,
                parameters=[
                    {"metadata_filepath": LaunchConfiguration("metadata_filepath")},
                ],
                extra_arguments=[{"use_intra_process_comms": True}],
            ),
        ),
    )

    return launch.LaunchDescription(
        [
            DeclareLaunchArgument("metadata_filepath"),
            agent_container,
        ]
    )

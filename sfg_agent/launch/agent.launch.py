from typing import Any

import launch
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode
from rospkg import get_package_name
from sfg_utils.fqn import RosFQNBuilder, RosFQNSegment, Scope

package_name = get_package_name(__file__)


def get_nodes(
    local_namespace: str,
    global_namespace: str,
    **arguments: Any,
) -> tuple[list[Node], list[ComposableNode]]:
    agent_status_provider_node = ComposableNode(
        package=package_name,
        plugin="sfg_agent::AgentStatusProvider",
        namespace=local_namespace,
        parameters=[
            {"metadata_filepath": arguments["metadata_filepath"]},
        ],
        extra_arguments=[{"use_intra_process_comms": True}],
    )

    return [], [agent_status_provider_node]


def generate_launch_description():
    local_namespace, global_namespace = (
        RosFQNBuilder()
        .scope(Scope.Local)
        .agent()
        .build(begin=RosFQNSegment.Scope, end=RosFQNSegment.Agent),
        RosFQNBuilder()
        .scope(Scope.Global)
        .agent()
        .build(begin=RosFQNSegment.Scope, end=RosFQNSegment.Agent),
    )

    nodes, composable_nodes = get_nodes(
        local_namespace,
        global_namespace,
        metadata_filepath=LaunchConfiguration("metadata_filepath"),
    )

    return launch.LaunchDescription(
        [
            DeclareLaunchArgument("metadata_filepath"),
            *nodes,
            ComposableNodeContainer(
                package="rclcpp_components",
                executable="component_container_mt",
                namespace=local_namespace,
                name="agent_container",
                output="screen",
                composable_node_descriptions=composable_nodes,
            ),
        ]
    )

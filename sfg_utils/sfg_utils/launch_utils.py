from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescriptionEntity
from launch.actions import DeclareLaunchArgument
from launch.launch_description_sources.python_launch_file_utilities import (
    get_launch_description_from_python_launch_file,
)
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode


@dataclass
class LaunchDescriptionEntities:
    nodes: list[Node]
    composable_nodes: list[ComposableNode]
    launch_arguments: list[DeclareLaunchArgument]
    other_entities: list[LaunchDescriptionEntity]


def get_launch_description_entities(
    package_name: str,
    launch_file: str,
) -> LaunchDescriptionEntities:
    path = Path(get_package_share_directory(package_name)) / "launch" / launch_file
    launch_description = get_launch_description_from_python_launch_file(path.as_posix())

    nodes: list[Node] = []
    composable_nodes: list[ComposableNode] = []
    launch_arguments: list[DeclareLaunchArgument] = []
    other_entities: list[LaunchDescriptionEntity] = []

    for entity in launch_description.entities:
        if isinstance(entity, Node):
            nodes.append(entity)
        elif isinstance(entity, ComposableNodeContainer):
            composable_nodes.extend(entity.__composable_node_descriptions)
        elif isinstance(entity, DeclareLaunchArgument):
            launch_arguments.append(entity)
        else:
            other_entities.append(entity)

    return LaunchDescriptionEntities(
        nodes,
        composable_nodes,
        launch_arguments,
        other_entities,
    )

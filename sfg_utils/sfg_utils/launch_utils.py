from __future__ import annotations

import importlib.util
import inspect
from dataclasses import dataclass
from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription, LaunchDescriptionEntity
from launch.actions import DeclareLaunchArgument
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode


@dataclass(frozen=True)
class LaunchDescriptionEntities:
    nodes: list[Node]
    composable_nodes: list[ComposableNode]
    launch_arguments: list[DeclareLaunchArgument]
    other_entities: list[LaunchDescriptionEntity]

    def __add__(self, other: LaunchDescriptionEntities) -> LaunchDescriptionEntities:
        return LaunchDescriptionEntities(
            nodes=self.nodes + other.nodes,
            composable_nodes=self.composable_nodes + other.composable_nodes,
            launch_arguments=self.launch_arguments + other.launch_arguments,
            other_entities=self.other_entities + other.other_entities,
        )


def get_launch_description(package_name: str, launch_file: str) -> LaunchDescription:
    path = Path(get_package_share_directory(package_name)) / "launch" / launch_file
    spec = importlib.util.spec_from_file_location(
        f"{package_name}.launch_file.split('.')[0]", path
    )

    if spec is None or spec.loader is None:
        raise ImportError(f"Could not find launch file '{path}'.")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    if module is None:
        raise ImportError(f"Could not load module from spec '{spec}'.")

    generate_launch_description_function_name = "generate_launch_description"
    error_message = (
        f"Please define a {generate_launch_description_function_name} function with the following signature:\n"
        + f"def {generate_launch_description_function_name}() -> launch.LaunchDescription:"
    )

    if not hasattr(module, generate_launch_description_function_name):
        raise AttributeError(
            f"Module {module.__name__} has no {generate_launch_description_function_name} function.\n"
            + error_message
        )

    function = getattr(module, generate_launch_description_function_name)

    if not inspect.isfunction(function):
        raise AttributeError(
            f"Attribute {generate_launch_description_function_name} is not a function.\n"
            + error_message
        )

    signature = inspect.signature(function)
    parameters = list(signature.parameters.values())

    if len(parameters) != 0:
        raise AttributeError(
            f"Invalid signature for {generate_launch_description_function_name} function.\n"
            + error_message
        )

    return getattr(module, generate_launch_description_function_name)()


def extract_launch_description_entities(
    launch_description: LaunchDescription,
) -> LaunchDescriptionEntities:
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


def get_launch_description_entities(
    package_name: str, launch_file: str
) -> LaunchDescriptionEntities:
    launch_description = get_launch_description(package_name, launch_file)
    return extract_launch_description_entities(launch_description)

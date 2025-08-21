import importlib.util
import inspect
from pathlib import Path
from typing import Any

from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import Node
from launch_ros.descriptions import ComposableNode


def get_nodes(
    local_namespace: str,
    global_namespace: str,
    package_name: str,
    launch_file: str,
    **arguments: Any,
) -> tuple[list[Node], list[ComposableNode]]:
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

    get_nodes_function_name = "get_nodes"
    error_message = (
        f"Please define a {get_nodes_function_name} function with the following signature:\n"
        + f"def {get_nodes_function_name}(local_namespace: str, global_namespace: str, **arguments: Any) -> tuple[list[Node], list[ComposableNode]]:\n"
        + "    # ...\n"
        + "    # return [...], [...]"
    )

    if not hasattr(module, get_nodes_function_name):
        raise AttributeError(
            f"Module {module.__name__} has no {get_nodes_function_name} function.\n"
            + error_message
        )

    function = getattr(module, get_nodes_function_name)

    if not inspect.isfunction(function):
        raise AttributeError(
            f"Attribute {get_nodes_function_name} is not a function.\n" + error_message
        )

    signature = inspect.signature(function)
    parameters = list(signature.parameters.values())

    if len(parameters) != 3:
        raise AttributeError(
            f"Invalid signature for {get_nodes_function_name} function.\n"
            + error_message
        )

    return getattr(module, get_nodes_function_name)(
        local_namespace, global_namespace, **arguments
    )

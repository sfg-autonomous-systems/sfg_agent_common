import launch
from launch.actions import DeclareLaunchArgument, GroupAction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node, SetParameter
from launch_ros.descriptions import ParameterFile
from launch_ros.substitutions import FindPackageShare
from nav2_common.launch import RewrittenYaml
from rospkg import get_package_name
from sfg_utils.fqn import Component, Resource, RosFqnBuilder, RosFqnSegment, Scope

package_name = get_package_name(__file__)
local_namespace = RosFqnBuilder().scope(Scope.Local).agent()
global_namespace = RosFqnBuilder().scope(Scope.Global).agent()


def generate_launch_description() -> launch.LaunchDescription:
    autostart = LaunchConfiguration("autostart")
    log_level = LaunchConfiguration("log_level")
    params_file = LaunchConfiguration("params_file")
    params_package_name = LaunchConfiguration("params_package_name")
    use_respawn = LaunchConfiguration("use_respawn")
    use_sim_time = LaunchConfiguration("use_sim_time")

    lifecycle_nodes = [
        "controller_server",
        "smoother_server",
        "planner_server",
        "behavior_server",
        "velocity_smoother",
        "collision_monitor",
        "bt_navigator",
        "waypoint_follower",
    ]

    configured_params = ParameterFile(
        RewrittenYaml(
            source_file=params_file,
            root_key=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
            param_rewrites={
                "autostart": autostart,
                "collision_monitor.ros__parameters.pointcloud.topic": global_namespace.component(
                    Component.Lidar, "back"
                )
                .resource(Resource.PointCloud)
                .build(),
                "global_frame": local_namespace.resource(Resource.Custom, "odom").build(
                    RosFqnSegment.Agent, RosFqnSegment.Resource
                ),
                "global_costmap.global_costmap.ros__parameters.obstacle_layer.lidar.topic": global_namespace.component(
                    Component.Lidar, "back"
                )
                .resource(Resource.PointCloud)
                .build(),
                "local_costmap.local_costmap.ros__parameters.obstacle_layer.lidar.topic": global_namespace.component(
                    Component.Lidar, "back"
                )
                .resource(Resource.PointCloud)
                .build(),
                "robot_base_frame": local_namespace.resource(
                    Resource.Custom, "base_link"
                ).build(RosFqnSegment.Agent, RosFqnSegment.Resource),
            },
            convert_types=True,
        ),
        allow_substs=True,
    )

    nav2_nodes = GroupAction(
        actions=[
            SetParameter("use_sim_time", use_sim_time),
            Node(
                package="nav2_controller",
                executable="controller_server",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[
                    configured_params,
                    {
                        "odom_topic": global_namespace.component(
                            Component.LocomotionController
                        )
                        .resource(Resource.Odom)
                        .build(),
                    },
                ],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=[
                    ("cmd_vel", "cmd_vel_nav"),
                    (
                        local_namespace.resource(
                            Resource.Custom, "follow_path"
                        ).build(),
                        global_namespace.resource(
                            Resource.Custom, "follow_path"
                        ).build(),
                    ),
                ],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
            Node(
                package="nav2_planner",
                executable="planner_server",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=[],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
            Node(
                package="nav2_collision_monitor",
                executable="collision_monitor",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[
                    configured_params,
                    {
                        "base_frame_id": local_namespace.resource(
                            Resource.Custom, "base_link"
                        ).build(RosFqnSegment.Agent, RosFqnSegment.Resource),
                        "odom_frame_id": local_namespace.resource(
                            Resource.Custom, "odom"
                        ).build(RosFqnSegment.Agent, RosFqnSegment.Resource),
                        "cmd_vel_out_topic": local_namespace.component(
                            Component.LocomotionController
                        )
                        .resource(Resource.CmdVel)
                        .build(),
                    },
                ],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=[],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
            Node(
                package="nav2_smoother",
                executable="smoother_server",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[
                    configured_params,
                    {
                        "odom_topic": global_namespace.component(
                            Component.LocomotionController
                        )
                        .resource(Resource.Odom)
                        .build(),
                    },
                ],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=[],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
            Node(
                package="nav2_velocity_smoother",
                executable="velocity_smoother",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[
                    configured_params,
                    {
                        "odom_topic": global_namespace.component(
                            Component.LocomotionController
                        )
                        .resource(Resource.Odom)
                        .build()
                    },
                ],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=[("cmd_vel", "cmd_vel_nav")],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
            Node(
                package="nav2_waypoint_follower",
                executable="waypoint_follower",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=[],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
            Node(
                package="nav2_behaviors",
                executable="behavior_server",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[
                    configured_params,
                    {
                        "robot_base_frame": local_namespace.resource(
                            Resource.Custom, "base_link"
                        ).build(RosFqnSegment.Agent, RosFqnSegment.Resource)
                    },
                ],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=[("cmd_vel", "cmd_vel_nav")],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
            Node(
                package="nav2_bt_navigator",
                executable="bt_navigator",
                output="screen",
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[
                    configured_params,
                    {
                        "robot_base_frame": local_namespace.resource(
                            Resource.Custom, "base_link"
                        ).build(RosFqnSegment.Agent, RosFqnSegment.Resource)
                    },
                ],
                arguments=["--ros-args", "--log-level", log_level],
                remappings=[],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
            Node(
                package="nav2_lifecycle_manager",
                executable="lifecycle_manager",
                name="lifecycle_manager_navigation",
                output="screen",
                arguments=["--ros-args", "--log-level", log_level],
                parameters=[{"autostart": autostart, "node_names": lifecycle_nodes}],
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
            ),
        ]
    )

    return launch.LaunchDescription(
        [
            DeclareLaunchArgument("autostart", default_value="true"),
            DeclareLaunchArgument("log_level", default_value="info"),
            DeclareLaunchArgument(
                "params_file",
                default_value=PathJoinSubstitution(
                    [
                        FindPackageShare(params_package_name),
                        "config",
                        "nav2_params.yaml",
                    ]
                ),
            ),
            DeclareLaunchArgument("params_package_name", default_value=package_name),
            DeclareLaunchArgument("use_respawn", default_value="false"),
            DeclareLaunchArgument("use_sim_time", default_value="false"),
            nav2_nodes,
        ]
    )

import launch
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import ComposableNodeContainer, SetParameter
from launch_ros.descriptions import ComposableNode, ParameterFile
from launch_ros.substitutions import FindPackageShare
from nav2_common.launch import RewrittenYaml
from rospkg import get_package_name
from sfg_utils.fqn import Component, Resource, RosFqnBuilder, RosFqnSegment, Scope

package_name = get_package_name(__file__)
local_namespace = RosFqnBuilder().scope(Scope.Local).agent()
global_namespace = RosFqnBuilder().scope(Scope.Global).agent()


def generate_launch_description() -> launch.LaunchDescription:
    common_params_file = PathJoinSubstitution(
        [
            FindPackageShare(package_name),
            "config",
            "nav2_params_common.yaml",
        ]
    )

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

    common_params = ParameterFile(
        RewrittenYaml(
            source_file=common_params_file,
            root_key=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
            param_rewrites={},
            convert_types=True,
        ),
        allow_substs=True,
    )

    configured_params = ParameterFile(
        RewrittenYaml(
            source_file=LaunchConfiguration("params_file"),
            root_key=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
            param_rewrites={
                "autostart": LaunchConfiguration("autostart"),
                "collision_monitor.ros__parameters.pointcloud.topic": global_namespace.component(
                    Component.Lidar, "back"
                )
                .resource(Resource.PointCloud)
                .build(),
                "local_costmap.local_costmap.ros__parameters.global_frame": local_namespace.resource(
                    Resource.Custom, "odom"
                ).build(RosFqnSegment.Agent, RosFqnSegment.Resource),
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
                "use_sim_time": LaunchConfiguration("use_sim_time"),
            },
            convert_types=True,
        ),
        allow_substs=True,
    )

    controller_node = ComposableNode(
        package="nav2_controller",
        plugin="nav2_controller::ControllerServer",
        name="controller_server",
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
        parameters=[
            common_params,
            configured_params,
            {
                "odom_topic": global_namespace.component(Component.LocomotionController)
                .resource(Resource.Odom)
                .build()
            },
        ],
        remappings=[
            ("cmd_vel", "cmd_vel_nav"),
            (
                local_namespace.resource(Resource.Custom, "follow_path").build(),
                global_namespace.resource(Resource.Custom, "follow_path").build(),
            ),
        ],
    )

    planner_node = ComposableNode(
        package="nav2_planner",
        plugin="nav2_planner::PlannerServer",
        name="planner_server",
        parameters=[common_params, configured_params],
        remappings=[],
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
    )

    collision_monitor_node = ComposableNode(
        package="nav2_collision_monitor",
        plugin="nav2_collision_monitor::CollisionMonitor",
        name="collision_monitor",
        parameters=[
            common_params,
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
        remappings=[],
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
    )

    smoother_server_node = ComposableNode(
        package="nav2_smoother",
        plugin="nav2_smoother::SmootherServer",
        name="smoother_server",
        parameters=[
            common_params,
            configured_params,
            {
                "odom_topic": global_namespace.component(Component.LocomotionController)
                .resource(Resource.Odom)
                .build()
            },
        ],
        remappings=[],
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
    )

    velocity_smoother_node = ComposableNode(
        package="nav2_velocity_smoother",
        plugin="nav2_velocity_smoother::VelocitySmoother",
        name="velocity_smoother",
        parameters=[
            common_params,
            configured_params,
            {
                "odom_topic": global_namespace.component(Component.LocomotionController)
                .resource(Resource.Odom)
                .build()
            },
        ],
        remappings=[("cmd_vel", "cmd_vel_nav")],
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
    )

    waypoint_follower_node = ComposableNode(
        package="nav2_waypoint_follower",
        plugin="nav2_waypoint_follower::WaypointFollower",
        name="waypoint_follower",
        parameters=[
            common_params,
            configured_params,
        ],
        remappings=[],
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
    )

    behavior_server_node = ComposableNode(
        package="nav2_behaviors",
        plugin="behavior_server::BehaviorServer",
        name="behavior_server",
        parameters=[
            common_params,
            configured_params,
            {
                "global_frame": global_namespace.component(
                    Component.LocomotionController
                )
                .resource(Resource.Odom)
                .build(),
            },
        ],
        remappings=[("cmd_vel", "cmd_vel_nav")],
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
    )

    bt_navigator_node = ComposableNode(
        package="nav2_bt_navigator",
        plugin="nav2_bt_navigator::BtNavigator",
        name="bt_navigator",
        parameters=[
            common_params,
            configured_params,
            {
                "odom_topic": global_namespace.component(Component.LocomotionController)
                .resource(Resource.Odom)
                .build(),
            },
        ],
        remappings=[],
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
    )

    lifecycle_manager_node = ComposableNode(
        package="nav2_lifecycle_manager",
        plugin="nav2_lifecycle_manager::LifecycleManager",
        name="lifecycle_manager_navigation",
        parameters=[{"node_names": lifecycle_nodes}],
        namespace=local_namespace.build(RosFqnSegment.Scope, RosFqnSegment.Agent),
    )

    return launch.LaunchDescription(
        [
            DeclareLaunchArgument("autostart", default_value="true"),
            DeclareLaunchArgument("log_level", default_value="info"),
            DeclareLaunchArgument("params_file"),
            DeclareLaunchArgument("use_respawn", default_value="false"),
            DeclareLaunchArgument("use_sim_time", default_value="false"),
            ComposableNodeContainer(
                package="rclcpp_components",
                executable="component_container_mt",
                namespace=local_namespace.build(
                    RosFqnSegment.Scope, RosFqnSegment.Agent
                ),
                name="nav2_container",
                output="screen",
                respawn=LaunchConfiguration("use_respawn"),
                arguments=[
                    "--ros-args",
                    "--log-level",
                    LaunchConfiguration("log_level"),
                ],
                composable_node_descriptions=[
                    controller_node,
                    planner_node,
                    collision_monitor_node,
                    smoother_server_node,
                    velocity_smoother_node,
                    waypoint_follower_node,
                    behavior_server_node,
                    bt_navigator_node,
                    lifecycle_manager_node,
                ],
            ),
        ]
    )

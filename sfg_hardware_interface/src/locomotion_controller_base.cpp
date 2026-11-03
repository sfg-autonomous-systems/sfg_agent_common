#include "sfg_hardware_interface/locomotion_controller_base.hpp"

#include <cmath>
#include <geometry_msgs/msg/transform_stamped.hpp>

#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/fqn/ros_fqn_builder.hpp"

namespace sfg_hardware_interface
{
    template <typename VectorT>
    bool is_finite(const VectorT &vector)
    {
        return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
    }

    LocomotionControllerBase::LocomotionControllerBase(const std::string &name, const rclcpp::NodeOptions &options) : Node(name, options)
    {
        // Declare and retrieve ROS parameters.
        m_cmd_timeout = declare_parameter(
            "cmd_timeout",
            0.25f,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description(
                    "Messages that have been received with a timestamp older "
                    "than this value will be ignored. Unit is [s]."));

        // The trailing return type is required because else Eigen returns a lazily evaluated expression that relies on the local matrix 'parsed_limits'.
        auto declare_limits_parameter = [this](const std::string &parameter_name, const std::vector<double> &default_values, const std::string &description) -> Eigen::Matrix<float, 3, 2>
        {
            auto raw_limits = declare_parameter(
                parameter_name,
                default_values,
                rcl_interfaces::msg::ParameterDescriptor().set__description(description));

            if (raw_limits.size() != 6)
            {
                throw std::runtime_error("The parameter '" + parameter_name + "' must contain exactly 6 values.");
            }

            const Eigen::Matrix<double, 3, 2> parsed_limits = Eigen::Matrix<double, 2, 3>(raw_limits.data()).transpose();

            if ((parsed_limits.col(0).array() > 0.0).any() || (parsed_limits.col(1).array() < 0.0).any())
            {
                throw std::runtime_error("The parameter '" + parameter_name + "' must satisfy min <= 0 <= max for every axis.");
            }

            if (!parsed_limits.allFinite())
            {
                throw std::runtime_error("The parameter '" + parameter_name + "' must contain only finite values.");
            }
            return parsed_limits.cast<float>();
        };

        m_linear_limits = declare_limits_parameter(
            "linear_limits",
            std::vector<double>{-0.5, 0.5, -0.5, 0.5, -0.5, 0.5},
            "The linear velocity limits along the robot's x, y, and z axes in [m/s]. "
            "Each consecutive pair of values defines the minimum and maximum speed for the respective axis. "
            "The first two values are for x, the next two for y, and the last two for z.");

        m_angular_limits = declare_limits_parameter(
            "angular_limits",
            std::vector<double>{-1.0, 1.0, -1.0, 1.0, -1.0, 1.0},
            "The angular velocity limits around the robot's x, y, and z axes in [rad/s]. "
            "Each consecutive pair of values defines the minimum and maximum speed for the respective axis. "
            "The first two values are for x, the next two for y, and the last two for z.");

        m_odom_frame_id = declare_parameter<std::string>(
            "odom_frame_id",
            sfg_utils::agent_utils::sanitize_agent_name(sfg_utils::agent_utils::get_agent_name()) + "/odom",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The frame ID of the odometry frame."));

        m_base_link_frame_id = declare_parameter<std::string>(
            "base_link_frame_id",
            sfg_utils::agent_utils::sanitize_agent_name(sfg_utils::agent_utils::get_agent_name()) + "/base_link",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The frame ID of the robot base link."));

        m_action_map = {
            {"arm", std::bind(&LocomotionControllerBase::arm, this)},
            {"disarm", std::bind(&LocomotionControllerBase::disarm, this)},
            {"emergency_stop", std::bind(&LocomotionControllerBase::emergency_stop, this)},
        };

        using namespace sfg_utils::fqn;

        // Set up interfaces.
        m_cmd_vel_subscriber = create_subscription<geometry_msgs::msg::TwistStamped>(
            RosFqnBuilder().resource(Resource::CmdVel).build(RosFqnSegment::Resource),
            rclcpp::SensorDataQoS(),
            std::bind(&LocomotionControllerBase::cmd_vel_callback, this, std::placeholders::_1));
        m_reset_cmd_timer = create_wall_timer(
            std::chrono::duration<float>(m_cmd_timeout),
            std::bind(&LocomotionControllerBase::reset_cmd_callback, this));
        m_trigger_action_service = create_service<sfg_agent_msgs::srv::TriggerAction>(
            RosFqnBuilder().resource(Resource::TriggerAction).build(RosFqnSegment::Resource),
            std::bind(&LocomotionControllerBase::trigger_action_callback, this, std::placeholders::_1, std::placeholders::_2));
        m_odometry_publisher = create_publisher<nav_msgs::msg::Odometry>(RosFqnBuilder().scope(Scope::Global).agent().component(sfg_utils::fqn::Component::LocomotionController).resource(Resource::Custom, "odom").build(), 10);
        m_tf_broadcaster = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
    }

    void LocomotionControllerBase::arm()
    {
        throw UnsupportedActionException();
    }

    void LocomotionControllerBase::disarm()
    {
        throw UnsupportedActionException();
    };

    void LocomotionControllerBase::emergency_stop()
    {
        throw UnsupportedActionException();
    };

    void LocomotionControllerBase::add_action(const std::string &name, std::function<void()> function)
    {
        if (m_action_map.find(name) != m_action_map.end())
        {
            RCLCPP_ERROR(get_logger(), "Cannot add action '%s' because it is already registered.", name.c_str());
            return;
        }
        m_action_map[name] = function;
    }

    bool LocomotionControllerBase::try_publish_odometry(
        const rclcpp::Time &stamp,
        const geometry_msgs::msg::PoseWithCovariance &pose,
        const geometry_msgs::msg::TwistWithCovariance &twist)
    {
        const auto quaternion_norm = std::sqrt(
            pose.pose.orientation.x * pose.pose.orientation.x +
            pose.pose.orientation.y * pose.pose.orientation.y +
            pose.pose.orientation.z * pose.pose.orientation.z +
            pose.pose.orientation.w * pose.pose.orientation.w);

        constexpr auto quaternion_norm_tolerance = 4e-3;

        if (!std::isfinite(quaternion_norm) || std::abs(quaternion_norm - 1.0) > quaternion_norm_tolerance)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Ignoring odometry message with a non-normalized orientation (norm: %f).", quaternion_norm);
            return false;
        }

        if (!is_finite(pose.pose.position) || !is_finite(twist.twist.linear) || !is_finite(twist.twist.angular))
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Ignoring odometry message with non-finite values.");
            return false;
        }

        auto msg = std::make_unique<nav_msgs::msg::Odometry>();
        msg->header.stamp = stamp;
        msg->header.frame_id = m_odom_frame_id;
        msg->child_frame_id = m_base_link_frame_id;
        msg->pose = pose;
        msg->twist = twist;

        geometry_msgs::msg::TransformStamped transform;
        transform.header = msg->header;
        transform.child_frame_id = msg->child_frame_id;
        transform.transform.translation.x = pose.pose.position.x;
        transform.transform.translation.y = pose.pose.position.y;
        transform.transform.translation.z = pose.pose.position.z;
        transform.transform.rotation = pose.pose.orientation;

        m_odometry_publisher->publish(std::move(msg));
        m_tf_broadcaster->sendTransform(transform);
        return true;
    }

    void LocomotionControllerBase::cmd_vel_callback(const geometry_msgs::msg::TwistStamped::ConstSharedPtr &msg)
    {
        if (std::abs((now() - msg->header.stamp).seconds()) >= m_cmd_timeout)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Received command message outside the accepted time window. Ignoring.");
            return;
        }

        if (!is_finite(msg->twist.linear) || !is_finite(msg->twist.angular))
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Received command message with non-finite values. Ignoring.");
            return;
        }

        auto cmd = geometry_msgs::msg::TwistStamped();
        cmd.header = msg->header;
        cmd.twist.linear = clamp_velocity(msg->twist.linear, m_linear_limits);
        cmd.twist.angular = clamp_velocity(msg->twist.angular, m_angular_limits);

        try
        {
            apply_cmd(cmd);
        }
        catch (const std::exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to apply command message: %s", exception.what());
        }
        m_reset_cmd_timer->reset();
    }

    void LocomotionControllerBase::reset_cmd_callback()
    {
        auto msg = geometry_msgs::msg::TwistStamped();
        msg.header.stamp = now();

        try
        {
            apply_cmd(msg);
            m_reset_cmd_timer->cancel();
        }
        catch (const std::exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to apply command message: %s", exception.what());
        }
    }

    void LocomotionControllerBase::trigger_action_callback(
        const std::shared_ptr<sfg_agent_msgs::srv::TriggerAction::Request> request,
        std::shared_ptr<sfg_agent_msgs::srv::TriggerAction::Response> response)
    {
        auto iterator = m_action_map.find(request->action);

        if (iterator == m_action_map.end())
        {
            response->message = "Failed to trigger action: The action '" + request->action + "' is unknown.\n";
            response->message += "Available actions are: ";

            for (const auto &pair : m_action_map)
            {
                response->message += pair.first + ", ";
            }
            response->message = response->message.substr(0, response->message.size() - 2);
            response->result = sfg_agent_msgs::srv::TriggerAction::Response::UNSUPPORTED;
            return;
        }

        try
        {
            iterator->second();
        }
        catch (const UnsupportedActionException &exception)
        {
            response->message = "Failed to trigger action: The action '" + request->action + "' is not supported by this locomotion controller.";
            response->result = sfg_agent_msgs::srv::TriggerAction::Response::UNSUPPORTED;
            return;
        }
        catch (const std::exception &exception)
        {
            response->message = "Failed to trigger action: The action '" + request->action + "' threw an exception: ";
            response->message += exception.what();
            response->result = sfg_agent_msgs::srv::TriggerAction::Response::FAILURE;
            return;
        }
        response->result = sfg_agent_msgs::srv::TriggerAction::Response::SUCCESS;
    }

    geometry_msgs::msg::Vector3 LocomotionControllerBase::clamp_velocity(const geometry_msgs::msg::Vector3 &velocity, const Eigen::Matrix<float, 3, 2> &limits)
    {
        Eigen::Vector3f clamped_velocity = {
            static_cast<float>(velocity.x),
            static_cast<float>(velocity.y),
            static_cast<float>(velocity.z),
        };
        clamped_velocity = clamped_velocity.cwiseMax(limits.col(0)).cwiseMin(limits.col(1));

        const auto max_speed = limits.cwiseAbs().maxCoeff();
        const auto speed = clamped_velocity.norm();

        if (speed > max_speed)
        {
            clamped_velocity *= max_speed / speed;
        }

        geometry_msgs::msg::Vector3 result;
        result.x = clamped_velocity.x();
        result.y = clamped_velocity.y();
        result.z = clamped_velocity.z();
        return result;
    }
}

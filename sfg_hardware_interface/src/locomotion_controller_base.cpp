#include "sfg_hardware_interface/locomotion_controller_base.hpp"

#include "sfg_utils/fqn/ros_fqn_builder.hpp"

namespace sfg_hardware_interface
{
    void LocomotionControllerBase::arm() { throw UnsupportedActionException(); }
    void LocomotionControllerBase::disarm() { throw UnsupportedActionException(); };
    void LocomotionControllerBase::emergency_stop() { throw UnsupportedActionException(); };

    void LocomotionControllerBase::add_action(const std::string &name, std::function<void()> function)
    {
        if (m_action_map.find(name) != m_action_map.end())
        {
            RCLCPP_ERROR(get_logger(), "Cannot add action '%s' because it is already registered.", name.c_str());
            return;
        }
        m_action_map[name] = function;
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

        m_linear_limits = Eigen::Matrix<double, 3, 2>(
                              declare_parameter(
                                  "linear_limits",
                                  std::vector<double>{0.5, 0.5, 0.5, 0.5, 0.5, 0.5},
                                  rcl_interfaces::msg::ParameterDescriptor()
                                      .set__description(
                                          "The linear velocity limits along the robot's x, y, and z axes in [m/s]. "
                                          "Each consecutive pair of values defines the minimum and maximum absolute speed for the respective axis. "
                                          "The first two values are for x, the next two for y, and the last two for z."))
                                  .data())
                              .cast<float>();

        m_angular_limits = Eigen::Matrix<double, 3, 2>(
                               declare_parameter(
                                   "angular_limits",
                                   std::vector<double>{1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
                                   rcl_interfaces::msg::ParameterDescriptor()
                                       .set__description(
                                           "The angular velocity limits around the robot's x, y, and z axes in [rad/s]. "
                                           "Each consecutive pair of values defines the minimum and maximum absolute speed for the respective axis. "
                                           "The first two values are for x, the next two for y, and the last two for z."))
                                   .data())
                               .cast<float>();

        m_action_map = {
            {"arm", std::bind(&LocomotionControllerBase::arm, this)},
            {"disarm", std::bind(&LocomotionControllerBase::disarm, this)},
            {"emergency_stop", std::bind(&LocomotionControllerBase::emergency_stop, this)},
        };

        using namespace sfg_utils::fqn;

        // Set up interfaces.
        m_cmd_vel_subscriber = create_subscription<geometry_msgs::msg::TwistStamped>(
            RosFqnBuilder().resource(Resource::CmdVel).build(RosFqnSegment::Resource),
            10,
            std::bind(&LocomotionControllerBase::cmd_vel_callback, this, std::placeholders::_1));
        m_reset_cmd_timer = create_wall_timer(
            std::chrono::duration<float>(m_cmd_timeout),
            std::bind(&LocomotionControllerBase::reset_cmd_callback, this));
        m_trigger_action_service = create_service<sfg_agent_msgs::srv::TriggerAction>(
            RosFqnBuilder().resource(Resource::TriggerAction).build(RosFqnSegment::Resource),
            std::bind(&LocomotionControllerBase::trigger_action_callback, this, std::placeholders::_1, std::placeholders::_2));
    }

    void LocomotionControllerBase::cmd_vel_callback(const geometry_msgs::msg::TwistStamped::ConstSharedPtr &msg)
    {
        if (now() - msg->header.stamp > rclcpp::Duration::from_seconds(m_cmd_timeout))
        {
            // Just to be safe...
            RCLCPP_WARN(get_logger(), "Received outdated command message. Ignoring.");
            m_reset_cmd_timer->reset();
            return;
        }

        auto cmd = geometry_msgs::msg::TwistStamped();
        cmd.header = msg->header;
        cmd.twist.linear = clamp_velocity(msg->twist.linear, m_linear_limits);
        cmd.twist.angular = clamp_velocity(msg->twist.angular, m_angular_limits);
        apply_cmd(cmd);
        m_reset_cmd_timer->reset();
    }

    void LocomotionControllerBase::reset_cmd_callback()
    {
        auto msg = geometry_msgs::msg::TwistStamped();
        msg.header.stamp = now();
        apply_cmd(msg);
        m_reset_cmd_timer->cancel();
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
        Eigen::Vector3f sign = clamped_velocity.cwiseSign();
        clamped_velocity = clamped_velocity.cwiseAbs().cwiseMin(limits.col(1)).cwiseMax(limits.col(0)).cwiseProduct(sign);

        geometry_msgs::msg::Vector3 result;
        result.x = clamped_velocity.x();
        result.y = clamped_velocity.y();
        result.z = clamped_velocity.z();
        return result;
    }
}
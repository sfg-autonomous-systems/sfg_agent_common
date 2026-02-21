#pragma once

#include <eigen3/Eigen/Dense>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <rclcpp/rclcpp.hpp>

#include "sfg_agent_msgs/srv/trigger_action.hpp"

namespace sfg_hardware_interface
{
    class LocomotionControllerBase : public rclcpp::Node
    {
    public:
        LocomotionControllerBase(const std::string &name, const rclcpp::NodeOptions &options);

    protected:
        virtual void arm();
        virtual void disarm();
        virtual void emergency_stop();
        virtual void apply_cmd(const geometry_msgs::msg::TwistStamped &cmd) = 0;

        void add_action(const std::string &name, std::function<void()> function);

        std::map<std::string, std::function<void()>> m_action_map;

    private:
        class UnsupportedActionException : public std::exception
        {
        };

        void cmd_vel_callback(const geometry_msgs::msg::TwistStamped::ConstSharedPtr &msg);
        void reset_cmd_callback();
        void trigger_action_callback(
            const std::shared_ptr<sfg_agent_msgs::srv::TriggerAction::Request> request,
            std::shared_ptr<sfg_agent_msgs::srv::TriggerAction::Response> response);

        geometry_msgs::msg::Vector3 clamp_velocity(const geometry_msgs::msg::Vector3 &velocity, const Eigen::Matrix<float, 3, 2> &limits);

        // ROS parameters
        float m_cmd_timeout;
        Eigen::Matrix<float, 3, 2> m_linear_limits;
        Eigen::Matrix<float, 3, 2> m_angular_limits;

        rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr m_cmd_vel_subscriber;
        rclcpp::TimerBase::SharedPtr m_reset_cmd_timer;
        rclcpp::Service<sfg_agent_msgs::srv::TriggerAction>::SharedPtr m_trigger_action_service;
    };
}
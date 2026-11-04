#pragma once

#include <eigen3/Eigen/Dense>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/transform_broadcaster.hpp>

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

        // Publishes the odometry message and the corresponding odom -> base_link transform.
        // stamp is the time the measurement was taken, pose is expressed in the odom frame and
        // twist in the base_link frame. Returns false if the values are invalid.
        bool try_publish_odometry(
            const rclcpp::Time &stamp,
            const geometry_msgs::msg::PoseWithCovariance &pose,
            const geometry_msgs::msg::TwistWithCovariance &twist);

        std::map<std::string, std::function<void()>> m_action_map;

    private:
        using LimitsMatrix = Eigen::Matrix<float, 3, 6>;

        class UnsupportedActionException : public std::exception
        {
        };

        void cmd_vel_callback(const geometry_msgs::msg::TwistStamped::ConstSharedPtr &msg);
        void reset_cmd_callback();
        void trigger_action_callback(
            const std::shared_ptr<sfg_agent_msgs::srv::TriggerAction::Request> request,
            std::shared_ptr<sfg_agent_msgs::srv::TriggerAction::Response> response);

        geometry_msgs::msg::Vector3 apply_velocity_limits(const geometry_msgs::msg::Vector3 &velocity, const LimitsMatrix &limits);

        // ROS parameters
        float m_cmd_timeout;
        LimitsMatrix m_linear_limits;
        LimitsMatrix m_angular_limits;
        std::string m_odom_frame_id;
        std::string m_base_link_frame_id;

        rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr m_cmd_vel_subscriber;
        rclcpp::TimerBase::SharedPtr m_reset_cmd_timer;
        rclcpp::Service<sfg_agent_msgs::srv::TriggerAction>::SharedPtr m_trigger_action_service;
        rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr m_odometry_publisher;
        std::unique_ptr<tf2_ros::TransformBroadcaster> m_tf_broadcaster;
    };
}

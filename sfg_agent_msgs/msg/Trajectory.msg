# An array of trajectory points that represents a trajectory for a robot to follow.

# The header's frame ID indicates which coordinate frame should follow the trajectory.
# The header's timestamp indicates the start time of the trajectory.
std_msgs/Header header
# Array of trajectory points to follow.
TrajectoryPoint[] points
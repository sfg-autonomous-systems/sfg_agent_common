# The header contains the frame ID that should follow the trajectory,
# as well as the timestamp when the trajectory formally starts. 
# Conceptually, the difference in time between the header timestamp 
# and the first waypoint's time from start should be the time it takes
# for the agent to reach the initial waypoint.
std_msgs/Header header
Waypoint[] waypoints

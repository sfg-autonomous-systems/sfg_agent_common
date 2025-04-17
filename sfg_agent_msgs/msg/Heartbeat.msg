# If set, the agent is publishing camera data on /global/<hostname>/camera.
uint8 CAPABILITY_CAMERA = 1
# If set, the agent is publishing lidar data on /global/<hostname>/lidar.
uint8 CAPABILITY_LIDAR = 2

std_msgs/Header header
# Contains the sanitized hostname of the agent.
string hostname
# Specifies the amount of seconds between heartbeat messages.
int8 interval
# Flags that specify the various capabilities of the agent.
uint64 capabilities

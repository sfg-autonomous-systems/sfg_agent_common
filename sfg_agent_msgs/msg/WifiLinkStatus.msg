# State of one Wi-Fi interface.
# When connected is false, the fields that describe the link are not valid.
std_msgs/Header header
string interface_name
bool connected

# Network the interface is associated with.
string ssid
string bssid
uint32 frequency_mhz

# Signal quality.
int8 signal_strength_dbm
int8 signal_strength_average_dbm
uint32 inactive_for_ms

# Nominal PHY rates, i.e. how fast bits travel over the air while a frame is being sent.
# tx is the rate this interface uses towards the access point, rx is the rate of the last frame received from it.
# They are an upper bound for the actual bitrate, not a measurement of it.
float32 rx_nominal_bitrate_mbps
float32 tx_nominal_bitrate_mbps

# Data actually transferred to and from the access point, averaged since the previous message.
# NaN if there is no valid previous sample, e.g. in the first message after (re)connecting or roaming.
float32 rx_actual_bitrate_mbps
float32 tx_actual_bitrate_mbps

# Cumulative counters since association with the access point. Reset on (re)association.
uint64 rx_byte_count
uint64 tx_byte_count
uint32 tx_retry_count
uint32 tx_failed_count

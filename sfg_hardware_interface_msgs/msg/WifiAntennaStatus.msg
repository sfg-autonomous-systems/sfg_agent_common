# Signal strength measured at one receive antenna (chain) of a Wi-Fi interface.
# Values the driver does not report are -128.

# Frame ID of the antenna, mapped from chain_index. Empty if no frame is configured for the chain.
string frame_id
# Index of the receive chain as reported by the driver.
uint8 chain_index
int8 signal_strength_dbm
int8 signal_strength_average_dbm

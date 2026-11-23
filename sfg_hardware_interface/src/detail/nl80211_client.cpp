#include "sfg_hardware_interface/detail/nl80211_client.hpp"

#include <cstring>
#include <linux/if_ether.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <netlink/genl/ctrl.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "sfg_hardware_interface/detail/interface_info.hpp"
#include "sfg_hardware_interface/detail/station_info.hpp"

namespace sfg_hardware_interface::detail
{
    Nl80211Client::Nl80211Client(std::chrono::milliseconds receive_timeout) : m_socket(nl_socket_alloc())
    {
        if (!m_socket)
        {
            throw Error("nl_socket_alloc failed");
        }

        if (auto return_code = genl_connect(m_socket.get()); return_code < 0)
        {
            throw Error(std::string("genl_connect: ") + nl_geterror(return_code));
        }

        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(receive_timeout);
        const timeval timeout{static_cast<time_t>(seconds.count()), static_cast<suseconds_t>(std::chrono::duration_cast<std::chrono::microseconds>(receive_timeout - seconds).count())};
        if (setsockopt(nl_socket_get_fd(m_socket.get()), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        {
            throw Error(std::string("setsockopt(SO_RCVTIMEO): ") + std::strerror(errno));
        }

        m_netlink_family = genl_ctrl_resolve(m_socket.get(), "nl80211");
        if (m_netlink_family < 0)
        {
            throw Error("nl80211 not available");
        }
    }

    void Nl80211Client::SocketDeleter::operator()(nl_sock *socket) const
    {
        nl_socket_free(socket);
    }

    void Nl80211Client::MsgDeleter::operator()(nl_msg *msg) const
    {
        nlmsg_free(msg);
    }

    void Nl80211Client::CallbackDeleter::operator()(nl_cb *callback) const
    {
        nl_cb_put(callback);
    }

    InterfaceInfo Nl80211Client::get_interface(const std::string &interface_name)
    {
        std::optional<InterfaceInfo> result;
        request(
            NL80211_CMD_GET_INTERFACE,
            0,
            get_interface_index(interface_name),
            [&](nlattr **attributes)
            {
                result = parse_interface(attributes);
            });

        if (!result)
        {
            throw Error("no reply for interface " + interface_name);
        }
        return *result;
    }

    std::vector<StationInfo> Nl80211Client::get_stations(const std::string &interface_name)
    {
        std::vector<StationInfo> out;
        request(
            NL80211_CMD_GET_STATION,
            NLM_F_DUMP,
            get_interface_index(interface_name),
            [&](nlattr **attributes)
            {
                if (auto info = parse_station(attributes))
                {
                    out.push_back(*info);
                }
            });

        return out;
    }

    std::uint32_t Nl80211Client::get_interface_index(const std::string &interface_name)
    {
        unsigned index = if_nametoindex(interface_name.c_str());
        if (index == 0)
        {
            throw Error("unknown interface: " + interface_name);
        }
        return index;
    }

    InterfaceInfo Nl80211Client::parse_interface(nlattr **attributes)
    {
        InterfaceInfo interface_info;
        if (attributes[NL80211_ATTR_IFNAME])
        {
            interface_info.m_interface_name = nla_get_string(attributes[NL80211_ATTR_IFNAME]);
        }
        if (attributes[NL80211_ATTR_IFINDEX])
        {
            interface_info.m_interface_index = nla_get_u32(attributes[NL80211_ATTR_IFINDEX]);
        }
        if (attributes[NL80211_ATTR_IFTYPE])
        {
            interface_info.m_interface_type = nla_get_u32(attributes[NL80211_ATTR_IFTYPE]);
        }
        if (attributes[NL80211_ATTR_SSID])
        {
            interface_info.m_ssid = std::string(static_cast<const char *>(nla_data(attributes[NL80211_ATTR_SSID])), nla_len(attributes[NL80211_ATTR_SSID]));
        }
        if (attributes[NL80211_ATTR_BSSID])
        {
            interface_info.m_bssid = std::string(static_cast<const char *>(nla_data(attributes[NL80211_ATTR_BSSID])), nla_len(attributes[NL80211_ATTR_BSSID]));
        }
        if (attributes[NL80211_ATTR_WIPHY_FREQ])
        {
            interface_info.m_frequency_mhz = nla_get_u32(attributes[NL80211_ATTR_WIPHY_FREQ]);
        }
        return interface_info;
    }

    std::optional<StationInfo> Nl80211Client::parse_station(nlattr **attributes)
    {
        if (!attributes[NL80211_ATTR_STA_INFO])
        {
            return std::nullopt;
        }

        nlattr *station_info_attributes[NL80211_STA_INFO_MAX + 1];
        if (nla_parse_nested(station_info_attributes, NL80211_STA_INFO_MAX, attributes[NL80211_ATTR_STA_INFO], nullptr) < 0)
        {
            return std::nullopt;
        }

        StationInfo station_info;
        if (attributes[NL80211_ATTR_MAC] && nla_len(attributes[NL80211_ATTR_MAC]) == ETH_ALEN)
        {
            std::memcpy(station_info.m_mac_address.data(), nla_data(attributes[NL80211_ATTR_MAC]), ETH_ALEN);
        }

        auto to_u32 = [&](size_t attribute_index) -> std::optional<std::uint32_t>
        {
            return station_info_attributes[attribute_index] ? std::optional<uint32_t>(nla_get_u32(station_info_attributes[attribute_index])) : std::nullopt;
        };
        auto to_dbm = [&](size_t attribute_index) -> std::optional<std::int32_t>
        {
            return station_info_attributes[attribute_index] ? std::optional<std::int32_t>(std::int8_t(nla_get_u8(station_info_attributes[attribute_index]))) : std::nullopt;
        };
        // NL80211 can provide the number of bytes received/transmitted either as a 32-bit (legacy) or a 64-bit (modern) integer.
        // We check both and give priority to the 64-bit variant.
        auto to_bytes = [&](size_t attribute_64_bit_index, size_t attribute_32_bit_index) -> std::optional<std::uint64_t>
        {
            if (station_info_attributes[attribute_64_bit_index])
            {
                return nla_get_u64(station_info_attributes[attribute_64_bit_index]);
            }
            if (station_info_attributes[attribute_32_bit_index])
            {
                return nla_get_u32(station_info_attributes[attribute_32_bit_index]);
            }
            return std::nullopt;
        };
        auto to_bitrate = [&](nlattr *attribute) -> std::optional<double>
        {
            if (!attribute)
            {
                return std::nullopt;
            }

            nlattr *rate_info_attribute[NL80211_RATE_INFO_MAX + 1];
            if (nla_parse_nested(rate_info_attribute, NL80211_RATE_INFO_MAX, attribute, nullptr) < 0)
            {
                return std::nullopt;
            }
            if (rate_info_attribute[NL80211_RATE_INFO_BITRATE32])
            {
                return nla_get_u32(rate_info_attribute[NL80211_RATE_INFO_BITRATE32]) / 10.0;
            }
            if (rate_info_attribute[NL80211_RATE_INFO_BITRATE])
            {
                return nla_get_u16(rate_info_attribute[NL80211_RATE_INFO_BITRATE]) / 10.0;
            }
            return std::nullopt;
        };

        station_info.m_signal_strength_dbm = to_dbm(NL80211_STA_INFO_SIGNAL);
        station_info.m_signal_strength_average_dbm = to_dbm(NL80211_STA_INFO_SIGNAL_AVG);
        station_info.m_tx_nominal_bitrate_mbps = to_bitrate(station_info_attributes[NL80211_STA_INFO_TX_BITRATE]);
        station_info.m_rx_nominal_bitrate_mbps = to_bitrate(station_info_attributes[NL80211_STA_INFO_RX_BITRATE]);
        station_info.m_rx_byte_count = to_bytes(NL80211_STA_INFO_RX_BYTES64, NL80211_STA_INFO_RX_BYTES);
        station_info.m_tx_byte_count = to_bytes(NL80211_STA_INFO_TX_BYTES64, NL80211_STA_INFO_TX_BYTES);
        station_info.m_tx_retry_count = to_u32(NL80211_STA_INFO_TX_RETRIES);
        station_info.m_tx_failed_count = to_u32(NL80211_STA_INFO_TX_FAILED);
        station_info.m_inactive_for_ms = to_u32(NL80211_STA_INFO_INACTIVE_TIME);
        station_info.m_connected_for_ms = to_u32(NL80211_STA_INFO_CONNECTED_TIME);
        return station_info;
    }

    void Nl80211Client::request(std::uint8_t cmd, std::uint32_t flags, std::uint32_t interface_index, const Handler &handler)
    {
        std::unique_ptr<nl_msg, MsgDeleter> msg(nlmsg_alloc());
        if (!msg)
        {
            throw Error("nlmsg_alloc failed");
        }
        if (!genlmsg_put(msg.get(), NL_AUTO_PORT, NL_AUTO_SEQ, m_netlink_family, 0, flags, cmd, 0))
        {
            throw Error("genlmsg_put failed");
        }
        if (interface_index && nla_put_u32(msg.get(), NL80211_ATTR_IFINDEX, interface_index) < 0)
        {
            throw Error("nla_put_u32 failed");
        }

        std::unique_ptr<nl_cb, CallbackDeleter> callback(nl_cb_alloc(NL_CB_DEFAULT));
        if (!callback)
        {
            throw Error("nl_cb_alloc failed");
        }

        Context context{&handler, 1, nullptr};

        nl_cb_set(
            callback.get(),
            NL_CB_VALID,
            NL_CB_CUSTOM,
            [](nl_msg *msg, void *custom_data) -> std::int32_t
            {
                auto *context = static_cast<Context *>(custom_data);

                if (context->m_exception)
                {
                    return NL_SKIP;
                }
                try
                {
                    auto *header = static_cast<genlmsghdr *>(nlmsg_data(nlmsg_hdr(msg)));
                    nlattr *attributes[NL80211_ATTR_MAX + 1];
                    nla_parse(attributes, NL80211_ATTR_MAX, genlmsg_attrdata(header, 0), genlmsg_attrlen(header, 0), nullptr);
                    (*context->m_handler)(attributes);
                }
                catch (...)
                {
                    context->m_exception = std::current_exception();
                }
                return NL_SKIP;
            },
            &context);
        nl_cb_set(
            callback.get(),
            NL_CB_FINISH,
            NL_CB_CUSTOM,
            [](nl_msg *, void *custom_data) -> std::int32_t
            {
                static_cast<Context *>(custom_data)->m_status = 0;
                return NL_SKIP;
            },
            &context);
        nl_cb_set(
            callback.get(),
            NL_CB_ACK,
            NL_CB_CUSTOM,
            [](nl_msg *, void *custom_data) -> std::int32_t
            {
                static_cast<Context *>(custom_data)->m_status = 0;
                return NL_STOP;
            },
            &context);
        nl_cb_err(
            callback.get(),
            NL_CB_CUSTOM,
            [](sockaddr_nl *, nlmsgerr *error_msg, void *custom_data) -> std::int32_t
            {
                static_cast<Context *>(custom_data)->m_status = error_msg->error;
                return NL_STOP;
            },
            &context);

        if (auto return_code = nl_send_auto(m_socket.get(), msg.get()); return_code < 0)
        {
            throw Error(std::string("nl_send_auto: ") + nl_geterror(return_code));
        }

        while (context.m_status > 0)
        {
            if (auto return_code = nl_recvmsgs(m_socket.get(), callback.get()); return_code < 0)
            {
                if (return_code == -NLE_AGAIN)
                {
                    throw Error("nl_recvmsgs: timeout");
                }
                throw Error(std::string("nl_recvmsgs: ") + nl_geterror(return_code));
            }
        }
        if (context.m_exception)
        {
            std::rethrow_exception(context.m_exception);
        }
        if (context.m_status < 0)
        {
            throw Error(std::string("kernel: ") + nl_geterror(nl_syserr2nlerr(-context.m_status)));
        }
    }
}

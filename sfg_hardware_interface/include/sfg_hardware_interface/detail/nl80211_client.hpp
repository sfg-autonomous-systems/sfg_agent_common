#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <netlink/genl/genl.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfg_hardware_interface/detail/interface_info.hpp"
#include "sfg_hardware_interface/detail/station_info.hpp"

namespace sfg_hardware_interface::detail
{

    class Nl80211Client
    {
    public:
        explicit Nl80211Client(std::chrono::milliseconds receive_timeout);
        Nl80211Client(const Nl80211Client &) = delete;
        Nl80211Client &operator=(const Nl80211Client &) = delete;
        Nl80211Client(Nl80211Client &&) noexcept = default;
        Nl80211Client &operator=(Nl80211Client &&) noexcept = default;

        InterfaceInfo get_interface(const std::string &interface_name);
        std::vector<StationInfo> get_stations(const std::string &interface_name);

        class Error : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

    private:
        using Handler = std::function<void(nlattr **)>;

        struct SocketDeleter
        {
            void operator()(nl_sock *socket) const;
        };

        struct MsgDeleter
        {
            void operator()(nl_msg *msg) const;
        };

        struct CallbackDeleter
        {
            void operator()(nl_cb *callback) const;
        };

        struct Context
        {
            const Handler *m_handler;
            int m_status = 1;
            std::exception_ptr m_exception;
        };

        static std::uint32_t get_interface_index(const std::string &interface_name);
        static InterfaceInfo parse_interface(nlattr **attributes);
        static std::optional<StationInfo> parse_station(nlattr **attributes);

        void request(std::uint8_t cmd, std::uint32_t flags, std::uint32_t interface_index, const Handler &handler);

        std::unique_ptr<nl_sock, SocketDeleter> m_socket;
        int m_netlink_family = -1;
    };
}

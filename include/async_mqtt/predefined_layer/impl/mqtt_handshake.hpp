// Copyright Takatoshi Kondo 2024
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PREDEFINED_LAYER_IMPL_MQTT_HANDSHAKE_HPP)
#define ASYNC_MQTT_PREDEFINED_LAYER_IMPL_MQTT_HANDSHAKE_HPP

#include <async_mqtt/predefined_layer/mqtt.hpp>

namespace async_mqtt {
namespace as = boost::asio;

template <
    typename Socket,
    typename Executor
>
struct mqtt_handshake_op {
    mqtt_handshake_op(
        as::basic_stream_socket<Socket, Executor>& layer,
        std::string_view host,
        std::string_view port
    ):layer{layer},
      host{host},
      port{port}
    {}

    as::basic_stream_socket<Socket, Executor>& layer;
    std::string_view host;
    std::string_view port;

    template <typename Self>
    void operator()(
        Self& self
    ) {
        auto& a_layer{layer};
        auto res = std::make_shared<as::ip::tcp::resolver>(layer.get_executor());
        res->async_resolve(
            host,
            port,
            as::bind_executor(
                a_layer.get_executor(),
                as::consign(
                    force_move(self),
                    res
                )
            )
        );
    }

    template <typename Self>
    void operator()(
        Self& self,
        error_code ec,
        as::ip::tcp::resolver::results_type eps
    ) {
        if (ec) {
            self.complete(ec);
            return;
        }
        auto& a_layer{layer};
        as::async_connect(
            a_layer,
            eps,
            as::bind_executor(
                a_layer.get_executor(),
                force_move(self)
            )
        );
    }

    template <typename Self>
    void operator()(
        Self& self,
        error_code ec,
        as::ip::tcp::endpoint /*unused*/
    ) {
        self.complete(ec);
    }
};

template <
    typename Socket,
    typename Executor,
    typename CompletionToken
>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(error_code)
)
underlying_handshake(
    as::basic_stream_socket<Socket, Executor>& layer,
    std::string_view host,
    std::string_view port,
    CompletionToken&& token
) {
    return
        as::async_compose<
            CompletionToken,
            void(error_code)
        >(
            mqtt_handshake_op{
                layer,
                host,
                port
            },
            token
        );
}

} // namespace async_mqtt

#endif // ASYNC_MQTT_PREDEFINED_LAYER_IMPL_MQTT_HANDSHAKE_HPP

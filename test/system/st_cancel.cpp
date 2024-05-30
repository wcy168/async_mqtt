// Copyright Takatoshi Kondo 2023
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"
#include "broker_runner.hpp"
#include "coro_base.hpp"

#include <async_mqtt/all.hpp>
#include <boost/asio/yield.hpp>

BOOST_AUTO_TEST_SUITE(st_cancel)

namespace am = async_mqtt;
namespace as = boost::asio;

BOOST_AUTO_TEST_CASE(cb) {
    broker_runner br;
    as::io_context ioc;
    using ep_t = am::endpoint<am::role::client, am::protocol::mqtt>;
    auto amep = ep_t::create(
        am::protocol_version::v3_1_1,
        ioc.get_executor()
    );

    as::cancellation_signal sig1;
    bool canceled = false;
    am::async_underlying_handshake(
        amep->next_layer(),
        "127.0.0.1",
        "1883",
        [&](am::error_code const& ec) {
            BOOST_TEST(ec == am::error_code{});
            amep->async_send(
                am::v3_1_1::connect_packet{
                    true,   // clean_session
                    0x1234, // keep_alive
                    "cid1",
                    std::nullopt, // will
                    "u1",
                    "passforu1"
                },
                [&](am::error_code const& ec) {
                    BOOST_TEST(!ec);
                    amep->async_recv(
                        [&](am::error_code const& ec, am::packet_variant pv) {
                            BOOST_TEST(!ec);
                            pv.visit(
                                am::overload {
                                    [&](am::v3_1_1::connack_packet const& p) {
                                        BOOST_TEST(!p.session_present());
                                    },
                                    [](auto const&) {
                                        BOOST_TEST(false);
                                    }
                                }
                            );

                            // test case
                            amep->async_recv(
                                as::bind_cancellation_slot(
                                    sig1.slot(),
                                    [&](am::error_code const& ec, am::packet_variant pv) {
                                        BOOST_TEST(ec == am::errc::operation_canceled);
                                        BOOST_TEST(!pv);
                                        canceled = true;
                                    }
                                )
                            );
                            auto tim = std::make_shared<as::steady_timer>(
                                ioc.get_executor(),
                                std::chrono::milliseconds(100)
                            );
                            tim->async_wait(
                                [&, tim](am::error_code const& ec) {
                                    BOOST_TEST(ec == am::errc::success);
                                    sig1.emit(as::cancellation_type::terminal);
                                }
                            );
                        }
                    );
                }
            );
        }
    );

    ioc.run();
    BOOST_TEST(canceled);
}

BOOST_AUTO_TEST_SUITE_END()

#include <boost/asio/unyield.hpp>
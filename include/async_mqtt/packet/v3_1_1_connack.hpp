// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PACKET_V3_1_1_CONNACK_HPP)
#define ASYNC_MQTT_PACKET_V3_1_1_CONNACK_HPP


#include <async_mqtt/buffer_to_packet_variant.hpp>
#include <async_mqtt/util/buffer.hpp>

#include <async_mqtt/util/static_vector.hpp>

#include <async_mqtt/packet/control_packet_type.hpp>
#include <async_mqtt/packet/connect_return_code.hpp>

/**
 * @defgroup connack_v3_1_1 CONNACK packet (v3.1.1)
 * @ingroup packet_v3_1_1
 */

namespace async_mqtt::v3_1_1 {

namespace as = boost::asio;

/**
 * @ingroup connack_v3_1_1
 * @brief MQTT CONNACK packet (v3.1.1)
 *
 * Only MQTT broker(sever) can send this packet.
 * \n See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718033
 */
class connack_packet {
public:
    /**
     * @brief constructor
     * @param session_present If the broker stores the session, then true, otherwise false.
     *                        When the endpoint receives CONNACK packet with session_present is false,
     *                        then stored packets are erased.
     * @param return_code ConnectReturnCode
     *                    See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc385349256
     */
    connack_packet(
        bool session_present,
        connect_return_code return_code
    );

    /**
     * @brief Get MQTT control packet type
     * @return control packet type
     */
    static constexpr control_packet_type type();

    /**
     * @brief Create const buffer sequence.
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const;

    /**
     * @brief Get packet size.
     * @return packet size
     */
    std::size_t size() const;

    /**
     * @brief Get number of element of const_buffer_sequence.
     * @return number of element of const_buffer_sequence
     */
    static constexpr std::size_t num_of_const_buffer_sequence();

    /**
     * @brief Get session_present.
     * @return session_present
     */
    bool session_present() const;

    /**
     * @brief Get connect_return_code.
     * @return connect_return_code
     */
    connect_return_code code() const;

private:

    template <std::size_t PacketIdBytesArg>
    friend basic_packet_variant<PacketIdBytesArg>
    async_mqtt::buffer_to_basic_packet_variant(buffer buf, protocol_version ver);

#if defined(ASYNC_MQTT_UNIT_TEST_FOR_PACKET)
    friend struct ::ut_packet::v311_connack;
#endif // defined(ASYNC_MQTT_UNIT_TEST_FOR_PACKET)

    // private constructor for internal use
    connack_packet(buffer buf);

private:
    static_vector<char, 4> all_;
};

/**
 * @related connack_packet
 * @brief stream output operator
 * @param o output stream
 * @param v target
 * @return  output stream
 */
std::ostream& operator<<(std::ostream& o, connack_packet const& v);

} // namespace async_mqtt::v3_1_1

#include <async_mqtt/packet/impl/v3_1_1_connack.hpp>

#endif // ASYNC_MQTT_PACKET_V3_1_1_CONNACK_HPP

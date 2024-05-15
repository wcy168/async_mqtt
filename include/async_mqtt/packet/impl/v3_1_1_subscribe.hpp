// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PACKET_IMPL_V3_1_1_SUBSCRIBE_HPP)
#define ASYNC_MQTT_PACKET_IMPL_V3_1_1_SUBSCRIBE_HPP

#include <boost/numeric/conversion/cast.hpp>

#include <async_mqtt/packet/v3_1_1_subscribe.hpp>
#include <async_mqtt/exception.hpp>
#include <async_mqtt/util/buffer.hpp>

#include <async_mqtt/util/move.hpp>
#include <async_mqtt/util/static_vector.hpp>
#include <async_mqtt/util/endian_convert.hpp>
#include <async_mqtt/util/utf8validate.hpp>

#include <async_mqtt/packet/packet_id_type.hpp>
#include <async_mqtt/packet/fixed_header.hpp>
#include <async_mqtt/packet/topic_subopts.hpp>
#include <async_mqtt/variable_bytes.hpp>
#include <async_mqtt/packet/copy_to_static_vector.hpp>

namespace async_mqtt::v3_1_1 {

namespace as = boost::asio;

template <std::size_t PacketIdBytes>
inline
basic_subscribe_packet<PacketIdBytes>::basic_subscribe_packet(
    packet_id_t packet_id,
    std::vector<topic_subopts> params
)
    : fixed_header_{make_fixed_header(control_packet_type::subscribe, 0b0010)},
      entries_{force_move(params)},
      remaining_length_{PacketIdBytes}
{
    topic_length_buf_entries_.reserve(entries_.size());
    for (auto const& e : entries_) {
        topic_length_buf_entries_.push_back(
            endian_static_vector(
                boost::numeric_cast<std::uint16_t>(e.all_topic().size())
            )
        );
    }

    endian_store(packet_id, packet_id_.data());

    for (auto const& e : entries_) {
        // reserved bits check
        if (static_cast<std::uint8_t>(e.opts()) & 0b11111100) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet subopts is invalid"
            );
        }
        switch (e.opts().get_qos()) {
        case qos::at_most_once:
        case qos::at_least_once:
        case qos::exactly_once:
            break;
        default:
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet qos is invalid"
            );
            break;
        }

        auto size = e.all_topic().size();
        if (size > 0xffff) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet length of topic is invalid"
            );
        }
        remaining_length_ +=
            2 +                     // topic filter length
            size +                  // topic filter
            1;                      // opts

        if (!utf8string_check(e.all_topic())) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet topic filter invalid utf8"
            );
        }
    }

    remaining_length_buf_ = val_to_variable_bytes(boost::numeric_cast<std::uint32_t>(remaining_length_));
}

template <std::size_t PacketIdBytes>
inline
constexpr control_packet_type basic_subscribe_packet<PacketIdBytes>::type() {
    return control_packet_type::subscribe;
}

template <std::size_t PacketIdBytes>
inline
std::vector<as::const_buffer> basic_subscribe_packet<PacketIdBytes>::const_buffer_sequence() const {
    std::vector<as::const_buffer> ret;
    ret.reserve(num_of_const_buffer_sequence());

    ret.emplace_back(as::buffer(&fixed_header_, 1));

    ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));

    ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));

    BOOST_ASSERT(entries_.size() == topic_length_buf_entries_.size());
    auto it = topic_length_buf_entries_.begin();
    for (auto const& e : entries_) {
        ret.emplace_back(as::buffer(it->data(), it->size()));
        ret.emplace_back(as::buffer(e.all_topic()));
        ret.emplace_back(as::buffer(&e.opts(), 1));
        ++it;
    }

    return ret;
}

template <std::size_t PacketIdBytes>
inline
std::size_t basic_subscribe_packet<PacketIdBytes>::size() const {
    return
        1 +                            // fixed header
        remaining_length_buf_.size() +
        remaining_length_;
}

template <std::size_t PacketIdBytes>
inline
std::size_t basic_subscribe_packet<PacketIdBytes>::num_of_const_buffer_sequence() const {
    return
        1 +                   // fixed header
        1 +                   // remaining length
        1 +                   // packet id
        entries_.size() * 3;  // topic name length, topic name, qos
}

template <std::size_t PacketIdBytes>
inline
typename basic_subscribe_packet<PacketIdBytes>::packet_id_t basic_subscribe_packet<PacketIdBytes>::packet_id() const {
    return endian_load<packet_id_t>(packet_id_.data());
}

template <std::size_t PacketIdBytes>
inline
std::vector<topic_subopts> const& basic_subscribe_packet<PacketIdBytes>::entries() const {
    return entries_;
}

template <std::size_t PacketIdBytes>
inline
basic_subscribe_packet<PacketIdBytes>::basic_subscribe_packet(buffer buf) {
    // fixed_header
    if (buf.empty()) {
        throw make_error(
            errc::bad_message,
            "v3_1_1::subscribe_packet fixed_header doesn't exist"
        );
    }
    fixed_header_ = static_cast<std::uint8_t>(buf.front());
    buf.remove_prefix(1);
    auto cpt_opt = get_control_packet_type_with_check(static_cast<std::uint8_t>(fixed_header_));
    if (!cpt_opt || *cpt_opt != control_packet_type::subscribe) {
        throw make_error(
            errc::bad_message,
            "v3_1_1::subscribe_packet fixed_header is invalid"
        );
    }

    // remaining_length
    if (auto vl_opt = insert_advance_variable_length(buf, remaining_length_buf_)) {
        remaining_length_ = *vl_opt;
    }
    else {
        throw make_error(errc::bad_message, "v3_1_1::subscribe_packet remaining length is invalid");
    }
    if (remaining_length_ != buf.size()) {
        throw make_error(errc::bad_message, "v3_1_1::subscribe_packet remaining length doesn't match buf.size()");
    }

    // packet_id
    if (!copy_advance(buf, packet_id_)) {
        throw make_error(
            errc::bad_message,
            "v3_1_1::subscribe_packet packet_id doesn't exist"
        );
    }

    if (remaining_length_ == 0) {
        throw make_error(errc::bad_message, "v3_1_1::subscribe_packet doesn't have entries");
    }

    while (!buf.empty()) {
        // topic_length
        static_vector<char, 2> topic_length_buf;
        if (!insert_advance(buf, topic_length_buf)) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet length of topic is invalid"
            );
        }
        auto topic_length = endian_load<std::uint16_t>(topic_length_buf.data());
        topic_length_buf_entries_.push_back(topic_length_buf);

        // topic
        if (buf.size() < topic_length) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet topic doesn't match its length"
            );
        }
        auto topic = buf.substr(0, topic_length);

        if (!utf8string_check(topic)) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet topic filter invalid utf8"
            );
        }

        buf.remove_prefix(topic_length);

        // opts
        if (buf.empty()) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet subscribe options  doesn't exist"
            );
        }
        auto opts = static_cast<sub::opts>(std::uint8_t(buf.front()));
        if (static_cast<std::uint8_t>(opts) & 0b11111100) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet subopts is invalid"
            );
        }
        switch (opts.get_qos()) {
        case qos::at_most_once:
        case qos::at_least_once:
        case qos::exactly_once:
            break;
        default:
            throw make_error(
                errc::bad_message,
                "v3_1_1::subscribe_packet qos is invalid"
            );
            break;
        }
        entries_.emplace_back(std::string{topic}, opts);
        buf.remove_prefix(1);
    }
}

template <std::size_t PacketIdBytes>
inline
std::ostream& operator<<(std::ostream& o, basic_subscribe_packet<PacketIdBytes> const& v) {
    o <<
        "v3_1_1::subscribe{" <<
        "pid:" << v.packet_id() << ",[";
    auto b = v.entries().cbegin();
    auto e = v.entries().cend();
    if (b != e) {
        o <<
            "{topic:" <<
            b->all_topic() << "," <<
            "qos:" << b->opts().get_qos() << "}";
        ++b;
    }
    for (; b != e; ++b) {
        o << "," <<
            "{topic:" <<
            b->all_topic() << "," <<
            "qos:" << b->opts().get_qos() << "}";
    }
    o << "]}";
    return o;
}

} // namespace async_mqtt::v3_1_1

#endif // ASYNC_MQTT_PACKET_IMPL_V3_1_1_SUBSCRIBE_HPP

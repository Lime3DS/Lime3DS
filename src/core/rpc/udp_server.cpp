// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <thread>
#include <boost/asio.hpp>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/rpc/packet.h"
#include "core/rpc/udp_server.h"

namespace Core::RPC {

class UDPServer::Impl {
public:
    explicit Impl(std::function<void(std::unique_ptr<Packet>)> new_request_callback)
        // Use a random high port
        // TODO: Make configurable or increment port number on failure
        : socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 45987)),
          new_request_callback(std::move(new_request_callback)) {

        StartReceive();
        worker_thread = std::thread([this] { io_context.run(); });
    }

    ~Impl() {
        io_context.stop();
        worker_thread.join();
    }

private:
    void StartReceive() {
        socket.async_receive_from(boost::asio::buffer(request_buffer), remote_endpoint,
                                  [this](const boost::system::error_code& error, std::size_t size) {
                                      HandleReceive(error, size);
                                  });
    }

    void HandleReceive(const boost::system::error_code& error, std::size_t size) {
        if (error) {
            LOG_WARNING(RPC_Server, "Failed to receive data on UDP socket: {}", error.message());
        } else if (size >= MIN_PACKET_SIZE && size <= MAX_PACKET_SIZE) {
            PacketHeader header;
            std::memcpy(&header, request_buffer.data(), sizeof(header));
            if ((size - MIN_PACKET_SIZE) == header.packet_size) {
                u8* data = request_buffer.data() + MIN_PACKET_SIZE;
                std::function<void(Packet&)> send_reply_callback =
                    std::bind(&Impl::SendReply, this, remote_endpoint, std::placeholders::_1);
                std::unique_ptr<Packet> new_packet =
                    std::make_unique<Packet>(header, data, send_reply_callback);

                // Send the request to the upper layer for handling
                new_request_callback(std::move(new_packet));
            }
        } else {
            LOG_WARNING(RPC_Server, "Received message with wrong size: {}", size);
        }
        StartReceive();
    }

    void SendReply(boost::asio::ip::udp::endpoint endpoint, Packet& reply_packet) {
        std::vector<u8> reply_buffer(MIN_PACKET_SIZE + reply_packet.GetPacketDataSize());
        auto reply_header = reply_packet.GetHeader();

        std::memcpy(reply_buffer.data(), &reply_header, sizeof(reply_header));
        std::memcpy(reply_buffer.data() + (4 * sizeof(u32)), reply_packet.GetPacketData().data(),
                    reply_packet.GetPacketDataSize());

        boost::system::error_code error;
        socket.send_to(boost::asio::buffer(reply_buffer), endpoint, 0, error);

        if (error) {
            LOG_WARNING(RPC_Server, "Failed to send reply: {}", error.message());
        } else {
            LOG_INFO(RPC_Server, "Sent reply version({}) id=({}) type=({}) size=({})",
                     reply_packet.GetVersion(), reply_packet.GetId(), reply_packet.GetPacketType(),
                     reply_packet.GetPacketDataSize());
        }
    }

    std::thread worker_thread;

    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket;
    std::array<u8, MAX_PACKET_SIZE> request_buffer;
    boost::asio::ip::udp::endpoint remote_endpoint;

    std::function<void(std::unique_ptr<Packet>)> new_request_callback;
};

UDPServer::UDPServer(std::function<void(std::unique_ptr<Packet>)> new_request_callback)
    : impl(std::make_unique<Impl>(new_request_callback)) {}

UDPServer::~UDPServer() = default;

} // namespace Core::RPC

// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include "common/logging/log.h"
#include "input_common/udp/client.h"
#include "input_common/udp/protocol.h"

using boost::asio::ip::udp;

namespace InputCommon::CemuhookUDP {

struct SocketCallback {
    std::function<void(Response::Version)> version;
    std::function<void(Response::PortInfo)> port_info;
    std::function<void(Response::PadData)> pad_data;
};

class Socket {
public:
    using clock = std::chrono::system_clock;

    explicit Socket(const std::string& host, u16 port, u8 pad_index, u32 client_id,
                    SocketCallback callback)
        : callback(std::move(callback)), timer(io_service),
          socket(io_service, udp::endpoint(udp::v4(), 0)), client_id(client_id),
          pad_index(pad_index) {
        boost::system::error_code ec{};
        auto ipv4 = boost::asio::ip::make_address_v4(host, ec);
        if (ec.value() != boost::system::errc::success) {
            LOG_ERROR(Input, "Invalid IPv4 address \"{}\" provided to socket", host);
            ipv4 = boost::asio::ip::address_v4{};
        }

        send_endpoint = {udp::endpoint(ipv4, port)};
    }

    void Stop() {
        io_service.stop();
    }

    void Loop() {
        io_service.run();
    }

    void StartSend(const clock::time_point& from) {
        timer.expires_at(from + std::chrono::seconds(3));
        timer.async_wait([this](const boost::system::error_code& error) { HandleSend(error); });
    }

    void StartReceive() {
        socket.async_receive_from(
            boost::asio::buffer(receive_buffer), receive_endpoint,
            [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
                HandleReceive(error, bytes_transferred);
            });
    }

private:
    void HandleReceive(const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (auto type = Response::Validate(receive_buffer.data(), bytes_transferred)) {
            switch (*type) {
            case Type::Version: {
                Response::Version version;
                std::memcpy(&version, &receive_buffer[sizeof(Header)], sizeof(Response::Version));
                callback.version(std::move(version));
                break;
            }
            case Type::PortInfo: {
                Response::PortInfo port_info;
                std::memcpy(&port_info, &receive_buffer[sizeof(Header)],
                            sizeof(Response::PortInfo));
                callback.port_info(std::move(port_info));
                break;
            }
            case Type::PadData: {
                Response::PadData pad_data;
                std::memcpy(&pad_data, &receive_buffer[sizeof(Header)], sizeof(Response::PadData));
                callback.pad_data(std::move(pad_data));
                break;
            }
            }
        }
        StartReceive();
    }

    void HandleSend(const boost::system::error_code& error) {
        boost::system::error_code _ignored{};
        // Send a request for getting port info for the pad
        Request::PortInfo port_info{1, {pad_index, 0, 0, 0}};
        const auto port_message = Request::Create(port_info, client_id);
        std::memcpy(&send_buffer1, &port_message, PORT_INFO_SIZE);
        socket.send_to(boost::asio::buffer(send_buffer1), send_endpoint, {}, _ignored);

        // Send a request for getting pad data for the pad
        Request::PadData pad_data{Request::PadData::Flags::Id, pad_index, EMPTY_MAC_ADDRESS};
        const auto pad_message = Request::Create(pad_data, client_id);
        std::memcpy(send_buffer2.data(), &pad_message, PAD_DATA_SIZE);
        socket.send_to(boost::asio::buffer(send_buffer2), send_endpoint, {}, _ignored);
        StartSend(timer.expiry());
    }

    SocketCallback callback;
    boost::asio::io_service io_service;
    boost::asio::basic_waitable_timer<clock> timer;
    udp::socket socket;

    u32 client_id{};
    u8 pad_index{};

    static constexpr std::size_t PORT_INFO_SIZE = sizeof(Message<Request::PortInfo>);
    static constexpr std::size_t PAD_DATA_SIZE = sizeof(Message<Request::PadData>);
    std::array<u8, PORT_INFO_SIZE> send_buffer1;
    std::array<u8, PAD_DATA_SIZE> send_buffer2;
    udp::endpoint send_endpoint;

    std::array<u8, MAX_PACKET_SIZE> receive_buffer;
    udp::endpoint receive_endpoint;
};

static void SocketLoop(Socket* socket) {
    socket->StartReceive();
    socket->StartSend(Socket::clock::now());
    socket->Loop();
}

Client::Client(std::shared_ptr<DeviceStatus> status, const std::string& host, u16 port,
               u8 pad_index, u32 client_id)
    : status(std::move(status)) {
    StartCommunication(host, port, pad_index, client_id);
}

Client::~Client() {
    socket->Stop();
    thread.join();
}

void Client::ReloadSocket(const std::string& host, u16 port, u8 pad_index, u32 client_id) {
    socket->Stop();
    thread.join();
    StartCommunication(host, port, pad_index, client_id);
}

void Client::OnVersion(Response::Version data) {
    LOG_TRACE(Input, "Version packet received: {}", data.version);
}

void Client::OnPortInfo(Response::PortInfo data) {
    LOG_TRACE(Input, "PortInfo packet received: {}", data.model);
}

void Client::OnPadData(Response::PadData data) {
    LOG_TRACE(Input, "PadData packet received");
    if (data.packet_counter <= packet_sequence) {
        LOG_WARNING(
            Input,
            "PadData packet dropped because its stale info. Current count: {} Packet count: {}",
            packet_sequence, data.packet_counter);
        return;
    }
    packet_sequence = data.packet_counter;
    // Due to differences between the 3ds and cemuhookudp motion directions, we need to invert
    // accel.x and accel.z and also invert pitch and yaw. See
    // https://github.com/citra-emu/citra/pull/4049 for more details on gyro/accel
    Common::Vec3f accel = Common::MakeVec<float>(-data.accel.x, data.accel.y, -data.accel.z);
    Common::Vec3f gyro = Common::MakeVec<float>(-data.gyro.pitch, -data.gyro.yaw, data.gyro.roll);
    {
        std::lock_guard guard(status->update_mutex);

        status->motion_status = {accel, gyro};

        // TODO: add a setting for "click" touch. Click touch refers to a device that differentiates
        // between a simple "tap" and a hard press that causes the touch screen to click.
        const bool is_active = data.touch_1.is_active != 0;

        float x = 0;
        float y = 0;

        if (is_active && status->touch_calibration) {
            const u16 min_x = status->touch_calibration->min_x;
            const u16 max_x = status->touch_calibration->max_x;
            const u16 min_y = status->touch_calibration->min_y;
            const u16 max_y = status->touch_calibration->max_y;

            x = (std::clamp(static_cast<u16>(data.touch_1.x), min_x, max_x) - min_x) /
                static_cast<float>(max_x - min_x);
            y = (std::clamp(static_cast<u16>(data.touch_1.y), min_y, max_y) - min_y) /
                static_cast<float>(max_y - min_y);
        }

        status->touch_status = {x, y, is_active};
    }
}

void Client::StartCommunication(const std::string& host, u16 port, u8 pad_index, u32 client_id) {
    SocketCallback callback{[this](Response::Version version) { OnVersion(version); },
                            [this](Response::PortInfo info) { OnPortInfo(info); },
                            [this](Response::PadData data) { OnPadData(data); }};
    LOG_INFO(Input, "Starting communication with UDP input server on {}:{}", host, port);
    socket = std::make_unique<Socket>(host, port, pad_index, client_id, callback);
    thread = std::thread{SocketLoop, this->socket.get()};
}

void TestCommunication(const std::string& host, u16 port, u8 pad_index, u32 client_id,
                       const std::function<void()>& success_callback,
                       const std::function<void()>& failure_callback) {
    std::thread([=] {
        Common::Event success_event;
        SocketCallback callback{[](Response::Version version) {}, [](Response::PortInfo info) {},
                                [&](Response::PadData data) { success_event.Set(); }};
        Socket socket{host, port, pad_index, client_id, std::move(callback)};
        std::thread worker_thread{SocketLoop, &socket};
        bool result = success_event.WaitFor(std::chrono::seconds(8));
        socket.Stop();
        worker_thread.join();
        if (result) {
            success_callback();
        } else {
            failure_callback();
        }
    }).detach();
}

CalibrationConfigurationJob::CalibrationConfigurationJob(
    const std::string& host, u16 port, u8 pad_index, u32 client_id,
    std::function<void(Status)> status_callback,
    std::function<void(u16, u16, u16, u16)> data_callback) {

    std::thread([=, this] {
        u16 min_x{UINT16_MAX};
        u16 min_y{UINT16_MAX};
        u16 max_x{};
        u16 max_y{};

        Status current_status{Status::Initialized};
        SocketCallback callback{[](Response::Version version) {}, [](Response::PortInfo info) {},
                                [&](Response::PadData data) {
                                    constexpr u16 CALIBRATION_THRESHOLD = 100;

                                    if (current_status == Status::Initialized) {
                                        // Receiving data means the communication is ready now
                                        current_status = Status::Ready;
                                        status_callback(current_status);
                                    }
                                    if (!data.touch_1.is_active) {
                                        return;
                                    }
                                    LOG_DEBUG(Input, "Current touch: {} {}", data.touch_1.x,
                                              data.touch_1.y);
                                    min_x = std::min(min_x, static_cast<u16>(data.touch_1.x));
                                    min_y = std::min(min_y, static_cast<u16>(data.touch_1.y));
                                    if (current_status == Status::Ready) {
                                        // First touch - min data (min_x/min_y)
                                        current_status = Status::Stage1Completed;
                                        status_callback(current_status);
                                    }
                                    if (data.touch_1.x - min_x > CALIBRATION_THRESHOLD &&
                                        data.touch_1.y - min_y > CALIBRATION_THRESHOLD) {
                                        // Set the current position as max value and finishes
                                        // configuration
                                        max_x = data.touch_1.x;
                                        max_y = data.touch_1.y;
                                        current_status = Status::Completed;
                                        data_callback(min_x, min_y, max_x, max_y);
                                        status_callback(current_status);

                                        complete_event.Set();
                                    }
                                }};
        Socket socket{host, port, pad_index, client_id, std::move(callback)};
        std::thread worker_thread{SocketLoop, &socket};
        complete_event.Wait();
        socket.Stop();
        worker_thread.join();
    }).detach();
}

CalibrationConfigurationJob::~CalibrationConfigurationJob() {
    Stop();
}

void CalibrationConfigurationJob::Stop() {
    complete_event.Set();
}

} // namespace InputCommon::CemuhookUDP

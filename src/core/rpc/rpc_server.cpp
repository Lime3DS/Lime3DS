// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "core/memory.h"
#include "core/rpc/packet.h"
#include "core/rpc/rpc_server.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"

namespace Core::RPC {

RPCServer::RPCServer(Core::System& system_) : system{system_} {
    LOG_INFO(RPC_Server, "Starting RPC server.");
    request_handler_thread =
        std::jthread([this](std::stop_token stop_token) { HandleRequestsLoop(stop_token); });
}

RPCServer::~RPCServer() = default;

void RPCServer::HandleReadMemory(Packet& packet, u32 address, u32 data_size) {
    if (data_size > MAX_READ_SIZE) {
        return;
    }

    // Note: Memory read occurs asynchronously from the state of the emulator
    system.Memory().ReadBlock(address, packet.GetPacketData().data(), data_size);
    packet.SetPacketDataSize(data_size);
    packet.SendReply();
}

void RPCServer::HandleWriteMemory(Packet& packet, u32 address, std::span<const u8> data) {
    // Only allow writing to certain memory regions
    if ((address >= Memory::PROCESS_IMAGE_VADDR && address <= Memory::PROCESS_IMAGE_VADDR_END) ||
        (address >= Memory::HEAP_VADDR && address <= Memory::HEAP_VADDR_END) ||
        (address >= Memory::N3DS_EXTRA_RAM_VADDR && address <= Memory::N3DS_EXTRA_RAM_VADDR_END)) {
        // Note: Memory write occurs asynchronously from the state of the emulator
        system.Memory().WriteBlock(address, data.data(), data.size());
        // If the memory happens to be executable code, make sure the changes become visible

        // Is current core correct here?
        system.InvalidateCacheRange(address, data.size());
    }
    packet.SetPacketDataSize(0);
    packet.SendReply();
}

void RPCServer::HandleSendKey(Packet& packet, u32 key_code, u8 state) {
    if (state == 0) {
        InputCommon::GetKeyboard()->ReleaseKey(key_code);
    } else if (state == 1) {
        InputCommon::GetKeyboard()->PressKey(key_code);
    }
    packet.SetPacketDataSize(0);
    packet.SendReply();
}

void RPCServer::HandleSendSignal(Packet& packet, u32 signal_code, u32 signal_parameter) {
    system.SendSignal(static_cast<Core::System::Signal>(signal_code), signal_parameter);

    packet.SetPacketDataSize(0);
    packet.SendReply();
}

bool RPCServer::ValidatePacket(const PacketHeader& packet_header) {
    if (packet_header.version <= CURRENT_VERSION) {
        switch (packet_header.packet_type) {
        case PacketType::ReadMemory:
        case PacketType::WriteMemory:
            if (packet_header.packet_size >= (sizeof(u32) * 2)) {
                return true;
            }
            break;
        case PacketType::SendKey:
            if (packet_header.packet_size >= (sizeof(u32) + sizeof(u8))) {
                return true;
            }
            break;
        case PacketType::SendSignal:
            if (packet_header.packet_size >= sizeof(u32)) {
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}

void RPCServer::HandleSingleRequest(std::unique_ptr<Packet> request_packet) {
    bool success = false;
    const auto packet_data = request_packet->GetPacketData();

    if (ValidatePacket(request_packet->GetHeader())) {
        u32 address = 0;
        u32 data_size = 0;
        u32 key_code = 0;
        u8 key_state = 0;
        u32 signal_code = 0;
        u32 signal_parameter = 0;
        switch (request_packet->GetPacketType()) {
        case PacketType::ReadMemory:
            std::memcpy(&address, packet_data.data(), sizeof(address));
            std::memcpy(&data_size, packet_data.data() + sizeof(address), sizeof(data_size));
            if (data_size > 0 && data_size <= MAX_READ_SIZE) {
                HandleReadMemory(*request_packet, address, data_size);
                success = true;
            }
        case PacketType::WriteMemory:
            std::memcpy(&address, packet_data.data(), sizeof(address));
            std::memcpy(&data_size, packet_data.data() + sizeof(address), sizeof(data_size));
            if (data_size > 0 && data_size <= MAX_PACKET_DATA_SIZE - (sizeof(u32) * 2)) {
                const auto data = packet_data.subspan(sizeof(u32) * 2, data_size);
                HandleWriteMemory(*request_packet, address, data);
                success = true;
            }
            break;
        case PacketType::SendKey:
            std::memcpy(&key_code, packet_data.data(), sizeof(key_code));
            std::memcpy(&key_state, packet_data.data() + sizeof(key_code), sizeof(key_state));
            HandleSendKey(*request_packet, key_code, key_state);
            success = true;
            break;
        case PacketType::SendSignal:
            std::memcpy(&signal_code, packet_data.data(), sizeof(signal_code));
            std::memcpy(&signal_parameter, packet_data.data() + sizeof(signal_code),
                        sizeof(signal_parameter));
            HandleSendSignal(*request_packet, signal_code, signal_parameter);
            break;
        default:
            break;
        }
    }

    if (!success) {
        // Send an empty reply, so as not to hang the client
        request_packet->SetPacketDataSize(0);
        request_packet->SendReply();
    }
}

void RPCServer::HandleRequestsLoop(std::stop_token stop_token) {
    std::unique_ptr<RPC::Packet> request_packet;

    LOG_INFO(RPC_Server, "Request handler started.");

    while ((request_packet = request_queue.PopWait(stop_token))) {
        HandleSingleRequest(std::move(request_packet));
    }
}

void RPCServer::QueueRequest(std::unique_ptr<RPC::Packet> request) {
    request_queue.Push(std::move(request));
}

}; // namespace Core::RPC

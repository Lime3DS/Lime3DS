// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>
#include <boost/crc.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <fmt/format.h>
#include "common/archives.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/ir/extra_hid.h"
#include "core/hle/service/ir/ir_user.h"

SERIALIZE_EXPORT_IMPL(Service::IR::IR_USER)
SERVICE_CONSTRUCT_IMPL(Service::IR::IR_USER)

namespace Service::IR {

template <class Archive>
void IR_USER::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar & conn_status_event;
    ar & send_event;
    ar & receive_event;
    ar & shared_memory;
    ar & connected_device;
    ar & receive_buffer;
    ar&* extra_hid.get();
}

// This is a header that will present in the ir:USER shared memory if it is initialized with
// InitializeIrNopShared service function. Otherwise the shared memory doesn't have this header if
// it is initialized with InitializeIrNop service function.
struct SharedMemoryHeader {
    u32_le latest_receive_error_result;
    u32_le latest_send_error_result;
    // TODO(wwylele): for these fields below, make them enum when the meaning of values is known.
    u8 connection_status;
    u8 trying_to_connect_status;
    u8 connection_role;
    u8 machine_id;
    u8 connected;
    u8 network_id;
    u8 initialized;
    u8 unknown;

    // This is not the end of the shared memory. It is followed by a receive buffer and a send
    // buffer. We handle receive buffer in the BufferManager class. For the send buffer, because
    // games usually don't access it, we don't emulate it.
};
static_assert(sizeof(SharedMemoryHeader) == 16, "SharedMemoryHeader has wrong size!");

/**
 * A manager of the send/receive buffers in the shared memory. Currently it is only used for the
 * receive buffer.
 *
 * A buffer consists of three parts:
 *     - BufferInfo: stores available count of packets, and their position in the PacketInfo
 *         circular queue.
 *     - PacketInfo circular queue: stores the position of each avaiable packets in the Packet data
 *         buffer. Each entry is a pair of {offset, size}.
 *     - Packet data circular buffer: stores the actual data of packets.
 *
 * IR packets can be put into and get from the buffer.
 *
 * When a new packet is put into the buffer, its data is put into the data circular buffer,
 * following the end of previous packet data. A new entry is also added to the PacketInfo circular
 * queue pointing to the added packet data. Then BufferInfo is updated.
 *
 * Packets can be released from the other end of the buffer. When releasing a packet, the front
 * entry in thePacketInfo circular queue is removed, and as a result the corresponding memory in the
 * data circular buffer is also released. BufferInfo is updated as well.
 *
 * The client application usually has a similar manager constructed over the same shared memory
 * region, performing the same put/get/release operation. This way the client and the service
 * communicate via a pair of manager of the same buffer.
 *
 * TODO(wwylele): implement Get function, which is used by ReceiveIrnop service function.
 */
class BufferManager {
public:
    BufferManager(std::shared_ptr<Kernel::SharedMemory> shared_memory_, u32 info_offset_,
                  u32 buffer_offset_, u32 max_packet_count_, u32 buffer_size)
        : shared_memory(shared_memory_), info_offset(info_offset_), buffer_offset(buffer_offset_),
          max_packet_count(max_packet_count_),
          max_data_size(buffer_size - sizeof(PacketInfo) * max_packet_count_) {
        UpdateBufferInfo();
    }

    /**
     * Puts a packet to the head of the buffer.
     * @params packet The data of the packet to put.
     * @returns whether the operation is successful.
     */
    bool Put(std::span<const u8> packet) {
        if (info.packet_count == max_packet_count) {
            return false;
        }

        u32 write_offset;

        // finds free space offset in data buffer
        if (info.packet_count == 0) {
            write_offset = 0;
            if (packet.size() > max_data_size)
                return false;
        } else {
            const u32 last_index = (info.end_index + max_packet_count - 1) % max_packet_count;
            const PacketInfo first = GetPacketInfo(info.begin_index);
            const PacketInfo last = GetPacketInfo(last_index);
            write_offset = (last.offset + last.size) % max_data_size;
            const u32 free_space = (first.offset + max_data_size - write_offset) % max_data_size;
            if (packet.size() > free_space)
                return false;
        }

        // writes packet info
        PacketInfo packet_info{write_offset, static_cast<u32>(packet.size())};
        SetPacketInfo(info.end_index, packet_info);

        // writes packet data
        for (std::size_t i = 0; i < packet.size(); ++i) {
            *GetDataBufferPointer((write_offset + i) % max_data_size) = packet[i];
        }

        // updates buffer info
        info.end_index++;
        info.end_index %= max_packet_count;
        info.packet_count++;
        UpdateBufferInfo();
        return true;
    }

    /**
     * Release packets from the tail of the buffer
     * @params count Numbers of packets to release.
     * @returns whether the operation is successful.
     */
    bool Release(u32 count) {
        if (info.packet_count < count)
            return false;

        info.packet_count -= count;
        info.begin_index += count;
        info.begin_index %= max_packet_count;
        UpdateBufferInfo();
        return true;
    }

private:
    struct BufferInfo {
        u32_le begin_index;
        u32_le end_index;
        u32_le packet_count;
        u32_le unknown;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & begin_index;
            ar & end_index;
            ar & packet_count;
            ar & unknown;
        }
        friend class boost::serialization::access;
    };
    static_assert(sizeof(BufferInfo) == 16, "BufferInfo has wrong size!");

    struct PacketInfo {
        u32_le offset;
        u32_le size;
    };
    static_assert(sizeof(PacketInfo) == 8, "PacketInfo has wrong size!");

    u8* GetPacketInfoPointer(u32 index) {
        return shared_memory->GetPointer(buffer_offset + sizeof(PacketInfo) * index);
    }

    void SetPacketInfo(u32 index, const PacketInfo& packet_info) {
        std::memcpy(GetPacketInfoPointer(index), &packet_info, sizeof(PacketInfo));
    }

    PacketInfo GetPacketInfo(u32 index) {
        PacketInfo packet_info;
        std::memcpy(&packet_info, GetPacketInfoPointer(index), sizeof(PacketInfo));
        return packet_info;
    }

    u8* GetDataBufferPointer(u32 offset) {
        return shared_memory->GetPointer(buffer_offset + sizeof(PacketInfo) * max_packet_count +
                                         offset);
    }

    void UpdateBufferInfo() {
        if (info_offset) {
            std::memcpy(shared_memory->GetPointer(info_offset), &info, sizeof(info));
        }
    }

    BufferInfo info{0, 0, 0, 0};
    std::shared_ptr<Kernel::SharedMemory> shared_memory;
    u32 info_offset;
    u32 buffer_offset;
    u32 max_packet_count;
    u32 max_data_size;

private:
    BufferManager() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & info;
        ar & shared_memory;
        ar & info_offset;
        ar & buffer_offset;
        ar & max_packet_count;
        ar & max_data_size;
    }
    friend class boost::serialization::access;
};

/// Wraps the payload into packet and puts it to the receive buffer
void IR_USER::PutToReceive(std::span<const u8> payload) {
    LOG_TRACE(Service_IR, "called, data={}", fmt::format("{:02x}", fmt::join(payload, " ")));
    std::size_t size = payload.size();

    std::vector<u8> packet;

    // Builds packet header. For the format info:
    // https://www.3dbrew.org/wiki/IRUSER_Shared_Memory#Packet_structure

    // fixed value
    packet.push_back(0xA5);
    // destination network ID
    u8 network_id = *(shared_memory->GetPointer(offsetof(SharedMemoryHeader, network_id)));
    packet.push_back(network_id);

    // puts the size info.
    // The highest bit of the first byte is unknown, which is set to zero here. The second highest
    // bit is a flag that determines whether the size info is in extended form. If the packet size
    // can be represent within 6 bits, the short form (1 byte) of size info is chosen, the size is
    // put to the lower bits of this byte, and the flag is clear. If the packet size cannot be
    // represent within 6 bits, the extended form (2 bytes) is chosen, the lower 8 bits of the size
    // is put to the second byte, the higher bits of the size is put to the lower bits of the first
    // byte, and the flag is set. Note that the packet size must be within 14 bits due to this
    // format restriction, or it will overlap with the flag bit.
    if (size < 0x40) {
        packet.push_back(static_cast<u8>(size));
    } else if (size < 0x4000) {
        packet.push_back(static_cast<u8>(size >> 8) | 0x40);
        packet.push_back(static_cast<u8>(size));
    } else {
        ASSERT(false);
    }

    // puts the payload
    packet.insert(packet.end(), payload.begin(), payload.end());

    // calculates CRC and puts to the end
    packet.push_back(boost::crc<8, 0x07, 0, 0, false, false>(packet.data(), packet.size()));

    if (receive_buffer->Put(packet)) {
        receive_event->Signal();
    } else {
        LOG_ERROR(Service_IR, "receive buffer is full!");
    }
}

void IR_USER::InitializeIrNopShared(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 shared_buff_size = rp.Pop<u32>();
    const u32 recv_buff_size = rp.Pop<u32>();
    const u32 recv_buff_packet_count = rp.Pop<u32>();
    const u32 send_buff_size = rp.Pop<u32>();
    const u32 send_buff_packet_count = rp.Pop<u32>();
    const u8 baud_rate = rp.Pop<u8>();
    shared_memory = rp.PopObject<Kernel::SharedMemory>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    shared_memory->SetName("IR_USER: shared memory");

    receive_buffer = std::make_unique<BufferManager>(shared_memory, 0x10, 0x20,
                                                     recv_buff_packet_count, recv_buff_size);
    SharedMemoryHeader shared_memory_init{};
    shared_memory_init.initialized = 1;
    std::memcpy(shared_memory->GetPointer(), &shared_memory_init, sizeof(SharedMemoryHeader));

    rb.Push(ResultSuccess);

    LOG_INFO(Service_IR,
             "called, shared_buff_size={}, recv_buff_size={}, "
             "recv_buff_packet_count={}, send_buff_size={}, "
             "send_buff_packet_count={}, baud_rate={}",
             shared_buff_size, recv_buff_size, recv_buff_packet_count, send_buff_size,
             send_buff_packet_count, baud_rate);
}

void IR_USER::RequireConnection(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u8 device_id = rp.Pop<u8>();

    u8* shared_memory_ptr = shared_memory->GetPointer();
    if (device_id == 1) {
        // These values are observed on a New 3DS. The meaning of them is unclear.
        // TODO (wwylele): should assign network_id a (random?) number
        shared_memory_ptr[offsetof(SharedMemoryHeader, connection_status)] = 2;
        shared_memory_ptr[offsetof(SharedMemoryHeader, connection_role)] = 2;
        shared_memory_ptr[offsetof(SharedMemoryHeader, connected)] = 1;

        connected_device = true;
        extra_hid->OnConnect();
        conn_status_event->Signal();
    } else {
        LOG_WARNING(Service_IR, "unknown device id {}. Won't connect.", device_id);
        shared_memory_ptr[offsetof(SharedMemoryHeader, connection_status)] = 1;
        shared_memory_ptr[offsetof(SharedMemoryHeader, trying_to_connect_status)] = 2;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_INFO(Service_IR, "called, device_id = {}", device_id);
}

void IR_USER::GetReceiveEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestBuilder rb(ctx, 0x0A, 1, 2);

    rb.Push(ResultSuccess);
    rb.PushCopyObjects(receive_event);

    LOG_INFO(Service_IR, "called");
}

void IR_USER::GetSendEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestBuilder rb(ctx, 0x0B, 1, 2);

    rb.Push(ResultSuccess);
    rb.PushCopyObjects(send_event);

    LOG_INFO(Service_IR, "called");
}

void IR_USER::Disconnect(Kernel::HLERequestContext& ctx) {
    if (connected_device) {
        extra_hid->OnDisconnect();
        connected_device = false;
        conn_status_event->Signal();
    }

    u8* shared_memory_ptr = shared_memory->GetPointer();
    shared_memory_ptr[offsetof(SharedMemoryHeader, connection_status)] = 0;
    shared_memory_ptr[offsetof(SharedMemoryHeader, connected)] = 0;

    IPC::RequestBuilder rb(ctx, 0x09, 1, 0);
    rb.Push(ResultSuccess);

    LOG_INFO(Service_IR, "called");
}

void IR_USER::GetConnectionStatusEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestBuilder rb(ctx, 0x0C, 1, 2);

    rb.Push(ResultSuccess);
    rb.PushCopyObjects(conn_status_event);

    LOG_INFO(Service_IR, "called");
}

void IR_USER::FinalizeIrNop(Kernel::HLERequestContext& ctx) {
    if (connected_device) {
        extra_hid->OnDisconnect();
        connected_device = false;
    }

    shared_memory = nullptr;
    receive_buffer = nullptr;

    IPC::RequestBuilder rb(ctx, 0x02, 1, 0);
    rb.Push(ResultSuccess);

    LOG_INFO(Service_IR, "called");
}

void IR_USER::SendIrNop(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    std::vector<u8> buffer = rp.PopStaticBuffer();
    ASSERT(size == buffer.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (connected_device) {
        extra_hid->OnReceive(buffer);
        send_event->Signal();
        rb.Push(ResultSuccess);
    } else {
        LOG_ERROR(Service_IR, "not connected");
        rb.Push(Result(static_cast<ErrorDescription>(13), ErrorModule::IR,
                       ErrorSummary::InvalidState, ErrorLevel::Status));
    }

    LOG_TRACE(Service_IR, "called, data={}", fmt::format("{:02x}", fmt::join(buffer, " ")));
}

void IR_USER::ReleaseReceivedData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 count = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (receive_buffer->Release(count)) {
        rb.Push(ResultSuccess);
    } else {
        LOG_ERROR(Service_IR, "failed to release {} packets", count);
        rb.Push(Result(ErrorDescription::NoData, ErrorModule::IR, ErrorSummary::NotFound,
                       ErrorLevel::Status));
    }

    LOG_TRACE(Service_IR, "called, count={}", count);
}

IR_USER::IR_USER(Core::System& system) : ServiceFramework("ir:USER", 1) {
    const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "InitializeIrNop"},
        {0x0002, &IR_USER::FinalizeIrNop, "FinalizeIrNop"},
        {0x0003, nullptr, "ClearReceiveBuffer"},
        {0x0004, nullptr, "ClearSendBuffer"},
        {0x0005, nullptr, "WaitConnection"},
        {0x0006, &IR_USER::RequireConnection, "RequireConnection"},
        {0x0007, nullptr, "AutoConnection"},
        {0x0008, nullptr, "AnyConnection"},
        {0x0009, &IR_USER::Disconnect, "Disconnect"},
        {0x000A, &IR_USER::GetReceiveEvent, "GetReceiveEvent"},
        {0x000B, &IR_USER::GetSendEvent, "GetSendEvent"},
        {0x000C, &IR_USER::GetConnectionStatusEvent, "GetConnectionStatusEvent"},
        {0x000D, &IR_USER::SendIrNop, "SendIrNop"},
        {0x000E, nullptr, "SendIrNopLarge"},
        {0x000F, nullptr, "ReceiveIrnop"},
        {0x0010, nullptr, "ReceiveIrnopLarge"},
        {0x0011, nullptr, "GetLatestReceiveErrorResult"},
        {0x0012, nullptr, "GetLatestSendErrorResult"},
        {0x0013, nullptr, "GetConnectionStatus"},
        {0x0014, nullptr, "GetTryingToConnectStatus"},
        {0x0015, nullptr, "GetReceiveSizeFreeAndUsed"},
        {0x0016, nullptr, "GetSendSizeFreeAndUsed"},
        {0x0017, nullptr, "GetConnectionRole"},
        {0x0018, &IR_USER::InitializeIrNopShared, "InitializeIrNopShared"},
        {0x0019, &IR_USER::ReleaseReceivedData, "ReleaseReceivedData"},
        {0x001A, nullptr, "SetOwnMachineId"},
        // clang-format on
    };
    RegisterHandlers(functions);

    using namespace Kernel;

    connected_device = false;
    conn_status_event = system.Kernel().CreateEvent(ResetType::OneShot, "IR:ConnectionStatusEvent");
    send_event = system.Kernel().CreateEvent(ResetType::OneShot, "IR:SendEvent");
    receive_event = system.Kernel().CreateEvent(ResetType::OneShot, "IR:ReceiveEvent");

    extra_hid = std::make_unique<ExtraHID>([this](std::span<const u8> data) { PutToReceive(data); },
                                           system.CoreTiming(), system.Movie());
}

IR_USER::~IR_USER() {
    if (connected_device) {
        extra_hid->OnDisconnect();
    }
}

void IR_USER::ReloadInputDevices() {
    extra_hid->RequestInputDevicesReload();
}

void IR_USER::UseArticController(const std::shared_ptr<Service::HID::ArticBaseController>& ac) {
    if (extra_hid.get()) {
        extra_hid->UseArticController(ac);
    }
}

IRDevice::IRDevice(SendFunc send_func_) : send_func(send_func_) {}
IRDevice::~IRDevice() = default;

void IRDevice::Send(std::span<const u8> data) {
    send_func(data);
}

} // namespace Service::IR

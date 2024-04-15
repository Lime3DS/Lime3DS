// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace FileSys {
class ArchiveBackend;
class FileBackend;
} // namespace FileSys

namespace Core {
class System;
}

namespace Service::CECD {

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    enum class CecCommand : u32 {
        None = 0,
        Start = 1,
        ResetStart = 2,
        ReadyScan = 3,
        ReadyScanWait = 4,
        StartScan = 5,
        Rescan = 6,
        NdmResume = 7,
        NdmSuspend = 8,
        NdmSuspendImmediate = 9,
        StopWait = 0x0A,
        Stop = 0x0B,
        StopForce = 0x0C,
        StopForceWait = 0x0D,
        ResetFilter = 0x0E,
        DaemonStop = 0x0F,
        DaemonStart = 0x10,
        Exit = 0x11,
        OverBoss = 0x12,
        OverBossForce = 0x13,
        OverBossForceWait = 0x14,
        End = 0x15,
    };

    /**
     * CecDataPathType possible missing values; need to figure out placement
     *
     * data:/CEC/TMP
     * data:/CEC/test
     */
    enum class CecDataPathType : u32 {
        Invalid = 0,
        MboxList = 1,        /// data:/CEC/MBoxList____
        MboxInfo = 2,        /// data:/CEC/<id>/MBoxInfo____
        InboxInfo = 3,       /// data:/CEC/<id>/InBox___/BoxInfo_____
        OutboxInfo = 4,      /// data:/CEC/<id>/OutBox__/BoxInfo_____
        OutboxIndex = 5,     /// data:/CEC/<id>/OutBox__/OBIndex_____
        InboxMsg = 6,        /// data:/CEC/<id>/InBox___/_<message_id>
        OutboxMsg = 7,       /// data:/CEC/<id>/OutBox__/_<message_id>
        RootDir = 10,        /// data:/CEC
        MboxDir = 11,        /// data:/CEC/<id>
        InboxDir = 12,       /// data:/CEC/<id>/InBox___
        OutboxDir = 13,      /// data:/CEC/<id>/OutBox__
        MboxData = 100,      /// data:/CEC/<id>/MBoxData.0<i-100>
        MboxIcon = 101,      /// data:/CEC/<id>/MBoxData.001
        MboxTitle = 110,     /// data:/CEC/<id>/MBoxData.010
        MboxProgramId = 150, /// data:/CEC/<id>/MBoxData.050
    };

    enum class CecState : u32 {
        None = 0,
        Init = 1,
        WirelessParamSetup = 2,
        WirelessReady = 3,
        WirelessStartConfig = 4,
        Scan = 5,
        Scanning = 6,
        Connect = 7,
        Connecting = 8,
        Connected = 9,
        ConnectTcp = 10,
        ConnectingTcp = 11,
        ConnectedTcp = 12,
        Negotiation = 13,
        SendRecvStart = 14,
        SendRecvInit = 15,
        SendReady = 16,
        ReceiveReady = 17,
        Receive = 18,
        ConnectionFinishTcp = 19,
        ConnectionFinish = 20,
        SendPost = 21,
        ReceivePost = 22,
        Finishing = 23,
        Finish = 24,
        OverBoss = 25,
        Idle = 26
    };

    enum class CecSystemInfoType : u32 { EulaVersion = 1, Eula = 2, ParentControl = 3 };

    struct CecBoxInfoHeader {
        u16_le magic; // 0x6262 'bb'
        INSERT_PADDING_BYTES(2);
        u32_le box_info_size;
        u32_le max_box_size;
        u32_le box_size;
        u32_le max_message_num;
        u32_le message_num;
        u32_le max_batch_size;
        u32_le max_message_size;
    };
    static_assert(sizeof(CecBoxInfoHeader) == 0x20, "CecBoxInfoHeader struct has incorrect size.");

    struct CecMBoxInfoHeader {
        u16_le magic; // 0x6363 'cc'
        INSERT_PADDING_BYTES(2);
        u32_le program_id;
        u32_le private_id;
        u8 flag;
        u8 flag2;
        INSERT_PADDING_BYTES(2);
        std::array<u8, 32> hmac_key;
        INSERT_PADDING_BYTES(4);
        struct Time {
            u32_le year;
            u8 month;
            u8 day;
            u8 hour;
            u8 minute;
            u8 second;
            u8 millisecond;
            u8 microsecond;
            u8 padding;
        } last_accessed;
        INSERT_PADDING_BYTES(4);
        Time last_received;
        INSERT_PADDING_BYTES(4);
        Time unknown_time;
    };
    static_assert(sizeof(CecMBoxInfoHeader) == 0x60,
                  "CecMBoxInfoHeader struct has incorrect size.");

    struct CecMBoxListHeader {
        u16_le magic; // 0x6868 'hh'
        INSERT_PADDING_BYTES(2);
        u16_le version; // 0x01 00, maybe activated flag?
        INSERT_PADDING_BYTES(2);
        u16_le num_boxes; // 24 max
        INSERT_PADDING_BYTES(2);
        std::array<std::array<u8, 16>, 24> box_names; // 16 char names, 24 boxes
    };
    static_assert(sizeof(CecMBoxListHeader) == 0x18C,
                  "CecMBoxListHeader struct has incorrect size.");

    struct CecMessageHeader {
        u16_le magic; // 0x6060 ``
        INSERT_PADDING_BYTES(2);
        u32_le message_size;
        u32_le header_size;
        u32_le body_size;

        u32_le title_id;
        u32_le title_id2;
        u32_le batch_id;
        u32_le unknown_id;

        std::array<u8, 8> message_id;
        u32_le version;
        std::array<u8, 8> message_id2;
        u8 flag;
        u8 send_method;
        u8 is_unopen;
        u8 is_new;
        u64_le sender_id;
        u64_le sender_id2;
        struct Time {
            u32_le year;
            u8 month;
            u8 day;
            u8 hour;
            u8 minute;
            u8 second;
            u8 millisecond;
            u8 microsecond;
            u8 padding;
        } send_time, recv_time, create_time;
        u8 send_count;
        u8 forward_count;
        u16_le user_data;
    };
    static_assert(sizeof(CecMessageHeader) == 0x70, "CecMessageHeader struct has incorrect size.");

    struct CecOBIndexHeader {
        u16_le magic; // 0x6767 'gg'
        INSERT_PADDING_BYTES(2);
        u32_le message_num;
        // Array of messageid's 8 bytes each, same as CecMessageHeader.message_id[8]
    };
    static_assert(sizeof(CecOBIndexHeader) == 0x08, "CecOBIndexHeader struct has incorrect size.");

    enum class CecdState : u32 {
        NdmStatusWorking = 0,
        NdmStatusIdle = 1,
        NdmStatusSuspending = 2,
        NdmStatusSuspended = 3,
    };

    union CecOpenMode {
        u32 raw;
        BitField<0, 1, u32> unknown; // 1 delete?
        BitField<1, 1, u32> read;    // 2
        BitField<2, 1, u32> write;   // 4
        BitField<3, 1, u32> create;  // 8
        BitField<4, 1, u32> check;   // 16 maybe validate sig?
        BitField<30, 1, u32> unk_flag;
    };

    enum class CecTest : u32 {
        CEC_TEST_000 = 0,
        CEC_TEST_001 = 1,
        CEC_TEST_002 = 2,
        CEC_TEST_003 = 3,
        CEC_TEST_004 = 4,
        CEC_TEST_005 = 5,
        CEC_TEST_006 = 6,
    };

    /// Opening a file and reading/writing can be handled by two different functions
    /// So, we need to pass that file data around
    struct SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
        SessionData();
        ~SessionData();

        u32 ncch_program_id;
        CecDataPathType data_path_type;
        CecOpenMode open_mode;
        FileSys::Path path;

        std::unique_ptr<FileSys::FileBackend> file;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& boost::serialization::base_object<Kernel::SessionRequestHandler::SessionDataBase>(
                *this);
            ar & ncch_program_id;
            ar & data_path_type;
            ar & open_mode.raw;
            ar & path;
            ar & file;
        }
        friend class boost::serialization::access;
    };

    class Interface : public ServiceFramework<Interface, SessionData> {
    public:
        Interface(std::shared_ptr<Module> cecd, const char* name, u32 max_session);
        ~Interface() = default;

    protected:
        /**
         * CECD::Open service function
         *  Inputs:
         *      0 : Header Code[0x000100C2]
         *      1 : NCCH Program ID
         *      2 : Path type
         *      3 : File open flag
         *      4 : Descriptor for process ID
         *      5 : Placeholder for process ID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : File size?
         */
        void Open(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Read service function
         *  Inputs:
         *      0 : Header Code[0x00020042]
         *      1 : Buffer size (unused)
         *      2 : Descriptor for mapping a write-only buffer in the target process
         *      3 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Read size
         *      3 : Descriptor for mapping a write-only buffer in the target process
         *      4 : Buffer address
         */
        void Read(Kernel::HLERequestContext& ctx);

        /**
         * CECD::ReadMessage service function
         *  Inputs:
         *      0 : Header Code[0x00030104]
         *      1 : NCCH Program ID
         *      2 : bool is_outbox
         *      3 : Message ID size (unused, always 8)
         *      4 : Buffer size (unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Message ID address
         *      7 : Descriptor for mapping a write-only buffer in the target process
         *      8 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Read size
         *      3 : Descriptor for mapping a read-only buffer in the target process
         *      4 : Message ID address
         *      5 : Descriptor for mapping a write-only buffer in the target process
         *      6 : Buffer address
         */
        void ReadMessage(Kernel::HLERequestContext& ctx);

        /**
         * CECD::ReadMessageWithHMAC service function
         *  Inputs:
         *      0 : Header Code[0x00040106]
         *      1 : NCCH Program ID
         *      2 : bool is_outbox
         *      3 : Message ID size(unused, always 8)
         *      4 : Buffer size(unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Message ID address
         *      7 : Descriptor for mapping a read-only buffer in the target process
         *      8 : HMAC key address
         *      9 : Descriptor for mapping a write-only buffer in the target process
         *     10 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Read size
         *      3 : Descriptor for mapping a read-only buffer in the target process
         *      4 : Message ID address
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : HMAC key address
         *      7 : Descriptor for mapping a write-only buffer in the target process
         *      8 : Buffer address
         */
        void ReadMessageWithHMAC(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Write service function
         *  Inputs:
         *      0 : Header Code[0x00050042]
         *      1 : Buffer size(unused)
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         */
        void Write(Kernel::HLERequestContext& ctx);

        /**
         * CECD::WriteMessage service function
         *  Inputs:
         *      0 : Header Code[0x00060104]
         *      1 : NCCH Program ID
         *      2 : bool is_outbox
         *      3 : Message ID size(unused, always 8)
         *      4 : Buffer size(unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Buffer address
         *      7 : Descriptor for mapping a read/write buffer in the target process
         *      8 : Message ID address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         *      4 : Descriptor for mapping a read/write buffer in the target process
         *      5 : Message ID address
         */
        void WriteMessage(Kernel::HLERequestContext& ctx);

        /**
         * CECD::WriteMessageWithHMAC service function
         *  Inputs:
         *      0 : Header Code[0x00070106]
         *      1 : NCCH Program ID
         *      2 : bool is_outbox
         *      3 : Message ID size(unused, always 8)
         *      4 : Buffer size(unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Buffer address
         *      7 : Descriptor for mapping a read-only buffer in the target process
         *      8 : HMAC key address
         *      9 : Descriptor for mapping a read/write buffer in the target process
         *     10 : Message ID address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         *      4 : Descriptor for mapping a read-only buffer in the target process
         *      5 : HMAC key address
         *      6 : Descriptor for mapping a read/write buffer in the target process
         *      7 : Message ID address
         */
        void WriteMessageWithHMAC(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Delete service function
         *  Inputs:
         *      0 : Header Code[0x00080102]
         *      1 : NCCH Program ID
         *      2 : Path type
         *      3 : bool is_outbox
         *      4 : Message ID size (unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Message ID address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Message ID address
         */
        void Delete(Kernel::HLERequestContext& ctx);

        /**
         * CECD::SetData service function
         *  Inputs:
         *      0 : Header Code[0x000900C2]
         *      1 : NCCH Program ID
         *      2 : Path type
         *      3 : bool is_outbox
         *      4 : Descriptor for mapping a read-only buffer in the target process
         *      5 : Message ID address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Message ID address
         */
        void SetData(Kernel::HLERequestContext& ctx);

        /**
         * CECD::ReadData service function
         *  Inputs:
         *      0 : Header Code[0x000A00C4]
         *      1 : Destination buffer size (unused)
         *      2 : Info type
         *      3 : Param buffer size (unused)
         *      4 : Descriptor for mapping a read-only buffer in the target process
         *      5 : Param buffer address
         *      6 : Descriptor for mapping a write-only buffer in the target process
         *      7 : Destination buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Param buffer address
         *      4 : Descriptor for mapping a write-only buffer in the target process
         *      5 : Destination buffer address
         */
        void ReadData(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Start service function
         *  Inputs:
         *      0 : Header Code[0x000B0040]
         *      1 : Command
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Start(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Stop service function
         *  Inputs:
         *      0 : Header Code[0x000C0040]
         *      1 : Command
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Stop(Kernel::HLERequestContext& ctx);

        /**
         * CECD::GetCecInfoBuffer service function
         *  Inputs:
         *      0 : Header Code[0x000D0082]
         *      1 : unknown
         *      2 : unknown, buffer size?
         *      3 : Descriptor for mapping a write-only buffer in the target process
         *      4 : Destination buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *    2-3 : MappedBuffer
         */
        void GetCecInfoBuffer(Kernel::HLERequestContext& ctx);

        /**
         * GetCecdState service function
         *  Inputs:
         *      0: Header Code[0x000E0000]
         *  Outputs:
         *      1: Result
         *      2: CecdState
         */
        void GetCecdState(Kernel::HLERequestContext& ctx);

        /**
         * GetCecInfoEventHandle service function
         *  Inputs:
         *      0: Header Code[0x000F0000]
         *  Outputs:
         *      1: Result
         *      3: Event Handle
         */
        void GetCecInfoEventHandle(Kernel::HLERequestContext& ctx);

        /**
         * GetChangeStateEventHandle service function
         *  Inputs:
         *      0: Header Code[0x00100000]
         *  Outputs:
         *      1: Result
         *      3: Event Handle
         */
        void GetChangeStateEventHandle(Kernel::HLERequestContext& ctx);

        /**
         * CECD::OpenAndWrite service function
         *  Inputs:
         *      0 : Header Code[0x00110104]
         *      1 : Buffer size (unused)
         *      2 : NCCH Program ID
         *      3 : Path type
         *      4 : File open flag
         *      5 : Descriptor for process ID
         *      6 : Placeholder for process ID
         *      7 : Descriptor for mapping a read-only buffer in the target process
         *      8 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         */
        void OpenAndWrite(Kernel::HLERequestContext& ctx);

        /**
         * CECD::OpenAndRead service function
         *  Inputs:
         *      0 : Header Code[0x00120104]
         *      1 : Buffer size (unused)
         *      2 : NCCH Program ID
         *      3 : Path type
         *      4 : File open flag
         *      5 : Descriptor for process ID
         *      6 : Placeholder for process ID
         *      7 : Descriptor for mapping a write-only buffer in the target process
         *      8 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Toal bytes read
         *      3 : Descriptor for mapping a write-only buffer in the target process
         *      4 : Buffer address
         */
        void OpenAndRead(Kernel::HLERequestContext& ctx);

        /**
         * CECD::GetEventLog service function
         *  Inputs:
         *      0 : Header Code[0x001E0082]
         *      1 : unknown
         *      2 : unknown
         *      3 : buffer descriptor
         *      4 : buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : unknown
         */
        void GetEventLog(Kernel::HLERequestContext& ctx);

        /**
         * CECD::GetEventLogStart service function
         *  Inputs:
         *      0 : Header Code[0x001F0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : unknown
         */
        void GetEventLogStart(Kernel::HLERequestContext& ctx);

        /**
         * GetCecInfoEventHandleSys service function
         *  Inputs:
         *      0: Header Code[0x40020002]
         *  Outputs:
         *      1: Result
         *      3: Event Handle
         */
        void GetCecInfoEventHandleSys(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> cecd;
    };

private:
    /// String used by cecd for base64 encoding found in the sysmodule disassembly
    const std::string base64_dict =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

    const std::vector<u8> cecd_system_savedata_id = {0x00, 0x00, 0x00, 0x00,
                                                     0x26, 0x00, 0x01, 0x00};

    /// Encoding function used for the message id
    std::string EncodeBase64(std::span<const u8> in) const;

    std::string GetCecDataPathTypeAsString(
        const CecDataPathType type, const u32 program_id,
        std::span<const u8> msg_id = std::span<const u8>{}) const;

    std::string GetCecCommandAsString(const CecCommand command) const;

    void CheckAndUpdateFile(const CecDataPathType path_type, const u32 ncch_program_id,
                            std::vector<u8>& file_buffer);

    std::unique_ptr<FileSys::ArchiveBackend> cecd_system_save_data_archive;

    std::shared_ptr<Kernel::Event> cecinfo_event;
    std::shared_ptr<Kernel::Event> cecinfosys_event;
    std::shared_ptr<Kernel::Event> change_state_event;

    Core::System& system;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

/// Initialize CECD service(s)
void InstallInterfaces(Core::System& system);

} // namespace Service::CECD

SERVICE_CONSTRUCT(Service::CECD::Module)
BOOST_CLASS_EXPORT_KEY(Service::CECD::Module)
BOOST_CLASS_EXPORT_KEY(Service::CECD::Module::SessionData)

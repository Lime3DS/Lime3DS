// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_m.h"
#include "core/hle/service/nfc/nfc_u.h"

SERVICE_CONSTRUCT_IMPL(Service::NFC::Module)
SERIALIZE_EXPORT_IMPL(Service::NFC::Module)

namespace Service::NFC {

template <class Archive>
void Module::serialize(Archive& ar, const unsigned int) {
    ar & nfc_mode;
    ar & device;
}
SERIALIZE_IMPL(Module)

void Module::Interface::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto communication_mode = rp.PopEnum<CommunicationMode>();

    LOG_INFO(Service_NFC, "called, communication_mode={}", communication_mode);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (nfc->nfc_mode != CommunicationMode::NotInitialized) {
        rb.Push(ResultInvalidOperation);
        return;
    }

    Result result = ResultSuccess;
    switch (communication_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        nfc->device->Initialize();
        break;
    case CommunicationMode::TrainTag:
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", communication_mode);
        break;
    default:
        result = ResultInvalidArgumentValue;
        break;
    }

    if (result.IsSuccess()) {
        nfc->nfc_mode = communication_mode;
    }

    rb.Push(result);
}

void Module::Interface::Finalize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto communication_mode = rp.PopEnum<CommunicationMode>();

    LOG_INFO(Service_NFC, "called, communication_mode={}", communication_mode);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (nfc->nfc_mode != communication_mode) {
        rb.Push(ResultInvalidOperation);
        return;
    }

    Result result = ResultSuccess;
    switch (communication_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        nfc->device->Finalize();
        break;
    case CommunicationMode::TrainTag:
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", communication_mode);
        break;
    default:
        result = ResultInvalidArgumentValue;
        break;
    }

    if (result.IsSuccess()) {
        nfc->nfc_mode = CommunicationMode::NotInitialized;
    }

    rb.Push(result);
}

void Module::Interface::Connect(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_WARNING(Service_NFC, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        rb.Push(ResultSuccess);
        return;
    }

    // TODO: call start communication instead
    const auto result = nfc->device->StartCommunication();
    rb.Push(result);
}

void Module::Interface::Disconnect(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_WARNING(Service_NFC, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        rb.Push(ResultSuccess);
        return;
    }

    // TODO: call stop communication instead
    const auto result = nfc->device->StopCommunication();
    rb.Push(result);
}

void Module::Interface::StartDetection(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u16 in_val = rp.Pop<u16>();

    LOG_INFO(Service_NFC, "called, in_val={:04x}", in_val);

    Result result = ResultSuccess;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        // in_val probably correlates to the tag protocol to be detected
        result = nfc->device->StartDetection(TagProtocol::All);
        break;
    default:
        result = ResultInvalidArgumentValue;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::StopDetection(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    Result result = ResultSuccess;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        result = nfc->device->StopDetection();
        break;
    case CommunicationMode::TrainTag:
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        break;
    default:
        result = ResultInvalidOperation;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::Mount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    nfc->device->RescheduleTagRemoveEvent();

    Result result = ResultSuccess;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
        result = nfc->device->Mount();
        break;
    case CommunicationMode::Amiibo:
        result = nfc->device->MountAmiibo();
        break;
    default:
        result = ResultInvalidOperation;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::Unmount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    Result result = ResultSuccess;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
    case CommunicationMode::Amiibo:
        result = nfc->device->ResetTagScanState();
        break;
    default:
        result = ResultInvalidOperation;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::Flush(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    Result result = ResultSuccess;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        break;
    case CommunicationMode::Amiibo:
        result = nfc->device->Flush();
        break;
    default:
        result = ResultInvalidOperation;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::GetActivateEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(nfc->device->GetActivateEvent());
}

void Module::Interface::GetDeactivateEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(nfc->device->GetDeactivateEvent());
}

void Module::Interface::GetStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    DeviceState state = DeviceState::NotInitialized;

    LOG_DEBUG(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
    } else {
        state = nfc->device->GetCurrentState();
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(state);
}

void Module::Interface::GetTargetConnectionStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_DEBUG(Service_NFC, "called");

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(ResultSuccess);
        rb.PushEnum(CommunicationState::Idle);
        return;
    }

    CommunicationState status{};
    const auto result = nfc->device->GetCommunicationStatus(status);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(result);
    rb.PushEnum(status);
}

void Module::Interface::GetTagInfo2(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    nfc->device->RescheduleTagRemoveEvent();

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(25, 0);
        rb.Push(ResultSuccess);
        rb.PushRaw<TagInfo2>({});
        return;
    }

    TagInfo2 tag_info{};
    const auto result = nfc->device->GetTagInfo2(tag_info);
    IPC::RequestBuilder rb = rp.MakeBuilder(25, 0);
    rb.Push(result);
    rb.PushRaw<TagInfo2>(tag_info);
}

void Module::Interface::GetTagInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    nfc->device->RescheduleTagRemoveEvent();

    if (nfc->nfc_mode == CommunicationMode::TrainTag) {
        LOG_ERROR(Service_NFC, "CommunicationMode  {} not implemented", nfc->nfc_mode);
        IPC::RequestBuilder rb = rp.MakeBuilder(12, 0);
        rb.Push(ResultSuccess);
        rb.PushRaw<TagInfo>({});
        return;
    }

    TagInfo tag_info{};
    const auto result = nfc->device->GetTagInfo(tag_info);
    IPC::RequestBuilder rb = rp.MakeBuilder(12, 0);
    rb.Push(result);
    rb.PushRaw<TagInfo>(tag_info);
}

void Module::Interface::GetConnectResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(0);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::OpenApplicationArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 access_id = rp.Pop<u32>();

    LOG_INFO(Service_NFC, "called, access_id={}", access_id);

    nfc->device->RescheduleTagRemoveEvent();

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    const auto result = nfc->device->OpenApplicationArea(access_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::CreateApplicationArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 access_id = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    std::vector<u8> buffer = rp.PopStaticBuffer();

    LOG_INFO(Service_NFC, "called, size={}", size);

    if (buffer.size() > size) {
        buffer.resize(size);
    }

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    const auto result = nfc->device->CreateApplicationArea(access_id, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::ReadApplicationArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 size = rp.Pop<u32>();

    LOG_INFO(Service_NFC, "called, size={}", size);

    nfc->device->RescheduleTagRemoveEvent();

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    std::vector<u8> buffer(size);
    const auto result = nfc->device->GetApplicationArea(buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(result);
    rb.PushStaticBuffer(buffer, 0);
}

void Module::Interface::WriteApplicationArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 size = rp.Pop<u32>();
    std::vector<u8> tag_uuid_info = rp.PopStaticBuffer();
    std::vector<u8> buffer = rp.PopStaticBuffer();

    LOG_INFO(Service_NFC, "called, size={}", size);

    if (buffer.size() > size) {
        buffer.resize(size);
    }

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    const auto result = nfc->device->SetApplicationArea(buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::GetNfpRegisterInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    nfc->device->RescheduleTagRemoveEvent();

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    RegisterInfo settings_info{};
    const auto result = nfc->device->GetRegisterInfo(settings_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(43, 0);
    rb.Push(result);
    rb.PushRaw<RegisterInfo>(settings_info);
}

void Module::Interface::GetNfpCommonInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    nfc->device->RescheduleTagRemoveEvent();

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    CommonInfo amiibo_config{};
    const auto result = nfc->device->GetCommonInfo(amiibo_config);

    IPC::RequestBuilder rb = rp.MakeBuilder(17, 0);
    rb.Push(result);
    rb.PushRaw<CommonInfo>(amiibo_config);
}

void Module::Interface::InitializeCreateInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    using InitialStruct = std::array<u8, 0x3c>;
    InitialStruct empty{};

    IPC::RequestBuilder rb = rp.MakeBuilder(16, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw<InitialStruct>(empty);
}

void Module::Interface::MountRom(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    Result result = ResultSuccess;
    switch (nfc->nfc_mode) {
    case CommunicationMode::Ntag:
        result = nfc->device->PartiallyMount();
        break;
    case CommunicationMode::Amiibo:
        result = nfc->device->PartiallyMountAmiibo();
        break;
    default:
        result = ResultInvalidOperation;
        break;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::GetIdentificationBlock(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    nfc->device->RescheduleTagRemoveEvent();

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    ModelInfo model_info{};
    const auto result = nfc->device->GetModelInfo(model_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(14, 0);
    rb.Push(result);
    rb.PushRaw<ModelInfo>(model_info);
}

void Module::Interface::Format(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] u32 unknown1 = rp.Pop<u32>();
    [[maybe_unused]] u32 unknown2 = rp.Pop<u32>();
    [[maybe_unused]] u32 unknown3 = rp.Pop<u32>();
    [[maybe_unused]] std::vector<u8> buffer = rp.PopStaticBuffer();

    const auto result = nfc->device->Format();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Module::Interface::GetAdminInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    nfc->device->RescheduleTagRemoveEvent();

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    AdminInfo admin_info{};
    const auto result = nfc->device->GetAdminInfo(admin_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(17, 0);
    rb.Push(result);
    rb.PushRaw<AdminInfo>(admin_info);
}

void Module::Interface::GetEmptyRegisterInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(43, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw<RegisterInfo>({});
}

void Module::Interface::SetRegisterInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto register_info = rp.PopRaw<RegisterInfoPrivate>();

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    const auto result = nfc->device->SetRegisterInfoPrivate(register_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::DeleteRegisterInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    const auto result = nfc->device->DeleteRegisterInfo();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::DeleteApplicationArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NFC, "called");

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    const auto result = nfc->device->DeleteApplicationArea();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::ExistsApplicationArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    if (nfc->nfc_mode != CommunicationMode::Amiibo) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidOperation);
        return;
    }

    bool has_application_area = false;
    const auto result = nfc->device->ApplicationAreaExist(has_application_area);

    IPC::RequestBuilder rb = rp.MakeBuilder(0x2, 0);
    rb.Push(result);
    rb.Push(has_application_area);
    LOG_INFO(Service_NFC, "called");
}

std::shared_ptr<Module> Module::Interface::GetModule() const {
    return nfc;
}

bool Module::Interface::IsSearchingForAmiibos() {
    const auto state = nfc->device->GetCurrentState();
    return state == DeviceState::SearchingForTag;
}

bool Module::Interface::IsTagActive() {
    const auto state = nfc->device->GetCurrentState();
    return state == DeviceState::TagFound || state == DeviceState::TagMounted ||
           state == DeviceState::TagPartiallyMounted;
}

bool Module::Interface::LoadAmiibo(const std::string& fullpath) {
    return nfc->device->LoadAmiibo(fullpath);
}

void Module::Interface::RemoveAmiibo() {
    nfc->device->UnloadAmiibo();
}

Module::Interface::Interface(std::shared_ptr<Module> nfc, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), nfc(std::move(nfc)) {}

Module::Interface::~Interface() = default;

Module::Module(Core::System& system) {
    device = std::make_shared<NfcDevice>(system);
}

Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto nfc = std::make_shared<Module>(system);
    std::make_shared<NFC_M>(nfc)->InstallAsService(service_manager);
    std::make_shared<NFC_U>(nfc)->InstallAsService(service_manager);
}

} // namespace Service::NFC

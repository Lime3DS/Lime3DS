// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include <boost/crc.hpp>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/mii_selector.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"

namespace HLE::Applets {

Result MiiSelector::ReceiveParameterImpl(const Service::APT::MessageParameter& parameter) {
    if (parameter.signal != Service::APT::SignalType::Request) {
        LOG_ERROR(Service_APT, "unsupported signal {}", parameter.signal);
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultUnknown;
    }

    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
    // memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());

    std::memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));

    using Kernel::MemoryPermission;
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = system.Kernel().CreateSharedMemoryForApplet(
        0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
        "MiiSelector Memory");

    // Send the response message with the newly created SharedMemory
    SendParameter({
        .sender_id = id,
        .destination_id = parent,
        .signal = Service::APT::SignalType::Response,
        .object = framebuffer_memory,
    });

    return ResultSuccess;
}

Result MiiSelector::Start(const Service::APT::MessageParameter& parameter) {
    ASSERT_MSG(parameter.buffer.size() == sizeof(config),
               "The size of the parameter (MiiConfig) is wrong");

    std::memcpy(&config, parameter.buffer.data(), parameter.buffer.size());

    using namespace Frontend;
    frontend_applet = system.GetMiiSelector();
    ASSERT(frontend_applet);

    MiiSelectorConfig frontend_config = ToFrontendConfig(config);
    frontend_applet->Setup(frontend_config);

    return ResultSuccess;
}

void MiiSelector::Update() {
    using namespace Frontend;
    const MiiSelectorData& data = frontend_applet->ReceiveData();
    result.return_code = data.return_code;
    result.selected_mii_data = data.mii;
    result.selected_guest_mii_index = 0xFFFFFFFF;

    // TODO(Subv): We're finalizing the applet immediately after it's started,
    // but we should defer this call until after all the input has been collected.
    Finalize();
}

Result MiiSelector::Finalize() {
    std::vector<u8> buffer(sizeof(MiiResult));
    std::memcpy(buffer.data(), &result, buffer.size());
    CloseApplet(nullptr, buffer);
    return ResultSuccess;
}

MiiResult MiiSelector::GetStandardMiiResult() {
    // This data was obtained by writing the returned buffer in AppletManager::GlanceParameter of
    // the LLEd Mii picker of version system version 11.8.0 to a file and then matching the values
    // to the members of the MiiResult struct
    Mii::MiiData mii_data;
    mii_data.version = 0x03;
    mii_data.mii_options.raw = 0x00;
    mii_data.mii_pos.raw = 0x10;
    mii_data.console_identity.raw = 0x30;
    mii_data.system_id = 0xD285B6B300C8850A;
    mii_data.mii_id = 0x98391EE4;
    mii_data.mac = {0x40, 0xF4, 0x07, 0xB7, 0x37, 0x10};
    mii_data.pad = 0x0000;
    mii_data.mii_details.raw = 0xA600;
    mii_data.mii_name = {'C', 'i', 't', 'r', 'a', 0x0, 0x0, 0x0, 0x0, 0x0};
    mii_data.height = 0x40;
    mii_data.width = 0x40;
    mii_data.face_style.raw = 0x00;
    mii_data.face_details.raw = 0x00;
    mii_data.hair_style = 0x21;
    mii_data.hair_details.raw = 0x01;
    mii_data.eye_details.raw = 0x02684418;
    mii_data.eyebrow_details.raw = 0x26344614;
    mii_data.nose_details.raw = 0x8112;
    mii_data.mouth_details.raw = 0x1768;
    mii_data.mustache_details.raw = 0x0D00;
    mii_data.beard_details.raw = 0x0029;
    mii_data.glasses_details.raw = 0x0052;
    mii_data.mole_details.raw = 0x4850;
    mii_data.author_name = {u'f', u'l', u'T', u'o', u'b', u'i'};

    MiiResult result;
    result.return_code = 0x0;
    result.is_guest_mii_selected = 0x0;
    result.selected_guest_mii_index = 0xFFFFFFFF;
    result.selected_mii_data = mii_data;
    result.guest_mii_name.fill(0x0);

    return result;
}

Frontend::MiiSelectorConfig MiiSelector::ToFrontendConfig(const MiiConfig& config) const {
    Frontend::MiiSelectorConfig frontend_config;
    frontend_config.enable_cancel_button = config.enable_cancel_button == 1;
    frontend_config.title = Common::UTF16BufferToUTF8(config.title);
    frontend_config.initially_selected_mii_index = config.initially_selected_mii_index;
    return frontend_config;
}
} // namespace HLE::Applets

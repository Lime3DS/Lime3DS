// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "core/hle/applets/mii_selector.h"

namespace Frontend {

/// Default English button text mappings. Frontends may need to copy this to internationalize it.
constexpr char MII_BUTTON_OKAY[] = "Ok";
constexpr char MII_BUTTON_CANCEL[] = "Cancel";

/// Configuration that's relevant to frontend implementation of applet. Anything missing that we
/// later learn is needed can be added here and filled in by the backend HLE applet
struct MiiSelectorConfig {
    bool enable_cancel_button;
    std::string title;
    u32 initially_selected_mii_index;
};

struct MiiSelectorData {
    u32 return_code;
    Mii::MiiData mii;
};

class MiiSelector {
public:
    virtual ~MiiSelector() = default;
    virtual void Setup(const MiiSelectorConfig& config_) {
        config = MiiSelectorConfig(config_);
    }

    const MiiSelectorData& ReceiveData() const {
        return data;
    }

    /**
     * Stores the data so that the HLE applet in core can
     * send this to the calling application
     */
    void Finalize(u32 return_code, Mii::MiiData mii);

protected:
    MiiSelectorConfig config;
    MiiSelectorData data;
};

std::vector<Mii::MiiData> LoadMiis();

class DefaultMiiSelector final : public MiiSelector {
public:
    void Setup(const MiiSelectorConfig& config) override;
};

} // namespace Frontend

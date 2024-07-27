// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"
#include "core/3ds.h"
#include "input_common/touch_from_button.h"

namespace InputCommon {

class TouchFromButtonDevice final : public Input::TouchDevice {
public:
    TouchFromButtonDevice() {
        for (const auto& config_entry :
             Settings::values
                 .touch_from_button_maps[Settings::values.current_input_profile
                                             .touch_from_button_map_index]
                 .buttons) {

            const Common::ParamPackage package{config_entry};
            map.emplace_back(Input::CreateDevice<Input::ButtonDevice>(config_entry),
                             std::clamp(package.Get("x", 0), 0, Core::kScreenBottomWidth),
                             std::clamp(package.Get("y", 0), 0, Core::kScreenBottomHeight));
        }
    }

    std::tuple<float, float, bool> GetStatus() const override {
        for (const auto& m : map) {
            const bool state = std::get<0>(m)->GetStatus();
            if (state) {
                const float x = static_cast<float>(std::get<1>(m)) / Core::kScreenBottomWidth;
                const float y = static_cast<float>(std::get<2>(m)) / Core::kScreenBottomHeight;
                return {x, y, true};
            }
        }
        return {};
    }

private:
    // A vector of the mapped button, its x and its y-coordinate
    std::vector<std::tuple<std::unique_ptr<Input::ButtonDevice>, int, int>> map;
};

std::unique_ptr<Input::TouchDevice> TouchFromButtonFactory::Create(
    const Common::ParamPackage& params) {
    return std::make_unique<TouchFromButtonDevice>();
}

} // namespace InputCommon

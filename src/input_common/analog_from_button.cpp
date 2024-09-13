// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "input_common/analog_from_button.h"

namespace InputCommon {

class Analog final : public Input::AnalogDevice {
public:
    using Button = std::unique_ptr<Input::ButtonDevice>;

    Analog(Button up_, Button down_, Button left_, Button right_, Button up_left_, Button up_right_,
           Button down_left_, Button down_right_, Button modifier_, float modifier_scale_)
        : up(std::move(up_)), down(std::move(down_)), left(std::move(left_)),
          right(std::move(right_)), up_left(std::move(up_left_)), up_right(std::move(up_right_)),
          down_left(std::move(down_left_)), down_right(std::move(down_right_)),
          modifier(std::move(modifier_)), modifier_scale(modifier_scale_) {}

    std::tuple<float, float> GetStatus() const override {
        constexpr float SQRT_HALF = 0.707106781f;
        int x = 0, y = 0;

        if (right->GetStatus())
            ++x;
        if (left->GetStatus())
            --x;
        if (up->GetStatus())
            ++y;
        if (down->GetStatus())
            --y;
        if (up_right->GetStatus()) {
            ++x;
            ++y;
        }
        if (up_left->GetStatus()) {
            --x;
            ++y;
        }
        if (down_right->GetStatus()) {
            ++x;
            --y;
        }
        if (down_left->GetStatus()) {
            --x;
            --y;
        }

        float coef = modifier->GetStatus() ? modifier_scale : 1.0f;
        return std::make_tuple(x * coef * (y == 0 ? 1.0f : SQRT_HALF),
                               y * coef * (x == 0 ? 1.0f : SQRT_HALF));
    }

private:
    Button up;
    Button down;
    Button left;
    Button right;
    Button up_left;
    Button up_right;
    Button down_left;
    Button down_right;
    Button modifier;
    float modifier_scale;
};

std::unique_ptr<Input::AnalogDevice> AnalogFromButton::Create(const Common::ParamPackage& params) {
    const std::string null_engine = Common::ParamPackage{{"engine", "null"}}.Serialize();
    auto up = Input::CreateDevice<Input::ButtonDevice>(params.Get("up", null_engine));
    auto down = Input::CreateDevice<Input::ButtonDevice>(params.Get("down", null_engine));
    auto left = Input::CreateDevice<Input::ButtonDevice>(params.Get("left", null_engine));
    auto right = Input::CreateDevice<Input::ButtonDevice>(params.Get("right", null_engine));
    auto up_left = Input::CreateDevice<Input::ButtonDevice>(params.Get("up_left", null_engine));
    auto up_right = Input::CreateDevice<Input::ButtonDevice>(params.Get("up_right", null_engine));
    auto down_left = Input::CreateDevice<Input::ButtonDevice>(params.Get("down_left", null_engine));
    auto down_right =
        Input::CreateDevice<Input::ButtonDevice>(params.Get("down_right", null_engine));
    auto modifier = Input::CreateDevice<Input::ButtonDevice>(params.Get("modifier", null_engine));
    auto modifier_scale = params.Get("modifier_scale", 0.5f);
    return std::make_unique<Analog>(std::move(up), std::move(down), std::move(left),
                                    std::move(right), std::move(up_left), std::move(up_right),
                                    std::move(down_left), std::move(down_right),
                                    std::move(modifier), modifier_scale);
}

} // namespace InputCommon

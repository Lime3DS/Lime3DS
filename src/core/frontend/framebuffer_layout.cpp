// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>

#include "common/assert.h"
#include "common/settings.h"
#include "core/3ds.h"
#include "core/frontend/framebuffer_layout.h"

namespace Layout {

static constexpr float TOP_SCREEN_ASPECT_RATIO =
    static_cast<float>(Core::kScreenTopHeight) / Core::kScreenTopWidth;
static constexpr float BOT_SCREEN_ASPECT_RATIO =
    static_cast<float>(Core::kScreenBottomHeight) / Core::kScreenBottomWidth;
static constexpr float TOP_SCREEN_UPRIGHT_ASPECT_RATIO =
    static_cast<float>(Core::kScreenTopWidth) / Core::kScreenTopHeight;
static constexpr float BOT_SCREEN_UPRIGHT_ASPECT_RATIO =
    static_cast<float>(Core::kScreenBottomWidth) / Core::kScreenBottomHeight;

u32 FramebufferLayout::GetScalingRatio() const {
    if (is_rotated) {
        return static_cast<u32>(((top_screen.GetWidth() - 1) / Core::kScreenTopWidth) + 1);
    } else {
        return static_cast<u32>(((top_screen.GetWidth() - 1) / Core::kScreenTopHeight) + 1);
    }
}

// Finds the largest size subrectangle contained in window area that is confined to the aspect ratio
template <class T>
static Common::Rectangle<T> MaxRectangle(Common::Rectangle<T> window_area,
                                         float screen_aspect_ratio) {
    float scale = std::min(static_cast<float>(window_area.GetWidth()),
                           window_area.GetHeight() / screen_aspect_ratio);
    return Common::Rectangle<T>{0, 0, static_cast<T>(std::round(scale)),
                                static_cast<T>(std::round(scale * screen_aspect_ratio))};
}

FramebufferLayout DefaultFrameLayout(u32 width, u32 height, bool swapped, bool upright) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}, !upright};
    Common::Rectangle<u32> screen_window_area;
    Common::Rectangle<u32> top_screen;
    Common::Rectangle<u32> bot_screen;
    float emulation_aspect_ratio;
    if (upright) {
        // Default layout gives equal screen sizes to the top and bottom screen
        screen_window_area = {0, 0, width / 2, height};
        top_screen = MaxRectangle(screen_window_area, TOP_SCREEN_UPRIGHT_ASPECT_RATIO);
        bot_screen = MaxRectangle(screen_window_area, BOT_SCREEN_UPRIGHT_ASPECT_RATIO);
        // both screens width are taken into account by dividing by 2
        emulation_aspect_ratio = TOP_SCREEN_UPRIGHT_ASPECT_RATIO / 2;
    } else {
        // Default layout gives equal screen sizes to the top and bottom screen
        screen_window_area = {0, 0, width, height / 2};
        top_screen = MaxRectangle(screen_window_area, TOP_SCREEN_ASPECT_RATIO);
        bot_screen = MaxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);
        // both screens height are taken into account by multiplying by 2
        emulation_aspect_ratio = TOP_SCREEN_ASPECT_RATIO * 2;
    }

    float window_aspect_ratio = static_cast<float>(height) / width;

    if (window_aspect_ratio < emulation_aspect_ratio) {
        // Window is wider than the emulation content => apply borders to the right and left sides
        if (upright) {
            // Recalculate the bottom screen to account for the height difference between right and
            // left
            screen_window_area = {0, 0, top_screen.GetWidth(), height};
            bot_screen = MaxRectangle(screen_window_area, BOT_SCREEN_UPRIGHT_ASPECT_RATIO);
            bot_screen =
                bot_screen.TranslateY((top_screen.GetHeight() - bot_screen.GetHeight()) / 2);
            if (swapped) {
                bot_screen = bot_screen.TranslateX(width / 2 - bot_screen.GetWidth());
            } else {
                top_screen = top_screen.TranslateX(width / 2 - top_screen.GetWidth());
            }
        } else {
            top_screen =
                top_screen.TranslateX((screen_window_area.GetWidth() - top_screen.GetWidth()) / 2);
            bot_screen =
                bot_screen.TranslateX((screen_window_area.GetWidth() - bot_screen.GetWidth()) / 2);
        }
    } else {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        if (upright) {
            top_screen = top_screen.TranslateY(
                (screen_window_area.GetHeight() - top_screen.GetHeight()) / 2);
            bot_screen = bot_screen.TranslateY(
                (screen_window_area.GetHeight() - bot_screen.GetHeight()) / 2);
        } else {
            // Recalculate the bottom screen to account for the width difference between top and
            // bottom
            screen_window_area = {0, 0, width, top_screen.GetHeight()};
            bot_screen = MaxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);
            bot_screen = bot_screen.TranslateX((top_screen.GetWidth() - bot_screen.GetWidth()) / 2);
            if (swapped) {
                bot_screen = bot_screen.TranslateY(height / 2 - bot_screen.GetHeight());
            } else {
                top_screen = top_screen.TranslateY(height / 2 - top_screen.GetHeight());
            }
        }
    }
    if (upright) {
        // Move the top screen to the right if we are swapped.
        res.top_screen = swapped ? top_screen.TranslateX(width / 2) : top_screen;
        res.bottom_screen = swapped ? bot_screen : bot_screen.TranslateX(width / 2);
    } else {
        // Move the top screen to the bottom if we are swapped.
        res.top_screen = swapped ? top_screen.TranslateY(height / 2) : top_screen;
        res.bottom_screen = swapped ? bot_screen : bot_screen.TranslateY(height / 2);
    }
    return res;
}

FramebufferLayout MobilePortraitFrameLayout(u32 width, u32 height, bool swapped) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}};
    // Default layout gives equal screen sizes to the top and bottom screen
    Common::Rectangle<u32> screen_window_area{0, 0, width, height / 2};
    Common::Rectangle<u32> top_screen = MaxRectangle(screen_window_area, TOP_SCREEN_ASPECT_RATIO);
    Common::Rectangle<u32> bot_screen = MaxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);

    float window_aspect_ratio = static_cast<float>(height) / width;
    // both screens height are taken into account by multiplying by 2
    float emulation_aspect_ratio = TOP_SCREEN_ASPECT_RATIO * 2;

    if (window_aspect_ratio < emulation_aspect_ratio) {
        // Apply borders to the left and right sides of the window.
        top_screen =
            top_screen.TranslateX((screen_window_area.GetWidth() - top_screen.GetWidth()) / 2);
        bot_screen =
            bot_screen.TranslateX((screen_window_area.GetWidth() - bot_screen.GetWidth()) / 2);
    } else {
        // Window is narrower than the emulation content
        // Recalculate the bottom screen to account for the width difference between top and bottom

        bot_screen = bot_screen.TranslateX((top_screen.GetWidth() - bot_screen.GetWidth()) / 2);
    }

    // Move the top screen to the bottom if we are swapped.
    res.top_screen = swapped ? top_screen.TranslateY(bot_screen.GetHeight()) : top_screen;
    res.bottom_screen = swapped ? bot_screen : bot_screen.TranslateY(top_screen.GetHeight());

    return res;
}

FramebufferLayout SingleFrameLayout(u32 width, u32 height, bool swapped, bool upright) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    // The drawing code needs at least somewhat valid values for both screens
    // so just calculate them both even if the other isn't showing.
    FramebufferLayout res{width, height, !swapped, swapped, {}, {}, !upright};

    Common::Rectangle<u32> screen_window_area{0, 0, width, height};
    Common::Rectangle<u32> top_screen;
    Common::Rectangle<u32> bot_screen;
    float emulation_aspect_ratio;
    if (upright) {
        top_screen = MaxRectangle(screen_window_area, TOP_SCREEN_UPRIGHT_ASPECT_RATIO);
        bot_screen = MaxRectangle(screen_window_area, BOT_SCREEN_UPRIGHT_ASPECT_RATIO);
        emulation_aspect_ratio =
            (swapped) ? BOT_SCREEN_UPRIGHT_ASPECT_RATIO : TOP_SCREEN_UPRIGHT_ASPECT_RATIO;
    } else {
        top_screen = MaxRectangle(screen_window_area, TOP_SCREEN_ASPECT_RATIO);
        bot_screen = MaxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);
        emulation_aspect_ratio = (swapped) ? BOT_SCREEN_ASPECT_RATIO : TOP_SCREEN_ASPECT_RATIO;
    }

    float window_aspect_ratio = static_cast<float>(height) / width;

    if (window_aspect_ratio < emulation_aspect_ratio) {
        top_screen =
            top_screen.TranslateX((screen_window_area.GetWidth() - top_screen.GetWidth()) / 2);
        bot_screen =
            bot_screen.TranslateX((screen_window_area.GetWidth() - bot_screen.GetWidth()) / 2);
    } else {
        top_screen = top_screen.TranslateY((height - top_screen.GetHeight()) / 2);
        bot_screen = bot_screen.TranslateY((height - bot_screen.GetHeight()) / 2);
    }
    res.top_screen = top_screen;
    res.bottom_screen = bot_screen;
    return res;
}

FramebufferLayout LargeFrameLayout(u32 width, u32 height, bool swapped, bool upright,
                                   float scale_factor, VerticalAlignment vertical_alignment) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}, !upright};
    // Split the window into two parts. Give 4x width to the main screen and 1x width to the small
    // To do that, find the total emulation box and maximize that based on window size
    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio;
    float large_screen_aspect_ratio;
    float small_screen_aspect_ratio;
    if (upright) {
        if (swapped) {
            emulation_aspect_ratio =
                (Core::kScreenBottomWidth * scale_factor + Core::kScreenTopWidth) /
                (Core::kScreenBottomHeight * scale_factor);
            large_screen_aspect_ratio = BOT_SCREEN_UPRIGHT_ASPECT_RATIO;
            small_screen_aspect_ratio = TOP_SCREEN_UPRIGHT_ASPECT_RATIO;
        } else {
            emulation_aspect_ratio =
                (Core::kScreenTopWidth * scale_factor + Core::kScreenBottomWidth) /
                (Core::kScreenTopHeight * scale_factor);
            large_screen_aspect_ratio = TOP_SCREEN_UPRIGHT_ASPECT_RATIO;
            small_screen_aspect_ratio = BOT_SCREEN_UPRIGHT_ASPECT_RATIO;
        }
    } else {
        if (swapped) {
            emulation_aspect_ratio =
                Core::kScreenBottomHeight * scale_factor /
                (Core::kScreenBottomWidth * scale_factor + Core::kScreenTopWidth);
            large_screen_aspect_ratio = BOT_SCREEN_ASPECT_RATIO;
            small_screen_aspect_ratio = TOP_SCREEN_ASPECT_RATIO;
        } else {
            emulation_aspect_ratio =
                Core::kScreenTopHeight * scale_factor /
                (Core::kScreenTopWidth * scale_factor + Core::kScreenBottomWidth);
            large_screen_aspect_ratio = TOP_SCREEN_ASPECT_RATIO;
            small_screen_aspect_ratio = BOT_SCREEN_ASPECT_RATIO;
        }
    }

    Common::Rectangle<u32> screen_window_area{0, 0, width, height};
    Common::Rectangle<u32> total_rect = MaxRectangle(screen_window_area, emulation_aspect_ratio);
    Common::Rectangle<u32> large_screen = MaxRectangle(total_rect, large_screen_aspect_ratio);
    Common::Rectangle<u32> scaled_rect = total_rect.Scale(1.f / scale_factor);
    Common::Rectangle<u32> small_screen = MaxRectangle(scaled_rect, small_screen_aspect_ratio);

    if (window_aspect_ratio < emulation_aspect_ratio) {
        large_screen = large_screen.TranslateX((width - total_rect.GetWidth()) / 2);
    } else {
        large_screen = large_screen.TranslateY((height - total_rect.GetHeight()) / 2);
    }
    if (upright) {
        large_screen = large_screen.TranslateY(small_screen.GetHeight());
        small_screen = small_screen.TranslateY(large_screen.top - small_screen.GetHeight());
        switch (vertical_alignment) {
        case VerticalAlignment::Top:
            // Shift the small screen to the top right corner
            small_screen = small_screen.TranslateX(large_screen.left);
            break;
        case VerticalAlignment::Middle:
            // Shift the small screen to the center right
            small_screen = small_screen.TranslateX(
                ((large_screen.GetWidth() - small_screen.GetWidth()) / 2) + large_screen.left);
            break;
        case VerticalAlignment::Bottom:
            // Shift the small screen to the bottom right corner
            small_screen = small_screen.TranslateX(large_screen.right - small_screen.GetWidth());
            break;
        default:
            UNREACHABLE();
            break;
        }

    } else {
        small_screen = small_screen.TranslateX(large_screen.right);
        switch (vertical_alignment) {
        case VerticalAlignment::Top:
            // Shift the small screen to the top right corner
            small_screen = small_screen.TranslateY(large_screen.top);
            break;
        case VerticalAlignment::Middle:
            // Shift the small screen to the center right
            small_screen = small_screen.TranslateY(
                ((large_screen.GetHeight() - small_screen.GetHeight()) / 2) + large_screen.top);
            break;
        case VerticalAlignment::Bottom:
            // Shift the small screen to the bottom right corner
            small_screen = small_screen.TranslateY(large_screen.bottom - small_screen.GetHeight());
            break;
        default:
            UNREACHABLE();
            break;
        }
    }
    res.top_screen = swapped ? small_screen : large_screen;
    res.bottom_screen = swapped ? large_screen : small_screen;
    return res;
}

FramebufferLayout HybridScreenLayout(u32 width, u32 height, bool swapped, bool upright) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}, !upright, true, {}};

    // Split the window into two parts. Give 2.25x width to the main screen,
    // and make a bar on the right side with 1x width top screen and 1.25x width bottom screen
    // To do that, find the total emulation box and maximize that based on window size
    const float window_aspect_ratio = static_cast<float>(height) / width;
    const float scale_factor = 2.25f;

    float main_screen_aspect_ratio = TOP_SCREEN_ASPECT_RATIO;
    float hybrid_area_aspect_ratio = 27.f / 65;
    float top_screen_aspect_ratio = TOP_SCREEN_ASPECT_RATIO;
    float bot_screen_aspect_ratio = BOT_SCREEN_ASPECT_RATIO;

    if (swapped) {
        main_screen_aspect_ratio = BOT_SCREEN_ASPECT_RATIO;
        hybrid_area_aspect_ratio =
            Core::kScreenBottomHeight * scale_factor /
            (Core::kScreenBottomWidth * scale_factor + Core::kScreenTopWidth);
    }

    if (upright) {
        hybrid_area_aspect_ratio = 1.f / hybrid_area_aspect_ratio;
        main_screen_aspect_ratio = 1.f / main_screen_aspect_ratio;
        top_screen_aspect_ratio = TOP_SCREEN_UPRIGHT_ASPECT_RATIO;
        bot_screen_aspect_ratio = BOT_SCREEN_UPRIGHT_ASPECT_RATIO;
    }

    Common::Rectangle<u32> screen_window_area{0, 0, width, height};
    Common::Rectangle<u32> total_rect = MaxRectangle(screen_window_area, hybrid_area_aspect_ratio);
    Common::Rectangle<u32> large_main_screen = MaxRectangle(total_rect, main_screen_aspect_ratio);
    Common::Rectangle<u32> side_rect = total_rect.Scale(1.f / scale_factor);
    Common::Rectangle<u32> small_top_screen = MaxRectangle(side_rect, top_screen_aspect_ratio);
    Common::Rectangle<u32> small_bottom_screen = MaxRectangle(side_rect, bot_screen_aspect_ratio);

    if (window_aspect_ratio < hybrid_area_aspect_ratio) {
        large_main_screen = large_main_screen.TranslateX((width - total_rect.GetWidth()) / 2);
    } else {
        large_main_screen = large_main_screen.TranslateY((height - total_rect.GetHeight()) / 2);
    }

    // Scale the bottom screen so it's width is the same as top screen
    small_bottom_screen = small_bottom_screen.Scale(1.25f);
    if (upright) {
        large_main_screen = large_main_screen.TranslateY(small_bottom_screen.GetHeight());
        // Shift small bottom screen to upper right corner
        small_bottom_screen =
            small_bottom_screen.TranslateX(large_main_screen.right - small_bottom_screen.GetWidth())
                .TranslateY(large_main_screen.top - small_bottom_screen.GetHeight());

        // Shift small top screen to upper left corner
        small_top_screen = small_top_screen.TranslateX(large_main_screen.left)
                               .TranslateY(large_main_screen.top - small_bottom_screen.GetHeight());
    } else {
        // Shift the small bottom screen to the bottom right corner
        small_bottom_screen =
            small_bottom_screen.TranslateX(large_main_screen.right)
                .TranslateY(large_main_screen.GetHeight() + large_main_screen.top -
                            small_bottom_screen.GetHeight());

        // Shift small top screen to upper right corner
        small_top_screen =
            small_top_screen.TranslateX(large_main_screen.right).TranslateY(large_main_screen.top);
    }

    res.top_screen = small_top_screen;
    res.additional_screen = swapped ? small_bottom_screen : large_main_screen;
    res.bottom_screen = swapped ? large_main_screen : small_bottom_screen;
    return res;
}

FramebufferLayout SeparateWindowsLayout(u32 width, u32 height, bool is_secondary, bool upright) {
    // When is_secondary is true, we disable the top screen, and enable the bottom screen.
    // The same logic is found in the SingleFrameLayout using the is_swapped bool.
    is_secondary = Settings::values.swap_screen ? !is_secondary : is_secondary;
    return SingleFrameLayout(width, height, is_secondary, upright);
}

FramebufferLayout CustomFrameLayout(u32 width, u32 height, bool is_swapped) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}, !Settings::values.upright_screen};

    Common::Rectangle<u32> top_screen{Settings::values.custom_top_left.GetValue(),
                                      Settings::values.custom_top_top.GetValue(),
                                      Settings::values.custom_top_right.GetValue(),
                                      Settings::values.custom_top_bottom.GetValue()};
    Common::Rectangle<u32> bot_screen{Settings::values.custom_bottom_left.GetValue(),
                                      Settings::values.custom_bottom_top.GetValue(),
                                      Settings::values.custom_bottom_right.GetValue(),
                                      Settings::values.custom_bottom_bottom.GetValue()};

    if (is_swapped) {
        res.top_screen = bot_screen;
        res.bottom_screen = top_screen;
    } else {
        res.top_screen = top_screen;
        res.bottom_screen = bot_screen;
    }
    return res;
}

FramebufferLayout FrameLayoutFromResolutionScale(u32 res_scale, bool is_secondary) {
    if (Settings::values.custom_layout.GetValue() == true) {
        return CustomFrameLayout(std::max(Settings::values.custom_top_right.GetValue(),
                                          Settings::values.custom_bottom_right.GetValue()),
                                 std::max(Settings::values.custom_top_bottom.GetValue(),
                                          Settings::values.custom_bottom_bottom.GetValue()),
                                 Settings::values.swap_screen.GetValue());
    }

    int width, height;
    switch (Settings::values.layout_option.GetValue()) {
    case Settings::LayoutOption::SingleScreen:
#ifndef ANDROID
    case Settings::LayoutOption::SeparateWindows:
#endif
    {
        const bool swap_screens = is_secondary || Settings::values.swap_screen.GetValue();
        if (swap_screens) {
            width = Core::kScreenBottomWidth * res_scale;
            height = Core::kScreenBottomHeight * res_scale;
        } else {
            width = Core::kScreenTopWidth * res_scale;
            height = Core::kScreenTopHeight * res_scale;
        }
        if (Settings::values.upright_screen.GetValue()) {
            std::swap(width, height);
        }
        return SingleFrameLayout(width, height, swap_screens,
                                 Settings::values.upright_screen.GetValue());
    }

    case Settings::LayoutOption::LargeScreen:
        if (Settings::values.swap_screen.GetValue()) {
            width = (Core::kScreenBottomWidth +
                     Core::kScreenTopWidth /
                         static_cast<int>(Settings::values.large_screen_proportion.GetValue())) *
                    res_scale;
            height = Core::kScreenBottomHeight * res_scale;
        } else {
            width = (Core::kScreenTopWidth +
                     Core::kScreenBottomWidth /
                         static_cast<int>(Settings::values.large_screen_proportion.GetValue())) *
                    res_scale;
            height = Core::kScreenTopHeight * res_scale;
        }
        if (Settings::values.upright_screen.GetValue()) {
            std::swap(width, height);
        }
        return LargeFrameLayout(width, height, Settings::values.swap_screen.GetValue(),
                                Settings::values.upright_screen.GetValue(),
                                Settings::values.large_screen_proportion.GetValue(),
                                VerticalAlignment::Bottom);

    case Settings::LayoutOption::SideScreen:
        width = (Core::kScreenTopWidth + Core::kScreenBottomWidth) * res_scale;
        height = Core::kScreenTopHeight * res_scale;

        if (Settings::values.upright_screen.GetValue()) {
            std::swap(width, height);
        }
        return LargeFrameLayout(width, height, Settings::values.swap_screen.GetValue(),
                                Settings::values.upright_screen.GetValue(), 1,
                                VerticalAlignment::Middle);

    case Settings::LayoutOption::MobilePortrait:
        width = Core::kScreenTopWidth * res_scale;
        height = (Core::kScreenTopHeight + Core::kScreenBottomHeight) * res_scale;
        return MobilePortraitFrameLayout(width, height, Settings::values.swap_screen.GetValue());

    case Settings::LayoutOption::MobileLandscape: {
        constexpr float large_screen_proportion = 2.25f;
        if (Settings::values.swap_screen.GetValue()) {
            width = (Core::kScreenBottomWidth +
                     static_cast<int>(Core::kScreenTopWidth / large_screen_proportion)) *
                    res_scale;
            height = Core::kScreenBottomHeight * res_scale;
        } else {
            width = (Core::kScreenTopWidth +
                     static_cast<int>(Core::kScreenBottomWidth / large_screen_proportion)) *
                    res_scale;
            height = Core::kScreenTopHeight * res_scale;
        }
        return LargeFrameLayout(width, height, Settings::values.swap_screen.GetValue(), false,
                                large_screen_proportion, VerticalAlignment::Top);
    }

    case Settings::LayoutOption::Default:
    default:
        width = Core::kScreenTopWidth * res_scale;
        height = (Core::kScreenTopHeight + Core::kScreenBottomHeight) * res_scale;

        if (Settings::values.upright_screen.GetValue()) {
            std::swap(width, height);
        }
        return DefaultFrameLayout(width, height, Settings::values.swap_screen.GetValue(),
                                  Settings::values.upright_screen.GetValue());
    }
    UNREACHABLE();
}

FramebufferLayout GetCardboardSettings(const FramebufferLayout& layout) {
    u32 top_screen_left = 0;
    u32 top_screen_top = 0;
    u32 bottom_screen_left = 0;
    u32 bottom_screen_top = 0;

    u32 cardboard_screen_scale = Settings::values.cardboard_screen_size.GetValue();
    u32 top_screen_width = ((layout.top_screen.GetWidth() / 2) * cardboard_screen_scale) / 100;
    u32 top_screen_height = ((layout.top_screen.GetHeight() / 2) * cardboard_screen_scale) / 100;
    u32 bottom_screen_width =
        ((layout.bottom_screen.GetWidth() / 2) * cardboard_screen_scale) / 100;
    u32 bottom_screen_height =
        ((layout.bottom_screen.GetHeight() / 2) * cardboard_screen_scale) / 100;
    const bool is_swapped = Settings::values.swap_screen.GetValue();
    const bool is_portrait = layout.height > layout.width;

    u32 cardboard_screen_width;
    u32 cardboard_screen_height;
    switch (Settings::values.layout_option.GetValue()) {
    case Settings::LayoutOption::MobileLandscape:
    case Settings::LayoutOption::SideScreen:
        // If orientation is portrait, only use MobilePortrait
        if (!is_portrait) {
            cardboard_screen_width = top_screen_width + bottom_screen_width;
            cardboard_screen_height = is_swapped ? bottom_screen_height : top_screen_height;
            if (is_swapped)
                top_screen_left += bottom_screen_width;
            else
                bottom_screen_left += top_screen_width;
            break;
        } else {
            [[fallthrough]];
        }
    case Settings::LayoutOption::SingleScreen:
    default:
        if (!is_portrait) {
            // Default values when using LayoutOption::SingleScreen
            cardboard_screen_width = is_swapped ? bottom_screen_width : top_screen_width;
            cardboard_screen_height = is_swapped ? bottom_screen_height : top_screen_height;
            break;
        } else {
            [[fallthrough]];
        }
    case Settings::LayoutOption::MobilePortrait:
        cardboard_screen_width = top_screen_width;
        cardboard_screen_height = top_screen_height + bottom_screen_height;
        bottom_screen_left += (top_screen_width - bottom_screen_width) / 2;
        if (is_swapped)
            top_screen_top += bottom_screen_height;
        else
            bottom_screen_top += top_screen_height;
        break;
    }
    s32 cardboard_max_x_shift = (layout.width / 2 - cardboard_screen_width) / 2;
    s32 cardboard_user_x_shift =
        (Settings::values.cardboard_x_shift.GetValue() * cardboard_max_x_shift) / 100;
    s32 cardboard_max_y_shift = (layout.height - cardboard_screen_height) / 2;
    s32 cardboard_user_y_shift =
        (Settings::values.cardboard_y_shift.GetValue() * cardboard_max_y_shift) / 100;

    // Center the screens and apply user Y shift
    FramebufferLayout new_layout = layout;
    new_layout.top_screen.left = top_screen_left + cardboard_max_x_shift;
    new_layout.top_screen.top = top_screen_top + cardboard_max_y_shift + cardboard_user_y_shift;
    new_layout.bottom_screen.left = bottom_screen_left + cardboard_max_x_shift;
    new_layout.bottom_screen.top =
        bottom_screen_top + cardboard_max_y_shift + cardboard_user_y_shift;

    // Set the X coordinates for the right eye and apply user X shift
    new_layout.cardboard.top_screen_right_eye = new_layout.top_screen.left - cardboard_user_x_shift;
    new_layout.top_screen.left += cardboard_user_x_shift;
    new_layout.cardboard.bottom_screen_right_eye =
        new_layout.bottom_screen.left - cardboard_user_x_shift;
    new_layout.bottom_screen.left += cardboard_user_x_shift;
    new_layout.cardboard.user_x_shift = cardboard_user_x_shift;

    // Update right/bottom instead of passing new variables for width/height
    new_layout.top_screen.right = new_layout.top_screen.left + top_screen_width;
    new_layout.top_screen.bottom = new_layout.top_screen.top + top_screen_height;
    new_layout.bottom_screen.right = new_layout.bottom_screen.left + bottom_screen_width;
    new_layout.bottom_screen.bottom = new_layout.bottom_screen.top + bottom_screen_height;

    return new_layout;
}

std::pair<unsigned, unsigned> GetMinimumSizeFromLayout(Settings::LayoutOption layout,
                                                       bool upright_screen) {
    u32 min_width, min_height;

    switch (layout) {
    case Settings::LayoutOption::SingleScreen:
#ifndef ANDROID
    case Settings::LayoutOption::SeparateWindows:
#endif
        min_width = Settings::values.swap_screen ? Core::kScreenBottomWidth : Core::kScreenTopWidth;
        min_height = Core::kScreenBottomHeight;
        break;
    case Settings::LayoutOption::LargeScreen:
        min_width = static_cast<u32>(
            Settings::values.swap_screen
                ? Core::kScreenTopWidth / Settings::values.large_screen_proportion.GetValue() +
                      Core::kScreenBottomWidth
                : Core::kScreenTopWidth + Core::kScreenBottomWidth /
                                              Settings::values.large_screen_proportion.GetValue());
        min_height = Core::kScreenBottomHeight;
        break;
    case Settings::LayoutOption::SideScreen:
        min_width = Core::kScreenTopWidth + Core::kScreenBottomWidth;
        min_height = Core::kScreenBottomHeight;
        break;
    case Settings::LayoutOption::Default:
    default:
        min_width = Core::kScreenTopWidth;
        min_height = Core::kScreenTopHeight + Core::kScreenBottomHeight;
        break;
    }
    if (upright_screen) {
        return std::make_pair(min_height, min_width);
    } else {
        return std::make_pair(min_width, min_height);
    }
}

} // namespace Layout

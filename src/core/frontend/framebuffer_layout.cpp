// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
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

u32 FramebufferLayout::GetScalingRatio() const {
    if (is_rotated) {
        return static_cast<u32>(((top_screen.GetWidth() - 1) / Core::kScreenTopWidth) + 1);
    } else {
        return static_cast<u32>(((top_screen.GetWidth() - 1) / Core::kScreenTopHeight) + 1);
    }
}

// Finds the largest size subrectangle contained in window area that is confined to the aspect ratio
// aligned to the upper-left corner of the bounding rectangle
template <class T>
static Common::Rectangle<T> MaxRectangle(Common::Rectangle<T> window_area,
                                         float window_aspect_ratio) {
    float scale = std::min(static_cast<float>(window_area.GetWidth()),
                           window_area.GetHeight() / window_aspect_ratio);
    return Common::Rectangle<T>{
        window_area.left, window_area.top, window_area.left + static_cast<T>(std::round(scale)),
        window_area.top + static_cast<T>(std::round(scale * window_aspect_ratio))};
}

// overload of the above that takes an inner rectangle instead of an aspect ratio, and can be
// limited to integer scaling or pixel by pixel mapping if desired
template <class T>
static Common::Rectangle<T> MaxRectangle(Common::Rectangle<T> bounding_window,
                                         Common::Rectangle<T> inner_window,
                                         Settings::ScalingMode scaling_mode) {
    float scale =
        std::min(static_cast<float>(bounding_window.GetWidth()) / inner_window.GetWidth(),
                 static_cast<float>(bounding_window.GetHeight()) / inner_window.GetHeight());

    if (scaling_mode == Settings::ScalingMode::PixelByPixel) {
        if (Settings::values.resolution_factor.GetValue() != 0) {
            const float resolution_factor = Settings::values.resolution_factor.GetValue();
            u32 divisor = 1;
            while (resolution_factor * inner_window.GetWidth() / divisor >
                       bounding_window.GetWidth() ||
                   resolution_factor * inner_window.GetHeight() / divisor >
                       bounding_window.GetHeight()) {
                divisor *= 2;
            };
            scale = resolution_factor / divisor;
        } else if (scale >= 1.0) {
            scale = std::floor(scale);
        }
    }

    if (scaling_mode == Settings::ScalingMode::FitToScreenInteger && scale >= 1.0) {
        scale = std::floor(scale);
    }

    return Common::Rectangle(bounding_window.left, bounding_window.top,
                             bounding_window.left + static_cast<T>(inner_window.GetWidth() * scale),
                             bounding_window.top +
                                 static_cast<T>(inner_window.GetHeight() * scale));
}

FramebufferLayout DefaultFrameLayout(u32 width, u32 height, bool swapped, bool upright) {
    return LargeFrameLayout(width, height, swapped, upright, 1.0f,
                            Settings::SmallScreenPosition::BelowLarge);
}

FramebufferLayout PortraitTopFullFrameLayout(u32 width, u32 height, bool swapped, bool upright) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    const float scale_factor = swapped ? 1.25f : 0.8f;
    FramebufferLayout res = LargeFrameLayout(width, height, swapped, upright, scale_factor,
                                             Settings::SmallScreenPosition::BelowLarge);
    const int shiftY = -(int)(swapped ? res.bottom_screen.top : res.top_screen.top);
    res.top_screen = res.top_screen.TranslateY(shiftY);
    res.bottom_screen = res.bottom_screen.TranslateY(shiftY);
    return res;
}

FramebufferLayout PortraitOriginalLayout(u32 width, u32 height, bool swapped, bool upright) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    const float scale_factor = 1;
    FramebufferLayout res = LargeFrameLayout(width, height, swapped, upright, scale_factor,
                                             Settings::SmallScreenPosition::BelowLarge);
    const int shiftY = -(int)(swapped ? res.bottom_screen.top : res.top_screen.top);
    res.top_screen = res.top_screen.TranslateY(shiftY);
    res.bottom_screen = res.bottom_screen.TranslateY(shiftY);
    return res;
}

FramebufferLayout SingleFrameLayout(u32 width, u32 height, bool swapped, bool upright) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    // The drawing code needs at least somewhat valid values for both screens
    // so just calculate them both even if the other isn't showing.
    if (upright) {
        std::swap(width, height);
    }
    FramebufferLayout res{width, height, !swapped, swapped, {}, {}, !upright};

    Common::Rectangle<u32> screen_window_area{0, 0, width, height};
    Common::Rectangle<u32> top_screen{0, 0, Core::kScreenTopWidth, Core::kScreenTopHeight};
    Common::Rectangle<u32> bot_screen{0, 0, Core::kScreenBottomWidth, Core::kScreenBottomHeight};

    const float window_aspect_ratio = static_cast<float>(height) / static_cast<float>(width);
    const auto aspect_ratio_setting = Settings::values.aspect_ratio.GetValue();

    switch (aspect_ratio_setting) {
    case Settings::AspectRatio::Default:
        // this is the only one where we allow integer scaling to apply
        // also the only option on desktop
        top_screen =
            MaxRectangle(screen_window_area, top_screen, Settings::values.scaling_mode.GetValue());
        bot_screen =
            MaxRectangle(screen_window_area, bot_screen, Settings::values.scaling_mode.GetValue());
        break;
    case Settings::AspectRatio::Stretch:
        top_screen = MaxRectangle(screen_window_area, window_aspect_ratio);
        bot_screen = MaxRectangle(screen_window_area, window_aspect_ratio);
        break;
    default:
        float emulation_aspect_ratio = FramebufferLayout::GetAspectRatioValue(aspect_ratio_setting);
        top_screen = MaxRectangle(screen_window_area, emulation_aspect_ratio);
        bot_screen = MaxRectangle(screen_window_area, emulation_aspect_ratio);
    }

    const bool stretched = (Settings::values.screen_top_stretch.GetValue() && !swapped) ||
                           (Settings::values.screen_bottom_stretch.GetValue() && swapped);
    if (stretched) {
        top_screen = {Settings::values.screen_top_leftright_padding.GetValue(),
                      Settings::values.screen_top_topbottom_padding.GetValue(),
                      width - Settings::values.screen_top_leftright_padding.GetValue(),
                      height - Settings::values.screen_top_topbottom_padding.GetValue()};
        bot_screen = {Settings::values.screen_bottom_leftright_padding.GetValue(),
                      Settings::values.screen_bottom_topbottom_padding.GetValue(),
                      width - Settings::values.screen_bottom_leftright_padding.GetValue(),
                      height - Settings::values.screen_bottom_topbottom_padding.GetValue()};
    } else {
        top_screen = top_screen.TranslateX((width - top_screen.GetWidth()) / 2)
                         .TranslateY((height - top_screen.GetHeight()) / 2);
        bot_screen = bot_screen.TranslateX((width - bot_screen.GetWidth()) / 2)
                         .TranslateY((height - bot_screen.GetHeight()) / 2);
    }

    res.top_screen = top_screen;
    res.bottom_screen = bot_screen;
    if (upright) {
        return reverseLayout(res);
    } else {
        return res;
    }
}

FramebufferLayout LargeFrameLayout(u32 width, u32 height, bool swapped, bool upright,
                                   float scale_factor,
                                   Settings::SmallScreenPosition small_screen_position) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    if (upright) {
        std::swap(width, height);
    }
    const bool vertical = (small_screen_position == Settings::SmallScreenPosition::AboveLarge ||
                           small_screen_position == Settings::SmallScreenPosition::BelowLarge);
    FramebufferLayout res{width, height, true, true, {}, {}, !upright};
    // Split the window into two parts. Give proportional width to the smaller screen
    // To do that, find the total emulation box and maximize that based on window size
    u32 gap = (u32)(Settings::values.screen_gap.GetValue());

    u32 large_height = swapped ? Core::kScreenBottomHeight : Core::kScreenTopHeight;
    u32 small_height = static_cast<u32>(swapped ? Core::kScreenTopHeight / scale_factor
                                                : Core::kScreenBottomHeight / scale_factor);
    u32 large_width = swapped ? Core::kScreenBottomWidth : Core::kScreenTopWidth;
    u32 small_width = static_cast<u32>(swapped ? Core::kScreenTopWidth / scale_factor
                                               : Core::kScreenBottomWidth / scale_factor);

    u32 emulation_width;
    u32 emulation_height;
    if (vertical) {
        // width is just the larger size at this point
        emulation_width = std::max(large_width, small_width);
        emulation_height = large_height + small_height + gap;
    } else {
        emulation_width = large_width + small_width + gap;
        emulation_height = std::max(large_height, small_height);
    }

    Common::Rectangle<u32> screen_window_area{0, 0, width, height};
    Common::Rectangle<u32> total_rect{0, 0, emulation_width, emulation_height};
    total_rect =
        MaxRectangle(screen_window_area, total_rect, Settings::values.scaling_mode.GetValue());
    total_rect = total_rect.TranslateX((width - total_rect.GetWidth()) / 2)
                     .TranslateY((height - total_rect.GetHeight()) / 2);

    const float scale_amount = static_cast<float>(total_rect.GetHeight()) / emulation_height;
    gap = static_cast<u32>(static_cast<float>(gap) * scale_amount);

    Common::Rectangle<u32> large_screen =
        Common::Rectangle<u32>{total_rect.left, total_rect.top,
                               static_cast<u32>(large_width * scale_amount + total_rect.left),
                               static_cast<u32>(large_height * scale_amount + total_rect.top)};
    Common::Rectangle<u32> small_screen =
        Common::Rectangle<u32>{total_rect.left, total_rect.top,
                               static_cast<u32>(small_width * scale_amount + total_rect.left),
                               static_cast<u32>(small_height * scale_amount + total_rect.top)};

    switch (small_screen_position) {
    case Settings::SmallScreenPosition::TopRight:
        // Shift the small screen to the top right corner
        small_screen = small_screen.TranslateX(large_screen.GetWidth() + gap);
        small_screen = small_screen.TranslateY(large_screen.top - small_screen.top);
        break;
    case Settings::SmallScreenPosition::MiddleRight:
        // Shift the small screen to the center right
        small_screen = small_screen.TranslateX(large_screen.GetWidth() + gap);
        small_screen =
            small_screen.TranslateY(((large_screen.GetHeight() - small_screen.GetHeight()) / 2) +
                                    large_screen.top - small_screen.top);
        break;
    case Settings::SmallScreenPosition::BottomRight:
        // Shift the small screen to the bottom right corner
        small_screen = small_screen.TranslateX(large_screen.GetWidth() + gap);
        small_screen = small_screen.TranslateY(large_screen.bottom - small_screen.bottom);
        break;
    case Settings::SmallScreenPosition::TopLeft:
        // shift the large screen to the upper right of the small screen
        large_screen = large_screen.TranslateX(small_screen.GetWidth() + gap);
        break;
    case Settings::SmallScreenPosition::MiddleLeft:
        // shift the small screen to the middle left and shift the large screen to its right
        large_screen = large_screen.TranslateX(small_screen.GetWidth() + gap);
        small_screen =
            small_screen.TranslateY(((large_screen.GetHeight() - small_screen.GetHeight()) / 2));
        break;
    case Settings::SmallScreenPosition::BottomLeft:
        // shift the small screen to the bottom left and shift the large screen to its right
        large_screen = large_screen.TranslateX(small_screen.GetWidth() + gap);
        small_screen = small_screen.TranslateY(large_screen.bottom - small_screen.bottom);
        break;
    case Settings::SmallScreenPosition::AboveLarge:
        // shift the large screen down
        large_screen = large_screen.TranslateY(small_screen.GetHeight() + gap);
        // If the "large screen" is actually smaller, center it
        if (large_screen.GetWidth() < total_rect.GetWidth()) {
            large_screen =
                large_screen.TranslateX((total_rect.GetWidth() - large_screen.GetWidth()) / 2);
        }
        small_screen =
            small_screen.TranslateX((large_screen.left - total_rect.left) +
                                    large_screen.GetWidth() / 2 - small_screen.GetWidth() / 2);
        break;
    case Settings::SmallScreenPosition::BelowLarge:
        // shift the bottom_screen down and then over to the center
        // If the "large screen" is actually smaller, center it
        if (large_screen.GetWidth() < total_rect.GetWidth()) {
            large_screen =
                large_screen.TranslateX((total_rect.GetWidth() - large_screen.GetWidth()) / 2);
        }
        small_screen = small_screen.TranslateY(large_screen.GetHeight() + gap);
        small_screen =
            small_screen.TranslateX((large_screen.left - total_rect.left) +
                                    large_screen.GetWidth() / 2 - small_screen.GetWidth() / 2);
        break;
    default:
        UNREACHABLE();
        break;
    }
    res.top_screen = swapped ? small_screen : large_screen;
    res.bottom_screen = swapped ? large_screen : small_screen;
    if (upright) {
        return reverseLayout(res);
    } else {
        return res;
    }
}

FramebufferLayout HybridScreenLayout(u32 width, u32 height, bool swapped, bool upright) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    if (upright) {
        std::swap(width, height);
    }

    // use Large Screen layout with these specific ratios to get two of the pieces
    const float scale_factor = swapped ? 2.25 : 1.8;
    const Settings::SmallScreenPosition pos = swapped ? Settings::SmallScreenPosition::TopRight
                                                      : Settings::SmallScreenPosition::BottomRight;
    // always pass false as the upright value here, as it is being handled here not there
    FramebufferLayout res = LargeFrameLayout(width, height, swapped, false, scale_factor, pos);
    const Common::Rectangle<u32> main = swapped ? res.bottom_screen : res.top_screen;
    const Common::Rectangle<u32> small = swapped ? res.top_screen : res.bottom_screen;
    res.additional_screen = Common::Rectangle<u32>{small.left, swapped ? small.bottom : main.top,
                                                   small.right, swapped ? main.bottom : small.top};
    res.additional_screen_is_bottom = swapped;
    res.additional_screen_enabled = true;
    res.is_rotated = !upright;
    if (upright) {
        return reverseLayout(res);
    } else {
        return res;
    }
}

FramebufferLayout SeparateWindowsLayout(u32 width, u32 height, bool is_secondary, bool upright) {
    // When is_secondary is true, we disable the top screen, and enable the bottom screen.
    // The same logic is found in the SingleFrameLayout using the is_swapped bool.
    is_secondary = Settings::values.swap_screen ? !is_secondary : is_secondary;
    return SingleFrameLayout(width, height, is_secondary, upright);
}

FramebufferLayout AndroidSecondaryLayout(u32 width, u32 height) {
    const Settings::SecondaryDisplayLayout layout =
        Settings::values.secondary_display_layout.GetValue();
    switch (layout) {
    case Settings::SecondaryDisplayLayout::TopScreenOnly:
        return SingleFrameLayout(width, height, false, Settings::values.upright_screen.GetValue());

    case Settings::SecondaryDisplayLayout::BottomScreenOnly:
        return SingleFrameLayout(width, height, true, Settings::values.upright_screen.GetValue());
    case Settings::SecondaryDisplayLayout::SideBySide:
        return LargeFrameLayout(width, height, false, Settings::values.upright_screen.GetValue(),
                                1.0f, Settings::SmallScreenPosition::MiddleRight);
    case Settings::SecondaryDisplayLayout::LargeScreen:
        return LargeFrameLayout(width, height, false, Settings::values.upright_screen.GetValue(),
                                Settings::values.large_screen_proportion.GetValue(),
                                Settings::values.small_screen_position.GetValue());
    case Settings::SecondaryDisplayLayout::Original:
        return LargeFrameLayout(width, height, false, Settings::values.upright_screen.GetValue(),
                                1.0f, Settings::SmallScreenPosition::BelowLarge);
    case Settings::SecondaryDisplayLayout::Hybrid:
        return HybridScreenLayout(width, height, false, Settings::values.upright_screen.GetValue());
    case Settings::SecondaryDisplayLayout::None:
        // this should never happen - if "none" is set this method shouldn't run - but if it does,
        // somehow, use OppositeScreenOnly
    case Settings::SecondaryDisplayLayout::OppositeScreenOnly:
    default:
        return SingleFrameLayout(width, height, !Settings::values.swap_screen.GetValue(),
                                 Settings::values.upright_screen.GetValue());
    }
}

FramebufferLayout CustomFrameLayout(u32 width, u32 height, bool is_swapped, bool is_portrait_mode) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    const bool upright = Settings::values.upright_screen.GetValue();
    if (upright) {
        std::swap(width, height);
    }
    FramebufferLayout res{
        width, height, true, true, {}, {}, !Settings::values.upright_screen, is_portrait_mode};
    float opacity_value = Settings::values.custom_second_layer_opacity.GetValue() / 100.0f;

    if (!is_portrait_mode && opacity_value < 1) {
        is_swapped ? res.top_opacity = opacity_value : res.bottom_opacity = opacity_value;
    }

    const u16 top_x = is_portrait_mode ? Settings::values.custom_portrait_top_x.GetValue()
                                       : Settings::values.custom_top_x.GetValue();
    const u16 top_width = is_portrait_mode ? Settings::values.custom_portrait_top_width.GetValue()
                                           : Settings::values.custom_top_width.GetValue();
    const u16 top_y = is_portrait_mode ? Settings::values.custom_portrait_top_y.GetValue()
                                       : Settings::values.custom_top_y.GetValue();
    const u16 top_height = is_portrait_mode ? Settings::values.custom_portrait_top_height.GetValue()
                                            : Settings::values.custom_top_height.GetValue();
    const u16 bottom_x = is_portrait_mode ? Settings::values.custom_portrait_bottom_x.GetValue()
                                          : Settings::values.custom_bottom_x.GetValue();
    const u16 bottom_width = is_portrait_mode
                                 ? Settings::values.custom_portrait_bottom_width.GetValue()
                                 : Settings::values.custom_bottom_width.GetValue();
    const u16 bottom_y = is_portrait_mode ? Settings::values.custom_portrait_bottom_y.GetValue()
                                          : Settings::values.custom_bottom_y.GetValue();
    const u16 bottom_height = is_portrait_mode
                                  ? Settings::values.custom_portrait_bottom_height.GetValue()
                                  : Settings::values.custom_bottom_height.GetValue();

    Common::Rectangle<u32> top_screen{top_x, top_y, (u32)(top_x + top_width),
                                      (u32)(top_y + top_height)};
    Common::Rectangle<u32> bot_screen{bottom_x, bottom_y, (u32)(bottom_x + bottom_width),
                                      (u32)(bottom_y + bottom_height)};

    if (is_swapped) {
        res.top_screen = bot_screen;
        res.bottom_screen = top_screen;
    } else {
        res.top_screen = top_screen;
        res.bottom_screen = bot_screen;
    }
    if (upright) {
        return reverseLayout(res);
    } else {
        return res;
    }
}

FramebufferLayout FrameLayoutFromResolutionScale(u32 res_scale, bool is_secondary,
                                                 bool is_portrait) {
    u32 width, height, gap;
    gap = (int)(Settings::values.screen_gap.GetValue()) * res_scale;

    FramebufferLayout layout;
    if (is_portrait) {
        auto layout_option = Settings::values.portrait_layout_option.GetValue();
        switch (layout_option) {
        case Settings::PortraitLayoutOption::PortraitCustomLayout:
            width = std::max(Settings::values.custom_portrait_top_x.GetValue() +
                                 Settings::values.custom_portrait_top_width.GetValue(),
                             Settings::values.custom_portrait_bottom_x.GetValue() +
                                 Settings::values.custom_portrait_bottom_width.GetValue());
            height = std::max(Settings::values.custom_portrait_top_y.GetValue() +
                                  Settings::values.custom_portrait_top_height.GetValue(),
                              Settings::values.custom_portrait_bottom_y.GetValue() +
                                  Settings::values.custom_portrait_bottom_height.GetValue());
            layout = CustomFrameLayout(width, height, Settings::values.swap_screen.GetValue(),
                                       is_portrait);

            break;
        case Settings::PortraitLayoutOption::PortraitTopFullWidth:
            width = Core::kScreenTopWidth * res_scale;
            // clang-format off
            height = (static_cast<int>(Core::kScreenTopHeight + Core::kScreenBottomHeight * 1.25) *
                     res_scale) + gap;
            // clang-format on
            layout =
                PortraitTopFullFrameLayout(width, height, Settings::values.swap_screen.GetValue(),
                                           Settings::values.upright_screen.GetValue());
            break;
        case Settings::PortraitLayoutOption::PortraitOriginal:
            width = Core::kScreenTopWidth * res_scale;
            height = (Core::kScreenTopHeight + Core::kScreenBottomHeight) * res_scale;
            layout = PortraitOriginalLayout(width, height, Settings::values.swap_screen.GetValue());
            break;
        }
    } else {
        auto layout_option = Settings::values.layout_option.GetValue();
        switch (layout_option) {
        case Settings::LayoutOption::CustomLayout:
            layout =
                CustomFrameLayout(std::max(Settings::values.custom_top_x.GetValue() +
                                               Settings::values.custom_top_width.GetValue(),
                                           Settings::values.custom_bottom_x.GetValue() +
                                               Settings::values.custom_bottom_width.GetValue()),
                                  std::max(Settings::values.custom_top_y.GetValue() +
                                               Settings::values.custom_top_height.GetValue(),
                                           Settings::values.custom_bottom_y.GetValue() +
                                               Settings::values.custom_bottom_height.GetValue()),
                                  Settings::values.swap_screen.GetValue(), is_portrait);
            break;
        case Settings::LayoutOption::SingleScreen: {
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

            layout = SingleFrameLayout(width, height, swap_screens,
                                       Settings::values.upright_screen.GetValue());
            break;
        }

        case Settings::LayoutOption::LargeScreen: {
            const bool swapped = Settings::values.swap_screen.GetValue();
            const int largeWidth = swapped ? Core::kScreenBottomWidth : Core::kScreenTopWidth;
            const int largeHeight = swapped ? Core::kScreenBottomHeight : Core::kScreenTopHeight;
            const int smallWidth =
                static_cast<int>((swapped ? Core::kScreenTopWidth : Core::kScreenBottomWidth) /
                                 Settings::values.large_screen_proportion.GetValue());
            const int smallHeight =
                static_cast<int>((swapped ? Core::kScreenTopHeight : Core::kScreenBottomHeight) /
                                 Settings::values.large_screen_proportion.GetValue());

            if (Settings::values.small_screen_position.GetValue() ==
                    Settings::SmallScreenPosition::AboveLarge ||
                Settings::values.small_screen_position.GetValue() ==
                    Settings::SmallScreenPosition::BelowLarge) {
                // vertical, so height is sum of heights, width is larger of widths
                width = std::max(largeWidth, smallWidth) * res_scale;
                height = (largeHeight + smallHeight) * res_scale + gap;
            } else {
                width = (largeWidth + smallWidth) * res_scale + gap;
                height = std::max(largeHeight, smallHeight) * res_scale;
            }

            if (Settings::values.upright_screen.GetValue()) {
                std::swap(width, height);
            }
            layout = LargeFrameLayout(width, height, Settings::values.swap_screen.GetValue(),
                                      Settings::values.upright_screen.GetValue(),
                                      Settings::values.large_screen_proportion.GetValue(),
                                      Settings::values.small_screen_position.GetValue());
            break;
        }
        case Settings::LayoutOption::SideScreen:
            width = (Core::kScreenTopWidth + Core::kScreenBottomWidth) * res_scale + gap;
            height = Core::kScreenTopHeight * res_scale;

            if (Settings::values.upright_screen.GetValue()) {
                std::swap(width, height);
            }
            layout = LargeFrameLayout(width, height, Settings::values.swap_screen.GetValue(),
                                      Settings::values.upright_screen.GetValue(), 1,
                                      Settings::SmallScreenPosition::MiddleRight);
            break;
        case Settings::LayoutOption::HybridScreen:
            height = Core::kScreenTopHeight * res_scale;

            if (Settings::values.swap_screen.GetValue()) {
                width = Core::kScreenBottomWidth;
            } else {
                width = Core::kScreenTopWidth;
            }
            // 2.25f comes from HybridScreenLayout's scale_factor value.
            width = static_cast<int>((width + (Core::kScreenTopWidth / 2.25f)) * res_scale);

            if (Settings::values.upright_screen.GetValue()) {
                std::swap(width, height);
            }

            layout = HybridScreenLayout(width, height, Settings::values.swap_screen.GetValue(),
                                        Settings::values.upright_screen.GetValue());
            break;
        case Settings::LayoutOption::Default:
        default:
            width = Core::kScreenTopWidth * res_scale;
            height = (Core::kScreenTopHeight + Core::kScreenBottomHeight) * res_scale + gap;

            if (Settings::values.upright_screen.GetValue()) {
                std::swap(width, height);
            }
            layout = DefaultFrameLayout(width, height, Settings::values.swap_screen.GetValue(),
                                        Settings::values.upright_screen.GetValue());
            break;
        }
    }

    return layout;
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
    if (is_portrait) {
        switch (Settings::values.portrait_layout_option.GetValue()) {
        case Settings::PortraitLayoutOption::PortraitTopFullWidth:
        case Settings::PortraitLayoutOption::PortraitOriginal:
            cardboard_screen_width = top_screen_width;
            cardboard_screen_height = top_screen_height + bottom_screen_height;
            bottom_screen_left += (top_screen_width - bottom_screen_width) / 2;
            if (is_swapped)
                top_screen_top += bottom_screen_height;
            else
                bottom_screen_top += top_screen_height;
            break;
        default:
            cardboard_screen_width = is_swapped ? bottom_screen_width : top_screen_width;
            cardboard_screen_height = is_swapped ? bottom_screen_height : top_screen_height;
        }
    } else {
        switch (Settings::values.layout_option.GetValue()) {
        case Settings::LayoutOption::SideScreen:
            cardboard_screen_width = top_screen_width + bottom_screen_width;
            cardboard_screen_height = is_swapped ? bottom_screen_height : top_screen_height;
            if (is_swapped)
                top_screen_left += bottom_screen_width;
            else
                bottom_screen_left += top_screen_width;
            break;

        case Settings::LayoutOption::SingleScreen:
        default:

            cardboard_screen_width = is_swapped ? bottom_screen_width : top_screen_width;
            cardboard_screen_height = is_swapped ? bottom_screen_height : top_screen_height;
            break;
        }
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

FramebufferLayout reverseLayout(FramebufferLayout layout) {
    std::swap(layout.height, layout.width);
    u32 oldLeft, oldRight, oldTop, oldBottom;

    oldLeft = layout.top_screen.left;
    oldRight = layout.top_screen.right;
    oldTop = layout.top_screen.top;
    oldBottom = layout.top_screen.bottom;
    layout.top_screen.left = oldTop;
    layout.top_screen.right = oldBottom;
    layout.top_screen.top = layout.height - oldRight;
    layout.top_screen.bottom = layout.height - oldLeft;

    oldLeft = layout.bottom_screen.left;
    oldRight = layout.bottom_screen.right;
    oldTop = layout.bottom_screen.top;
    oldBottom = layout.bottom_screen.bottom;
    layout.bottom_screen.left = oldTop;
    layout.bottom_screen.right = oldBottom;
    layout.bottom_screen.top = layout.height - oldRight;
    layout.bottom_screen.bottom = layout.height - oldLeft;

    if (layout.additional_screen_enabled) {
        oldLeft = layout.additional_screen.left;
        oldRight = layout.additional_screen.right;
        oldTop = layout.additional_screen.top;
        oldBottom = layout.additional_screen.bottom;
        layout.additional_screen.left = oldTop;
        layout.additional_screen.right = oldBottom;
        layout.additional_screen.top = layout.height - oldRight;
        layout.additional_screen.bottom = layout.height - oldLeft;
    }
    return layout;
}

std::pair<unsigned, unsigned> GetMinimumSizeFromPortraitLayout() {
    const u32 min_width = Core::kScreenTopWidth;
    const u32 min_height = Core::kScreenTopHeight + Core::kScreenBottomHeight;
    return std::make_pair(min_width, min_height);
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
    case Settings::LayoutOption::LargeScreen: {
        const bool swapped = Settings::values.swap_screen.GetValue();
        const int largeWidth = swapped ? Core::kScreenBottomWidth : Core::kScreenTopWidth;
        const int largeHeight = swapped ? Core::kScreenBottomHeight : Core::kScreenTopHeight;
        int smallWidth = swapped ? Core::kScreenTopWidth : Core::kScreenBottomWidth;
        int smallHeight = swapped ? Core::kScreenTopHeight : Core::kScreenBottomHeight;
        smallWidth =
            static_cast<int>(smallWidth / Settings::values.large_screen_proportion.GetValue());
        smallHeight =
            static_cast<int>(smallHeight / Settings::values.large_screen_proportion.GetValue());
        min_width = static_cast<u32>(Settings::values.small_screen_position.GetValue() ==
                                                 Settings::SmallScreenPosition::AboveLarge ||
                                             Settings::values.small_screen_position.GetValue() ==
                                                 Settings::SmallScreenPosition::BelowLarge
                                         ? std::max(largeWidth, smallWidth)
                                         : largeWidth + smallWidth);
        min_height = static_cast<u32>(Settings::values.small_screen_position.GetValue() ==
                                                  Settings::SmallScreenPosition::AboveLarge ||
                                              Settings::values.small_screen_position.GetValue() ==
                                                  Settings::SmallScreenPosition::BelowLarge
                                          ? largeHeight + smallHeight
                                          : std::max(largeHeight, smallHeight));
        break;
    }
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

float FramebufferLayout::GetAspectRatioValue(Settings::AspectRatio aspect_ratio) {
    switch (aspect_ratio) {
    case Settings::AspectRatio::R16_9:
        return 9.0f / 16.0f;
    case Settings::AspectRatio::R4_3:
        return 3.0f / 4.0f;
    case Settings::AspectRatio::R21_9:
        return 9.0f / 21.0f;
    case Settings::AspectRatio::R16_10:
        return 10.0f / 16.0f;
    default:
        LOG_ERROR(Frontend, "Unknown aspect ratio enum value: {}",
                  static_cast<std::underlying_type<Settings::AspectRatio>::type>(aspect_ratio));
        return 1.0f; // Arbitrary fallback value
    }
}

} // namespace Layout

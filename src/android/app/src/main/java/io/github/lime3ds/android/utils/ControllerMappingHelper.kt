// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent

/**
 * Some controllers have incorrect mappings. This class has special-case fixes for them.
 */
object ControllerMappingHelper {
    /**
     * Some controllers report extra button presses that can be ignored.
     */
    fun shouldKeyBeIgnored(inputDevice: InputDevice, keyCode: Int): Boolean {
        return if (isDualShock4(inputDevice)) {
            // The two analog triggers generate analog motion events as well as a keycode.
            // We always prefer to use the analog values, so throw away the button press
            keyCode == KeyEvent.KEYCODE_BUTTON_L2 || keyCode == KeyEvent.KEYCODE_BUTTON_R2
        } else false
    }

    /**
     * Scale an axis to be zero-centered with a proper range.
     */
    fun scaleAxis(inputDevice: InputDevice, axis: Int, value: Float): Float {
        if (isDualShock4(inputDevice)) {
            // Android doesn't have correct mappings for this controller's triggers. It reports them
            // as RX & RY, centered at -1.0, and with a range of [-1.0, 1.0]
            // Scale them to properly zero-centered with a range of [0.0, 1.0].
            if (axis == MotionEvent.AXIS_RX || axis == MotionEvent.AXIS_RY) {
                return (value + 1) / 2.0f
            }
        } else if (isXboxOneWireless(inputDevice)) {
            // Same as the DualShock 4, the mappings are missing.
            if (axis == MotionEvent.AXIS_Z || axis == MotionEvent.AXIS_RZ) {
                return (value + 1) / 2.0f
            }
            if (axis == MotionEvent.AXIS_GENERIC_1) {
                // This axis is stuck at ~.5. Ignore it.
                return 0.0f
            }
        } else if (isMogaPro2Hid(inputDevice)) {
            // This controller has a broken axis that reports a constant value. Ignore it.
            if (axis == MotionEvent.AXIS_GENERIC_1) {
                return 0.0f
            }
        }
        return value
    }

    private fun isDualShock4(inputDevice: InputDevice): Boolean {
        // Sony DualShock 4 controller
        return inputDevice.vendorId == 0x54c && inputDevice.productId == 0x9cc
    }

    private fun isXboxOneWireless(inputDevice: InputDevice): Boolean {
        // Microsoft Xbox One controller
        return inputDevice.vendorId == 0x45e && inputDevice.productId == 0x2e0
    }

    private fun isMogaPro2Hid(inputDevice: InputDevice): Boolean {
        // Moga Pro 2 HID
        return inputDevice.vendorId == 0x20d6 && inputDevice.productId == 0x6271
    }
}

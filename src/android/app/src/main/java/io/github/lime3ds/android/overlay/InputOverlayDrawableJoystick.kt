// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.overlay

import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.BitmapDrawable
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import io.github.lime3ds.android.NativeLibrary
import io.github.lime3ds.android.utils.EmulationMenuSettings
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Custom [BitmapDrawable] that is capable
 * of storing it's own ID.
 *
 * @param res                [Resources] instance.
 * @param bitmapOuter        [Bitmap] which represents the outer non-movable part of the joystick.
 * @param bitmapInnerDefault [Bitmap] which represents the default inner movable part of the joystick.
 * @param bitmapInnerPressed [Bitmap] which represents the pressed inner movable part of the joystick.
 * @param rectOuter          [Rect] which represents the outer joystick bounds.
 * @param rectInner          [Rect] which represents the inner joystick bounds.
 * @param joystickId         Identifier for which joystick this is.
 * @param opacity            0-255 alpha value
 */
class InputOverlayDrawableJoystick(
    res: Resources,
    bitmapOuter: Bitmap,
    bitmapInnerDefault: Bitmap,
    bitmapInnerPressed: Bitmap,
    rectOuter: Rect,
    rectInner: Rect,
    val joystickId: Int,
    val opacity: Int
) {
    var trackId = -1
    var xAxis = 0f
    var yAxis = 0f
    var angle = 0f
    var radius = 0f
    private var controlPositionX = 0
    private var controlPositionY = 0
    private var previousTouchX = 0
    private var previousTouchY = 0
    val width: Int
    val height: Int
    private var virtBounds: Rect
    private var origBounds: Rect
    private val outerBitmap: BitmapDrawable
    private val defaultStateInnerBitmap: BitmapDrawable
    private val pressedStateInnerBitmap: BitmapDrawable
    private val boundsBoxBitmap: BitmapDrawable
    private var pressedState = false

    var bounds: Rect
        get() = outerBitmap.bounds
        set(bounds) {
            outerBitmap.bounds = bounds
        }

    init {
        outerBitmap = BitmapDrawable(res, bitmapOuter)
        defaultStateInnerBitmap = BitmapDrawable(res, bitmapInnerDefault)
        pressedStateInnerBitmap = BitmapDrawable(res, bitmapInnerPressed)
        boundsBoxBitmap = BitmapDrawable(res, bitmapOuter)
        width = bitmapOuter.width
        height = bitmapOuter.height
        bounds = rectOuter
        defaultStateInnerBitmap.bounds = rectInner
        pressedStateInnerBitmap.bounds = rectInner
        virtBounds = bounds
        origBounds = outerBitmap.copyBounds()
        boundsBoxBitmap.alpha = 0
        boundsBoxBitmap.bounds = virtBounds
        setInnerBounds()
        defaultStateInnerBitmap.alpha = opacity
        pressedStateInnerBitmap.alpha = opacity
        outerBitmap.alpha = opacity
    }

    fun draw(canvas: Canvas?) {
        outerBitmap.draw(canvas!!)
        currentStateBitmapDrawable.alpha = opacity
        currentStateBitmapDrawable.draw(canvas)
        boundsBoxBitmap.draw(canvas)
    }

    fun updateStatus(event: MotionEvent, overlay:InputOverlay): Boolean {
        val pointerIndex = event.actionIndex
        val xPosition = event.getX(pointerIndex).toInt()
        val yPosition = event.getY(pointerIndex).toInt()
        val pointerId = event.getPointerId(pointerIndex)
        val motionEvent = event.action and MotionEvent.ACTION_MASK
        val isActionDown =
            motionEvent == MotionEvent.ACTION_DOWN || motionEvent == MotionEvent.ACTION_POINTER_DOWN
        val isActionUp =
            motionEvent == MotionEvent.ACTION_UP || motionEvent == MotionEvent.ACTION_POINTER_UP
        if (isActionDown) {
            if (!bounds.contains(xPosition, yPosition)) {
                return false
            }
            pressedState = true
            outerBitmap.alpha = 0
            boundsBoxBitmap.alpha = opacity
            if (EmulationMenuSettings.joystickRelCenter) {
                virtBounds.offset(
                    xPosition - virtBounds.centerX(),
                    yPosition - virtBounds.centerY()
                )
            }
            boundsBoxBitmap.bounds = virtBounds
            trackId = pointerId
            overlay.hapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
        }
        if (isActionUp) {
            if (trackId != pointerId) {
                return false
            }
            pressedState = false
            xAxis = 0.0f
            yAxis = 0.0f
            angle = 0.0f
            radius = 0.0f
            outerBitmap.alpha = opacity
            boundsBoxBitmap.alpha = 0
            virtBounds = Rect(origBounds.left, origBounds.top, origBounds.right, origBounds.bottom)
            bounds = Rect(origBounds.left, origBounds.top, origBounds.right, origBounds.bottom)
            setInnerBounds()
            trackId = -1
            overlay.hapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY_RELEASE)
            return true
        }
        if (trackId == -1) return false
        for (i in 0 until event.pointerCount) {
            if (trackId != event.getPointerId(i)) {
                continue
            }
            var touchX = event.getX(i)
            var touchY = event.getY(i)
            var maxY = virtBounds.bottom.toFloat()
            var maxX = virtBounds.right.toFloat()
            touchX -= virtBounds.centerX().toFloat()
            maxX -= virtBounds.centerX().toFloat()
            touchY -= virtBounds.centerY().toFloat()
            maxY -= virtBounds.centerY().toFloat()
            val xAxis = touchX / maxX
            val yAxis = touchY / maxY
            val oldXAxis = this.xAxis
            val oldYAxis = this.yAxis
            val oldAngle = this.angle
            val oldRadius = this.radius

            // Clamp the circle pad input to a circle
            val angle = atan2(yAxis.toDouble(), xAxis.toDouble()).toFloat()
            var radius = sqrt((xAxis * xAxis + yAxis * yAxis).toDouble()).toFloat()
            if (radius > 1.0f) {
                radius = 1.0f
            }
            this.xAxis = cos(angle.toDouble()).toFloat() * radius
            this.yAxis = sin(angle.toDouble()).toFloat() * radius
            setInnerBounds()

            if (kotlin.math.abs(oldRadius - radius) > .34f
                    || radius > .5f && kotlin.math.abs(oldAngle - angle) > kotlin.math.PI / 8) {
                this.radius = radius
                this.angle = angle

                overlay.hapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
            }

            return oldXAxis != this.xAxis && oldYAxis != this.yAxis
        }
        return false
    }

    fun onConfigureTouch(event: MotionEvent): Boolean {
        val pointerIndex = event.actionIndex
        val fingerPositionX = event.getX(pointerIndex).toInt()
        val fingerPositionY = event.getY(pointerIndex).toInt()
        var scale = 1
        if (joystickId == NativeLibrary.ButtonType.STICK_C) {
            // C-stick is scaled down to be half the size of the circle pad
            scale = 2
        }
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                previousTouchX = fingerPositionX
                previousTouchY = fingerPositionY
            }

            MotionEvent.ACTION_MOVE -> {
                val deltaX = fingerPositionX - previousTouchX
                val deltaY = fingerPositionY - previousTouchY
                controlPositionX += deltaX
                controlPositionY += deltaY
                bounds = Rect(
                    controlPositionX,
                    controlPositionY,
                    outerBitmap.intrinsicWidth / scale + controlPositionX,
                    outerBitmap.intrinsicHeight / scale + controlPositionY
                )
                virtBounds = Rect(
                    controlPositionX,
                    controlPositionY,
                    outerBitmap.intrinsicWidth / scale + controlPositionX,
                    outerBitmap.intrinsicHeight / scale + controlPositionY
                )
                setInnerBounds()
                setOrigBounds(
                    Rect(
                        Rect(
                            controlPositionX,
                            controlPositionY,
                            outerBitmap.intrinsicWidth / scale + controlPositionX,
                            outerBitmap.intrinsicHeight / scale + controlPositionY
                        )
                    )
                )
                previousTouchX = fingerPositionX
                previousTouchY = fingerPositionY
            }
        }
        return true
    }

    private fun setInnerBounds() {
        var x = virtBounds.centerX() + (xAxis * (virtBounds.width() / 2)).toInt()
        var y = virtBounds.centerY() + (yAxis * (virtBounds.height() / 2)).toInt()
        if (x > virtBounds.centerX() + virtBounds.width() / 2) x =
            virtBounds.centerX() + virtBounds.width() / 2
        if (x < virtBounds.centerX() - virtBounds.width() / 2) x =
            virtBounds.centerX() - virtBounds.width() / 2
        if (y > virtBounds.centerY() + virtBounds.height() / 2) y =
            virtBounds.centerY() + virtBounds.height() / 2
        if (y < virtBounds.centerY() - virtBounds.height() / 2) y =
            virtBounds.centerY() - virtBounds.height() / 2
        val width = pressedStateInnerBitmap.bounds.width() / 2
        val height = pressedStateInnerBitmap.bounds.height() / 2
        defaultStateInnerBitmap.setBounds(x - width, y - height, x + width, y + height)
        pressedStateInnerBitmap.bounds = defaultStateInnerBitmap.bounds
    }

    fun setPosition(x: Int, y: Int) {
        controlPositionX = x
        controlPositionY = y
    }

    private val currentStateBitmapDrawable: BitmapDrawable
        get() = if (pressedState) pressedStateInnerBitmap else defaultStateInnerBitmap

    private fun setOrigBounds(bounds: Rect) {
        origBounds = bounds
    }
}

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.display

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.features.settings.model.IntSetting
import org.citra.citra_emu.features.settings.utils.SettingsFile

/**
 * Manages the custom framebuffer layout editor.
 *
 * Handles:
 * - Opening and closing the editor
 * - Saving layout changes
 * - Resetting screen positions
 * - Applying framebuffer updates
 *
 * The visual editing logic is handled by CustomLayoutEditorView.
 */
class CustomLayoutManager(
    private val editorView: CustomLayoutEditorView,
    private val doneButton: View,
    private val cancelButton: View,
    private val resetButton: View,
    private val toolbar: View
) {
    /**
     * Initializes editor controls and connects button actions.
     *
     * Sets up save, cancel, and reset button behavior.
     */
    fun bindControls() {
        setupSaveButton()
        setupCancelButton()
        setupResetButton()
    }

    /**
     * Opens the custom layout editor.
     *
     * Makes the editor and toolbar visible and loads
     * the current framebuffer layout settings.
     */
    fun showEditor() {
        editorView.visibility = View.VISIBLE
        toolbar.visibility = View.VISIBLE
        editorView.post { editorView.loadLayoutSettings() }
    }

    /**
     * Closes the custom layout editor.
     *
     * Hides the editor and toolbar while keeping
     * the current layout settings unchanged.
     */
    fun hideEditor() {
        editorView.visibility = View.GONE
        toolbar.visibility = View.GONE
    }

    /**
     * Configures the save button.
     *
     * Applies the edited screen positions to the emulator layout,
     * saves the layout settings, and closes the editor.
     *
     * The framebuffer is updated automatically through
     * NativeLibrary.setCustomLayout().
     */
    private fun setupSaveButton() {
        doneButton.setOnClickListener {
            editorView.applyLayoutChanges()
            saveLayoutSettings()
            hideEditor()
        }
    }

    /** Configures the cancel button. Discards current changes and closes the editor. */
    private fun setupCancelButton() {
        cancelButton.setOnClickListener { hideEditor() }
    }

    /** Configures the reset button. Restores default screen positions. */
    private fun setupResetButton() {
        resetButton.setOnClickListener { editorView.resetToDefaultLayout() }
    }

    /**
     * Saves the current custom layout values to the configuration file.
     *
     * Saves either portrait or landscape layout settings depending
     * on the current orientation.
     */
    fun saveLayoutSettings() {
        if (NativeLibrary.isPortraitMode()) {
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.PORTRAIT_TOP_X)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.PORTRAIT_TOP_Y)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.PORTRAIT_TOP_WIDTH)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.PORTRAIT_TOP_HEIGHT)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.PORTRAIT_BOTTOM_X)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.PORTRAIT_BOTTOM_Y)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.PORTRAIT_BOTTOM_WIDTH)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.PORTRAIT_BOTTOM_HEIGHT)
        } else {
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.LANDSCAPE_TOP_X)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.LANDSCAPE_TOP_Y)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.LANDSCAPE_TOP_WIDTH)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.LANDSCAPE_TOP_HEIGHT)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.LANDSCAPE_BOTTOM_X)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.LANDSCAPE_BOTTOM_Y)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.LANDSCAPE_BOTTOM_WIDTH)
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, IntSetting.LANDSCAPE_BOTTOM_HEIGHT)
        }
    }
}

/**
 * Custom framebuffer layout editor view.
 *
 * Provides:
 * - Visual top/bottom screen preview
 * - Screen selection
 * - Dragging support
 * - Resize handles
 * - Layout editing canvas
 *
 * Layout changes are applied through NativeLibrary.
 */
class CustomLayoutEditorView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs) {

    // ==================== PAINTS ====================

    /**
     * Transparent preview fill for the top screen.
     * Uses the same blue color as the outline.
     */
    private val topScreenPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(20, 42, 119, 240) // #2A77F0 with low transparency
    }

    /**
     * Transparent preview fill for the bottom screen.
     * Uses the same orange color as the outline.
     */
    private val bottomScreenPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(20, 203, 79, 3) // #CB4F03 with low transparency
    }

    /**
     * Normal outline for the top screen.
     */
    private val topBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(42, 119, 240)
        style = Paint.Style.STROKE
        strokeWidth = 4f
    }

    /**
     * Normal outline for the bottom screen.
     */
    private val bottomBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(203, 79, 3)
        style = Paint.Style.STROKE
        strokeWidth = 4f
    }

    /**
     * Thicker outline used when the top screen is selected.
     */
    private val selectedTopBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(42, 119, 240)
        style = Paint.Style.STROKE
        strokeWidth = 10f
    }

    /**
     * Thicker outline used when the bottom screen is selected.
     */
    private val selectedBottomBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(203, 79, 3)
        style = Paint.Style.STROKE
        strokeWidth = 10f
    }

    /**
     * Resize handles for the top screen.
     */
    private val topHandlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(42, 119, 240)
        style = Paint.Style.FILL
    }

    /**
     * Resize handles for the bottom screen.
     */
    private val bottomHandlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(203, 79, 3)
        style = Paint.Style.FILL
    }

    /**
     * White outline behind resize handles.
     */
    private val handleOutlinePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }

    /**
     * Selected top handle.
     */
    private val selectedTopHandlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(42, 119, 240)
        style = Paint.Style.FILL
    }

/**
 * Selected bottom handle.
 */
private val selectedBottomHandlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
    color = Color.rgb(203, 79, 3)
    style = Paint.Style.FILL
}

    /**
     * Text used to label top and bottom screens.
     */
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 42f
    }

    // ==================== SCREEN RECTANGLES ====================

    /** Top screen rectangle */
    private val topScreenRect = RectF()

    /** Bottom screen rectangle */
    private val bottomScreenRect = RectF()

    // ==================== CONSTANTS ====================

    /** Resize handle display size */
    private val handleSize =36f

    /** Touch area around resize handles */
    private val touchHandleSize = 80f

    /** Snap distance between screens */
    private val snapDistance = 12f

    // ==================== EDITOR STATE ====================

    /** Currently selected screen */
    private var selectedScreen = SelectedScreen.NONE

    /** Current drag operation */
    private var dragMode = DragMode.NONE

    /** Currently active rectangle */
    private var activeRectangle: RectF? = null

    /** Active handle position */
    private var activeHandleX = -1f
    private var activeHandleY = -1f

    // ==================== ENUMS ====================

    /** Represents which screen is selected. */
    private enum class SelectedScreen { NONE, TOP, BOTTOM }

    /** Represents current drag operation. */
    private enum class DragMode {
        NONE,
        MOVE_TOP, MOVE_BOTTOM,
        TOP_TOP_LEFT, TOP_TOP, TOP_TOP_RIGHT, TOP_LEFT, TOP_RIGHT,
        TOP_BOTTOM_LEFT, TOP_BOTTOM, TOP_BOTTOM_RIGHT,
        BOTTOM_TOP_LEFT, BOTTOM_TOP, BOTTOM_TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
        BOTTOM_BOTTOM_LEFT, BOTTOM_BOTTOM, BOTTOM_BOTTOM_RIGHT
    }

    // ==================== VIEW LIFECYCLE ====================

    override fun onSizeChanged(w: Int, h: Int, oldW: Int, oldH: Int) {
        super.onSizeChanged(w, h, oldW, oldH)
        loadLayoutSettings()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        drawScreen(canvas, topScreenRect, topScreenPaint, topBorderPaint, topHandlePaint, selectedScreen == SelectedScreen.TOP, "Top Screen")
        drawScreen(canvas, bottomScreenRect, bottomScreenPaint, bottomBorderPaint, bottomHandlePaint, selectedScreen == SelectedScreen.BOTTOM, "Bottom Screen")
    }

    // ==================== SETTINGS ====================

    /** Loads screen positions from emulator settings. */
    fun loadLayoutSettings() {
        if (NativeLibrary.isPortraitMode()) {
            topScreenRect.set(
                IntSetting.PORTRAIT_TOP_X.int.toFloat(),
                IntSetting.PORTRAIT_TOP_Y.int.toFloat(),
                (IntSetting.PORTRAIT_TOP_X.int + IntSetting.PORTRAIT_TOP_WIDTH.int).toFloat(),
                (IntSetting.PORTRAIT_TOP_Y.int + IntSetting.PORTRAIT_TOP_HEIGHT.int).toFloat()
            )
            bottomScreenRect.set(
                IntSetting.PORTRAIT_BOTTOM_X.int.toFloat(),
                IntSetting.PORTRAIT_BOTTOM_Y.int.toFloat(),
                (IntSetting.PORTRAIT_BOTTOM_X.int + IntSetting.PORTRAIT_BOTTOM_WIDTH.int).toFloat(),
                (IntSetting.PORTRAIT_BOTTOM_Y.int + IntSetting.PORTRAIT_BOTTOM_HEIGHT.int).toFloat()
            )
        } else {
            topScreenRect.set(
                IntSetting.LANDSCAPE_TOP_X.int.toFloat(),
                IntSetting.LANDSCAPE_TOP_Y.int.toFloat(),
                (IntSetting.LANDSCAPE_TOP_X.int + IntSetting.LANDSCAPE_TOP_WIDTH.int).toFloat(),
                (IntSetting.LANDSCAPE_TOP_Y.int + IntSetting.LANDSCAPE_TOP_HEIGHT.int).toFloat()
            )
            bottomScreenRect.set(
                IntSetting.LANDSCAPE_BOTTOM_X.int.toFloat(),
                IntSetting.LANDSCAPE_BOTTOM_Y.int.toFloat(),
                (IntSetting.LANDSCAPE_BOTTOM_X.int + IntSetting.LANDSCAPE_BOTTOM_WIDTH.int).toFloat(),
                (IntSetting.LANDSCAPE_BOTTOM_Y.int + IntSetting.LANDSCAPE_BOTTOM_HEIGHT.int).toFloat()
            )
        }
        invalidate()
    }

    // ==================== TOUCH HANDLING ====================

    override fun onTouchEvent(event: MotionEvent): Boolean {
       if (event.pointerCount == 2) {
            when (event.actionMasked) {
                MotionEvent.ACTION_POINTER_DOWN -> {
                    activeRectangle = findTouchedRectangle(event.getX(0), event.getY(0))
                    if (activeRectangle != null) {
                        initialPinchDistance = getPinchDistance(event)
                        initialPinchWidth = activeRectangle!!.width()
                        initialPinchHeight = activeRectangle!!.height()
                    }
                }
                MotionEvent.ACTION_MOVE -> resizeWithPinch(event)
                MotionEvent.ACTION_POINTER_UP -> activeRectangle = null
            }
            return true
        }

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                activeRectangle = findTouchedRectangle(event.x, event.y)
                dragMode = detectDragMode(event.x, event.y, activeRectangle)
                selectedScreen = when (activeRectangle) {
                    topScreenRect -> SelectedScreen.TOP
                    bottomScreenRect -> SelectedScreen.BOTTOM
                    else -> SelectedScreen.NONE
                }
            }
            MotionEvent.ACTION_MOVE -> handleDrag(event)
            MotionEvent.ACTION_UP -> {
                dragMode = DragMode.NONE
                activeRectangle = null
                clearActiveHandle()
                invalidate()
            }
        }
        return true
    }

    // ==================== SAVING ====================

    /**
     * Applies the current editor positions to emulator settings.
     *
     * Converts the edited screen rectangles into layout values,
     * updates IntSetting values, and sends the new layout
     * to the renderer for live preview.
     */
    fun applyLayoutChanges() {

    if (NativeLibrary.isPortraitMode()) {

            IntSetting.PORTRAIT_TOP_X.int = topScreenRect.left.toInt()
            IntSetting.PORTRAIT_TOP_Y.int = topScreenRect.top.toInt()
            IntSetting.PORTRAIT_TOP_WIDTH.int = topScreenRect.width().toInt()
            IntSetting.PORTRAIT_TOP_HEIGHT.int = topScreenRect.height().toInt()

            IntSetting.PORTRAIT_BOTTOM_X.int = bottomScreenRect.left.toInt()
            IntSetting.PORTRAIT_BOTTOM_Y.int = bottomScreenRect.top.toInt()
            IntSetting.PORTRAIT_BOTTOM_WIDTH.int = bottomScreenRect.width().toInt()
            IntSetting.PORTRAIT_BOTTOM_HEIGHT.int = bottomScreenRect.height().toInt()

        } else {

            IntSetting.LANDSCAPE_TOP_X.int = topScreenRect.left.toInt()
            IntSetting.LANDSCAPE_TOP_Y.int = topScreenRect.top.toInt()
            IntSetting.LANDSCAPE_TOP_WIDTH.int = topScreenRect.width().toInt()
            IntSetting.LANDSCAPE_TOP_HEIGHT.int = topScreenRect.height().toInt()

            IntSetting.LANDSCAPE_BOTTOM_X.int = bottomScreenRect.left.toInt()
            IntSetting.LANDSCAPE_BOTTOM_Y.int = bottomScreenRect.top.toInt()
            IntSetting.LANDSCAPE_BOTTOM_WIDTH.int = bottomScreenRect.width().toInt()
            IntSetting.LANDSCAPE_BOTTOM_HEIGHT.int = bottomScreenRect.height().toInt()
        }

        NativeLibrary.setCustomLayout(
            topScreenRect.left.toInt(),
            topScreenRect.top.toInt(),
            topScreenRect.width().toInt(),
            topScreenRect.height().toInt(),

            bottomScreenRect.left.toInt(),
            bottomScreenRect.top.toInt(),
            bottomScreenRect.width().toInt(),
            bottomScreenRect.height().toInt(),

            NativeLibrary.isPortraitMode()
        )
    }

    /**
     * Restores the default top and bottom screen positions.
     *
     * Updates the editor preview and immediately applies
     * the default layout to the emulator.
     */
    fun resetToDefaultLayout() {
        if (NativeLibrary.isPortraitMode()) {
            IntSetting.PORTRAIT_TOP_X.int = 0
            IntSetting.PORTRAIT_TOP_Y.int = 0
            IntSetting.PORTRAIT_TOP_WIDTH.int = 800
            IntSetting.PORTRAIT_TOP_HEIGHT.int = 480

            IntSetting.PORTRAIT_BOTTOM_X.int = 80
            IntSetting.PORTRAIT_BOTTOM_Y.int = 480
            IntSetting.PORTRAIT_BOTTOM_WIDTH.int = 640
            IntSetting.PORTRAIT_BOTTOM_HEIGHT.int = 480
        } else {
            IntSetting.LANDSCAPE_TOP_X.int = 0
            IntSetting.LANDSCAPE_TOP_Y.int = 0
            IntSetting.LANDSCAPE_TOP_WIDTH.int = 800
            IntSetting.LANDSCAPE_TOP_HEIGHT.int = 480

            IntSetting.LANDSCAPE_BOTTOM_X.int = 80
            IntSetting.LANDSCAPE_BOTTOM_Y.int = 480
            IntSetting.LANDSCAPE_BOTTOM_WIDTH.int = 640
            IntSetting.LANDSCAPE_BOTTOM_HEIGHT.int = 480
        }

        // Update editor rectangles
        loadLayoutSettings()

        // Apply immediately to emulator renderer
        applyLayoutChanges()

        invalidate()
    }

    // ==================== DRAWING ====================

    /**
     * Draws a single screen preview.
     *
     * Top screen: blue tint, blue outline
     * Bottom screen: orange tint, orange outline
     * Selected screens use a thicker outline.
     */
    fun drawScreen(canvas: Canvas, rect: RectF, fillPaint: Paint, borderPaint: Paint, handlePaint: Paint, selected: Boolean, title: String) {
        canvas.drawRect(rect, fillPaint)

        val outlinePaint = when {
            selected && borderPaint == topBorderPaint -> selectedTopBorderPaint
            selected && borderPaint == bottomBorderPaint -> selectedBottomBorderPaint
            else -> borderPaint
        }
        canvas.drawRect(rect, outlinePaint)

        canvas.drawText(title, rect.left + 20f, rect.top + 50f, labelPaint)

        val finalHandlePaint = when {
            selected && borderPaint == topBorderPaint -> selectedTopHandlePaint
            selected && borderPaint == bottomBorderPaint -> selectedBottomHandlePaint
            else -> handlePaint
        }
        drawResizeHandles(canvas, rect, finalHandlePaint)
    }

    /**
     * Draws resize handles around a screen.
     *
     * Handles use the same color as the screen outline.
     */
    fun drawResizeHandles(canvas: Canvas, rect: RectF, handlePaint: Paint) {
        val points = listOf(
            rect.left to rect.top, rect.centerX() to rect.top, rect.right to rect.top,
            rect.left to rect.centerY(), rect.right to rect.centerY(),
            rect.left to rect.bottom, rect.centerX() to rect.bottom, rect.right to rect.bottom
        )
        for ((x, y) in points) {
            canvas.drawCircle(x, y, handleSize, handleOutlinePaint)
            canvas.drawCircle(x, y, handleSize - 3f, handlePaint)
        }
    }

    // ==================== DRAG PROCESSING ====================

    /**
     * Handles screen movement and resizing while editing.
     *
     * Applies the active drag operation and updates
     * the live layout preview.
     */
    private fun handleDrag(event: MotionEvent) {
        when (dragMode) {
            DragMode.MOVE_TOP -> moveScreen(topScreenRect, event)
            DragMode.MOVE_BOTTOM -> moveScreen(bottomScreenRect, event)
            DragMode.TOP_TOP_LEFT -> resizeTopLeft(topScreenRect, event)
            DragMode.TOP_TOP -> resizeTop(topScreenRect, event)
            DragMode.TOP_TOP_RIGHT -> resizeTopRight(topScreenRect, event)
            DragMode.TOP_LEFT -> resizeLeft(topScreenRect, event)
            DragMode.TOP_RIGHT -> resizeRight(topScreenRect, event)
            DragMode.TOP_BOTTOM_LEFT -> resizeBottomLeft(topScreenRect, event)
            DragMode.TOP_BOTTOM -> resizeBottom(topScreenRect, event)
            DragMode.TOP_BOTTOM_RIGHT -> resizeBottomRight(topScreenRect, event)
            DragMode.BOTTOM_TOP_LEFT -> resizeTopLeft(bottomScreenRect, event)
            DragMode.BOTTOM_TOP -> resizeTop(bottomScreenRect, event)
            DragMode.BOTTOM_TOP_RIGHT -> resizeTopRight(bottomScreenRect, event)
            DragMode.BOTTOM_LEFT -> resizeLeft(bottomScreenRect, event)
            DragMode.BOTTOM_RIGHT -> resizeRight(bottomScreenRect, event)
            DragMode.BOTTOM_BOTTOM_LEFT -> resizeBottomLeft(bottomScreenRect, event)
            DragMode.BOTTOM_BOTTOM -> resizeBottom(bottomScreenRect, event)
            DragMode.BOTTOM_BOTTOM_RIGHT -> resizeBottomRight(bottomScreenRect, event)
            else -> Unit
        }
    }

    // ==================== MOVEMENT ====================

    /** Moves a screen rectangle while keeping it inside bounds. */
    private fun moveScreen(rect: RectF, event: MotionEvent) {
        val deltaX = if (event.historySize > 0) event.x - event.getHistoricalX(0) else 0f
        val deltaY = if (event.historySize > 0) event.y - event.getHistoricalY(0) else 0f
        val maxX = width - rect.width()
        val maxY = height - rect.height()
        rect.offsetTo(
            (rect.left + deltaX).coerceIn(0f, maxX),
            (rect.top + deltaY).coerceIn(0f, maxY)
        )
        if (rect == topScreenRect) snapScreens(topScreenRect, bottomScreenRect)
        else snapScreens(bottomScreenRect, topScreenRect)
        applyLayoutChanges()
        invalidate()
    }

    // ==================== RESIZING ====================

    private fun resizeTopLeft(rect: RectF, event: MotionEvent) {
        rect.left = event.x.coerceIn(0f, rect.right - 100f)
        rect.top = event.y.coerceIn(0f, rect.bottom - 100f)
        finishResize(rect)
    }

    private fun resizeTopRight(rect: RectF, event: MotionEvent) {
        rect.right = event.x.coerceIn(rect.left + 100f, width.toFloat())
        rect.top = event.y.coerceIn(0f, rect.bottom - 100f)
        finishResize(rect)
    }

    private fun resizeBottomLeft(rect: RectF, event: MotionEvent) {
        rect.left = event.x.coerceIn(0f, rect.right - 100f)
        rect.bottom = event.y.coerceIn(rect.top + 100f, height.toFloat())
        finishResize(rect)
    }

    private fun resizeBottomRight(rect: RectF, event: MotionEvent) {
        rect.right = event.x.coerceIn(rect.left + 100f, width.toFloat())
        rect.bottom = event.y.coerceIn(rect.top + 100f, height.toFloat())
        finishResize(rect)
    }

    private fun resizeTop(rect: RectF, event: MotionEvent) {
        rect.top = event.y.coerceIn(0f, rect.bottom - 100f)
        finishResize(rect)
    }

    private fun resizeBottom(rect: RectF, event: MotionEvent) {
        rect.bottom = event.y.coerceIn(rect.top + 100f, height.toFloat())
        finishResize(rect)
    }

    private fun resizeLeft(rect: RectF, event: MotionEvent) {
        rect.left = event.x.coerceIn(0f, rect.right - 100f)
        finishResize(rect)
    }

    private fun resizeRight(rect: RectF, event: MotionEvent) {
        rect.right = event.x.coerceIn(rect.left + 100f, width.toFloat())
        finishResize(rect)
    }

    /** Finishes resize operation. */
    private fun finishResize(rect: RectF) {
        clampRectangle(rect)
        snapAfterResize(rect)
        applyLayoutChanges()
        invalidate()
    }
    
    // ==================== PINCH RESIZE ====================

    /**
     * Stores the distance between two fingers when pinch starts.
     */
    private var initialPinchDistance = 0f

    /**
     * Stores the screen size before pinch starts.
     */
    private var initialPinchWidth = 0f
    private var initialPinchHeight = 0f

    /**
     * Calculates the distance between two fingers.
     *
     * Used to detect pinch in/out gestures.
     */
    private fun getPinchDistance(event: MotionEvent): Float {
        val dx = event.getX(0) - event.getX(1)
        val dy = event.getY(0) - event.getY(1)
        return kotlin.math.sqrt(dx * dx + dy * dy)
    }

    /**
     * Resizes the currently touched screen using pinch gesture.
     *
     * Pinching outward enlarges the screen.
     * Pinching inward shrinks the screen.
     *
     * This does not affect drag handles because it only
     * activates when two fingers are detected.
     */
    private fun resizeWithPinch(event: MotionEvent) {
        if (activeRectangle == null) return
        if (initialPinchDistance <= 0f) return

        val currentDistance = getPinchDistance(event)
        val scale = currentDistance / initialPinchDistance

        val newWidth = initialPinchWidth * scale
        val newHeight = initialPinchHeight * scale

        val rect = activeRectangle ?: return

        val centerX = rect.centerX()
        val centerY = rect.centerY()

        rect.set(
            centerX - newWidth / 2f,
            centerY - newHeight / 2f,
            centerX + newWidth / 2f,
            centerY + newHeight / 2f
        )

        clampRectangle(rect)
        applyLayoutChanges()
        invalidate()
    }

    // ==================== TOUCH DETECTION ====================

    /** Finds which screen was touched. */
    private fun findTouchedRectangle(x: Float, y: Float): RectF? {
        if (isOnResizeHandle(bottomScreenRect, x, y)) return bottomScreenRect
        if (isOnResizeHandle(topScreenRect, x, y)) return topScreenRect
        if (bottomScreenRect.contains(x, y)) return bottomScreenRect
        if (topScreenRect.contains(x, y)) return topScreenRect
        return null
    }

    /** Determines the current drag operation. */
    private fun detectDragMode(x: Float, y: Float, rect: RectF?): DragMode {
        if (rect == null) return DragMode.NONE
        return when (rect) {
            topScreenRect -> detectScreenHandle(x, y, true)
            bottomScreenRect -> detectScreenHandle(x, y, false)
            else -> DragMode.NONE
        }
    }

    // ==================== HANDLE DETECTION ====================

    /**
     * Detects which part of the screen was touched.
     * Returns either move or resize operation.
     */
    private fun detectScreenHandle(x: Float, y: Float, isTopScreen: Boolean): DragMode {
        val rect = if (isTopScreen) topScreenRect else bottomScreenRect
        return when {
            isInsideHandle(x, y, rect.left, rect.top) -> {
                setActiveHandle(rect.left, rect.top)
                if (isTopScreen) DragMode.TOP_TOP_LEFT else DragMode.BOTTOM_TOP_LEFT
            }
            isInsideHandle(x, y, rect.centerX(), rect.top) -> {
                setActiveHandle(rect.centerX(), rect.top)
                if (isTopScreen) DragMode.TOP_TOP else DragMode.BOTTOM_TOP
            }
            isInsideHandle(x, y, rect.right, rect.top) -> {
                setActiveHandle(rect.right, rect.top)
                if (isTopScreen) DragMode.TOP_TOP_RIGHT else DragMode.BOTTOM_TOP_RIGHT
            }
            isInsideHandle(x, y, rect.left, rect.centerY()) -> {
                setActiveHandle(rect.left, rect.centerY())
                if (isTopScreen) DragMode.TOP_LEFT else DragMode.BOTTOM_LEFT
            }
            isInsideHandle(x, y, rect.right, rect.centerY()) -> {
                setActiveHandle(rect.right, rect.centerY())
                if (isTopScreen) DragMode.TOP_RIGHT else DragMode.BOTTOM_RIGHT
            }
            isInsideHandle(x, y, rect.left, rect.bottom) -> {
                setActiveHandle(rect.left, rect.bottom)
                if (isTopScreen) DragMode.TOP_BOTTOM_LEFT else DragMode.BOTTOM_BOTTOM_LEFT
            }
            isInsideHandle(x, y, rect.centerX(), rect.bottom) -> {
                setActiveHandle(rect.centerX(), rect.bottom)
                if (isTopScreen) DragMode.TOP_BOTTOM else DragMode.BOTTOM_BOTTOM
            }
            isInsideHandle(x, y, rect.right, rect.bottom) -> {
                setActiveHandle(rect.right, rect.bottom)
                if (isTopScreen) DragMode.TOP_BOTTOM_RIGHT else DragMode.BOTTOM_BOTTOM_RIGHT
            }
            else -> {
                clearActiveHandle()
                if (isTopScreen) DragMode.MOVE_TOP else DragMode.MOVE_BOTTOM
            }
        }
    }

    /** Checks if the user touched a resize handle. */
    private fun isOnResizeHandle(rect: RectF, x: Float, y: Float): Boolean {
        return listOf(
            rect.left to rect.top, rect.centerX() to rect.top, rect.right to rect.top,
            rect.left to rect.centerY(), rect.right to rect.centerY(),
            rect.left to rect.bottom, rect.centerX() to rect.bottom, rect.right to rect.bottom
        ).any { (hx, hy) -> isInsideHandle(x, y, hx, hy) }
    }

    /** Checks if a coordinate is inside a resize handle area. */
    private fun isInsideHandle(x: Float, y: Float, handleX: Float, handleY: Float): Boolean {
        return x >= handleX - touchHandleSize && x <= handleX + touchHandleSize &&
                y >= handleY - touchHandleSize && y <= handleY + touchHandleSize
    }

    /** Sets the currently active resize handle. */
    private fun setActiveHandle(x: Float, y: Float) {
        activeHandleX = x; activeHandleY = y
    }

    /** Clears active resize handle. */
    private fun clearActiveHandle() {
        activeHandleX = -1f; activeHandleY = -1f
    }

    /** Checks if a handle is selected. */
    private fun isActiveHandle(x: Float, y: Float): Boolean {
        return kotlin.math.abs(x - activeHandleX) <= 2f && kotlin.math.abs(y - activeHandleY) <= 2f
    }

    // ==================== SNAP SYSTEM ====================

    /** Snaps a moving screen to another screen.  */
    fun snapScreens(moving: RectF, target: RectF) {
        if (kotlin.math.abs(moving.left - target.left) < snapDistance) moving.offset(target.left - moving.left, 0f)
        if (kotlin.math.abs(moving.right - target.right) < snapDistance) moving.offset(target.right - moving.right, 0f)
        if (kotlin.math.abs(moving.top - target.top) < snapDistance) moving.offset(0f, target.top - moving.top)
        if (kotlin.math.abs(moving.bottom - target.bottom) < snapDistance) moving.offset(0f, target.bottom - moving.bottom)
    }

    /** Snaps a resized screen. */
    private fun snapAfterResize(rect: RectF) {
        val target = if (rect == topScreenRect) bottomScreenRect else topScreenRect
        snapScreens(rect, target)
    }

    // ==================== UTILITY ====================

    /** Keeps a rectangle inside the editor bounds. */
    private fun clampRectangle(rect: RectF) {
        rect.left = rect.left.coerceIn(0f, width.toFloat())
        rect.top = rect.top.coerceIn(0f, height.toFloat())
        rect.right = rect.right.coerceIn(0f, width.toFloat())
        rect.bottom = rect.bottom.coerceIn(0f, height.toFloat())
    }
}

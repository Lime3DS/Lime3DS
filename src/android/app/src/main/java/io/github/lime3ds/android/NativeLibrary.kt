// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android

import android.Manifest.permission
import android.app.Dialog
import android.content.DialogInterface
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.net.Uri
import android.os.Bundle
import android.text.Html
import android.text.method.LinkMovementMethod
import android.view.Surface
import android.view.View
import android.widget.TextView
import androidx.annotation.Keep
import androidx.core.content.ContextCompat
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import io.github.lime3ds.android.activities.EmulationActivity
import io.github.lime3ds.android.utils.EmulationMenuSettings
import io.github.lime3ds.android.utils.FileUtil
import io.github.lime3ds.android.utils.Log
import java.lang.ref.WeakReference
import java.util.Date

/**
 * Class which contains methods that interact
 * with the native side of the Citra code.
 */
object NativeLibrary {
    /**
     * Default touchscreen device
     */
    const val TouchScreenDevice = "Touchscreen"

    @JvmField
    var sEmulationActivity = WeakReference<EmulationActivity?>(null)
    private var alertResult = false
    val alertLock = Object()

    init {
        try {
            System.loadLibrary("lime-android")
        } catch (ex: UnsatisfiedLinkError) {
            Log.error("[NativeLibrary] $ex")
        }
    }

    /**
     * Handles button press events for a gamepad.
     *
     * @param device The input descriptor of the gamepad.
     * @param button Key code identifying which button was pressed.
     * @param action Mask identifying which action is happening (button pressed down, or button released).
     * @return If we handled the button press.
     */
    external fun onGamePadEvent(device: String, button: Int, action: Int): Boolean

    /**
     * Handles gamepad movement events.
     *
     * @param device The device ID of the gamepad.
     * @param axis   The axis ID
     * @param xAxis The value of the x-axis represented by the given ID.
     * @param yAxis The value of the y-axis represented by the given ID
     */
    external fun onGamePadMoveEvent(device: String, axis: Int, xAxis: Float, yAxis: Float): Boolean

    /**
     * Handles gamepad movement events.
     *
     * @param device   The device ID of the gamepad.
     * @param axisId  The axis ID
     * @param axisVal The value of the axis represented by the given ID.
     */
    external fun onGamePadAxisEvent(device: String?, axisId: Int, axisVal: Float): Boolean

    /**
     * Handles touch events.
     *
     * @param xAxis  The value of the x-axis.
     * @param yAxis  The value of the y-axis
     * @param pressed To identify if the touch held down or released.
     * @return true if the pointer is within the touchscreen
     */
    external fun onTouchEvent(xAxis: Float, yAxis: Float, pressed: Boolean): Boolean

    /**
     * Handles touch movement.
     *
     * @param xAxis The value of the instantaneous x-axis.
     * @param yAxis The value of the instantaneous y-axis.
     */
    external fun onTouchMoved(xAxis: Float, yAxis: Float)

    external fun reloadSettings()

    external fun getTitleId(filename: String): Long

    external fun getIsSystemTitle(path: String): Boolean

    /**
     * Sets the current working user directory
     * If not set, it auto-detects a location
     */
    external fun setUserDirectory(directory: String)
    external fun getInstalledGamePaths(): Array<String?>

    // Create the config.ini file.
    external fun createConfigFile()
    external fun createLogFile()
    external fun logUserDirectory(directory: String)

    /**
     * Begins emulation.
     */
    external fun run(path: String)

    // Surface Handling
    external fun surfaceChanged(surf: Surface)
    external fun surfaceDestroyed()
    external fun doFrame()

    /**
     * Unpauses emulation from a paused state.
     */
    external fun unPauseEmulation()

    /**
     * Pauses emulation.
     */
    external fun pauseEmulation()

    /**
     * Stops emulation.
     */
    external fun stopEmulation()

    /**
     * Returns true if emulation is running (or is paused).
     */
    external fun isRunning(): Boolean

    /**
     * Returns the title ID of the currently running title, or 0 on failure.
     */
    external fun getRunningTitleId(): Long

    /**
     * Returns the performance stats for the current game
     */
    external fun getPerfStats(): DoubleArray

    /**
     * Notifies the core emulation that the layout should be updated
     */
    external fun updateFramebuffer(isPortrait: Boolean)

    /**
     * Swaps the top and bottom screens.
     */
    external fun swapScreens(swapScreens: Boolean, rotation: Int)

    external fun initializeGpuDriver(
        hookLibDir: String?,
        customDriverDir: String?,
        customDriverName: String?,
        fileRedirectDir: String?
    )

    external fun enableAdrenoTurboMode(enable: Boolean)

    external fun areKeysAvailable(): Boolean

    external fun getHomeMenuPath(region: Int): String

    external fun getSystemTitleIds(systemType: Int, region: Int): LongArray

    external fun downloadTitleFromNus(title: Long): InstallStatus

    private var coreErrorAlertResult = false
    private val coreErrorAlertLock = Object()

    private fun onCoreErrorImpl(title: String, message: String, canContinue: Boolean) {
        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity == null) {
            Log.error("[NativeLibrary] EmulationActivity not present")
            return
        }
        val fragment = CoreErrorDialogFragment.newInstance(title, message, canContinue)
        fragment.show(emulationActivity.supportFragmentManager, CoreErrorDialogFragment.TAG)
    }

    /**
     * Handles a core error.
     * @return true: continue; false: abort
     */
    @Keep
    @JvmStatic
    fun onCoreError(error: CoreError?, details: String): Boolean {
        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity == null) {
            Log.error("[NativeLibrary] EmulationActivity not present")
            return false
        }
        val title: String
        val message: String
        val canContinue: Boolean
        when (error) {
            CoreError.ErrorSystemFiles -> {
                title = emulationActivity.getString(R.string.system_archive_not_found)
                message = emulationActivity.getString(
                    R.string.system_archive_not_found_message,
                    details.ifEmpty { emulationActivity.getString(R.string.system_archive_general) }
                )
                canContinue = true
            }

            CoreError.ErrorSavestate -> {
                title = emulationActivity.getString(R.string.save_load_error)
                message = details
                canContinue = true
            }

            CoreError.ErrorArticDisconnected -> {
                title = emulationActivity.getString(R.string.artic_base)
                message = emulationActivity.getString(R.string.artic_server_comm_error)
                canContinue = false
            }

            CoreError.ErrorUnknown -> {
                title = emulationActivity.getString(R.string.fatal_error)
                message = emulationActivity.getString(R.string.fatal_error_message)
                canContinue = true
            }

            else -> {
                return true
            }
        }

        // Show the AlertDialog on the main thread.
        emulationActivity.runOnUiThread(Runnable { onCoreErrorImpl(title, message, canContinue) })

        // Wait for the lock to notify that it is complete.
        synchronized(coreErrorAlertLock) {
            try {
                coreErrorAlertLock.wait()
            } catch (ignored: Exception) {
            }
        }
        return coreErrorAlertResult
    }

    @get:Keep
    @get:JvmStatic
    val isPortraitMode: Boolean
        get() = LimeApplication.appContext.resources.configuration.orientation ==
                Configuration.ORIENTATION_PORTRAIT

    @Keep
    @JvmStatic
    fun displayAlertMsg(title: String, message: String, yesNo: Boolean): Boolean {
        Log.error("[NativeLibrary] Alert: $message")
        val emulationActivity = sEmulationActivity.get()
        var result = false
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null, can't do panic alert.")
        } else {
            // Show the AlertDialog on the main thread.
            emulationActivity.runOnUiThread {
                AlertMessageDialogFragment.newInstance(title, message, yesNo).showNow(
                    emulationActivity.supportFragmentManager,
                    AlertMessageDialogFragment.TAG
                )
            }

            // Wait for the lock to notify that it is complete.
            synchronized(alertLock) {
                try {
                    alertLock.wait()
                } catch (_: Exception) {
                }
            }
            if (yesNo) result = alertResult
        }
        return result
    }

    class AlertMessageDialogFragment : DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            // Create object used for waiting.
            val builder = MaterialAlertDialogBuilder(requireContext())
                .setTitle(requireArguments().getString(TITLE))
                .setMessage(requireArguments().getString(MESSAGE))

            // If not yes/no dialog just have one button that dismisses modal,
            // otherwise have a yes and no button that sets alertResult accordingly.
            if (!requireArguments().getBoolean(YES_NO)) {
                builder
                    .setCancelable(false)
                    .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                        synchronized(alertLock) { alertLock.notify() }
                    }
            } else {
                alertResult = false
                builder
                    .setPositiveButton(android.R.string.yes) { _: DialogInterface, _: Int ->
                        alertResult = true
                        synchronized(alertLock) { alertLock.notify() }
                    }
                    .setNegativeButton(android.R.string.no) { _: DialogInterface, _: Int ->
                        alertResult = false
                        synchronized(alertLock) { alertLock.notify() }
                    }
            }

            return builder.show()
        }

        companion object {
            const val TAG = "AlertMessageDialogFragment"

            const val TITLE = "title"
            const val MESSAGE = "message"
            const val YES_NO = "yesNo"

            fun newInstance(
                title: String,
                message: String,
                yesNo: Boolean
            ): AlertMessageDialogFragment {
                val args = Bundle()
                args.putString(TITLE, title)
                args.putString(MESSAGE, message)
                args.putBoolean(YES_NO, yesNo)
                val fragment = AlertMessageDialogFragment()
                fragment.arguments = args
                return fragment
            }
        }
    }

    @Keep
    @JvmStatic
    fun exitEmulationActivity(resultCode: Int) {
        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null, can't exit.")
            return
        }

        if (resultCode == EmulationErrorDialogFragment.ShutdownRequested) {
            emulationActivity.finish()
            return
        }

        emulationActivity.runOnUiThread {
            EmulationErrorDialogFragment.newInstance(resultCode).showNow(
                emulationActivity.supportFragmentManager,
                EmulationErrorDialogFragment.TAG
            )
        }
    }

    class EmulationErrorDialogFragment : DialogFragment() {
        private lateinit var emulationActivity: EmulationActivity

        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            emulationActivity = requireActivity() as EmulationActivity

            var captionId = R.string.loader_error_invalid_format
            val result = requireArguments().getInt(RESULT_CODE)
            if (result == ErrorLoader_ErrorEncrypted) {
                captionId = R.string.loader_error_encrypted
            }
            if (result == ErrorArticDisconnected) {
                captionId = R.string.artic_base
            }

            val alert = MaterialAlertDialogBuilder(requireContext())
                .setTitle(captionId)
                .setMessage(
                    Html.fromHtml(
                        if (result == ErrorArticDisconnected)
                            LimeApplication.appContext.resources.getString(R.string.artic_server_comm_error)
                        else
                            LimeApplication.appContext.resources.getString(R.string.redump_games),
                    Html.FROM_HTML_MODE_LEGACY
                    )
                )
                .setPositiveButton(android.R.string.ok) { _: DialogInterface?, _: Int ->
                    emulationActivity.finish()
                }
                .create()
            alert.show()

            val alertMessage = alert.findViewById<View>(android.R.id.message) as TextView
            alertMessage.movementMethod = LinkMovementMethod.getInstance()

            isCancelable = false
            return alert
        }

        companion object {
            const val TAG = "EmulationErrorDialogFragment"

            const val RESULT_CODE = "resultcode"

            const val Success = 0
            const val ErrorNotInitialized = 1
            const val ErrorGetLoader = 2
            const val ErrorSystemMode = 3
            const val ErrorLoader = 4
            const val ErrorLoader_ErrorEncrypted = 5
            const val ErrorLoader_ErrorInvalidFormat = 6
            const val ErrorLoader_ErrorGBATitle = 7
            const val ErrorSystemFiles = 8
            const val ErrorSavestate = 9
            const val ErrorArticDisconnected = 10
            const val ShutdownRequested = 11
            const val ErrorUnknown = 12

            fun newInstance(resultCode: Int): EmulationErrorDialogFragment {
                val args = Bundle()
                args.putInt(RESULT_CODE, resultCode)
                val fragment = EmulationErrorDialogFragment()
                fragment.arguments = args
                return fragment
            }
        }
    }

    fun setEmulationActivity(emulationActivity: EmulationActivity?) {
        Log.debug("[NativeLibrary] Registering EmulationActivity.")
        sEmulationActivity = WeakReference(emulationActivity)
    }

    fun clearEmulationActivity() {
        Log.debug("[NativeLibrary] Unregistering EmulationActivity.")
        sEmulationActivity.clear()
    }

    private val cameraPermissionLock = Object()
    private var cameraPermissionGranted = false
    const val REQUEST_CODE_NATIVE_CAMERA = 800

    @Keep
    @JvmStatic
    fun requestCameraPermission(): Boolean {
        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity == null) {
            Log.error("[NativeLibrary] EmulationActivity not present")
            return false
        }
        if (ContextCompat.checkSelfPermission(emulationActivity, permission.CAMERA) ==
            PackageManager.PERMISSION_GRANTED
        ) {
            // Permission already granted
            return true
        }
        emulationActivity.requestPermissions(arrayOf(permission.CAMERA), REQUEST_CODE_NATIVE_CAMERA)

        // Wait until result is returned
        synchronized(cameraPermissionLock) {
            try {
                cameraPermissionLock.wait()
            } catch (ignored: InterruptedException) {
            }
        }
        return cameraPermissionGranted
    }

    fun cameraPermissionResult(granted: Boolean) {
        cameraPermissionGranted = granted
        synchronized(cameraPermissionLock) { cameraPermissionLock.notify() }
    }

    private val micPermissionLock = Object()
    private var micPermissionGranted = false
    const val REQUEST_CODE_NATIVE_MIC = 900

    @Keep
    @JvmStatic
    fun requestMicPermission(): Boolean {
        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity == null) {
            Log.error("[NativeLibrary] EmulationActivity not present")
            return false
        }
        if (ContextCompat.checkSelfPermission(emulationActivity, permission.RECORD_AUDIO) ==
            PackageManager.PERMISSION_GRANTED
        ) {
            // Permission already granted
            return true
        }
        emulationActivity.requestPermissions(
            arrayOf(permission.RECORD_AUDIO),
            REQUEST_CODE_NATIVE_MIC
        )

        // Wait until result is returned
        synchronized(micPermissionLock) {
            try {
                micPermissionLock.wait()
            } catch (ignored: InterruptedException) {
            }
        }
        return micPermissionGranted
    }

    fun micPermissionResult(granted: Boolean) {
        micPermissionGranted = granted
        synchronized(micPermissionLock) { micPermissionLock.notify() }
    }

    // Notifies that the activity is now in foreground and camera devices can now be reloaded
    external fun reloadCameraDevices()

    external fun loadAmiibo(path: String?): Boolean

    external fun removeAmiibo()

    const val SAVESTATE_SLOT_COUNT = 10

    external fun getSavestateInfo(): Array<SaveStateInfo>?

    external fun saveState(slot: Int)

    external fun loadState(slot: Int)

    /**
     * Logs the Lime3DS version, Android version and, CPU.
     */
    external fun logDeviceInfo()

    @Keep
    @JvmStatic
    fun createFile(directory: String, filename: String): Boolean =
        if (FileUtil.isNativePath(directory)) {
            LimeApplication.documentsTree.createFile(directory, filename)
        } else {
            FileUtil.createFile(directory, filename) != null
        }

    @Keep
    @JvmStatic
    fun createDir(directory: String, directoryName: String): Boolean =
        if (FileUtil.isNativePath(directory)) {
            LimeApplication.documentsTree.createDir(directory, directoryName)
        } else {
            FileUtil.createDir(directory, directoryName) != null
        }

    @Keep
    @JvmStatic
    fun openContentUri(path: String, openMode: String): Int =
        if (FileUtil.isNativePath(path)) {
            LimeApplication.documentsTree.openContentUri(path, openMode)
        } else {
            FileUtil.openContentUri(path, openMode)
        }

    @Keep
    @JvmStatic
    fun getFilesName(path: String): Array<String?> =
        if (FileUtil.isNativePath(path)) {
            LimeApplication.documentsTree.getFilesName(path)
        } else {
            FileUtil.getFilesName(path)
        }

    @Keep
    @JvmStatic
    fun getSize(path: String): Long =
        if (FileUtil.isNativePath(path)) {
            LimeApplication.documentsTree.getFileSize(path)
        } else {
            FileUtil.getFileSize(path)
        }

    @Keep
    @JvmStatic
    fun fileExists(path: String): Boolean =
        if (FileUtil.isNativePath(path)) {
            LimeApplication.documentsTree.exists(path)
        } else {
            FileUtil.exists(path)
        }

    @Keep
    @JvmStatic
    fun isDirectory(path: String): Boolean =
        if (FileUtil.isNativePath(path)) {
            LimeApplication.documentsTree.isDirectory(path)
        } else {
            FileUtil.isDirectory(path)
        }

    @Keep
    @JvmStatic
    fun copyFile(
        sourcePath: String,
        destinationParentPath: String,
        destinationFilename: String
    ): Boolean =
        if (FileUtil.isNativePath(sourcePath) &&
            FileUtil.isNativePath(destinationParentPath)
        ) {
            LimeApplication.documentsTree
                .copyFile(sourcePath, destinationParentPath, destinationFilename)
        } else {
            FileUtil.copyFile(
                Uri.parse(sourcePath),
                Uri.parse(destinationParentPath),
                destinationFilename
            )
        }

    @Keep
    @JvmStatic
    fun renameFile(path: String, destinationFilename: String): Boolean =
        if (FileUtil.isNativePath(path)) {
            LimeApplication.documentsTree.renameFile(path, destinationFilename)
        } else {
            FileUtil.renameFile(path, destinationFilename)
        }

    @Keep
    @JvmStatic
    fun deleteDocument(path: String): Boolean =
        if (FileUtil.isNativePath(path)) {
            LimeApplication.documentsTree.deleteDocument(path)
        } else {
            FileUtil.deleteDocument(path)
        }

    enum class CoreError {
        ErrorSystemFiles,
        ErrorSavestate,
        ErrorArticDisconnected,
        ErrorUnknown
    }

    enum class InstallStatus {
        Success,
        ErrorFailedToOpenFile,
        ErrorFileNotFound,
        ErrorAborted,
        ErrorInvalid,
        ErrorEncrypted,
        Cancelled
    }

    class CoreErrorDialogFragment : DialogFragment() {
        private var userChosen = false
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val title = requireArguments().getString(TITLE)
            val message = requireArguments().getString(MESSAGE)
            val canContinue = requireArguments().getBoolean(CAN_CONTINUE)
            val dialog = MaterialAlertDialogBuilder(requireContext())
                .setTitle(title)
                .setMessage(message)
            if (canContinue) {
                dialog.setPositiveButton(R.string.continue_button) { _: DialogInterface?, _: Int ->
                    coreErrorAlertResult = true
                    userChosen = true
                }
            }
            dialog.setNegativeButton(R.string.abort_button) { _: DialogInterface?, _: Int ->
                coreErrorAlertResult = false
                userChosen = true
            }
            return dialog.show()
        }

        override fun onDismiss(dialog: DialogInterface) {
            super.onDismiss(dialog)
            val canContinue = requireArguments().getBoolean(CAN_CONTINUE)
            if (!userChosen) {
                coreErrorAlertResult = canContinue
            }
            synchronized(coreErrorAlertLock) { coreErrorAlertLock.notify() }
        }

        companion object {
            const val TAG = "CoreErrorDialogFragment"

            const val TITLE = "title"
            const val MESSAGE = "message"
            const val CAN_CONTINUE = "canContinue"

            fun newInstance(title: String, message: String, canContinue: Boolean): CoreErrorDialogFragment {
                val frag = CoreErrorDialogFragment()
                val args = Bundle()
                args.putString(TITLE, title)
                args.putString(MESSAGE, message)
                args.putBoolean(CAN_CONTINUE, canContinue)
                frag.arguments = args
                return frag
            }
        }
    }

    @Keep
    class SaveStateInfo {
        var slot = 0
        var time: Date? = null
    }

    /**
     * Button type for use in onTouchEvent
     */
    object ButtonType {
        const val BUTTON_A = 700
        const val BUTTON_B = 701
        const val BUTTON_X = 702
        const val BUTTON_Y = 703
        const val BUTTON_START = 704
        const val BUTTON_SELECT = 705
        const val BUTTON_HOME = 706
        const val BUTTON_ZL = 707
        const val BUTTON_ZR = 708
        const val DPAD_UP = 709
        const val DPAD_DOWN = 710
        const val DPAD_LEFT = 711
        const val DPAD_RIGHT = 712
        const val STICK_LEFT = 713
        const val STICK_LEFT_UP = 714
        const val STICK_LEFT_DOWN = 715
        const val STICK_LEFT_LEFT = 716
        const val STICK_LEFT_RIGHT = 717
        const val STICK_C = 718
        const val STICK_C_UP = 719
        const val STICK_C_DOWN = 720
        const val STICK_C_LEFT = 771
        const val STICK_C_RIGHT = 772
        const val TRIGGER_L = 773
        const val TRIGGER_R = 774
        const val DPAD = 780
        const val BUTTON_DEBUG = 781
        const val BUTTON_GPIO14 = 782
        const val BUTTON_SWAP = 800
    }

    /**
     * Button states
     */
    object ButtonState {
        const val RELEASED = 0
        const val PRESSED = 1
    }
}

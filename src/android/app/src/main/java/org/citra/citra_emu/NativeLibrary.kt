// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu

import android.Manifest.permission
import android.app.Dialog
import android.content.DialogInterface
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.provider.OpenableColumns
import android.text.Html
import android.text.method.LinkMovementMethod
import android.view.Surface
import android.view.View
import android.widget.TextView
import androidx.annotation.Keep
import androidx.annotation.StringRes
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import androidx.fragment.app.DialogFragment
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import java.lang.ref.WeakReference
import java.util.Date
import org.citra.citra_emu.activities.EmulationActivity
import org.citra.citra_emu.model.Game
import org.citra.citra_emu.utils.BuildUtil
import org.citra.citra_emu.utils.FileUtil
import org.citra.citra_emu.utils.GraphicsUtil
import org.citra.citra_emu.utils.Log
import org.citra.citra_emu.utils.NetPlayManager
import org.citra.citra_emu.utils.RemovableStorageHelper
import org.citra.citra_emu.viewmodel.CompressProgressDialogViewModel

/**
 * Class which contains methods that interact
 * with the native side of the Citra code.
 */
object NativeLibrary {
    /**
     * Default touchscreen device
     */
    const val TOUCHSCREEN_DEVICE = "Touchscreen"

    @JvmField
    var sEmulationActivity = WeakReference<EmulationActivity?>(null)
    private var alertResult = false
    val alertLock = Object()

    init {
        try {
            System.loadLibrary("citra-android")
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

    /**
     * Handles touch events on the secondary display.
     *
     * @param xAxis  The value of the x-axis.
     * @param yAxis  The value of the y-axis.
     * @param pressed To identify if the touch held down or released.
     * @return true if the pointer is within the touchscreen
     */
    external fun onSecondaryTouchEvent(xAxis: Float, yAxis: Float, pressed: Boolean): Boolean

    /**
     * Handles touch movement on the secondary display.
     *
     * @param xAxis The value of the instantaneous x-axis.
     * @param yAxis The value of the instantaneous y-axis.
     */
    external fun onSecondaryTouchMoved(xAxis: Float, yAxis: Float)

    external fun reloadSettings()

    external fun getTitleId(filename: String): Long

    external fun getIsSystemTitle(path: String): Boolean

    /**
     * Sets the current working user directory
     * If not set, it auto-detects a location
     */
    external fun setUserDirectory(directory: String)

    data class InstalledGame(val path: String, val mediaType: Game.MediaType)

    fun getInstalledGamePaths(): Array<InstalledGame> {
        val games = getInstalledGamePathsImpl()

        return games.mapNotNull { entry ->
            entry?.let {
                val sep = it.lastIndexOf('|')
                if (sep == -1) return@mapNotNull null

                val path = it.substring(0, sep)
                val mediaType = Game.MediaType.fromInt(it.substring(sep + 1).toInt())

                InstalledGame(path, mediaType!!)
            }
        }.toTypedArray()
    }

    private external fun getInstalledGamePathsImpl(): Array<String?>

    // Create the config.ini file.
    external fun createConfigFile()
    external fun createLogFile()
    external fun logUserDirectory(directory: String)

    /**
     * Set the inserted cartridge that will appear
     * in the home menu. Empty string to clear.
     */
    external fun setInsertedCartridge(path: String)

    /**
     * Begins emulation.
     */
    external fun run(path: String)

    // Surface Handling
    external fun surfaceChanged(surf: Surface)
    external fun surfaceDestroyed()
    external fun doFrame()

    // Second window
    external fun secondarySurfaceChanged(secondary_surface: Surface)
    external fun secondarySurfaceDestroyed()

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

    external fun areKeysAvailable(): Boolean

    external fun getHomeMenuPath(region: Int): String

    external fun getSystemTitleIds(systemType: Int, region: Int): LongArray

    external fun areSystemTitlesInstalled(): BooleanArray

    external fun uninstallSystemFiles(old3DS: Boolean)

    external fun isFullConsoleLinked(): Boolean

    external fun unlinkConsole()

    external fun setTemporaryFrameLimit(speed: Double)

    external fun disableTemporaryFrameLimit()

    external fun playTimeManagerInit()
    external fun playTimeManagerStart(titleId: Long)
    external fun playTimeManagerStop()
    external fun playTimeManagerGetPlayTime(titleId: Long): Long
    external fun playTimeManagerGetCurrentTitleId(): Long

    private external fun uninstallTitle(titleId: Long, mediaType: Int): Boolean
    fun uninstallTitle(titleId: Long, mediaType: Game.MediaType): Boolean =
        uninstallTitle(titleId, mediaType.value)

    external fun nativeFileExists(path: String): Boolean

    external fun deleteOpenGLShaderCache(titleId: Long)
    external fun deleteVulkanShaderCache(titleId: Long)

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

            CoreError.ErrorN3DSApplication -> {
                title = emulationActivity.getString(R.string.invalid_system_mode)
                message = emulationActivity.getString(R.string.invalid_system_mode_message)
                canContinue = false
            }

            CoreError.ErrorCoreExceptionRaised -> {
                title = emulationActivity.getString(R.string.fatal_error)
                message = emulationActivity.getString(R.string.fatal_error_message)
                canContinue = false
            }

            CoreError.ErrorSavestateBuildMismatch -> {
                title = emulationActivity.getString(R.string.core_error_savestate_build_mismatch)
                message =
                    emulationActivity.getString(R.string.savestate_build_mismatch_message, details)
                canContinue = true
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

    @Keep
    @JvmStatic
    fun isPortraitMode(): Boolean = (
        CitraApplication.appContext.resources.configuration.orientation ==
            Configuration.ORIENTATION_PORTRAIT
        )

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

        if (resultCode == CoreError.ShutdownRequested.value) {
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

            val coreError = CoreError.fromInt(requireArguments().getInt(RESULT_CODE))
            val title: String
            val message: String
            when (coreError) {
                CoreError.ErrorGetLoader,
                CoreError.ErrorLoaderErrorInvalidFormat,
                CoreError.ErrorSystemMode -> {
                    title = getString(R.string.loader_error_invalid_format)
                    message = getString(R.string.loader_error_invalid_format_description)
                }

                CoreError.ErrorLoaderErrorEncrypted -> {
                    title = getString(R.string.loader_error_encrypted)
                    message = getString(R.string.loader_error_encrypted_description)
                }

                CoreError.ErrorArticDisconnected -> {
                    title = getString(R.string.artic_base)
                    message = getString(R.string.artic_server_comm_error)
                }

                CoreError.ErrorN3DSApplication -> {
                    title = getString(R.string.loader_error_invalid_system_mode)
                    message = getString(R.string.loader_error_invalid_system_mode_description)
                }

                CoreError.ErrorLoaderErrorPatches -> {
                    title = getString(R.string.loader_error_applying_patches)
                    message = getString(R.string.loader_error_applying_patches_description)
                }

                CoreError.ErrorLoaderErrorPatchesInvalidTitle -> {
                    title = getString(R.string.loader_error_applying_patches)
                    message = getString(R.string.loader_error_patch_wrong_application)
                }

                else -> {
                    title = getString(R.string.loader_error_generic_title)
                    message = getString(
                        R.string.loader_error_generic,
                        getString(coreError.stringRes),
                        coreError.value
                    )
                }
            }

            val alert = MaterialAlertDialogBuilder(requireContext())
                .setTitle(title)
                .setMessage(
                    Html.fromHtml(
                        message,
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

    const val SAVESTATE_SLOT_COUNT = 11
    const val QUICKSAVE_SLOT = 0

    external fun getSavestateInfo(): Array<SaveStateInfo>?

    external fun saveState(slot: Int)

    fun loadStateIfAvailable(slot: Int): Boolean {
        var available = false
        getSavestateInfo()?.forEach {
            if (it.slot == slot) {
                available = true
                return@forEach
            }
        }
        if (available) {
            loadState(slot)
            return true
        }
        return false
    }

    external fun loadState(slot: Int)

    /**
     * Logs the Citra version, Android version and, CPU.
     */
    external fun logDeviceInfo()

    enum class CompressStatus(val value: Int) {
        SUCCESS(0),
        COMPRESS_UNSUPPORTED(1),
        COMPRESS_ALREADY_COMPRESSED(2),
        COMPRESS_FAILED(3),
        DECOMPRESS_UNSUPPORTED(4),
        DECOMPRESS_NOT_COMPRESSED(5),
        DECOMPRESS_FAILED(6),
        INSTALLED_APPLICATION(7);

        companion object {
            fun fromValue(value: Int): CompressStatus =
                CompressStatus.entries.first { it.value == value }
        }
    }

    // Compression / Decompression
    private external fun compressFileNative(inputPath: String?, outputPath: String): Int

    fun compressFile(inputPath: String?, outputPath: String): CompressStatus =
        CompressStatus.fromValue(
            compressFileNative(inputPath, outputPath)
        )

    private external fun decompressFileNative(inputPath: String?, outputPath: String): Int

    fun decompressFile(inputPath: String?, outputPath: String): CompressStatus =
        CompressStatus.fromValue(
            decompressFileNative(inputPath, outputPath)
        )

    external fun getRecommendedExtension(inputPath: String?, shouldCompress: Boolean): String

    @Keep
    @JvmStatic
    fun addNetPlayMessage(type: Int, message: String) {
        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity != null) {
            emulationActivity.addNetPlayMessages(type, message)
        } else {
            NetPlayManager.addNetPlayMessage(type, message)
        }
    }

    @Keep
    @JvmStatic
    fun clearChat() {
        NetPlayManager.clearChat()
    }

    external fun initMultiplayer()

    @Keep
    @JvmStatic
    fun onCompressProgress(total: Long, current: Long) {
        CompressProgressDialogViewModel.update(total, current)
    }

    @Keep
    @JvmStatic
    fun createFile(directory: String, filename: String): Boolean =
        if (FileUtil.isNativePath(directory)) {
            CitraApplication.documentsTree.createFile(directory, filename)
        } else {
            FileUtil.createFile(directory, filename) != null
        }

    @Keep
    @JvmStatic
    fun createDir(directory: String, directoryName: String): Boolean =
        if (FileUtil.isNativePath(directory)) {
            try {
                CitraApplication.documentsTree.createDir(directory, directoryName)
            } catch (e: Exception) {
                false
            }
        } else {
            FileUtil.createDir(directory, directoryName) != null
        }

    @Keep
    @JvmStatic
    fun openContentUri(path: String, openMode: String): Int = if (FileUtil.isNativePath(path)) {
        CitraApplication.documentsTree.openContentUri(path, openMode)
    } else {
        FileUtil.openContentUri(path, openMode)
    }

    @Keep
    @JvmStatic
    fun getFilesName(path: String): Array<String?> = if (FileUtil.isNativePath(path)) {
        CitraApplication.documentsTree.getFilesName(path)
    } else {
        FileUtil.getFilesName(path)
    }

    @Keep
    @JvmStatic
    fun getNativePath(uri: Uri): String {
        BuildUtil.assertNotGooglePlay()

        val context = CitraApplication.appContext
        val dirSep = "/"

        val uriString = uri.toString()
        if (!uriString.contains(":")) { // These raw URIs happen when generating the game list. Why?
            return uriString
        }

        if (uri.scheme == "file") {
            return uri.path!!
        }

        val pathSegment = uri.lastPathSegment ?: return ""
        val virtualPath = pathSegment.substringAfter(":")

        if (pathSegment.startsWith("primary:")) { // Path is located in primary storage
            val primaryStoragePath = Environment.getExternalStorageDirectory().absolutePath
            android.util.Log.v(
                "NativeLibrary",
                "Successfully resolved URI of type PRIMARY: $uri)"
            )
            return primaryStoragePath + dirSep + virtualPath
        } else if (uriString // Path is likely located in the /Download folder
                .startsWith("content://com.android.providers.downloads.documents/document/msf")
        ) {
            var fileName: String? = null
            context.contentResolver.query(
                uri,
                arrayOf(OpenableColumns.DISPLAY_NAME),
                null,
                null,
                null
            )?.use { cursor ->
                if (cursor.moveToFirst()) {
                    fileName = cursor.getString(0)
                }
            }
            if (fileName == null) {
                android.util.Log.e(
                    "NativeLibrary",
                    "Unable to resolve URI to real file (URI: $uri)"
                )
                return ""
            }
            val downloadsPath = Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOWNLOADS
            ).absolutePath
            android.util.Log.v(
                "NativeLibrary",
                "Successfully resolved URI of type DOWNLOAD with fileName '$fileName': $uri)"
            )
            return downloadsPath + dirSep + fileName
        } else { // Path is probably located on a removable storage device
            val storageIdString = pathSegment.substringBefore(":")
            val removablePath = RemovableStorageHelper.getRemovableStoragePath(
                CitraApplication.appContext,
                storageIdString
            )

            if (removablePath == null) {
                android.util.Log.e(
                    "NativeLibrary",
                    "Unknown mount location for storage device '$storageIdString' (URI: $uri)"
                )
                return ""
            }
            android.util.Log.v(
                "NativeLibrary",
                "Successfully resolved URI of type EXTERNAL: $uri)"
            )
            return removablePath + dirSep + virtualPath
        }
    }

    @Keep
    @JvmStatic
    fun getUserDirectory(): String {
        val preferences: SharedPreferences =
            PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)
        val userDirectoryUri = preferences.getString("CITRA_DIRECTORY", "")!!.toUri()
        return getNativePath(userDirectoryUri)
    }

    @Keep
    @JvmStatic
    fun getSize(path: String): Long = if (FileUtil.isNativePath(path)) {
        CitraApplication.documentsTree.getFileSize(path)
    } else {
        FileUtil.getFileSize(path)
    }

    @Keep
    @JvmStatic
    fun getBuildFlavor(): String = BuildConfig.FLAVOR

    @Keep
    @JvmStatic
    fun isUsingAngleForOpenGL(): Boolean = GraphicsUtil.isUsingAngleForOpenGL()

    @Keep
    @JvmStatic
    fun fileExists(path: String): Boolean = if (FileUtil.isNativePath(path)) {
        CitraApplication.documentsTree.exists(path)
    } else {
        FileUtil.exists(path)
    }

    @Keep
    @JvmStatic
    fun isDirectory(path: String): Boolean = if (FileUtil.isNativePath(path)) {
        CitraApplication.documentsTree.isDirectory(path)
    } else {
        FileUtil.isDirectory(path)
    }

    @Keep
    @JvmStatic
    fun copyFile(
        sourcePath: String,
        destinationParentPath: String,
        destinationFilename: String
    ): Boolean = if (FileUtil.isNativePath(sourcePath) &&
        FileUtil.isNativePath(destinationParentPath)
    ) {
        CitraApplication.documentsTree
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
            try {
                CitraApplication.documentsTree.renameFile(path, destinationFilename)
            } catch (e: Exception) {
                false
            }
        } else {
            FileUtil.renameFile(path, destinationFilename)
        }

    @Keep
    @JvmStatic
    fun updateDocumentLocation(sourcePath: String, destinationPath: String): Boolean =
        CitraApplication.documentsTree.updateDocumentLocation(sourcePath, destinationPath)

    @Keep
    @JvmStatic
    fun moveFile(filename: String, sourceDirPath: String, destinationDirPath: String): Boolean =
        if (FileUtil.isNativePath(sourceDirPath)) {
            try {
                CitraApplication.documentsTree.moveFile(filename, sourceDirPath, destinationDirPath)
            } catch (e: Exception) {
                false
            }
        } else {
            FileUtil.moveFile(filename, sourceDirPath, destinationDirPath)
        }

    @Keep
    @JvmStatic
    fun deleteDocument(path: String): Boolean = if (FileUtil.isNativePath(path)) {
        CitraApplication.documentsTree.deleteDocument(path)
    } else {
        FileUtil.deleteDocument(path)
    }

    enum class CoreError(val value: Int, @StringRes val stringRes: Int) {
        Success(0, R.string.core_error_success),
        ErrorNotInitialized(1, R.string.core_error_not_initialized),
        ErrorGetLoader(2, R.string.core_error_get_loader),
        ErrorSystemMode(3, R.string.core_error_system_mode),
        ErrorLoader(4, R.string.core_error_loader),
        ErrorLoaderErrorEncrypted(5, R.string.core_error_loader_encrypted),
        ErrorLoaderErrorInvalidFormat(6, R.string.core_error_loader_invalid_format),
        ErrorLoaderErrorGBATitle(7, R.string.core_error_loader_gba_title),
        ErrorLoaderErrorPatches(8, R.string.core_error_loader_error_patches),
        ErrorLoaderErrorPatchesInvalidTitle(9, R.string.core_error_loader_patches_invalid_title),
        ErrorSystemFiles(10, R.string.core_error_system_files),
        ErrorSavestate(11, R.string.core_error_savestate),
        ErrorArticDisconnected(12, R.string.core_error_artic_disconnected),
        ErrorN3DSApplication(13, R.string.core_error_n3ds_application),
        ErrorCoreExceptionRaised(14, R.string.core_error_core_exception_raised),
        ErrorSavestateBuildMismatch(15, R.string.core_error_savestate_build_mismatch),
        ShutdownRequested(16, R.string.core_error_shutdown_requested),
        ErrorUnknown(17, R.string.core_error_unknown);

        companion object {
            fun fromInt(value: Int): CoreError = entries.find { it.value == value } ?: ErrorUnknown
        }
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
                .setMessage(
                    Html.fromHtml(
                        message,
                        Html.FROM_HTML_MODE_LEGACY
                    )
                )
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
            val alert = dialog.create()
            alert.show()
            val alertMessage = alert.findViewById<View>(android.R.id.message) as TextView
            alertMessage.movementMethod = LinkMovementMethod.getInstance()
            return alert
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

            fun newInstance(
                title: String,
                message: String,
                canContinue: Boolean
            ): CoreErrorDialogFragment {
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
        const val BUTTON_TURBO = 801
    }

    /**
     * Button states
     */
    object ButtonState {
        const val RELEASED = 0
        const val PRESSED = 1
    }
}

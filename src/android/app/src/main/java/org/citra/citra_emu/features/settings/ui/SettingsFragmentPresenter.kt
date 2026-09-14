// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.ui

import android.content.Context
import android.content.SharedPreferences
import android.content.res.Resources
import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.os.Build
import android.text.TextUtils
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.citra.citra_emu.BuildConfig
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.R
import org.citra.citra_emu.display.ScreenLayout
import org.citra.citra_emu.display.StereoMode
import org.citra.citra_emu.display.StereoWhichDisplay
import org.citra.citra_emu.features.settings.model.AbstractBooleanSetting
import org.citra.citra_emu.features.settings.model.AbstractIntSetting
import org.citra.citra_emu.features.settings.model.AbstractSetting
import org.citra.citra_emu.features.settings.model.AbstractShortSetting
import org.citra.citra_emu.features.settings.model.AbstractStringSetting
import org.citra.citra_emu.features.settings.model.BooleanSetting
import org.citra.citra_emu.features.settings.model.FloatSetting
import org.citra.citra_emu.features.settings.model.IntListSetting
import org.citra.citra_emu.features.settings.model.IntSetting
import org.citra.citra_emu.features.settings.model.ScaledFloatSetting
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.features.settings.model.StringSetting
import org.citra.citra_emu.features.settings.model.view.DateTimeSetting
import org.citra.citra_emu.features.settings.model.view.HeaderSetting
import org.citra.citra_emu.features.settings.model.view.InputBindingSetting
import org.citra.citra_emu.features.settings.model.view.MultiChoiceSetting
import org.citra.citra_emu.features.settings.model.view.RunnableSetting
import org.citra.citra_emu.features.settings.model.view.SettingsItem
import org.citra.citra_emu.features.settings.model.view.SingleChoiceSetting
import org.citra.citra_emu.features.settings.model.view.SliderSetting
import org.citra.citra_emu.features.settings.model.view.StringInputSetting
import org.citra.citra_emu.features.settings.model.view.StringSingleChoiceSetting
import org.citra.citra_emu.features.settings.model.view.SubmenuSetting
import org.citra.citra_emu.features.settings.model.view.SwitchSetting
import org.citra.citra_emu.features.settings.utils.SettingsFile
import org.citra.citra_emu.fragments.ResetSettingsDialogFragment
import org.citra.citra_emu.utils.BirthdayMonth
import org.citra.citra_emu.utils.BuildUtil
import org.citra.citra_emu.utils.GraphicsUtil
import org.citra.citra_emu.utils.Log
import org.citra.citra_emu.utils.SystemSaveGame
import org.citra.citra_emu.utils.ThemeUtil

class SettingsFragmentPresenter(private val fragmentView: SettingsFragmentView) {
    private var menuTag: String? = null
    private lateinit var gameId: String
    private var settingsList: ArrayList<SettingsItem>? = null

    private val settingsActivity get() = fragmentView.activityView as SettingsActivity
    private val settings get() = fragmentView.activityView!!.settings
    private lateinit var settingsAdapter: SettingsAdapter

    private lateinit var preferences: SharedPreferences

    fun onCreate(menuTag: String, gameId: String) {
        this.gameId = gameId
        this.menuTag = menuTag
    }

    fun onViewCreated(settingsAdapter: SettingsAdapter) {
        this.settingsAdapter = settingsAdapter
        preferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)
        loadSettingsList()
    }

    fun putSetting(setting: AbstractSetting) {
        if (setting.section == null || setting.key == null) {
            return
        }

        val section = settings.getSection(setting.section!!)!!
        if (section.getSetting(setting.key!!) == null) {
            section.putSetting(setting)
        }
    }

    fun loadSettingsList() {
        if (!TextUtils.isEmpty(gameId)) {
            settingsActivity.setToolbarTitle("Application Settings: $gameId")
        }
        val sl = ArrayList<SettingsItem>()
        if (menuTag == null) {
            return
        }
        when (menuTag) {
            SettingsFile.FILE_NAME_CONFIG -> addConfigSettings(sl)

            Settings.SECTION_CORE -> addGeneralSettings(sl)

            Settings.SECTION_SYSTEM -> addSystemSettings(sl)

            Settings.SECTION_CAMERA -> addCameraSettings(sl)

            Settings.SECTION_CONTROLS -> addControlsSettings(sl)

            Settings.SECTION_RENDERER -> addGraphicsSettings(sl)

            Settings.SECTION_LAYOUT -> addLayoutSettings(sl)

            Settings.SECTION_NETWORK -> addNetworkSettings(sl)

            Settings.SECTION_AUDIO -> addAudioSettings(sl)

            Settings.SECTION_DEBUG -> addDebugSettings(sl)

            Settings.SECTION_THEME -> addThemeSettings(sl)

            Settings.SECTION_CUSTOM_LANDSCAPE -> addCustomLandscapeSettings(sl)

            Settings.SECTION_CUSTOM_PORTRAIT -> addCustomPortraitSettings(sl)

            Settings.SECTION_PERFORMANCE_OVERLAY -> addPerformanceOverlaySettings(sl)

            else -> {
                fragmentView.showToastMessage("Unimplemented menu", false)
                return
            }
        }
        settingsList = sl
        fragmentView.showSettingsList(settingsList!!)
    }

    /** Returns the portrait mode width */
    private fun getDimensions(): IntArray {
        val dm = Resources.getSystem().displayMetrics
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val wm = settingsActivity.windowManager.maximumWindowMetrics
            val height = wm.bounds.height().coerceAtLeast(dm.heightPixels)
            val width = wm.bounds.width().coerceAtLeast(dm.widthPixels)
            intArrayOf(width, height)
        } else {
            intArrayOf(dm.widthPixels, dm.heightPixels)
        }
    }

    private fun getSmallerDimension(): Int = getDimensions().min()

    private fun getLargerDimension(): Int = getDimensions().max()

    private fun addConfigSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_settings))
        sl.apply {
            add(
                SubmenuSetting(
                    R.string.preferences_general,
                    0,
                    R.drawable.ic_general_settings,
                    Settings.SECTION_CORE
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_system,
                    0,
                    R.drawable.ic_system_settings,
                    Settings.SECTION_SYSTEM
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_camera,
                    0,
                    R.drawable.ic_camera_settings,
                    Settings.SECTION_CAMERA
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_controls,
                    0,
                    R.drawable.ic_controls_settings,
                    Settings.SECTION_CONTROLS
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_graphics,
                    0,
                    R.drawable.ic_graphics,
                    Settings.SECTION_RENDERER
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_layout,
                    0,
                    R.drawable.ic_fit_screen,
                    Settings.SECTION_LAYOUT
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_network,
                    0,
                    R.drawable.ic_network,
                    Settings.SECTION_NETWORK
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_audio,
                    0,
                    R.drawable.ic_audio,
                    Settings.SECTION_AUDIO
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_debug,
                    0,
                    R.drawable.ic_code,
                    Settings.SECTION_DEBUG
                )
            )

            add(
                RunnableSetting(
                    R.string.reset_to_default,
                    0,
                    false,
                    R.drawable.ic_restore,
                    {
                        ResetSettingsDialogFragment().show(
                            settingsActivity.supportFragmentManager,
                            ResetSettingsDialogFragment.TAG
                        )
                    }
                )
            )
        }
    }

    private fun addGeneralSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_general))
        sl.apply {
            add(
                SwitchSetting(
                    BooleanSetting.USE_FRAME_LIMIT,
                    R.string.frame_limit_enable,
                    R.string.frame_limit_enable_description,
                    BooleanSetting.USE_FRAME_LIMIT.key,
                    BooleanSetting.USE_FRAME_LIMIT.defaultValue
                )
            )
            add(
                SliderSetting(
                    IntSetting.FRAME_LIMIT,
                    R.string.frame_limit_slider,
                    R.string.frame_limit_slider_description,
                    1,
                    200,
                    "%",
                    IntSetting.FRAME_LIMIT.key,
                    IntSetting.FRAME_LIMIT.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.TURBO_LIMIT,
                    R.string.turbo_limit,
                    R.string.turbo_limit_description,
                    100,
                    400,
                    "%",
                    IntSetting.TURBO_LIMIT.key,
                    IntSetting.TURBO_LIMIT.defaultValue.toFloat()
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.CHECK_FOR_UPDATES,
                    R.string.check_for_updates,
                    R.string.check_for_updates_description,
                    BooleanSetting.CHECK_FOR_UPDATES.key,
                    BooleanSetting.CHECK_FOR_UPDATES.defaultValue,
                    isEnabled = !BuildConfig.DEBUG
                )
            )
            if (!BuildUtil.isGooglePlayBuild) {
                add(
                    SingleChoiceSetting(
                        IntSetting.UPDATE_CHECK_CHANNEL,
                        R.string.update_check_channel,
                        R.string.update_check_channel_description,
                        R.array.updateCheckChannels,
                        R.array.updateCheckChannelsValues,
                        IntSetting.UPDATE_CHECK_CHANNEL.key,
                        IntSetting.UPDATE_CHECK_CHANNEL.defaultValue,
                        isEnabled = (!BuildConfig.DEBUG && BooleanSetting.CHECK_FOR_UPDATES.boolean)
                    )
                )
                add(
                    SwitchSetting(
                        BooleanSetting.ANDROID_HIDE_IMAGES,
                        R.string.android_hide_images,
                        R.string.android_hide_images_description,
                        BooleanSetting.ANDROID_HIDE_IMAGES.key,
                        BooleanSetting.ANDROID_HIDE_IMAGES.defaultValue
                    )
                )
            }
        }
    }

    private var countryCompatibilityChanged = true

    private fun checkCountryCompatibility() {
        if (countryCompatibilityChanged) {
            countryCompatibilityChanged = false
            val compatFlags = SystemSaveGame.getCountryCompatibility(IntSetting.EMULATED_REGION.int)
            if (compatFlags != 0) {
                var message = ""
                if (compatFlags and 1 != 0) {
                    message += settingsAdapter.context.getString(R.string.region_mismatch_emulated)
                }
                if (compatFlags and 2 != 0) {
                    if (message.isNotEmpty()) message += "\n\n"
                    message += settingsAdapter.context.getString(R.string.region_mismatch_console)
                }
                MaterialAlertDialogBuilder(settingsAdapter.context)
                    .setTitle(R.string.region_mismatch)
                    .setMessage(message)
                    .setPositiveButton(android.R.string.ok, null)
                    .show()
            }
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    private fun addSystemSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_system))
        sl.apply {
            val usernameSetting = object : AbstractStringSetting {
                override var string: String
                    get() = SystemSaveGame.getUsername()
                    set(value) = SystemSaveGame.setUsername(value)
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString get() = string
                override val defaultValue = "AZAHAR"
            }
            add(HeaderSetting(R.string.emulation_settings))
            add(
                SwitchSetting(
                    BooleanSetting.NEW_3DS,
                    R.string.new_3ds,
                    0,
                    BooleanSetting.NEW_3DS.key,
                    BooleanSetting.NEW_3DS.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.LLE_APPLETS,
                    R.string.lle_applets,
                    0,
                    BooleanSetting.LLE_APPLETS.key,
                    BooleanSetting.LLE_APPLETS.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.NOTIFY_GUEST_ON_SHUTDOWN,
                    R.string.notify_guest_on_shutdown,
                    R.string.notify_guest_on_shutdown_desc,
                    BooleanSetting.NOTIFY_GUEST_ON_SHUTDOWN.key,
                    BooleanSetting.NOTIFY_GUEST_ON_SHUTDOWN.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.REQUIRED_ONLINE_LLE_MODULES,
                    R.string.enable_required_online_lle_modules,
                    R.string.enable_required_online_lle_modules_desc,
                    BooleanSetting.REQUIRED_ONLINE_LLE_MODULES.key,
                    BooleanSetting.REQUIRED_ONLINE_LLE_MODULES.defaultValue
                )
            )
            add(HeaderSetting(R.string.profile_settings))
            val regionSetting = object : AbstractIntSetting {
                override var int: Int
                    get() {
                        val ret = IntSetting.EMULATED_REGION.int
                        checkCountryCompatibility()
                        return ret
                    }
                    set(value) {
                        IntSetting.EMULATED_REGION.int = value
                        countryCompatibilityChanged = true
                        checkCountryCompatibility()
                    }
                override val key = IntSetting.EMULATED_REGION.key
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString get() = int.toString()
                override val defaultValue = IntSetting.EMULATED_REGION.defaultValue
            }
            add(
                SingleChoiceSetting(
                    regionSetting,
                    R.string.emulated_region,
                    0,
                    R.array.regionNames,
                    R.array.regionValues
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.APPLY_REGION_FREE_PATCH,
                    R.string.apply_region_free_patch,
                    R.string.apply_region_free_patch_desc,
                    BooleanSetting.APPLY_REGION_FREE_PATCH.key,
                    BooleanSetting.APPLY_REGION_FREE_PATCH.defaultValue
                )
            )
            val systemCountrySetting = object : AbstractShortSetting {
                override var short: Short
                    get() {
                        val ret = SystemSaveGame.getCountryCode()
                        checkCountryCompatibility()
                        return ret
                    }
                    set(value) {
                        SystemSaveGame.setCountryCode(value)
                        countryCompatibilityChanged = true
                        checkCountryCompatibility()
                    }
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString = short.toString()
                override val defaultValue: Short = 49
            }
            var index = -1
            val countries = settingsActivity.resources.getStringArray(R.array.countries)
                .mapNotNull {
                    index++
                    if (it.isNotEmpty()) it to index.toString() else null
                }
            add(
                StringSingleChoiceSetting(
                    systemCountrySetting,
                    R.string.country,
                    0,
                    countries.map { it.first }.toTypedArray(),
                    countries.map { it.second }.toTypedArray()
                )
            )
            val systemLanguageSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = SystemSaveGame.getSystemLanguage()
                    set(value) = SystemSaveGame.setSystemLanguage(value)
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString get() = int.toString()
                override val defaultValue = 1
            }
            add(
                SingleChoiceSetting(
                    systemLanguageSetting,
                    R.string.emulated_language,
                    0,
                    R.array.languageNames,
                    R.array.languageValues
                )
            )
            add(
                StringInputSetting(
                    usernameSetting,
                    R.string.username,
                    0,
                    null,
                    "AZAHAR",
                    10
                )
            )
            val playCoinSettings = object : AbstractIntSetting {
                override var int: Int
                    get() = SystemSaveGame.getPlayCoins()
                    set(value) = SystemSaveGame.setPlayCoins(value)
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString = int.toString()
                override val defaultValue = 42
            }
            add(
                SliderSetting(
                    playCoinSettings,
                    R.string.play_coins,
                    0,
                    0,
                    300,
                    ""
                )
            )
            add(
                SliderSetting(
                    IntSetting.STEPS_PER_HOUR,
                    R.string.steps_per_hour,
                    R.string.steps_per_hour_description,
                    0,
                    65535,
                    " steps",
                    IntSetting.STEPS_PER_HOUR.key,
                    IntSetting.STEPS_PER_HOUR.defaultValue.toFloat()
                )
            )
            add(
                RunnableSetting(
                    R.string.console_id,
                    0,
                    false,
                    0,
                    { settingsAdapter.onClickRegenerateConsoleId() },
                    { "0x${SystemSaveGame.getConsoleId().toHexString().uppercase()}" }
                )
            )
            add(
                RunnableSetting(
                    R.string.mac_address,
                    0,
                    false,
                    0,
                    { settingsAdapter.onClickRegenerateMAC() },
                    { SystemSaveGame.getMac() }
                )
            )

            add(HeaderSetting(R.string.birthday))
            val systemBirthdayMonthSetting = object : AbstractShortSetting {
                override var short: Short
                    get() = SystemSaveGame.getBirthday()[0]
                    set(value) {
                        val birthdayDay = SystemSaveGame.getBirthday()[1]
                        val daysInNewMonth = BirthdayMonth.getMonthFromCode(value)?.days ?: 31
                        if (daysInNewMonth < birthdayDay) {
                            SystemSaveGame.setBirthday(value, 1)
                            settingsAdapter.notifyDataSetChanged()
                        } else {
                            SystemSaveGame.setBirthday(value, birthdayDay)
                        }
                    }
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString get() = short.toString()
                override val defaultValue: Short = 11
            }
            add(
                SingleChoiceSetting(
                    systemBirthdayMonthSetting,
                    R.string.birthday_month,
                    0,
                    R.array.months,
                    R.array.monthValues
                )
            )

            val systemBirthdayDaySetting = object : AbstractShortSetting {
                override var short: Short
                    get() = SystemSaveGame.getBirthday()[1]
                    set(value) {
                        val birthdayMonth = SystemSaveGame.getBirthday()[0]
                        val daysInNewMonth =
                            BirthdayMonth.getMonthFromCode(birthdayMonth)?.days ?: 31
                        if (value > daysInNewMonth) {
                            SystemSaveGame.setBirthday(birthdayMonth, 1)
                        } else {
                            SystemSaveGame.setBirthday(birthdayMonth, value)
                        }
                    }
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString get() = short.toString()
                override val defaultValue: Short = 7
            }
            val birthdayMonth = SystemSaveGame.getBirthday()[0]
            val daysInMonth = BirthdayMonth.getMonthFromCode(birthdayMonth)?.days ?: 31
            val dayArray = Array(daysInMonth) { "${it + 1}" }
            add(
                StringSingleChoiceSetting(
                    systemBirthdayDaySetting,
                    R.string.birthday_day,
                    0,
                    dayArray,
                    dayArray
                )
            )

            add(HeaderSetting(R.string.clock))
            add(
                SingleChoiceSetting(
                    IntSetting.INIT_CLOCK,
                    R.string.init_clock,
                    R.string.init_clock_description,
                    R.array.systemClockNames,
                    R.array.systemClockValues,
                    IntSetting.INIT_CLOCK.key,
                    IntSetting.INIT_CLOCK.defaultValue
                )
            )
            add(
                DateTimeSetting(
                    StringSetting.INIT_TIME,
                    R.string.simulated_clock,
                    R.string.simulated_clock_description,
                    StringSetting.INIT_TIME.key,
                    StringSetting.INIT_TIME.defaultValue
                )
            )

            add(HeaderSetting(R.string.plugin_loader))
            add(
                SwitchSetting(
                    BooleanSetting.PLUGIN_LOADER,
                    R.string.plugin_loader,
                    R.string.plugin_loader_description,
                    BooleanSetting.PLUGIN_LOADER.key,
                    BooleanSetting.PLUGIN_LOADER.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.ALLOW_PLUGIN_LOADER,
                    R.string.allow_plugin_loader,
                    R.string.allow_plugin_loader_description,
                    BooleanSetting.ALLOW_PLUGIN_LOADER.key,
                    BooleanSetting.ALLOW_PLUGIN_LOADER.defaultValue
                )
            )
            add(HeaderSetting(R.string.storage))
            add(
                SwitchSetting(
                    BooleanSetting.COMPRESS_INSTALLED_CIA_CONTENT,
                    R.string.compress_cia_installs,
                    R.string.compress_cia_installs_description,
                    BooleanSetting.COMPRESS_INSTALLED_CIA_CONTENT.key,
                    BooleanSetting.COMPRESS_INSTALLED_CIA_CONTENT.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.ASYNC_FS_OPERATIONS,
                    R.string.async_fs_operations,
                    R.string.async_fs_operations_description,
                    BooleanSetting.ASYNC_FS_OPERATIONS.key,
                    BooleanSetting.ASYNC_FS_OPERATIONS.defaultValue
                )
            )
        }
    }

    private fun addCameraSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.camera))

        // Get the camera IDs
        val cameraManager =
            settingsActivity.getSystemService(Context.CAMERA_SERVICE) as CameraManager?
        val supportedCameraNameList = ArrayList<String>()
        val supportedCameraIdList = ArrayList<String>()
        if (cameraManager != null) {
            try {
                for (id in cameraManager.cameraIdList) {
                    val characteristics = cameraManager.getCameraCharacteristics(id)
                    if (characteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL) ==
                        CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY
                    ) {
                        continue // Legacy cameras cannot be used with the NDK
                    }
                    supportedCameraIdList.add(id)
                    val facing = characteristics.get(CameraCharacteristics.LENS_FACING)
                    var stringId: Int = R.string.camera_facing_external
                    when (facing) {
                        CameraCharacteristics.LENS_FACING_FRONT ->
                            stringId =
                                R.string.camera_facing_front

                        CameraCharacteristics.LENS_FACING_BACK ->
                            stringId =
                                R.string.camera_facing_back

                        CameraCharacteristics.LENS_FACING_EXTERNAL ->
                            stringId =
                                R.string.camera_facing_external
                    }
                    supportedCameraNameList.add(
                        String.format("%1\$s (%2\$s)", id, settingsActivity.getString(stringId))
                    )
                }
            } catch (e: CameraAccessException) {
                Log.error("Couldn't retrieve camera list")
                e.printStackTrace()
            }
        }

        // Create the names and values for display
        val cameraDeviceNameList =
            settingsActivity.resources.getStringArray(R.array.cameraDeviceNames).toMutableList()
        cameraDeviceNameList.addAll(supportedCameraNameList)
        val cameraDeviceValueList =
            settingsActivity.resources.getStringArray(R.array.cameraDeviceValues).toMutableList()
        cameraDeviceValueList.addAll(supportedCameraIdList)

        val haveCameraDevices = supportedCameraIdList.isNotEmpty()

        val imageSourceNames =
            settingsActivity.resources.getStringArray(R.array.cameraImageSourceNames)
        val imageSourceValues =
            settingsActivity.resources.getStringArray(R.array.cameraImageSourceValues)
        if (!haveCameraDevices) {
            // Remove the last entry (ndk / Device Camera)
            imageSourceNames.copyOfRange(0, imageSourceNames.size - 1)
            imageSourceValues.copyOfRange(0, imageSourceValues.size - 1)
        }

        sl.apply {
            add(HeaderSetting(R.string.inner_camera))
            add(
                StringSingleChoiceSetting(
                    StringSetting.CAMERA_INNER_NAME,
                    R.string.image_source,
                    R.string.image_source_description,
                    imageSourceNames,
                    imageSourceValues,
                    StringSetting.CAMERA_INNER_NAME.key,
                    StringSetting.CAMERA_INNER_NAME.defaultValue
                )
            )
            if (haveCameraDevices) {
                add(
                    StringSingleChoiceSetting(
                        StringSetting.CAMERA_INNER_CONFIG,
                        R.string.camera_device,
                        R.string.camera_device_description,
                        cameraDeviceNameList.toTypedArray(),
                        cameraDeviceValueList.toTypedArray(),
                        StringSetting.CAMERA_INNER_CONFIG.key,
                        StringSetting.CAMERA_INNER_CONFIG.defaultValue
                    )
                )
            }
            add(
                SingleChoiceSetting(
                    IntSetting.CAMERA_INNER_FLIP,
                    R.string.image_flip,
                    0,
                    R.array.cameraFlipNames,
                    R.array.cameraDeviceValues,
                    IntSetting.CAMERA_INNER_FLIP.key,
                    IntSetting.CAMERA_INNER_FLIP.defaultValue
                )
            )

            add(HeaderSetting(R.string.outer_left_camera))
            add(
                StringSingleChoiceSetting(
                    StringSetting.CAMERA_OUTER_LEFT_NAME,
                    R.string.image_source,
                    R.string.image_source_description,
                    imageSourceNames,
                    imageSourceValues,
                    StringSetting.CAMERA_OUTER_LEFT_NAME.key,
                    StringSetting.CAMERA_OUTER_LEFT_NAME.defaultValue
                )
            )
            if (haveCameraDevices) {
                add(
                    StringSingleChoiceSetting(
                        StringSetting.CAMERA_OUTER_LEFT_CONFIG,
                        R.string.camera_device,
                        R.string.camera_device_description,
                        cameraDeviceNameList.toTypedArray(),
                        cameraDeviceValueList.toTypedArray(),
                        StringSetting.CAMERA_OUTER_LEFT_CONFIG.key,
                        StringSetting.CAMERA_OUTER_LEFT_CONFIG.defaultValue
                    )
                )
            }
            add(
                SingleChoiceSetting(
                    IntSetting.CAMERA_OUTER_LEFT_FLIP,
                    R.string.image_flip,
                    0,
                    R.array.cameraFlipNames,
                    R.array.cameraDeviceValues,
                    IntSetting.CAMERA_OUTER_LEFT_FLIP.key,
                    IntSetting.CAMERA_OUTER_LEFT_FLIP.defaultValue
                )
            )

            add(HeaderSetting(R.string.outer_right_camera))
            add(
                StringSingleChoiceSetting(
                    StringSetting.CAMERA_OUTER_RIGHT_NAME,
                    R.string.image_source,
                    R.string.image_source_description,
                    imageSourceNames,
                    imageSourceValues,
                    StringSetting.CAMERA_OUTER_RIGHT_NAME.key,
                    StringSetting.CAMERA_OUTER_RIGHT_NAME.defaultValue
                )
            )
            if (haveCameraDevices) {
                add(
                    StringSingleChoiceSetting(
                        StringSetting.CAMERA_OUTER_RIGHT_CONFIG,
                        R.string.camera_device,
                        R.string.camera_device_description,
                        cameraDeviceNameList.toTypedArray(),
                        cameraDeviceValueList.toTypedArray(),
                        StringSetting.CAMERA_OUTER_RIGHT_CONFIG.key,
                        StringSetting.CAMERA_OUTER_RIGHT_CONFIG.defaultValue
                    )
                )
            }
            add(
                SingleChoiceSetting(
                    IntSetting.CAMERA_OUTER_RIGHT_FLIP,
                    R.string.image_flip,
                    0,
                    R.array.cameraFlipNames,
                    R.array.cameraDeviceValues,
                    IntSetting.CAMERA_OUTER_RIGHT_FLIP.key,
                    IntSetting.CAMERA_OUTER_RIGHT_FLIP.defaultValue
                )
            )
        }
    }

    private fun addControlsSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_controls))

        sl.apply {
            add(
                RunnableSetting(
                    R.string.controller_auto_map,
                    R.string.controller_auto_map_description,
                    true,
                    R.drawable.ic_controller,
                    { settingsAdapter.onClickAutoMap() },
                    onLongClick = { settingsAdapter.onLongClickAutoMap() }
                )
            )

            add(HeaderSetting(R.string.generic_buttons))
            Settings.buttonKeys.forEachIndexed { i: Int, key: String ->
                val button = getInputObject(key)
                add(InputBindingSetting(button, Settings.buttonTitles[i]))
            }

            add(HeaderSetting(R.string.controller_circlepad))
            Settings.circlePadKeys.forEachIndexed { i: Int, key: String ->
                val button = getInputObject(key)
                add(InputBindingSetting(button, Settings.axisTitles[i]))
            }

            add(HeaderSetting(R.string.controller_c))
            Settings.cStickKeys.forEachIndexed { i: Int, key: String ->
                val button = getInputObject(key)
                add(InputBindingSetting(button, Settings.axisTitles[i]))
            }

            add(
                HeaderSetting(
                    R.string.controller_dpad_axis,
                    R.string.controller_dpad_axis_description
                )
            )
            Settings.dPadAxisKeys.forEachIndexed { i: Int, key: String ->
                val button = getInputObject(key)
                add(InputBindingSetting(button, Settings.axisTitles[i]))
            }
            add(
                HeaderSetting(
                    R.string.controller_dpad_button,
                    R.string.controller_dpad_button_description
                )
            )
            Settings.dPadButtonKeys.forEachIndexed { i: Int, key: String ->
                val button = getInputObject(key)
                add(InputBindingSetting(button, Settings.dPadTitles[i]))
            }

            add(HeaderSetting(R.string.controller_triggers))
            Settings.triggerKeys.forEachIndexed { i: Int, key: String ->
                val button = getInputObject(key)
                add(InputBindingSetting(button, Settings.triggerTitles[i]))
            }

            add(HeaderSetting(R.string.controller_hotkeys, R.string.controller_hotkeys_description))
            Settings.hotKeys.forEachIndexed { i: Int, key: String ->
                val button = getInputObject(key)
                add(InputBindingSetting(button, Settings.hotkeyTitles[i]))
            }

            add(HeaderSetting(R.string.miscellaneous))
            add(
                SwitchSetting(
                    BooleanSetting.USE_ARTIC_BASE_CONTROLLER,
                    R.string.use_artic_base_controller,
                    R.string.use_artic_base_controller_description,
                    BooleanSetting.USE_ARTIC_BASE_CONTROLLER.key,
                    BooleanSetting.USE_ARTIC_BASE_CONTROLLER.defaultValue
                )
            )

            add(
                MultiChoiceSetting(
                    IntListSetting.COMBO_BUTTON_BUTTONS,
                    R.string.combo_button_settings,
                    R.string.combo_button_settings_description,
                    R.array.comboOptions,
                    R.array.comboOptionValues,
                    IntListSetting.COMBO_BUTTON_BUTTONS.key,
                    IntListSetting.COMBO_BUTTON_BUTTONS.defaultValue
                )
            )
        }
    }

    private fun getInputObject(key: String): AbstractStringSetting =
        object : AbstractStringSetting {
            override var string: String
                get() = preferences.getString(key, "")!!
                set(value) {
                    preferences.edit()
                        .putString(key, value)
                        .apply()
                }
            override val key = key
            override val section = Settings.SECTION_CONTROLS
            override val isRuntimeEditable = true
            override val valueAsString = preferences.getString(key, "")!!
            override val defaultValue = ""
        }

    private fun addGraphicsSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_graphics))
        sl.apply {
            add(HeaderSetting(R.string.renderer))
            add(
                SingleChoiceSetting(
                    IntSetting.GRAPHICS_API,
                    R.string.graphics_api,
                    0,
                    R.array.graphicsApiNames,
                    R.array.graphicsApiValues,
                    IntSetting.GRAPHICS_API.key,
                    IntSetting.GRAPHICS_API.defaultValue,
                    isEnabled = !GraphicsUtil.isUsingAngleForOpenGL(),
                    disabledMessage = R.string.setting_disabled_description_angle
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.SPIRV_SHADER_GEN,
                    R.string.spirv_shader_gen,
                    R.string.spirv_shader_gen_description,
                    BooleanSetting.SPIRV_SHADER_GEN.key,
                    BooleanSetting.SPIRV_SHADER_GEN.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.DISABLE_SPIRV_OPTIMIZER,
                    R.string.disable_spirv_optimizer,
                    R.string.disable_spirv_optimizer_description,
                    BooleanSetting.DISABLE_SPIRV_OPTIMIZER.key,
                    BooleanSetting.DISABLE_SPIRV_OPTIMIZER.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.ASYNC_SHADERS,
                    R.string.async_shaders,
                    R.string.async_shaders_description,
                    BooleanSetting.ASYNC_SHADERS.key,
                    BooleanSetting.ASYNC_SHADERS.defaultValue
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RESOLUTION_FACTOR,
                    R.string.internal_resolution,
                    R.string.internal_resolution_description,
                    R.array.resolutionFactorNames,
                    R.array.resolutionFactorValues,
                    IntSetting.RESOLUTION_FACTOR.key,
                    IntSetting.RESOLUTION_FACTOR.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.USE_INTEGER_SCALING,
                    R.string.use_integer_scaling,
                    R.string.use_integer_scaling_description,
                    BooleanSetting.USE_INTEGER_SCALING.key,
                    BooleanSetting.USE_INTEGER_SCALING.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.LINEAR_FILTERING,
                    R.string.linear_filtering,
                    R.string.linear_filtering_description,
                    BooleanSetting.LINEAR_FILTERING.key,
                    BooleanSetting.LINEAR_FILTERING.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.SHADERS_ACCURATE_MUL,
                    R.string.shaders_accurate_mul,
                    R.string.shaders_accurate_mul_description,
                    BooleanSetting.SHADERS_ACCURATE_MUL.key,
                    BooleanSetting.SHADERS_ACCURATE_MUL.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.DISK_SHADER_CACHE,
                    R.string.use_disk_shader_cache,
                    R.string.use_disk_shader_cache_description,
                    BooleanSetting.DISK_SHADER_CACHE.key,
                    BooleanSetting.DISK_SHADER_CACHE.defaultValue
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.TEXTURE_FILTER,
                    R.string.texture_filter_name,
                    R.string.texture_filter_description,
                    R.array.textureFilterNames,
                    R.array.textureFilterValues,
                    IntSetting.TEXTURE_FILTER.key,
                    IntSetting.TEXTURE_FILTER.defaultValue
                )
            )
            add(
                SliderSetting(
                    IntSetting.DELAY_RENDER_THREAD_US,
                    R.string.delay_render_thread,
                    R.string.delay_render_thread_description,
                    0,
                    16000,
                    " μs",
                    IntSetting.DELAY_RENDER_THREAD_US.key,
                    IntSetting.DELAY_RENDER_THREAD_US.defaultValue.toFloat()
                )
            )

            add(HeaderSetting(R.string.stereoscopy))
            add(
                SingleChoiceSetting(
                    IntSetting.RENDER_3D_WHICH_DISPLAY,
                    R.string.render_3d_which_display,
                    R.string.render_3d_which_display_description,
                    R.array.render3dWhichDisplay,
                    R.array.render3dDisplayValues,
                    IntSetting.RENDER_3D_WHICH_DISPLAY.key,
                    IntSetting.RENDER_3D_WHICH_DISPLAY.defaultValue
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.STEREOSCOPIC_3D_MODE,
                    R.string.render3d,
                    R.string.render3d_description,
                    R.array.render3dModes,
                    R.array.render3dValues,
                    IntSetting.STEREOSCOPIC_3D_MODE.key,
                    IntSetting.STEREOSCOPIC_3D_MODE.defaultValue,
                    isEnabled =
                        IntSetting.RENDER_3D_WHICH_DISPLAY.int != StereoWhichDisplay.NONE.int
                )
            )

            add(
                SliderSetting(
                    IntSetting.STEREOSCOPIC_3D_DEPTH,
                    R.string.factor3d,
                    R.string.factor3d_description,
                    0,
                    255,
                    "%",
                    IntSetting.STEREOSCOPIC_3D_DEPTH.key,
                    IntSetting.STEREOSCOPIC_3D_DEPTH.defaultValue.toFloat()
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.DISABLE_RIGHT_EYE_RENDER,
                    R.string.disable_right_eye_render,
                    R.string.disable_right_eye_render_description,
                    BooleanSetting.DISABLE_RIGHT_EYE_RENDER.key,
                    BooleanSetting.DISABLE_RIGHT_EYE_RENDER.defaultValue
                )
            )

            add(
                SwitchSetting(
                    BooleanSetting.SWAP_EYES_3D,
                    R.string.swap_eyes_3d,
                    R.string.swap_eyes_3d_description,
                    BooleanSetting.SWAP_EYES_3D.key,
                    BooleanSetting.SWAP_EYES_3D.defaultValue,
                    isEnabled =
                        IntSetting.RENDER_3D_WHICH_DISPLAY.int != StereoWhichDisplay.NONE.int
                )
            )

            add(HeaderSetting(R.string.cardboard_vr))
            add(
                SliderSetting(
                    IntSetting.CARDBOARD_SCREEN_SIZE,
                    R.string.cardboard_screen_size,
                    R.string.cardboard_screen_size_description,
                    30,
                    100,
                    "%",
                    IntSetting.CARDBOARD_SCREEN_SIZE.key,
                    IntSetting.CARDBOARD_SCREEN_SIZE.defaultValue.toFloat(),
                    isEnabled = IntSetting.STEREOSCOPIC_3D_MODE.int == StereoMode.CARDBOARD_VR.int
                )
            )
            add(
                SliderSetting(
                    IntSetting.CARDBOARD_X_SHIFT,
                    R.string.cardboard_x_shift,
                    R.string.cardboard_x_shift_description,
                    -100,
                    100,
                    "%",
                    IntSetting.CARDBOARD_X_SHIFT.key,
                    IntSetting.CARDBOARD_X_SHIFT.defaultValue.toFloat(),
                    isEnabled = IntSetting.STEREOSCOPIC_3D_MODE.int == StereoMode.CARDBOARD_VR.int
                )
            )
            add(
                SliderSetting(
                    IntSetting.CARDBOARD_Y_SHIFT,
                    R.string.cardboard_y_shift,
                    R.string.cardboard_y_shift_description,
                    -100,
                    100,
                    "%",
                    IntSetting.CARDBOARD_Y_SHIFT.key,
                    IntSetting.CARDBOARD_Y_SHIFT.defaultValue.toFloat(),
                    isEnabled = IntSetting.STEREOSCOPIC_3D_MODE.int == StereoMode.CARDBOARD_VR.int
                )
            )

            add(HeaderSetting(R.string.utility))
            add(
                SwitchSetting(
                    BooleanSetting.DUMP_TEXTURES,
                    R.string.dump_textures,
                    R.string.dump_textures_description,
                    BooleanSetting.DUMP_TEXTURES.key,
                    BooleanSetting.DUMP_TEXTURES.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.CUSTOM_TEXTURES,
                    R.string.custom_textures,
                    R.string.custom_textures_description,
                    BooleanSetting.CUSTOM_TEXTURES.key,
                    BooleanSetting.CUSTOM_TEXTURES.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.ASYNC_CUSTOM_LOADING,
                    R.string.async_custom_loading,
                    R.string.async_custom_loading_description,
                    BooleanSetting.ASYNC_CUSTOM_LOADING.key,
                    BooleanSetting.ASYNC_CUSTOM_LOADING.defaultValue
                )
            )

            add(HeaderSetting(R.string.advanced))
            add(
                SingleChoiceSetting(
                    IntSetting.TEXTURE_SAMPLING,
                    R.string.texture_sampling_name,
                    R.string.texture_sampling_description,
                    R.array.textureSamplingNames,
                    R.array.textureSamplingValues,
                    IntSetting.TEXTURE_SAMPLING.key,
                    IntSetting.TEXTURE_SAMPLING.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.USE_SKIP_DUPLICATE_FRAMES,
                    R.string.use_skip_duplicate_frames,
                    R.string.use_skip_duplicate_frames_description,
                    BooleanSetting.USE_SKIP_DUPLICATE_FRAMES.key,
                    BooleanSetting.USE_SKIP_DUPLICATE_FRAMES.defaultValue
                )
            )

            // Disabled until custom texture implementation gets rewrite, current one overloads RAM
            // and crashes Citra.
            // add(
            //     SwitchSetting(
            //         BooleanSetting.PRELOAD_TEXTURES,
            //         R.string.preload_textures,
            //         R.string.preload_textures_description,
            //         BooleanSetting.PRELOAD_TEXTURES.key,
            //         BooleanSetting.PRELOAD_TEXTURES.defaultValue
            //     )
            // )
        }
    }

    private fun addLayoutSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_layout))
        sl.apply {
            add(
                SingleChoiceSetting(
                    IntSetting.ORIENTATION_OPTION,
                    R.string.layout_screen_orientation,
                    0,
                    R.array.screenOrientations,
                    R.array.screenOrientationValues,
                    IntSetting.ORIENTATION_OPTION.key,
                    IntSetting.ORIENTATION_OPTION.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.EXPAND_TO_CUTOUT_AREA,
                    R.string.expand_to_cutout_area,
                    R.string.expand_to_cutout_area_description,
                    BooleanSetting.EXPAND_TO_CUTOUT_AREA.key,
                    BooleanSetting.EXPAND_TO_CUTOUT_AREA.defaultValue
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.SCREEN_LAYOUT,
                    R.string.emulation_switch_screen_layout,
                    0,
                    R.array.landscapeLayouts,
                    R.array.landscapeLayoutValues,
                    IntSetting.SCREEN_LAYOUT.key,
                    IntSetting.SCREEN_LAYOUT.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.UPRIGHT_SCREEN,
                    R.string.emulation_rotate_upright,
                    0,
                    BooleanSetting.UPRIGHT_SCREEN.key,
                    BooleanSetting.UPRIGHT_SCREEN.defaultValue
                )
            )
            add(
                MultiChoiceSetting(
                    IntListSetting.LAYOUTS_TO_CYCLE,
                    R.string.layouts_to_cycle,
                    R.string.layouts_to_cycle_description,
                    R.array.landscapeLayouts,
                    R.array.landscapeLayoutValues,
                    IntListSetting.LAYOUTS_TO_CYCLE.key,
                    IntListSetting.LAYOUTS_TO_CYCLE.defaultValue
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.PORTRAIT_SCREEN_LAYOUT,
                    R.string.emulation_switch_portrait_layout,
                    0,
                    R.array.portraitLayouts,
                    R.array.portraitLayoutValues,
                    IntSetting.PORTRAIT_SCREEN_LAYOUT.key,
                    IntSetting.PORTRAIT_SCREEN_LAYOUT.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.ENABLE_SECONDARY_DISPLAY,
                    R.string.emulation_secondary_display_enable,
                    R.string.emulation_secondary_display_enable_description,
                    BooleanSetting.ENABLE_SECONDARY_DISPLAY.key,
                    BooleanSetting.ENABLE_SECONDARY_DISPLAY.defaultValue
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.SECONDARY_DISPLAY_LAYOUT,
                    R.string.emulation_switch_secondary_layout,
                    R.string.emulation_switch_secondary_layout_description,
                    R.array.secondaryLayouts,
                    R.array.secondaryLayoutValues,
                    IntSetting.SECONDARY_DISPLAY_LAYOUT.key,
                    IntSetting.SECONDARY_DISPLAY_LAYOUT.defaultValue,
                    BooleanSetting.ENABLE_SECONDARY_DISPLAY.boolean
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.ASPECT_RATIO,
                    R.string.emulation_aspect_ratio,
                    0,
                    R.array.aspectRatioNames,
                    R.array.aspectRatioValues,
                    IntSetting.ASPECT_RATIO.key,
                    IntSetting.ASPECT_RATIO.defaultValue,
                    isEnabled = IntSetting.SCREEN_LAYOUT.int == ScreenLayout.SINGLE_SCREEN.int
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.SMALL_SCREEN_POSITION,
                    R.string.emulation_small_screen_position,
                    R.string.small_screen_position_description,
                    R.array.smallScreenPositions,
                    R.array.smallScreenPositionValues,
                    IntSetting.SMALL_SCREEN_POSITION.key,
                    IntSetting.SMALL_SCREEN_POSITION.defaultValue
                )
            )
            add(
                SliderSetting(
                    IntSetting.SCREEN_GAP,
                    R.string.screen_gap,
                    R.string.screen_gap_description,
                    0,
                    480,
                    "px",
                    IntSetting.SCREEN_GAP.key,
                    IntSetting.SCREEN_GAP.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    FloatSetting.LARGE_SCREEN_PROPORTION,
                    R.string.large_screen_proportion,
                    R.string.large_screen_proportion_description,
                    1,
                    5,
                    "",
                    FloatSetting.LARGE_SCREEN_PROPORTION.key,
                    FloatSetting.LARGE_SCREEN_PROPORTION.defaultValue
                )
            )
            add(
                SliderSetting(
                    FloatSetting.SECOND_SCREEN_OPACITY,
                    R.string.second_screen_opacity,
                    R.string.second_screen_opacity_description,
                    0,
                    100,
                    "%",
                    FloatSetting.SECOND_SCREEN_OPACITY.key,
                    FloatSetting.SECOND_SCREEN_OPACITY.defaultValue,
                    isEnabled = IntSetting.SCREEN_LAYOUT.int == ScreenLayout.CUSTOM_LAYOUT.int
                )
            )
            add(HeaderSetting(R.string.bg_color, R.string.bg_color_description))
            val bgRedSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = (FloatSetting.BACKGROUND_RED.float * 255).toInt()
                    set(value) {
                        FloatSetting.BACKGROUND_RED.float = value.toFloat() / 255
                        settings.saveSetting(
                            FloatSetting.BACKGROUND_RED,
                            SettingsFile.FILE_NAME_CONFIG
                        )
                    }
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString = int.toString()
                override val defaultValue = FloatSetting.BACKGROUND_RED.defaultValue.toInt()
            }
            add(
                SliderSetting(
                    bgRedSetting,
                    R.string.bg_red,
                    0,
                    0,
                    255,
                    ""
                )
            )
            val bgGreenSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = (FloatSetting.BACKGROUND_GREEN.float * 255).toInt()
                    set(value) {
                        FloatSetting.BACKGROUND_GREEN.float = value.toFloat() / 255
                        settings.saveSetting(
                            FloatSetting.BACKGROUND_GREEN,
                            SettingsFile.FILE_NAME_CONFIG
                        )
                    }
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString = int.toString()
                override val defaultValue = FloatSetting.BACKGROUND_GREEN.defaultValue.toInt()
            }
            add(
                SliderSetting(
                    bgGreenSetting,
                    R.string.bg_green,
                    0,
                    0,
                    255,
                    ""
                )
            )
            val bgBlueSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = (FloatSetting.BACKGROUND_BLUE.float * 255).toInt()
                    set(value) {
                        FloatSetting.BACKGROUND_BLUE.float = value.toFloat() / 255
                        settings.saveSetting(
                            FloatSetting.BACKGROUND_BLUE,
                            SettingsFile.FILE_NAME_CONFIG
                        )
                    }
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString = int.toString()
                override val defaultValue = FloatSetting.BACKGROUND_BLUE.defaultValue.toInt()
            }
            add(
                SliderSetting(
                    bgBlueSetting,
                    R.string.bg_blue,
                    0,
                    0,
                    255,
                    ""
                )
            )
            add(
                SubmenuSetting(
                    R.string.performance_overlay_options,
                    R.string.performance_overlay_options_description,
                    R.drawable.ic_stats,
                    Settings.SECTION_PERFORMANCE_OVERLAY
                )
            )
            add(
                SubmenuSetting(
                    R.string.emulation_landscape_custom_layout,
                    0,
                    R.drawable.ic_fit_screen,
                    Settings.SECTION_CUSTOM_LANDSCAPE
                )
            )
            add(
                SubmenuSetting(
                    R.string.emulation_portrait_custom_layout,
                    0,
                    R.drawable.ic_portrait_fit_screen,
                    Settings.SECTION_CUSTOM_PORTRAIT
                )
            )
        }
    }

    private fun addPerformanceOverlaySettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(
            settingsActivity.getString(R.string.performance_overlay_options)
        )
        sl.apply {
            add(HeaderSetting(R.string.visibility))

            add(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_ENABLE,
                    R.string.performance_overlay_enable,
                    0,
                    BooleanSetting.PERF_OVERLAY_ENABLE.key,
                    BooleanSetting.PERF_OVERLAY_ENABLE.defaultValue
                )
            )

            add(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_BACKGROUND,
                    R.string.performance_overlay_background,
                    R.string.performance_overlay_background_description,
                    BooleanSetting.PERF_OVERLAY_BACKGROUND.key,
                    BooleanSetting.PERF_OVERLAY_BACKGROUND.defaultValue
                )
            )

            add(
                SingleChoiceSetting(
                    IntSetting.PERFORMANCE_OVERLAY_POSITION,
                    R.string.performance_overlay_position,
                    R.string.performance_overlay_position_description,
                    R.array.statsPosition,
                    R.array.statsPositionValues
                )
            )

            add(HeaderSetting(R.string.information))

            add(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_SHOW_FPS,
                    R.string.performance_overlay_show_fps,
                    R.string.performance_overlay_show_fps_description,
                    BooleanSetting.PERF_OVERLAY_SHOW_FPS.key,
                    BooleanSetting.PERF_OVERLAY_SHOW_FPS.defaultValue
                )
            )

            add(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_SHOW_FRAMETIME,
                    R.string.performance_overlay_show_frametime,
                    R.string.performance_overlay_show_frametime_description,
                    BooleanSetting.PERF_OVERLAY_SHOW_FRAMETIME.key,
                    BooleanSetting.PERF_OVERLAY_SHOW_FRAMETIME.defaultValue
                )
            )

            add(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_SHOW_SPEED,
                    R.string.performance_overlay_show_speed,
                    R.string.performance_overlay_show_speed_description,
                    BooleanSetting.PERF_OVERLAY_SHOW_SPEED.key,
                    BooleanSetting.PERF_OVERLAY_SHOW_SPEED.defaultValue
                )
            )

            add(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_SHOW_APP_RAM_USAGE,
                    R.string.performance_overlay_show_app_ram_usage,
                    R.string.performance_overlay_show_app_ram_usage_description,
                    BooleanSetting.PERF_OVERLAY_SHOW_APP_RAM_USAGE.key,
                    BooleanSetting.PERF_OVERLAY_SHOW_APP_RAM_USAGE.defaultValue
                )
            )

            add(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_SHOW_AVAILABLE_RAM,
                    R.string.performance_overlay_show_available_ram,
                    R.string.performance_overlay_show_available_ram_description,
                    BooleanSetting.PERF_OVERLAY_SHOW_AVAILABLE_RAM.key,
                    BooleanSetting.PERF_OVERLAY_SHOW_AVAILABLE_RAM.defaultValue
                )
            )

            add(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_SHOW_BATTERY_TEMP,
                    R.string.performance_overlay_show_battery_temp,
                    R.string.performance_overlay_show_battery_temp_description,
                    BooleanSetting.PERF_OVERLAY_SHOW_BATTERY_TEMP.key,
                    BooleanSetting.PERF_OVERLAY_SHOW_BATTERY_TEMP.defaultValue
                )
            )
        }
    }

    private fun addCustomLandscapeSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(
            settingsActivity.getString(R.string.emulation_landscape_custom_layout)
        )
        sl.apply {
            add(HeaderSetting(R.string.emulation_top_screen))
            add(
                SliderSetting(
                    IntSetting.LANDSCAPE_TOP_X,
                    R.string.emulation_custom_layout_x,
                    0,
                    0,
                    getLargerDimension(),
                    "px",
                    IntSetting.LANDSCAPE_TOP_X.key,
                    IntSetting.LANDSCAPE_TOP_X.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.LANDSCAPE_TOP_Y,
                    R.string.emulation_custom_layout_y,
                    0,
                    0,
                    getSmallerDimension(),
                    "px",
                    IntSetting.LANDSCAPE_TOP_Y.key,
                    IntSetting.LANDSCAPE_TOP_Y.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.LANDSCAPE_TOP_WIDTH,
                    R.string.emulation_custom_layout_width,
                    0,
                    0,
                    getLargerDimension(),
                    "px",
                    IntSetting.LANDSCAPE_TOP_WIDTH.key,
                    IntSetting.LANDSCAPE_TOP_WIDTH.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.LANDSCAPE_TOP_HEIGHT,
                    R.string.emulation_custom_layout_height,
                    0,
                    0,
                    getSmallerDimension(),
                    "px",
                    IntSetting.LANDSCAPE_TOP_HEIGHT.key,
                    IntSetting.LANDSCAPE_TOP_HEIGHT.defaultValue.toFloat()
                )
            )
            add(HeaderSetting(R.string.emulation_bottom_screen))
            add(
                SliderSetting(
                    IntSetting.LANDSCAPE_BOTTOM_X,
                    R.string.emulation_custom_layout_x,
                    0,
                    0,
                    getLargerDimension(),
                    "px",
                    IntSetting.LANDSCAPE_BOTTOM_X.key,
                    IntSetting.LANDSCAPE_BOTTOM_X.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.LANDSCAPE_BOTTOM_Y,
                    R.string.emulation_custom_layout_y,
                    0,
                    0,
                    getSmallerDimension(),
                    "px",
                    IntSetting.LANDSCAPE_BOTTOM_Y.key,
                    IntSetting.LANDSCAPE_BOTTOM_Y.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.LANDSCAPE_BOTTOM_WIDTH,
                    R.string.emulation_custom_layout_width,
                    0,
                    0,
                    getLargerDimension(),
                    "px",
                    IntSetting.LANDSCAPE_BOTTOM_WIDTH.key,
                    IntSetting.LANDSCAPE_BOTTOM_WIDTH.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.LANDSCAPE_BOTTOM_HEIGHT,
                    R.string.emulation_custom_layout_height,
                    0,
                    0,
                    getSmallerDimension(),
                    "px",
                    IntSetting.LANDSCAPE_BOTTOM_HEIGHT.key,
                    IntSetting.LANDSCAPE_BOTTOM_HEIGHT.defaultValue.toFloat()
                )
            )
        }
    }

    private fun addCustomPortraitSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(
            settingsActivity.getString(R.string.emulation_portrait_custom_layout)
        )
        sl.apply {
            add(HeaderSetting(R.string.emulation_top_screen))
            add(
                SliderSetting(
                    IntSetting.PORTRAIT_TOP_X,
                    R.string.emulation_custom_layout_x,
                    0,
                    0,
                    getSmallerDimension(),
                    "px",
                    IntSetting.PORTRAIT_TOP_X.key,
                    IntSetting.PORTRAIT_TOP_X.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.PORTRAIT_TOP_Y,
                    R.string.emulation_custom_layout_y,
                    0,
                    0,
                    getLargerDimension(),
                    "px",
                    IntSetting.PORTRAIT_TOP_Y.key,
                    IntSetting.PORTRAIT_TOP_Y.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.PORTRAIT_TOP_WIDTH,
                    R.string.emulation_custom_layout_width,
                    0,
                    0,
                    getSmallerDimension(),
                    "px",
                    IntSetting.PORTRAIT_TOP_WIDTH.key,
                    IntSetting.PORTRAIT_TOP_WIDTH.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.PORTRAIT_TOP_HEIGHT,
                    R.string.emulation_custom_layout_height,
                    0,
                    0,
                    getLargerDimension(),
                    "px",
                    IntSetting.PORTRAIT_TOP_HEIGHT.key,
                    IntSetting.PORTRAIT_TOP_HEIGHT.defaultValue.toFloat()
                )
            )
            add(HeaderSetting(R.string.emulation_bottom_screen))
            add(
                SliderSetting(
                    IntSetting.PORTRAIT_BOTTOM_X,
                    R.string.emulation_custom_layout_x,
                    0,
                    0,
                    getSmallerDimension(),
                    "px",
                    IntSetting.PORTRAIT_BOTTOM_X.key,
                    IntSetting.PORTRAIT_BOTTOM_X.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.PORTRAIT_BOTTOM_Y,
                    R.string.emulation_custom_layout_y,
                    0,
                    0,
                    getLargerDimension(),
                    "px",
                    IntSetting.PORTRAIT_BOTTOM_Y.key,
                    IntSetting.PORTRAIT_BOTTOM_Y.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.PORTRAIT_BOTTOM_WIDTH,
                    R.string.emulation_custom_layout_width,
                    0,
                    0,
                    getSmallerDimension(),
                    "px",
                    IntSetting.PORTRAIT_BOTTOM_WIDTH.key,
                    IntSetting.PORTRAIT_BOTTOM_WIDTH.defaultValue.toFloat()
                )
            )
            add(
                SliderSetting(
                    IntSetting.PORTRAIT_BOTTOM_HEIGHT,
                    R.string.emulation_custom_layout_height,
                    0,
                    0,
                    getLargerDimension(),
                    "px",
                    IntSetting.PORTRAIT_BOTTOM_HEIGHT.key,
                    IntSetting.PORTRAIT_BOTTOM_HEIGHT.defaultValue.toFloat()
                )
            )
        }
    }

    private fun addNetworkSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_network))
        sl.apply {
            add(
                StringInputSetting(
                    StringSetting.WEB_API_URL,
                    R.string.web_api_url,
                    R.string.web_api_url_description,
                    StringSetting.WEB_API_URL.key,
                    StringSetting.WEB_API_URL.defaultValue
                )
            )
            add(
                StringInputSetting(
                    StringSetting.NETWORK_TOKEN,
                    R.string.network_token,
                    R.string.network_token_description,
                    StringSetting.NETWORK_TOKEN.key,
                    StringSetting.NETWORK_TOKEN.defaultValue
                )
            )
        }
    }

    private fun addAudioSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_audio))
        sl.apply {
            add(
                SliderSetting(
                    ScaledFloatSetting.AUDIO_VOLUME,
                    R.string.audio_volume,
                    0,
                    0,
                    100,
                    "%",
                    ScaledFloatSetting.AUDIO_VOLUME.key,
                    ScaledFloatSetting.AUDIO_VOLUME.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.ENABLE_AUDIO_STRETCHING,
                    R.string.audio_stretch,
                    R.string.audio_stretch_description,
                    BooleanSetting.ENABLE_AUDIO_STRETCHING.key,
                    BooleanSetting.ENABLE_AUDIO_STRETCHING.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.ENABLE_REALTIME_AUDIO,
                    R.string.realtime_audio,
                    R.string.realtime_audio_description,
                    BooleanSetting.ENABLE_REALTIME_AUDIO.key,
                    BooleanSetting.ENABLE_REALTIME_AUDIO.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.SIMULATE_HEADPHONES_PLUGGED,
                    R.string.simulate_headphones_plugged,
                    R.string.simulate_headphones_plugged_description,
                    BooleanSetting.SIMULATE_HEADPHONES_PLUGGED.key,
                    BooleanSetting.SIMULATE_HEADPHONES_PLUGGED.defaultValue
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.AUDIO_INPUT_TYPE,
                    R.string.audio_input_type,
                    0,
                    R.array.audioInputTypeNames,
                    R.array.audioInputTypeValues,
                    IntSetting.AUDIO_INPUT_TYPE.key,
                    IntSetting.AUDIO_INPUT_TYPE.defaultValue
                )
            )

            val soundOutputModeSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = SystemSaveGame.getSoundOutputMode()
                    set(value) = SystemSaveGame.setSoundOutputMode(value)
                override val key = null
                override val section = null
                override val isRuntimeEditable = false
                override val valueAsString = int.toString()
                override val defaultValue = 1
            }
            add(
                SingleChoiceSetting(
                    soundOutputModeSetting,
                    R.string.sound_output_mode,
                    0,
                    R.array.soundOutputModes,
                    R.array.soundOutputModeValues
                )
            )
        }
    }

    private fun addDebugSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_debug))
        sl.apply {
            add(HeaderSetting(R.string.debug_warning))
            add(
                SliderSetting(
                    IntSetting.CPU_CLOCK_SPEED,
                    R.string.cpu_clock_speed,
                    0,
                    25,
                    400,
                    "%",
                    IntSetting.CPU_CLOCK_SPEED.key,
                    IntSetting.CPU_CLOCK_SPEED.defaultValue.toFloat()
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.CPU_JIT,
                    R.string.cpu_jit,
                    R.string.cpu_jit_description,
                    BooleanSetting.CPU_JIT.key,
                    BooleanSetting.CPU_JIT.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.HW_SHADER,
                    R.string.hw_shaders,
                    R.string.hw_shaders_description,
                    BooleanSetting.HW_SHADER.key,
                    BooleanSetting.HW_SHADER.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.SHADER_JIT,
                    R.string.shader_jit,
                    R.string.shader_jit_description,
                    BooleanSetting.SHADER_JIT.key,
                    BooleanSetting.SHADER_JIT.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.VSYNC,
                    R.string.vsync,
                    R.string.vsync_description,
                    BooleanSetting.VSYNC.key,
                    BooleanSetting.VSYNC.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.SIMULATE_3DS_GPU_TIMINGS,
                    R.string.simulate_3ds_gpu_timings,
                    R.string.simulate_3ds_gpu_timings_description,
                    BooleanSetting.SIMULATE_3DS_GPU_TIMINGS.key,
                    BooleanSetting.SIMULATE_3DS_GPU_TIMINGS.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.DEBUG_RENDERER,
                    R.string.renderer_debug,
                    R.string.renderer_debug_description,
                    BooleanSetting.DEBUG_RENDERER.key,
                    BooleanSetting.DEBUG_RENDERER.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.INSTANT_DEBUG_LOG,
                    R.string.instant_debug_log,
                    R.string.instant_debug_log_description,
                    BooleanSetting.INSTANT_DEBUG_LOG.key,
                    BooleanSetting.INSTANT_DEBUG_LOG.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.ENABLE_RPC_SERVER,
                    R.string.enable_rpc_server,
                    R.string.enable_rpc_server_desc,
                    BooleanSetting.ENABLE_RPC_SERVER.key,
                    BooleanSetting.ENABLE_RPC_SERVER.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.TOGGLE_UNIQUE_DATA_CONSOLE_TYPE,
                    R.string.toggle_unique_data_console_type,
                    R.string.toggle_unique_data_console_type_desc,
                    BooleanSetting.TOGGLE_UNIQUE_DATA_CONSOLE_TYPE.key,
                    BooleanSetting.TOGGLE_UNIQUE_DATA_CONSOLE_TYPE.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.DELAY_START_LLE_MODULES,
                    R.string.delay_start_lle_modules,
                    R.string.delay_start_lle_modules_description,
                    BooleanSetting.DELAY_START_LLE_MODULES.key,
                    BooleanSetting.DELAY_START_LLE_MODULES.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.DETERMINISTIC_ASYNC_OPERATIONS,
                    R.string.deterministic_async_operations,
                    R.string.deterministic_async_operations_description,
                    BooleanSetting.DETERMINISTIC_ASYNC_OPERATIONS.key,
                    BooleanSetting.DETERMINISTIC_ASYNC_OPERATIONS.defaultValue
                )
            )
        }
    }

    private fun addThemeSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_theme))
        sl.apply {
            val theme: AbstractBooleanSetting = object : AbstractBooleanSetting {
                override var boolean: Boolean
                    get() = preferences.getBoolean(Settings.PREF_MATERIAL_YOU, false)
                    set(value) {
                        preferences.edit()
                            .putBoolean(Settings.PREF_MATERIAL_YOU, value)
                            .apply()
                        settingsActivity.recreate()
                    }
                override val key: String? = null
                override val section: String? = null
                override val isRuntimeEditable: Boolean = false
                override val valueAsString: String
                    get() = preferences.getBoolean(Settings.PREF_MATERIAL_YOU, false).toString()
                override val defaultValue = false
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                add(
                    SwitchSetting(
                        theme,
                        R.string.material_you,
                        R.string.material_you_description
                    )
                )
            }

            val staticThemeColor: AbstractIntSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = preferences.getInt(Settings.PREF_STATIC_THEME_COLOR, 0)
                    set(value) {
                        preferences.edit()
                            .putInt(Settings.PREF_STATIC_THEME_COLOR, value)
                            .apply()
                        settingsActivity.recreate()
                    }
                override val key: String? = null
                override val section: String? = null
                override val isRuntimeEditable: Boolean = false
                override val valueAsString: String
                    get() = preferences.getInt(Settings.PREF_STATIC_THEME_COLOR, 0).toString()
                override val defaultValue: Any = 0
            }

            add(
                SingleChoiceSetting(
                    staticThemeColor,
                    R.string.static_theme_color,
                    R.string.static_theme_color_description,
                    R.array.staticThemeNames,
                    R.array.staticThemeValues
                )
            )

            val themeMode: AbstractIntSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = preferences.getInt(Settings.PREF_THEME_MODE, -1)
                    set(value) {
                        preferences.edit()
                            .putInt(Settings.PREF_THEME_MODE, value)
                            .apply()
                        ThemeUtil.setThemeMode(settingsActivity)
                        settingsActivity.recreate()
                    }
                override val key: String? = null
                override val section: String? = null
                override val isRuntimeEditable: Boolean = false
                override val valueAsString: String
                    get() = preferences.getInt(Settings.PREF_THEME_MODE, -1).toString()
                override val defaultValue: Any = -1
            }

            add(
                SingleChoiceSetting(
                    themeMode,
                    R.string.change_theme_mode,
                    0,
                    R.array.themeModeEntries,
                    R.array.themeModeValues
                )
            )

            val blackBackgrounds: AbstractBooleanSetting = object : AbstractBooleanSetting {
                override var boolean: Boolean
                    get() = preferences.getBoolean(Settings.PREF_BLACK_BACKGROUNDS, false)
                    set(value) {
                        preferences.edit()
                            .putBoolean(Settings.PREF_BLACK_BACKGROUNDS, value)
                            .apply()
                        settingsActivity.recreate()
                    }
                override val key: String? = null
                override val section: String? = null
                override val isRuntimeEditable: Boolean = false
                override val valueAsString: String
                    get() = preferences.getBoolean(Settings.PREF_BLACK_BACKGROUNDS, false)
                        .toString()
                override val defaultValue: Any = false
            }

            add(
                SwitchSetting(
                    blackBackgrounds,
                    R.string.use_black_backgrounds,
                    R.string.use_black_backgrounds_description
                )
            )
        }
    }
}

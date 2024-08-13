// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.Color
import androidx.annotation.ColorInt
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.preference.PreferenceManager
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.R
import io.github.lime3ds.android.features.settings.model.Settings
import io.github.lime3ds.android.ui.main.ThemeProvider
import kotlin.math.roundToInt

object ThemeUtil {
    const val SYSTEM_BAR_ALPHA = 0.9f

    private val preferences: SharedPreferences get() =
        PreferenceManager.getDefaultSharedPreferences(LimeApplication.appContext)

    private fun getSelectedStaticThemeColor(): Int {
        val themeIndex = preferences.getInt(Settings.PREF_STATIC_THEME_COLOR, 0)
        val themes = arrayOf(
            R.style.Theme_Citra_Blue,
            R.style.Theme_Citra_Cyan,
            R.style.Theme_Citra_Red,
            R.style.Theme_Citra_Green,
            R.style.Theme_Citra_LimeGreen,
            R.style.Theme_Citra_Yellow,
            R.style.Theme_Citra_Orange,
            R.style.Theme_Citra_Violet,
            R.style.Theme_Citra_Pink,
            R.style.Theme_Citra_Gray
        )
        return themes[themeIndex]
    }

    fun setTheme(activity: AppCompatActivity) {
        setThemeMode(activity)
        if (preferences.getBoolean(Settings.PREF_MATERIAL_YOU, false)) {
            activity.setTheme(R.style.Theme_Lime_Main_MaterialYou)
        } else {
            activity.setTheme(getSelectedStaticThemeColor())
        }

        // Using a specific night mode check because this could apply incorrectly when using the
        // light app mode, dark system mode, and black backgrounds. Launching the settings activity
        // will then show light mode colors/navigation bars but with black backgrounds.
        if (preferences.getBoolean(Settings.PREF_BLACK_BACKGROUNDS, false) &&
            isNightMode(activity)
        ) {
            activity.setTheme(R.style.ThemeOverlay_Citra_Dark)
        }
    }

    fun setThemeMode(activity: AppCompatActivity) {
        val themeMode = PreferenceManager.getDefaultSharedPreferences(activity.applicationContext)
            .getInt(Settings.PREF_THEME_MODE, AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
        activity.delegate.localNightMode = themeMode
        val windowController = WindowCompat.getInsetsController(
            activity.window,
            activity.window.decorView
        )
        when (themeMode) {
            AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM -> when (isNightMode(activity)) {
                false -> setLightModeSystemBars(windowController)
                true -> setDarkModeSystemBars(windowController)
            }
            AppCompatDelegate.MODE_NIGHT_NO -> setLightModeSystemBars(windowController)
            AppCompatDelegate.MODE_NIGHT_YES -> setDarkModeSystemBars(windowController)
        }
    }

    private fun isNightMode(activity: AppCompatActivity): Boolean {
        return when (activity.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
            Configuration.UI_MODE_NIGHT_NO -> false
            Configuration.UI_MODE_NIGHT_YES -> true
            else -> false
        }
    }

    private fun setLightModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = true
        windowController.isAppearanceLightNavigationBars = true
    }

    private fun setDarkModeSystemBars(windowController: WindowInsetsControllerCompat) {
        windowController.isAppearanceLightStatusBars = false
        windowController.isAppearanceLightNavigationBars = false
    }

    fun setCorrectTheme(activity: AppCompatActivity) {
        val currentTheme = (activity as ThemeProvider).themeId
        setTheme(activity)
        if (currentTheme != (activity as ThemeProvider).themeId) {
            activity.recreate()
        }
    }

    @ColorInt
    fun getColorWithOpacity(@ColorInt color: Int, alphaFactor: Float): Int {
        return Color.argb(
            (alphaFactor * Color.alpha(color)).roundToInt(),
            Color.red(color),
            Color.green(color),
            Color.blue(color)
        )
    }

    private var listener: SharedPreferences.OnSharedPreferenceChangeListener? = null

    // Listener that detects if the theme is being changed from the initial setup or from normal settings
    // Without this the dual popup on the setup was getting cut off becuase the activity was being recreated
    fun registerThemeChangeListener(activity: AppCompatActivity) {
        if (listener == null) {
            listener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
                if (key == Settings.PREF_STATIC_THEME_COLOR || key == Settings.PREF_MATERIAL_YOU) {
                    activity.runOnUiThread {
                        activity.recreate()
                    }
                }
            }
            preferences.registerOnSharedPreferenceChangeListener(listener)
        }
    }

    fun unregisterThemeChangeListener() {
        if (listener != null) {
            preferences.unregisterOnSharedPreferenceChangeListener(listener)
        }
    }
}
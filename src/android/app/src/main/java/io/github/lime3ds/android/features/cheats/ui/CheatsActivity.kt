// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.cheats.ui

import android.os.Bundle
import android.view.View
import android.view.View.OnFocusChangeListener
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.navigation.fragment.NavHostFragment
import com.google.android.material.color.MaterialColors
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.ActivityCheatsBinding
import io.github.lime3ds.android.utils.InsetsHelper
import io.github.lime3ds.android.utils.ThemeUtil

class CheatsActivity : AppCompatActivity() {
    private lateinit var binding: ActivityCheatsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeUtil.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityCheatsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)
        if (InsetsHelper.getSystemGestureType(applicationContext) !=
            InsetsHelper.GESTURE_NAVIGATION
        ) {
            binding.navigationBarShade.setBackgroundColor(
                ThemeUtil.getColorWithOpacity(
                    MaterialColors.getColor(
                        binding.navigationBarShade,
                        com.google.android.material.R.attr.colorSurface
                    ),
                    ThemeUtil.SYSTEM_BAR_ALPHA
                )
            )
        }

        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        val navController = navHostFragment.navController
        navController.setGraph(R.navigation.cheats_navigation, intent.extras)
    }

    companion object {
        fun setOnFocusChangeListenerRecursively(view: View, listener: OnFocusChangeListener?) {
            view.onFocusChangeListener = listener
            if (view is ViewGroup) {
                for (i in 0 until view.childCount) {
                    val child = view.getChildAt(i)
                    setOnFocusChangeListenerRecursively(child, listener)
                }
            }
        }
    }
}

// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.cheats.ui

import android.annotation.SuppressLint
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.OnBackPressedCallback
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsAnimationCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.slidingpanelayout.widget.SlidingPaneLayout
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import io.github.lime3ds.android.databinding.FragmentCheatsBinding
import io.github.lime3ds.android.features.cheats.model.Cheat
import io.github.lime3ds.android.features.cheats.model.CheatsViewModel
import io.github.lime3ds.android.ui.TwoPaneOnBackPressedCallback
import io.github.lime3ds.android.ui.main.MainActivity
import io.github.lime3ds.android.viewmodel.HomeViewModel

class CheatsFragment : Fragment(), SlidingPaneLayout.PanelSlideListener {
    private var cheatListLastFocus: View? = null
    private var cheatDetailsLastFocus: View? = null

    private var _binding: FragmentCheatsBinding? = null
    private val binding get() = _binding!!

    private val cheatsViewModel: CheatsViewModel by activityViewModels()
    private val homeViewModel: HomeViewModel by activityViewModels()

    private val args by navArgs<CheatsFragmentArgs>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentCheatsBinding.inflate(inflater)
        return binding.root
    }

    // This is using the correct scope, lint is just acting up
    @SuppressLint("UnsafeRepeatOnLifecycleDetector")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        homeViewModel.setNavigationVisibility(visible = false, animated = true)
        homeViewModel.setStatusBarShadeVisibility(visible = false)

        cheatsViewModel.initialize(args.titleId)

        cheatListLastFocus = binding.cheatListContainer
        cheatDetailsLastFocus = binding.cheatDetailsContainer
        binding.slidingPaneLayout.addPanelSlideListener(this)
        requireActivity().onBackPressedDispatcher.addCallback(
            viewLifecycleOwner,
            TwoPaneOnBackPressedCallback(binding.slidingPaneLayout)
        )
        requireActivity().onBackPressedDispatcher.addCallback(
            viewLifecycleOwner,
            object : OnBackPressedCallback(true) {
                override fun handleOnBackPressed() {
                    if (binding.slidingPaneLayout.isOpen) {
                        binding.slidingPaneLayout.close()
                    } else {
                        if (requireActivity() is MainActivity) {
                            view.findNavController().popBackStack()
                        } else {
                            requireActivity().finish()
                        }
                    }
                }
            }
        )

        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.selectedCheat.collect { onSelectedCheatChanged(it) }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.isEditing.collect { onIsEditingChanged(it) }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.openDetailsViewEvent.collect { openDetailsView(it) }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.closeDetailsViewEvent.collect { closeDetailsView(it) }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.listViewFocusChange.collect { onListViewFocusChange(it) }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.detailsViewFocusChange.collect { onDetailsViewFocusChange(it) }
                }
            }
        }

        setInsets()
    }

    override fun onStop() {
        super.onStop()
        cheatsViewModel.saveIfNeeded()
    }

    override fun onDestroy() {
        super.onDestroy()
        _binding = null
    }

    override fun onPanelSlide(panel: View, slideOffset: Float) {}
    override fun onPanelOpened(panel: View) {
        val rtl = ViewCompat.getLayoutDirection(panel) == ViewCompat.LAYOUT_DIRECTION_RTL
        cheatDetailsLastFocus!!.requestFocus(if (rtl) View.FOCUS_LEFT else View.FOCUS_RIGHT)
    }

    override fun onPanelClosed(panel: View) {
        val rtl = ViewCompat.getLayoutDirection(panel) == ViewCompat.LAYOUT_DIRECTION_RTL
        cheatListLastFocus!!.requestFocus(if (rtl) View.FOCUS_RIGHT else View.FOCUS_LEFT)
    }

    private fun onIsEditingChanged(isEditing: Boolean) {
        if (isEditing) {
            binding.slidingPaneLayout.lockMode = SlidingPaneLayout.LOCK_MODE_UNLOCKED
        }
    }

    private fun onSelectedCheatChanged(selectedCheat: Cheat?) {
        val cheatSelected = selectedCheat != null || cheatsViewModel.isEditing.value!!
        if (!cheatSelected && binding.slidingPaneLayout.isOpen) {
            binding.slidingPaneLayout.close()
        }
        binding.slidingPaneLayout.lockMode =
            if (cheatSelected) SlidingPaneLayout.LOCK_MODE_UNLOCKED else SlidingPaneLayout.LOCK_MODE_LOCKED_CLOSED
    }

    fun onListViewFocusChange(hasFocus: Boolean) {
        if (hasFocus) {
            cheatListLastFocus = binding.cheatListContainer.findFocus()
            if (cheatListLastFocus == null) throw NullPointerException()
            binding.slidingPaneLayout.close()
        }
    }

    fun onDetailsViewFocusChange(hasFocus: Boolean) {
        if (hasFocus) {
            cheatDetailsLastFocus = binding.cheatDetailsContainer.findFocus()
            if (cheatDetailsLastFocus == null) {
                throw NullPointerException()
            }
            binding.slidingPaneLayout.open()
        }
    }

    private fun openDetailsView(open: Boolean) {
        if (open) {
            binding.slidingPaneLayout.open()
        }
    }

    private fun closeDetailsView(close: Boolean) {
        if (close) {
            binding.slidingPaneLayout.close()
        }
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.slidingPaneLayout
        ) { _: View?, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val keyboardInsets = windowInsets.getInsets(WindowInsetsCompat.Type.ime())

            // Set keyboard insets if the system supports smooth keyboard animations
            val mlpDetails = binding.cheatDetailsContainer.layoutParams as ViewGroup.MarginLayoutParams
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
                if (keyboardInsets.bottom > 0) {
                    mlpDetails.bottomMargin = keyboardInsets.bottom
                } else {
                    mlpDetails.bottomMargin = barInsets.bottom
                }
            } else {
                if (mlpDetails.bottomMargin == 0) {
                    mlpDetails.bottomMargin = barInsets.bottom
                }
            }
            binding.cheatDetailsContainer.layoutParams = mlpDetails
            windowInsets
        }

        // Update the layout for every frame that the keyboard animates in
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            ViewCompat.setWindowInsetsAnimationCallback(
                binding.cheatDetailsContainer,
                object : WindowInsetsAnimationCompat.Callback(DISPATCH_MODE_STOP) {
                    var keyboardInsets = 0
                    var barInsets = 0
                    override fun onProgress(
                        insets: WindowInsetsCompat,
                        runningAnimations: List<WindowInsetsAnimationCompat>
                    ): WindowInsetsCompat {
                        val mlpDetails =
                            binding.cheatDetailsContainer.layoutParams as ViewGroup.MarginLayoutParams
                        keyboardInsets = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom
                        barInsets = insets.getInsets(WindowInsetsCompat.Type.systemBars()).bottom
                        mlpDetails.bottomMargin = keyboardInsets.coerceAtLeast(barInsets)
                        binding.cheatDetailsContainer.layoutParams = mlpDetails
                        return insets
                    }
                })
        }
    }
}

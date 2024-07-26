// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.content.res.Resources
import android.os.Bundle
import android.text.Html
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.findNavController
import androidx.preference.PreferenceManager
import com.google.android.material.textfield.MaterialAutoCompleteTextView
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.launch
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.HomeNavigationDirections
import io.github.lime3ds.android.NativeLibrary
import io.github.lime3ds.android.R
import io.github.lime3ds.android.activities.EmulationActivity
import io.github.lime3ds.android.databinding.FragmentSystemFilesBinding
import io.github.lime3ds.android.features.settings.model.Settings
import io.github.lime3ds.android.model.Game
import io.github.lime3ds.android.utils.SystemSaveGame
import io.github.lime3ds.android.viewmodel.GamesViewModel
import io.github.lime3ds.android.viewmodel.HomeViewModel
import io.github.lime3ds.android.viewmodel.SystemFilesViewModel

class SystemFilesFragment : Fragment() {
    private var _binding: FragmentSystemFilesBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()
    private val systemFilesViewModel: SystemFilesViewModel by activityViewModels()
    private val gamesViewModel: GamesViewModel by activityViewModels()

    private lateinit var regionValues: IntArray

    private val systemTypeDropdown = DropdownItem(R.array.systemFileTypeValues)
    private val systemRegionDropdown = DropdownItem(R.array.systemFileRegionValues)

    private val SYS_TYPE = "SysType"
    private val REGION = "Region"
    private val REGION_START = "RegionStart"

    private val homeMenuMap: MutableMap<String, String> = mutableMapOf()

    private val WARNING_SHOWN = "SystemFilesWarningShown"

    private class DropdownItem(val valuesId: Int) : AdapterView.OnItemClickListener {
        var position = 0

        fun getValue(resources: Resources): Int {
            return resources.getIntArray(valuesId)[position]
        }

        override fun onItemClick(p0: AdapterView<*>?, view: View?, position: Int, id: Long) {
            this.position = position
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        SystemSaveGame.load()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSystemFilesBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        homeViewModel.setNavigationVisibility(visible = false, animated = true)
        homeViewModel.setStatusBarShadeVisibility(visible = false)

        val preferences = PreferenceManager.getDefaultSharedPreferences(LimeApplication.appContext)
        if (!preferences.getBoolean(WARNING_SHOWN, false)) {
            MessageDialogFragment.newInstance(
                R.string.home_menu_warning,
                R.string.home_menu_warning_description
            ).show(childFragmentManager, MessageDialogFragment.TAG)
            preferences.edit()
                .putBoolean(WARNING_SHOWN, true)
                .apply()
        }

        binding.toolbarSystemFiles.setNavigationOnClickListener {
            binding.root.findNavController().popBackStack()
        }

        // TODO: Remove workaround for text filtering issue in material components when fixed
        // https://github.com/material-components/material-components-android/issues/1464
        binding.dropdownSystemType.isSaveEnabled = false
        binding.dropdownSystemRegion.isSaveEnabled = false
        binding.dropdownSystemRegionStart.isSaveEnabled = false

        viewLifecycleOwner.lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.CREATED) {
                systemFilesViewModel.shouldRefresh.collect {
                    if (it) {
                        reloadUi()
                        systemFilesViewModel.setShouldRefresh(false)
                    }
                }
            }
        }

        reloadUi()
        if (savedInstanceState != null) {
            setDropdownSelection(
                binding.dropdownSystemType,
                systemTypeDropdown,
                savedInstanceState.getInt(SYS_TYPE)
            )
            setDropdownSelection(
                binding.dropdownSystemRegion,
                systemRegionDropdown,
                savedInstanceState.getInt(REGION)
            )
            binding.dropdownSystemRegionStart
                .setText(savedInstanceState.getString(REGION_START), false)
        }

        setInsets()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putInt(SYS_TYPE, systemTypeDropdown.position)
        outState.putInt(REGION, systemRegionDropdown.position)
        outState.putString(REGION_START, binding.dropdownSystemRegionStart.text.toString())
    }

    override fun onPause() {
        super.onPause()
        SystemSaveGame.save()
    }

    private fun reloadUi() {
        val preferences = PreferenceManager.getDefaultSharedPreferences(LimeApplication.appContext)

        binding.switchRunSystemSetup.isChecked = SystemSaveGame.getIsSystemSetupNeeded()
        binding.switchRunSystemSetup.setOnCheckedChangeListener { _, isChecked ->
            SystemSaveGame.setSystemSetupNeeded(isChecked)
        }

        val showHomeApps = preferences.getBoolean(Settings.PREF_SHOW_HOME_APPS, false)
        binding.switchShowApps.isChecked = showHomeApps
        binding.switchShowApps.setOnCheckedChangeListener { _, isChecked ->
            preferences.edit()
                .putBoolean(Settings.PREF_SHOW_HOME_APPS, isChecked)
                .apply()
            gamesViewModel.setShouldSwapData(true)
        }

        if (!NativeLibrary.areKeysAvailable()) {
            binding.apply {
                systemType.isEnabled = false
                systemRegion.isEnabled = false
                buttonDownloadHomeMenu.isEnabled = false
                textKeysMissing.visibility = View.VISIBLE
                textKeysMissingHelp.visibility = View.VISIBLE
                textKeysMissingHelp.text =
                    Html.fromHtml(getString(R.string.how_to_get_keys), Html.FROM_HTML_MODE_LEGACY)
                textKeysMissingHelp.movementMethod = LinkMovementMethod.getInstance()
            }
        } else {
            populateDownloadOptions()
        }

        binding.buttonDownloadHomeMenu.setOnClickListener {
            val titleIds = NativeLibrary.getSystemTitleIds(
                systemTypeDropdown.getValue(resources),
                systemRegionDropdown.getValue(resources)
            )

            DownloadSystemFilesDialogFragment.newInstance(titleIds).show(
                childFragmentManager,
                DownloadSystemFilesDialogFragment.TAG
            )
        }

        populateHomeMenuOptions()
        binding.buttonStartHomeMenu.setOnClickListener {
            val menuPath = homeMenuMap[binding.dropdownSystemRegionStart.text.toString()]!!
            val menu = Game(
                title = getString(R.string.home_menu),
                path = menuPath,
                filename = ""
            )
            val action = HomeNavigationDirections.actionGlobalEmulationActivity(menu)
            binding.root.findNavController().navigate(action)
        }
    }

    private fun populateDropdown(
        dropdown: MaterialAutoCompleteTextView,
        valuesId: Int,
        dropdownItem: DropdownItem
    ) {
        val valuesAdapter = ArrayAdapter.createFromResource(
            requireContext(),
            valuesId,
            R.layout.support_simple_spinner_dropdown_item
        )
        dropdown.setAdapter(valuesAdapter)
        dropdown.onItemClickListener = dropdownItem
    }

    private fun setDropdownSelection(
        dropdown: MaterialAutoCompleteTextView,
        dropdownItem: DropdownItem,
        selection: Int
    ) {
        if (dropdown.adapter != null) {
            dropdown.setText(dropdown.adapter.getItem(selection).toString(), false)
        }
        dropdownItem.position = selection
    }

    private fun populateDownloadOptions() {
        populateDropdown(binding.dropdownSystemType, R.array.systemFileTypes, systemTypeDropdown)
        populateDropdown(
            binding.dropdownSystemRegion,
            R.array.systemFileRegions,
            systemRegionDropdown
        )

        setDropdownSelection(
            binding.dropdownSystemType,
            systemTypeDropdown,
            systemTypeDropdown.position
        )
        setDropdownSelection(
            binding.dropdownSystemRegion,
            systemRegionDropdown,
            systemRegionDropdown.position
        )
    }

    private fun populateHomeMenuOptions() {
        regionValues = resources.getIntArray(R.array.systemFileRegionValues)
        val regionEntries = resources.getStringArray(R.array.systemFileRegions)
        regionValues.forEachIndexed { i: Int, region: Int ->
            val regionString = regionEntries[i]
            val regionPath = NativeLibrary.getHomeMenuPath(region)
            homeMenuMap[regionString] = regionPath
        }

        val availableMenus = homeMenuMap.filter { it.value != "" }
        if (availableMenus.isNotEmpty()) {
            binding.systemRegionStart.isEnabled = true
            binding.buttonStartHomeMenu.isEnabled = true

            binding.dropdownSystemRegionStart.setAdapter(
                ArrayAdapter(
                    requireContext(),
                    R.layout.support_simple_spinner_dropdown_item,
                    availableMenus.keys.toList()
                )
            )
            binding.dropdownSystemRegionStart.setText(availableMenus.keys.first(), false)
        }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val mlpAppBar = binding.toolbarSystemFiles.layoutParams as ViewGroup.MarginLayoutParams
            mlpAppBar.leftMargin = leftInsets
            mlpAppBar.rightMargin = rightInsets
            binding.toolbarSystemFiles.layoutParams = mlpAppBar

            val mlpScrollSystemFiles =
                binding.scrollSystemFiles.layoutParams as ViewGroup.MarginLayoutParams
            mlpScrollSystemFiles.leftMargin = leftInsets
            mlpScrollSystemFiles.rightMargin = rightInsets
            binding.scrollSystemFiles.layoutParams = mlpScrollSystemFiles

            binding.scrollSystemFiles.updatePadding(bottom = barInsets.bottom)

            windowInsets
        }
}

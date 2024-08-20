// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.Manifest
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.findNavController
import androidx.preference.PreferenceManager
import androidx.viewpager2.widget.ViewPager2.OnPageChangeCallback
import com.google.android.material.snackbar.Snackbar
import com.google.android.material.transition.MaterialFadeThrough
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.R
import io.github.lime3ds.android.adapters.SetupAdapter
import io.github.lime3ds.android.databinding.FragmentSetupBinding
import io.github.lime3ds.android.features.settings.model.Settings
import io.github.lime3ds.android.model.SetupCallback
import io.github.lime3ds.android.model.SetupPage
import io.github.lime3ds.android.model.StepState
import io.github.lime3ds.android.ui.main.MainActivity
import io.github.lime3ds.android.utils.Lime3DSDirectoryHelper
import io.github.lime3ds.android.utils.GameHelper
import io.github.lime3ds.android.utils.PermissionsHandler
import io.github.lime3ds.android.utils.ViewUtils
import io.github.lime3ds.android.viewmodel.GamesViewModel
import io.github.lime3ds.android.viewmodel.HomeViewModel

class SetupFragment : Fragment() {
    private var _binding: FragmentSetupBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()
    private val gamesViewModel: GamesViewModel by activityViewModels()

    private lateinit var mainActivity: MainActivity

    private lateinit var hasBeenWarned: BooleanArray

    private lateinit var pages: MutableList<SetupPage>

    private val preferences: SharedPreferences
        get() = PreferenceManager.getDefaultSharedPreferences(LimeApplication.appContext)

    companion object {
        const val KEY_NEXT_VISIBILITY = "NextButtonVisibility"
        const val KEY_BACK_VISIBILITY = "BackButtonVisibility"
        const val KEY_HAS_BEEN_WARNED = "HasBeenWarned"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        exitTransition = MaterialFadeThrough()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSetupBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        mainActivity = requireActivity() as MainActivity

        homeViewModel.setNavigationVisibility(visible = false, animated = false)

        requireActivity().onBackPressedDispatcher.addCallback(
            viewLifecycleOwner,
            object : OnBackPressedCallback(true) {
                override fun handleOnBackPressed() {
                    if (binding.viewPager2.currentItem > 0) {
                        pageBackward()
                    } else {
                        requireActivity().finish()
                    }
                }
            }
        )

        requireActivity().window.navigationBarColor =
            ContextCompat.getColor(requireContext(), android.R.color.transparent)

        pages = mutableListOf()
        pages.apply {
            add(
                SetupPage(
                    R.drawable.ic_lime_full,
                    R.string.welcome,
                    R.string.welcome_description,
                    0,
                    true,
                    R.string.get_started,
                    { pageForward() }
                )
            )

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                add(
                    SetupPage(
                        R.drawable.ic_notification,
                        R.string.notifications,
                        R.string.notifications_description,
                        0,
                        false,
                        R.string.give_permission,
                        {
                            notificationCallback = it
                            permissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
                        },
                        false,
                        true,
                        {
                            if (NotificationManagerCompat.from(requireContext())
                                    .areNotificationsEnabled()
                            ) {
                                StepState.STEP_COMPLETE
                            } else {
                                StepState.STEP_INCOMPLETE
                            }
                        },
                        R.string.notification_warning,
                        R.string.notification_warning_description,
                        0
                    )
                )
            }

            add(
                SetupPage(
                    R.drawable.ic_microphone,
                    R.string.microphone_permission,
                    R.string.microphone_permission_description,
                    0,
                    false,
                    R.string.give_permission,
                    {
                        microphoneCallback = it
                        permissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
                    },
                    false,
                    false,
                    {
                        if (
                            ContextCompat.checkSelfPermission(
                                requireContext(),
                                Manifest.permission.RECORD_AUDIO
                            ) == PackageManager.PERMISSION_GRANTED
                        ) {
                            StepState.STEP_COMPLETE
                        } else {
                            StepState.STEP_INCOMPLETE
                        }
                    }
                )
            )
            add(
                SetupPage(
                    R.drawable.ic_camera,
                    R.string.camera_permission,
                    R.string.camera_permission_description,
                    0,
                    false,
                    R.string.give_permission,
                    {
                        cameraCallback = it
                        permissionLauncher.launch(Manifest.permission.CAMERA)
                    },
                    false,
                    false,
                    {
                        if (
                            ContextCompat.checkSelfPermission(
                                requireContext(),
                                Manifest.permission.CAMERA
                            ) == PackageManager.PERMISSION_GRANTED
                        ) {
                            StepState.STEP_COMPLETE
                        } else {
                            StepState.STEP_INCOMPLETE
                        }
                    }
                )
            )
            add(
                SetupPage(
                    R.drawable.ic_home,
                    R.string.select_lime3ds_user_folder,
                    R.string.select_lime3ds_user_folder_description,
                    0,
                    true,
                    R.string.select,
                    {
                        userDirCallback = it
                        openLime3DSDirectory.launch(null)
                    },
                    true,
                    true,
                    {
                        if (PermissionsHandler.hasWriteAccess(requireContext())) {
                            StepState.STEP_COMPLETE
                        } else {
                            StepState.STEP_INCOMPLETE
                        }
                    },
                    R.string.cannot_skip,
                    R.string.cannot_skip_directory_description,
                    R.string.cannot_skip_directory_help
                )
            )
            add(
                SetupPage(
                    R.drawable.ic_controller,
                    R.string.games,
                    R.string.games_description,
                    R.drawable.ic_add,
                    true,
                    R.string.add_games,
                    {
                        gamesDirCallback = it
                        getGamesDirectory.launch(
                            Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).data
                        )
                    },
                    false,
                    true,
                    {
                        if (preferences.getString(GameHelper.KEY_GAME_PATH, "")!!.isNotEmpty()) {
                            StepState.STEP_COMPLETE
                        } else {
                            StepState.STEP_INCOMPLETE
                        }
                    },
                    R.string.add_games_warning,
                    R.string.add_games_warning_description,
                    R.string.add_games_warning_help
                )
            )
            add(
                SetupPage(
                    R.drawable.ic_check,
                    R.string.done,
                    R.string.done_description,
                    R.drawable.ic_arrow_forward,
                    false,
                    R.string.text_continue,
                    { finishSetup() }
                )
            )
        }

        binding.viewPager2.apply {
            adapter = SetupAdapter(requireActivity() as AppCompatActivity, pages)
            offscreenPageLimit = 2
            isUserInputEnabled = false
        }

        binding.viewPager2.registerOnPageChangeCallback(object : OnPageChangeCallback() {
            var previousPosition: Int = 0

            override fun onPageSelected(position: Int) {
                super.onPageSelected(position)

                if (position == 1 && previousPosition == 0) {
                    ViewUtils.showView(binding.buttonNext)
                    ViewUtils.showView(binding.buttonBack)
                } else if (position == 0 && previousPosition == 1) {
                    ViewUtils.hideView(binding.buttonBack)
                    ViewUtils.hideView(binding.buttonNext)
                } else if (position == pages.size - 1 && previousPosition == pages.size - 2) {
                    ViewUtils.hideView(binding.buttonNext)
                } else if (position == pages.size - 2 && previousPosition == pages.size - 1) {
                    ViewUtils.showView(binding.buttonNext)
                }

                previousPosition = position
            }
        })

        binding.buttonNext.setOnClickListener {
            val index = binding.viewPager2.currentItem
            val currentPage = pages[index]

            // Checks if the user has completed the task on the current page
            if (currentPage.hasWarning || currentPage.isUnskippable) {
                val stepState = currentPage.stepCompleted.invoke()
                if (stepState == StepState.STEP_COMPLETE ||
                    stepState == StepState.STEP_UNDEFINED
                ) {
                    pageForward()
                    return@setOnClickListener
                }

                if (currentPage.isUnskippable) {
                    MessageDialogFragment.newInstance(
                        currentPage.warningTitleId,
                        currentPage.warningDescriptionId,
                        currentPage.warningHelpLinkId
                    ).show(childFragmentManager, MessageDialogFragment.TAG)
                    return@setOnClickListener
                }

                if (!hasBeenWarned[index]) {
                    SetupWarningDialogFragment.newInstance(
                        currentPage.warningTitleId,
                        currentPage.warningDescriptionId,
                        currentPage.warningHelpLinkId,
                        index
                    ).show(childFragmentManager, SetupWarningDialogFragment.TAG)
                    return@setOnClickListener
                }
            }
            pageForward()
        }
        binding.buttonBack.setOnClickListener { pageBackward() }

        if (savedInstanceState != null) {
            val nextIsVisible = savedInstanceState.getBoolean(KEY_NEXT_VISIBILITY)
            val backIsVisible = savedInstanceState.getBoolean(KEY_BACK_VISIBILITY)
            hasBeenWarned = savedInstanceState.getBooleanArray(KEY_HAS_BEEN_WARNED)!!

            if (nextIsVisible) {
                binding.buttonNext.visibility = View.VISIBLE
            }
            if (backIsVisible) {
                binding.buttonBack.visibility = View.VISIBLE
            }
        } else {
            hasBeenWarned = BooleanArray(pages.size)
        }

        setInsets()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putBoolean(KEY_NEXT_VISIBILITY, binding.buttonNext.isVisible)
        outState.putBoolean(KEY_BACK_VISIBILITY, binding.buttonBack.isVisible)
        outState.putBooleanArray(KEY_HAS_BEEN_WARNED, hasBeenWarned)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private lateinit var notificationCallback: SetupCallback
    private lateinit var microphoneCallback: SetupCallback
    private lateinit var cameraCallback: SetupCallback

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { isGranted ->
            if (isGranted) {
                val page = pages[binding.viewPager2.currentItem]
                when (page.titleId) {
                    R.string.notifications -> notificationCallback.onStepCompleted()
                    R.string.microphone_permission -> microphoneCallback.onStepCompleted()
                    R.string.camera_permission -> cameraCallback.onStepCompleted()
                }
                return@registerForActivityResult
            }

            Snackbar.make(binding.root, R.string.permission_denied, Snackbar.LENGTH_LONG)
                .setAnchorView(binding.buttonNext)
                .setAction(R.string.grid_menu_core_settings) {
                    val intent =
                        Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS)
                    val uri = Uri.fromParts("package", requireActivity().packageName, null)
                    intent.data = uri
                    startActivity(intent)
                }
                .show()
        }

    private lateinit var userDirCallback: SetupCallback

    private val openLime3DSDirectory = registerForActivityResult<Uri, Uri>(
        ActivityResultContracts.OpenDocumentTree()
    ) { result: Uri? ->
        if (result == null) {
            return@registerForActivityResult
        }

        Lime3DSDirectoryHelper(requireActivity()).showLime3DSDirectoryDialog(result, userDirCallback)
    }

    private lateinit var gamesDirCallback: SetupCallback

    private val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result == null) {
                return@registerForActivityResult
            }

            requireActivity().contentResolver.takePersistableUriPermission(
                result,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )

            // When a new directory is picked, we currently will reset the existing games
            // database. This effectively means that only one game directory is supported.
            preferences.edit()
                .putString(GameHelper.KEY_GAME_PATH, result.toString())
                .apply()

            homeViewModel.setGamesDir(requireActivity(), result.path!!)

            gamesDirCallback.onStepCompleted()
        }

    private fun finishSetup() {
        preferences.edit()
            .putBoolean(Settings.PREF_FIRST_APP_LAUNCH, false)
            .apply()
        mainActivity.finishSetup(binding.root.findNavController())
    }

    fun pageForward() {
        binding.viewPager2.currentItem = binding.viewPager2.currentItem + 1
    }

    fun pageBackward() {
        binding.viewPager2.currentItem = binding.viewPager2.currentItem - 1
    }

    fun setPageWarned(page: Int) {
        hasBeenWarned[page] = true
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftPadding = barInsets.left + cutoutInsets.left
            val topPadding = barInsets.top + cutoutInsets.top
            val rightPadding = barInsets.right + cutoutInsets.right
            val bottomPadding = barInsets.bottom + cutoutInsets.bottom

            if (resources.getBoolean(R.bool.small_layout)) {
                binding.viewPager2
                    .updatePadding(left = leftPadding, top = topPadding, right = rightPadding)
                binding.constraintButtons
                    .updatePadding(left = leftPadding, right = rightPadding, bottom = bottomPadding)
            } else {
                binding.viewPager2.updatePadding(top = topPadding, bottom = bottomPadding)
                binding.constraintButtons
                    .setPadding(
                        leftPadding + rightPadding,
                        topPadding,
                        rightPadding + leftPadding,
                        bottomPadding
                    )
            }
            windowInsets
        }
}

// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.adapters

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Icon
import android.net.Uri
import android.os.SystemClock
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.PopupMenu
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.ActivityResultLauncher
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.edit
import androidx.core.graphics.scale
import androidx.core.net.toUri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.navigation.findNavController
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import com.google.android.material.button.MaterialButton
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.HomeNavigationDirections
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.adapters.GameAdapter.GameViewHolder
import org.citra.citra_emu.databinding.CardGameBinding
import org.citra.citra_emu.databinding.DialogShortcutBinding
import org.citra.citra_emu.features.cheats.ui.CheatsFragmentDirections
import org.citra.citra_emu.fragments.IndeterminateProgressDialogFragment
import org.citra.citra_emu.model.Game
import org.citra.citra_emu.utils.BuildUtil
import org.citra.citra_emu.utils.FileUtil
import org.citra.citra_emu.utils.GameIconUtils
import org.citra.citra_emu.utils.Log
import org.citra.citra_emu.viewmodel.GamesViewModel

class GameAdapter(
    private val activity: AppCompatActivity,
    private val inflater: LayoutInflater,
    private val openImageLauncher: ActivityResultLauncher<String>?,
    private val onRequestCompressOrDecompress: (
        (inputPath: String, suggestedName: String, shouldCompress: Boolean) -> Unit
    )? = null
) : ListAdapter<Game, GameViewHolder>(AsyncDifferConfig.Builder(DiffCallback()).build()),
    View.OnClickListener,
    View.OnLongClickListener {
    private var lastClickTime = 0L
    private var imagePath: String? = null
    private var dialogShortcutBinding: DialogShortcutBinding? = null

    private val preferences: SharedPreferences
        get() = PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)

    fun handleShortcutImageResult(uri: Uri?) {
        val path = uri?.toString()
        if (path != null) {
            imagePath = path
            dialogShortcutBinding!!.imageScaleSwitch.isEnabled = imagePath != null
            refreshShortcutDialogIcon()
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        // Create a new view.
        val binding = CardGameBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        binding.cardGame.setOnClickListener(this)
        binding.cardGame.setOnLongClickListener(this)

        // Use that view to create a ViewHolder.
        return GameViewHolder(binding)
    }

    override fun onBindViewHolder(holder: GameViewHolder, position: Int) {
        holder.bind(currentList[position])
    }

    override fun getItemCount(): Int = currentList.size

    /**
     * Launches the game that was clicked on.
     *
     * @param view The card representing the game the user wants to play.
     */
    override fun onClick(view: View) {
        // Double-click prevention, using threshold of 1000 ms
        if (SystemClock.elapsedRealtime() - lastClickTime < 1000) {
            return
        }
        lastClickTime = SystemClock.elapsedRealtime()

        val holder = view.tag as GameViewHolder
        gameExists(holder)

        val preferences =
            PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)
        preferences.edit()
            .putLong(
                holder.game.keyLastPlayedTime,
                System.currentTimeMillis()
            )
            .apply()

        val action = HomeNavigationDirections.actionGlobalEmulationActivity(holder.game)
        view.findNavController().navigate(action)
    }

    /**
     * Opens the about game dialog for the game that was clicked on.
     *
     * @param view The view representing the game the user wants to play.
     */
    override fun onLongClick(view: View): Boolean {
        val context = view.context
        val holder = view.tag as GameViewHolder
        gameExists(holder)

        if (!holder.game.valid) {
            MaterialAlertDialogBuilder(context)
                .setTitle(R.string.properties)
                .setMessage(R.string.properties_not_loaded)
                .setPositiveButton(android.R.string.ok, null)
                .show()
        } else {
            showAboutGameDialog(context, holder.game, holder, view)
        }
        return true
    }

    // Triggers a library refresh if the user clicks on stale data
    private fun gameExists(holder: GameViewHolder): Boolean {
        if (holder.game.isInstalled) {
            return true
        }
        val path = holder.game.path
        val pathUri = path.toUri()
        var gameExists: Boolean
        if (BuildUtil.isGooglePlayBuild || FileUtil.isNativePath(path)) {
            gameExists =
                DocumentFile.fromSingleUri(
                    CitraApplication.appContext,
                    pathUri
                )?.exists() == true
        } else {
            val nativePath = NativeLibrary.getNativePath(pathUri)
            gameExists = NativeLibrary.nativeFileExists(nativePath)
        }
        return if (!gameExists) {
            Log.error("[GameAdapter] ROM file does not exist: $path")
            Toast.makeText(
                CitraApplication.appContext,
                R.string.loader_error_file_not_found,
                Toast.LENGTH_LONG
            ).show()

            ViewModelProvider(activity)[GamesViewModel::class.java].reloadGames(true)
            false
        } else {
            true
        }
    }

    inner class GameViewHolder(val binding: CardGameBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var game: Game

        init {
            binding.cardGame.tag = this
        }

        fun bind(game: Game) {
            this.game = game

            binding.imageGameScreen.scaleType = ImageView.ScaleType.CENTER_CROP
            GameIconUtils.loadGameIcon(activity, game, binding.imageGameScreen)

            binding.textCompany.visibility = if (game.company.isEmpty()) {
                View.GONE
            } else {
                View.VISIBLE
            }
            binding.textGameRegion.visibility = if (game.regions.isEmpty()) {
                View.GONE
            } else {
                View.VISIBLE
            }

            binding.textGameTitle.text = if (game.fileType == "unknown") {
                CitraApplication.appContext.getString(R.string.invalid_rom)
            } else {
                game.title
            }
            binding.textCompany.text = game.company
            binding.textGameRegion.text = translateRegions(game.regions)
            binding.imageCartridge.visibility =
                if (preferences.getString("insertedCartridge", "") != game.path) {
                    View.GONE
                } else {
                    View.VISIBLE
                }

            binding.cardContents.setBackgroundColor(
                MaterialColors.getColor(
                    binding.cardContents,
                    R.attr.colorSurface
                )
            )

            binding.textGameTitle.postDelayed(
                {
                    binding.textGameTitle.ellipsize = TextUtils.TruncateAt.MARQUEE
                    binding.textGameTitle.isSelected = true

                    binding.textCompany.ellipsize = TextUtils.TruncateAt.MARQUEE
                    binding.textCompany.isSelected = true

                    binding.textGameRegion.ellipsize = TextUtils.TruncateAt.MARQUEE
                    binding.textGameRegion.isSelected = true
                },
                3000
            )
        }
    }

    private data class GameDirectories(
        val gameDir: String,
        val saveDir: String,
        val modsDir: String,
        val texturesDir: String,
        val appDir: String,
        val dlcDir: String,
        val updatesDir: String,
        val extraDir: String
    )
    private fun getGameDirectories(game: Game): GameDirectories {
        val basePath =
            "sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000"
        return GameDirectories(
            gameDir = game.path.substringBeforeLast("/"),
            saveDir =
                basePath +
                    "/title/${String.format(
                        "%016x",
                        game.titleId
                    ).lowercase().substring(
                        0,
                        8
                    )}/${String.format(
                        "%016x",
                        game.titleId
                    ).lowercase().substring(8)}/data/00000001",
            modsDir = "load/mods/${String.format("%016X", game.titleId)}",
            texturesDir = "load/textures/${String.format("%016X", game.titleId)}",
            appDir = game.path.substringBeforeLast("/").split("/").filter {
                it.isNotEmpty()
            }.joinToString("/"),
            dlcDir =
                basePath +
                    "/title/0004008c/${String.format(
                        "%016x",
                        game.titleId
                    ).lowercase().substring(8)}/content",
            updatesDir =
                basePath +
                    "/title/0004000e/${String.format(
                        "%016x",
                        game.titleId
                    ).lowercase().substring(8)}/content",
            extraDir =
                basePath +
                    "/extdata/00000000/${String.format(
                        "%016X",
                        game.titleId
                    ).substring(8, 14).padStart(8, '0')}"
        )
    }

    private fun showOpenContextMenu(view: View, game: Game) {
        val dirs = getGameDirectories(game)

        val popup = PopupMenu(view.context, view).apply {
            menuInflater.inflate(R.menu.game_context_menu_open, menu)
            listOf(
                R.id.game_context_open_app to dirs.appDir,
                R.id.game_context_open_save_dir to dirs.saveDir,
                R.id.game_context_open_updates to dirs.updatesDir,
                R.id.game_context_open_dlc to dirs.dlcDir,
                R.id.game_context_open_extra to dirs.extraDir
            ).forEach { (id, dir) ->
                menu.findItem(id)?.isEnabled =
                    CitraApplication.documentsTree.folderUriHelper(dir)?.let {
                        DocumentFile.fromTreeUri(view.context, it)?.exists()
                    } ?: false
            }
        }

        popup.setOnMenuItemClickListener { menuItem ->
            val intent = Intent(Intent.ACTION_VIEW)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .setType("*/*")

            val uri = when (menuItem.itemId) {
                R.id.game_context_open_app -> CitraApplication.documentsTree.folderUriHelper(
                    dirs.appDir
                )

                R.id.game_context_open_save_dir -> CitraApplication.documentsTree.folderUriHelper(
                    dirs.saveDir
                )

                R.id.game_context_open_updates -> CitraApplication.documentsTree.folderUriHelper(
                    dirs.updatesDir
                )

                R.id.game_context_open_dlc -> CitraApplication.documentsTree.folderUriHelper(
                    dirs.dlcDir
                )

                R.id.game_context_open_extra -> CitraApplication.documentsTree.folderUriHelper(
                    dirs.extraDir
                )

                R.id.game_context_open_textures -> CitraApplication.documentsTree.folderUriHelper(
                    dirs.texturesDir,
                    true
                )

                R.id.game_context_open_mods -> CitraApplication.documentsTree.folderUriHelper(
                    dirs.modsDir,
                    true
                )

                else -> null
            }

            uri?.let {
                intent.data = it
                view.context.startActivity(intent)
                true
            } ?: false
        }

        popup.show()
    }

    private fun showUninstallContextMenu(
        view: View,
        game: Game,
        bottomSheetDialog: BottomSheetDialog
    ) {
        val dirs = getGameDirectories(game)
        val popup = PopupMenu(view.context, view).apply {
            menuInflater.inflate(R.menu.game_context_menu_uninstall, menu)
            listOf(
                R.id.game_context_uninstall to dirs.gameDir,
                R.id.game_context_uninstall_dlc to dirs.dlcDir,
                R.id.game_context_uninstall_updates to dirs.updatesDir
            ).forEach { (id, dir) ->
                menu.findItem(id)?.isEnabled =
                    CitraApplication.documentsTree.folderUriHelper(dir)?.let {
                        DocumentFile.fromTreeUri(view.context, it)?.exists()
                    } ?: false
            }
        }

        val titleId = game.titleId
        val dlcTitleId = titleId or 0x8C00000000L
        val updateTitleId = titleId or 0xE00000000L

        popup.setOnMenuItemClickListener { menuItem ->
            val uninstallAction: () -> Unit = {
                when (menuItem.itemId) {
                    R.id.game_context_uninstall -> NativeLibrary.uninstallTitle(
                        titleId,
                        game.mediaType
                    )

                    R.id.game_context_uninstall_dlc -> NativeLibrary.uninstallTitle(
                        dlcTitleId,
                        Game.MediaType.SDMC
                    )

                    R.id.game_context_uninstall_updates -> NativeLibrary.uninstallTitle(
                        updateTitleId,
                        Game.MediaType.SDMC
                    )
                }
                ViewModelProvider(activity)[GamesViewModel::class.java].reloadGames(true)
                bottomSheetDialog.dismiss()
            }

            if (menuItem.itemId in
                listOf(
                    R.id.game_context_uninstall,
                    R.id.game_context_uninstall_dlc,
                    R.id.game_context_uninstall_updates
                )
            ) {
                IndeterminateProgressDialogFragment.newInstance(
                    activity,
                    R.string.uninstalling,
                    false,
                    uninstallAction
                )
                    .show(activity.supportFragmentManager, IndeterminateProgressDialogFragment.TAG)
                true
            } else {
                false
            }
        }

        popup.show()
    }

    private fun translateRegions(regionsString: String): String {
        val regions = regionsString.split("|")

        var final = ""

        regions.forEach {
            var res: Int = -1

            when (it) {
                "Japan" -> res = R.string.japan

                "North America" -> res = R.string.north_america

                "Europe" -> res = R.string.europe

                "Australia" -> res = R.string.australia

                "China" -> res = R.string.china

                "Korea" -> res = R.string.korea

                "Taiwan" -> res = R.string.taiwan

                "Region free" -> res = R.string.region_free

                "Invalid region" -> res = R.string.invalid_region

                "" -> res = R.string.invalid_region

                else -> {
                    Log.error("[GameAdapter] Unrecognized region string \"$it\"")
                    res = R.string.region_get_error
                }
            }

            final += CitraApplication.appContext.getString(res) + "|"
        }

        final = final
            .dropLast(1) // Remove trailing seperator
            .replace("|", ", ")

        return final
    }

    private fun showAboutGameDialog(
        context: Context,
        game: Game,
        holder: GameViewHolder,
        view: View
    ) {
        val bottomSheetView = inflater.inflate(R.layout.dialog_about_game, null)

        val bottomSheetDialog = BottomSheetDialog(context)
        bottomSheetDialog.setContentView(bottomSheetView)

        val insertable = game.isInsertable
        val inserted = insertable && (preferences.getString("insertedCartridge", "") == game.path)

        bottomSheetView.findViewById<TextView>(R.id.about_game_title).text = game.title
        bottomSheetView.findViewById<TextView>(R.id.about_game_company).text = game.company
        bottomSheetView.findViewById<TextView>(R.id.about_game_region).text =
            context.getString(R.string.game_context_region) + " " + translateRegions(game.regions)
        bottomSheetView.findViewById<TextView>(R.id.about_game_id).text =
            context.getString(R.string.game_context_id) + " " + String.format("%016X", game.titleId)
        bottomSheetView.findViewById<TextView>(R.id.about_game_filename).text =
            context.getString(R.string.game_context_file) + " " + game.filename
        bottomSheetView.findViewById<TextView>(R.id.about_game_filetype).text =
            context.getString(R.string.game_context_type) + " " + game.fileType

        val insertButton = bottomSheetView.findViewById<MaterialButton>(
            R.id.insert_cartridge_button
        )
        insertButton.text =
            if (inserted) {
                context.getString(R.string.game_context_eject)
            } else {
                context.getString(R.string.game_context_insert)
            }
        insertButton.visibility = if (insertable) View.VISIBLE else View.GONE
        insertButton.setOnClickListener {
            if (inserted) {
                preferences.edit().putString("insertedCartridge", "").apply()
            } else {
                preferences.edit().putString("insertedCartridge", game.path).apply()
            }
            bottomSheetDialog.dismiss()
            notifyItemRangeChanged(0, currentList.size)
        }

        GameIconUtils.loadGameIcon(activity, game, bottomSheetView.findViewById(R.id.game_icon))

        bottomSheetView.findViewById<MaterialButton>(R.id.about_game_play).setOnClickListener {
            val action = HomeNavigationDirections.actionGlobalEmulationActivity(holder.game)
            view.findNavController().navigate(action)
        }

        bottomSheetView.findViewById<TextView>(R.id.about_game_playtime).text =
            buildString {
                val playTimeSeconds = NativeLibrary.playTimeManagerGetPlayTime(game.titleId)

                val hours = playTimeSeconds / 3600
                val minutes = (playTimeSeconds % 3600) / 60
                val seconds = playTimeSeconds % 60

                val readablePlayTime = when {
                    hours > 0 -> "${hours}h ${minutes}m ${seconds}s"
                    minutes > 0 -> "${minutes}m ${seconds}s"
                    else -> "${seconds}s"
                }

                append(
                    context.getString(R.string.game_context_playtime) +
                        " " +
                        readablePlayTime
                )
            }

        bottomSheetView.findViewById<MaterialButton>(R.id.game_shortcut).setOnClickListener {
            val preferences = PreferenceManager.getDefaultSharedPreferences(context)

            // Default to false for zoomed in shortcut icons
            preferences.edit {
                putBoolean(
                    "shouldStretchIcon",
                    false
                )
            }

            dialogShortcutBinding = DialogShortcutBinding.inflate(activity.layoutInflater)

            dialogShortcutBinding!!.shortcutNameInput.setText(game.title)
            GameIconUtils.loadGameIcon(activity, game, dialogShortcutBinding!!.shortcutIcon)

            dialogShortcutBinding!!.shortcutIcon.setOnClickListener {
                openImageLauncher?.launch("image/*")
            }

            dialogShortcutBinding!!.imageScaleSwitch.setOnCheckedChangeListener { _, isChecked ->
                preferences.edit {
                    putBoolean(
                        "shouldStretchIcon",
                        isChecked
                    )
                }
                refreshShortcutDialogIcon()
            }

            MaterialAlertDialogBuilder(context)
                .setTitle(R.string.create_shortcut)
                .setView(dialogShortcutBinding!!.root)
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    val shortcutName = dialogShortcutBinding!!.shortcutNameInput.text.toString()
                    if (shortcutName.isEmpty()) {
                        Toast.makeText(
                            context,
                            R.string.shortcut_name_empty,
                            Toast.LENGTH_LONG
                        ).show()
                        return@setPositiveButton
                    }
                    val iconBitmap =
                        (dialogShortcutBinding!!.shortcutIcon.drawable as BitmapDrawable).bitmap
                    val shortcutManager = activity.getSystemService(ShortcutManager::class.java)

                    CoroutineScope(Dispatchers.IO).launch {
                        val icon = Icon.createWithBitmap(iconBitmap)
                        val shortcut = ShortcutInfo.Builder(context, shortcutName)
                            .setShortLabel(shortcutName)
                            .setIcon(icon)
                            .setIntent(
                                game.launchIntent.apply {
                                    putExtra("launchedFromShortcut", true)
                                }
                            )
                            .build()

                        shortcutManager?.requestPinShortcut(shortcut, null)
                        imagePath = null
                    }
                }
                .setNegativeButton(android.R.string.cancel) { _, _ ->
                    imagePath = null
                }
                .show()

            bottomSheetDialog.dismiss()
        }

        bottomSheetView.findViewById<MaterialButton>(R.id.cheats).setOnClickListener {
            val action = CheatsFragmentDirections.actionGlobalCheatsFragment(holder.game.titleId)
            view.findNavController().navigate(action)
            bottomSheetDialog.dismiss()
        }

        val compressDecompressButton = bottomSheetView.findViewById<MaterialButton>(
            R.id.compress_decompress
        )
        if (game.isInstalled) {
            compressDecompressButton.setOnClickListener {
                Toast.makeText(
                    context,
                    context.getString(R.string.compress_decompress_installed_app),
                    Toast.LENGTH_LONG
                ).show()
            }
            compressDecompressButton.alpha = 0.38f
        } else {
            compressDecompressButton.setOnClickListener {
                val shouldCompress = !game.isCompressed
                val recommendedExt = NativeLibrary.getRecommendedExtension(
                    holder.game.path,
                    shouldCompress
                )
                val baseName = holder.game.filename.substringBeforeLast('.')
                onRequestCompressOrDecompress?.invoke(
                    holder.game.path,
                    "$baseName.$recommendedExt",
                    shouldCompress
                )
                bottomSheetDialog.dismiss()
            }
        }
        compressDecompressButton.text =
            context.getString(if (!game.isCompressed) R.string.compress else R.string.decompress)

        bottomSheetView.findViewById<MaterialButton>(R.id.menu_button_open).setOnClickListener {
            showOpenContextMenu(it, game)
        }

        bottomSheetView.findViewById<MaterialButton>(
            R.id.menu_button_uninstall
        ).setOnClickListener {
            showUninstallContextMenu(it, game, bottomSheetDialog)
        }

        bottomSheetView.findViewById<MaterialButton>(R.id.delete_cache).setOnClickListener {
            val options =
                arrayOf(context.getString(R.string.vulkan), context.getString(R.string.opengles))
            var selectedIndex = -1
            val dialog = MaterialAlertDialogBuilder(context)
                .setTitle(R.string.delete_cache_select_backend)
                .setSingleChoiceItems(options, -1) { dialog, which ->
                    selectedIndex = which
                }
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    val progToast = Toast.makeText(
                        CitraApplication.appContext,
                        R.string.deleting_shader_cache,
                        Toast.LENGTH_LONG
                    )
                    progToast.show()

                    activity.lifecycleScope.launch(Dispatchers.IO) {
                        when (selectedIndex) {
                            0 -> {
                                NativeLibrary.deleteVulkanShaderCache(game.titleId)
                            }

                            1 -> {
                                NativeLibrary.deleteOpenGLShaderCache(game.titleId)
                            }
                        }

                        activity.runOnUiThread {
                            progToast.cancel()
                            Toast.makeText(
                                CitraApplication.appContext,
                                R.string.shader_cache_deleted,
                                Toast.LENGTH_SHORT
                            ).show()
                        }
                    }
                }
                .setNegativeButton(android.R.string.cancel) { dialog, _ ->
                    dialog.dismiss()
                }
                .create()

            dialog.setOnShowListener {
                val positiveButton = dialog.getButton(android.app.AlertDialog.BUTTON_POSITIVE)

                positiveButton.isEnabled = false

                val listView = dialog.listView
                listView.setOnItemClickListener { _, _, position, _ ->
                    selectedIndex = position
                    positiveButton.isEnabled = true
                }
            }

            dialog.show()
        }

        val bottomSheetBehavior = bottomSheetDialog.getBehavior()
        bottomSheetBehavior.skipCollapsed = true
        bottomSheetBehavior.state = BottomSheetBehavior.STATE_EXPANDED

        bottomSheetDialog.show()
    }

    private fun refreshShortcutDialogIcon() {
        if (imagePath != null) {
            val originalBitmap = BitmapFactory.decodeStream(
                CitraApplication.appContext.contentResolver.openInputStream(
                    imagePath!!.toUri()
                )
            )
            val scaledBitmap = {
                val preferences =
                    PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)
                if (preferences.getBoolean("shouldStretchIcon", true)) {
                    // stretch to fit
                    originalBitmap.scale(108, 108)
                } else {
                    // Zoom in to fit the bitmap while keeping the aspect ratio
                    val width = originalBitmap.width
                    val height = originalBitmap.height
                    val targetSize = 108

                    if (width > height) {
                        // Landscape orientation
                        val scaleFactor = targetSize.toFloat() / height
                        val scaledWidth = (width * scaleFactor).toInt()
                        val scaledBmp = originalBitmap.scale(scaledWidth, targetSize)

                        val startX = (scaledWidth - targetSize) / 2
                        Bitmap.createBitmap(scaledBmp, startX, 0, targetSize, targetSize)
                    } else {
                        val scaleFactor = targetSize.toFloat() / width
                        val scaledHeight = (height * scaleFactor).toInt()
                        val scaledBmp = originalBitmap.scale(targetSize, scaledHeight)

                        val startY = (scaledHeight - targetSize) / 2
                        Bitmap.createBitmap(scaledBmp, 0, startY, targetSize, targetSize)
                    }
                }
            }()
            dialogShortcutBinding!!.shortcutIcon.setImageBitmap(scaledBitmap)
        }
    }

    private class DiffCallback : DiffUtil.ItemCallback<Game>() {
        override fun areItemsTheSame(oldItem: Game, newItem: Game): Boolean {
            // The title is taken into account to support 3DSX, which all have the titleID 0.
            // This only works now because we always return the English title, adjust if that changes.
            return oldItem.titleId == newItem.titleId && oldItem.title == newItem.title
        }

        override fun areContentsTheSame(oldItem: Game, newItem: Game): Boolean = oldItem == newItem
    }
}

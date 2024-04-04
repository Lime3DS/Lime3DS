// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.utils

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.R
import io.github.lime3ds.android.features.settings.model.AbstractSetting
import io.github.lime3ds.android.features.settings.model.BooleanSetting
import io.github.lime3ds.android.features.settings.model.FloatSetting
import io.github.lime3ds.android.features.settings.model.IntSetting
import io.github.lime3ds.android.features.settings.model.ScaledFloatSetting
import io.github.lime3ds.android.features.settings.model.SettingSection
import io.github.lime3ds.android.features.settings.model.Settings.SettingsSectionMap
import io.github.lime3ds.android.features.settings.model.StringSetting
import io.github.lime3ds.android.features.settings.ui.SettingsActivityView
import io.github.lime3ds.android.utils.BiMap
import io.github.lime3ds.android.utils.DirectoryInitialization.userDirectory
import io.github.lime3ds.android.utils.Log
import org.ini4j.Wini
import java.io.BufferedReader
import java.io.FileNotFoundException
import java.io.IOException
import java.io.InputStreamReader
import java.util.TreeMap


/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val FILE_NAME_CONFIG = "config"

    private var sectionsMap = BiMap<String?, String?>()

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of Settings, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param ini          The ini file to load the settings from
     * @param isCustomGame
     * @param view         The current view.
     * @return An Observable that emits a HashMap of the file's contents, then completes.
     */
    fun readFile(
        ini: DocumentFile,
        isCustomGame: Boolean,
        view: SettingsActivityView?
    ): HashMap<String, SettingSection?> {
        val sections: HashMap<String, SettingSection?> = SettingsSectionMap()
        var reader: BufferedReader? = null
        try {
            val context: Context = LimeApplication.appContext
            val inputStream = context.contentResolver.openInputStream(ini.uri)
            reader = BufferedReader(InputStreamReader(inputStream))
            var current: SettingSection? = null
            var line: String?
            while (reader.readLine().also { line = it } != null) {
                if (line!!.startsWith("[") && line!!.endsWith("]")) {
                    current = sectionFromLine(line!!, isCustomGame)
                    sections[current.name] = current
                } else if (current != null) {
                    val setting = settingFromLine(line!!)
                    if (setting != null) {
                        current.putSetting(setting)
                    }
                }
            }
        } catch (e: FileNotFoundException) {
            Log.error("[SettingsFile] File not found: " + ini.uri + e.message)
            view?.onSettingsFileNotFound()
        } catch (e: IOException) {
            Log.error("[SettingsFile] Error reading from: " + ini.uri + e.message)
            view?.onSettingsFileNotFound()
        } finally {
            if (reader != null) {
                try {
                    reader.close()
                } catch (e: IOException) {
                    Log.error("[SettingsFile] Error closing: " + ini.uri + e.message)
                }
            }
        }
        return sections
    }

    fun readFile(fileName: String, view: SettingsActivityView?): HashMap<String, SettingSection?> {
        return readFile(getSettingsFile(fileName), false, view)
    }

    fun readFile(fileName: String): HashMap<String, SettingSection?> = readFile(fileName, null)

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of SettingSections, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param gameId the id of the game to load it's settings.
     * @param view   The current view.
     */
    fun readCustomGameSettings(
        gameId: String,
        view: SettingsActivityView?
    ): HashMap<String, SettingSection?> {
        return readFile(getCustomGameSettingsFile(gameId), true, view)
    }

    /**
     * Saves a Settings HashMap to a given .ini file on disk. If unsuccessful, outputs an error
     * telling why it failed.
     *
     * @param fileName The target filename without a path or extension.
     * @param sections The HashMap containing the Settings we want to serialize.
     * @param view     The current view.
     */
    fun saveFile(
        fileName: String,
        sections: TreeMap<String, SettingSection?>,
        view: SettingsActivityView
    ) {
        val ini = getSettingsFile(fileName)
        try {
            val context: Context = LimeApplication.appContext
            val inputStream = context.contentResolver.openInputStream(ini.uri)
            val writer = Wini(inputStream)
            val keySet: Set<String> = sections.keys
            for (key in keySet) {
                val section = sections[key]
                writeSection(writer, section!!)
            }
            inputStream!!.close()
            val outputStream = context.contentResolver.openOutputStream(ini.uri, "wt")
            writer.store(outputStream)
            outputStream!!.flush()
            outputStream.close()
        } catch (e: Exception) {
            Log.error("[SettingsFile] File not found: $fileName.ini: ${e.message}")
            view.showToastMessage(
                LimeApplication.appContext
                    .getString(R.string.error_saving, fileName, e.message), false
            )
        }
    }

    fun saveFile(
        fileName: String,
        setting: AbstractSetting
    ) {
        val ini = getSettingsFile(fileName)
        try {
            val context: Context = LimeApplication.appContext
            val inputStream = context.contentResolver.openInputStream(ini.uri)
            val writer = Wini(inputStream)
            writer.put(setting.section, setting.key, setting.valueAsString)
            inputStream!!.close()
            val outputStream = context.contentResolver.openOutputStream(ini.uri, "wt")
            writer.store(outputStream)
            outputStream!!.flush()
            outputStream.close()
        } catch (e: Exception) {
            Log.error("[SettingsFile] File not found: $fileName.ini: ${e.message}")
        }
    }

    private fun mapSectionNameFromIni(generalSectionName: String): String? {
        return if (sectionsMap.getForward(generalSectionName) != null) {
            sectionsMap.getForward(generalSectionName)
        } else {
            generalSectionName
        }
    }

    private fun mapSectionNameToIni(generalSectionName: String): String {
        return if (sectionsMap.getBackward(generalSectionName) != null) {
            sectionsMap.getBackward(generalSectionName).toString()
        } else {
            generalSectionName
        }
    }

    fun getSettingsFile(fileName: String): DocumentFile {
        val root = DocumentFile.fromTreeUri(LimeApplication.appContext, Uri.parse(userDirectory))
        val configDirectory = root!!.findFile("config")
        return configDirectory!!.findFile("$fileName.ini")!!
    }

    private fun getCustomGameSettingsFile(gameId: String): DocumentFile {
        val root = DocumentFile.fromTreeUri(LimeApplication.appContext, Uri.parse(userDirectory))
        val configDirectory = root!!.findFile("GameSettings")
        return configDirectory!!.findFile("$gameId.ini")!!
    }

    private fun sectionFromLine(line: String, isCustomGame: Boolean): SettingSection {
        var sectionName: String = line.substring(1, line.length - 1)
        if (isCustomGame) {
            sectionName = mapSectionNameToIni(sectionName)
        }
        return SettingSection(sectionName)
    }

    /**
     * For a line of text, determines what type of data is being represented, and returns
     * a Setting object containing this data.
     *
     * @param line    The line of text being parsed.
     * @return A typed Setting containing the key/value contained in the line.
     */
    private fun settingFromLine(line: String): AbstractSetting? {
        val splitLine = line.split("=".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray()
        if (splitLine.size != 2) {
            return null
        }
        val key = splitLine[0].trim { it <= ' ' }
        val value = splitLine[1].trim { it <= ' ' }
        if (value.isEmpty()) {
            return null
        }

        val booleanSetting = BooleanSetting.from(key)
        if (booleanSetting != null) {
            booleanSetting.boolean = value.toBoolean()
            return booleanSetting
        }

        val intSetting = IntSetting.from(key)
        if (intSetting != null) {
            try {
                intSetting.int = value.toInt()
            } catch (e: NumberFormatException) {
                intSetting.int = if (value.toBoolean()) 1 else 0
            }
            return intSetting
        }

        val scaledFloatSetting = ScaledFloatSetting.from(key)
        if (scaledFloatSetting != null) {
            scaledFloatSetting.float = value.toFloat() * scaledFloatSetting.scale
            return scaledFloatSetting
        }

        val floatSetting = FloatSetting.from(key)
        if (floatSetting != null) {
            floatSetting.float = value.toFloat()
            return floatSetting
        }

        val stringSetting = StringSetting.from(key)
        if (stringSetting != null) {
            stringSetting.string = value
            return stringSetting
        }

        return null
    }

    /**
     * Writes the contents of a Section HashMap to disk.
     *
     * @param parser  A Wini pointed at a file on disk.
     * @param section A section containing settings to be written to the file.
     */
    private fun writeSection(parser: Wini, section: SettingSection) {
        // Write the section header.
        val header = section.name

        // Write this section's values.
        val settings = section.settings
        val keySet: Set<String> = settings.keys
        for (key in keySet) {
            val setting = settings[key]
            parser.put(header, setting!!.key, setting.valueAsString)
        }
    }
}

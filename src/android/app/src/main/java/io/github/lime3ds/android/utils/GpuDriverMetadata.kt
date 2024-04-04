// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import java.io.IOException
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.InputStream

class GpuDriverMetadata {
    /**
     * Tries to get driver metadata information from a meta.json [File]
     *
     * @param metadataFile meta.json file provided with a GPU driver
     */
    constructor(metadataFile: File) {
        if (metadataFile.length() > MAX_META_SIZE_BYTES) {
            return
        }

        try {
            val json = JSONObject(FileUtil.getStringFromFile(metadataFile))
            name = json.getString("name")
            description = json.getString("description")
            author = json.getString("author")
            vendor = json.getString("vendor")
            version = json.getString("driverVersion")
            minApi = json.getInt("minApi")
            libraryName = json.getString("libraryName")
        } catch (e: JSONException) {
            // JSON is malformed, ignore and treat as unsupported metadata.
        } catch (e: IOException) {
            // File is inaccessible, ignore and treat as unsupported metadata.
        }
    }

    /**
     * Tries to get driver metadata information from an input stream that's intended to be
     * from a zip file
     *
     * @param metadataStream ZipEntry input stream
     * @param size Size of the file in bytes
     */
    constructor(metadataStream: InputStream, size: Long) {
        if (size > MAX_META_SIZE_BYTES) {
            return
        }

        try {
            val json = JSONObject(FileUtil.getStringFromInputStream(metadataStream, size))
            name = json.getString("name")
            description = json.getString("description")
            author = json.getString("author")
            vendor = json.getString("vendor")
            version = json.getString("driverVersion")
            minApi = json.getInt("minApi")
            libraryName = json.getString("libraryName")
        } catch (e: JSONException) {
            // JSON is malformed, ignore and treat as unsupported metadata.
        } catch (e: IOException) {
            // File is inaccessible, ignore and treat as unsupported metadata.
        }
    }

    /**
     * Creates an empty metadata instance
     */
    constructor()

    override fun equals(other: Any?): Boolean {
        if (other !is GpuDriverMetadata) {
            return false
        }

        return other.name == name &&
                other.description == description &&
                other.author == author &&
                other.vendor == vendor &&
                other.version == version &&
                other.minApi == minApi &&
                other.libraryName == libraryName
    }

    override fun hashCode(): Int {
        var result = name?.hashCode() ?: 0
        result = 31 * result + (description?.hashCode() ?: 0)
        result = 31 * result + (author?.hashCode() ?: 0)
        result = 31 * result + (vendor?.hashCode() ?: 0)
        result = 31 * result + (version?.hashCode() ?: 0)
        result = 31 * result + minApi
        result = 31 * result + (libraryName?.hashCode() ?: 0)
        return result
    }

    override fun toString(): String =
        """
            Name - $name
            Description - $description
            Author - $author
            Vendor - $vendor
            Version - $version
            Min API - $minApi
            Library Name - $libraryName
        """.trimMargin().trimIndent()

    var name: String? = null
    var description: String? = null
    var author: String? = null
    var vendor: String? = null
    var version: String? = null
    var minApi = 0
    var libraryName: String? = null

    companion object {
        private const val MAX_META_SIZE_BYTES = 500000
    }
}

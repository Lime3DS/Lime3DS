// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.net.Uri
import android.provider.DocumentsContract
import androidx.documentfile.provider.DocumentFile
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.model.CheapDocument
import java.net.URLDecoder
import java.util.StringTokenizer
import java.util.concurrent.ConcurrentHashMap

/**
 * A cached document tree for Lime3DS user directory.
 * For every filepath which is not startsWith "content://" will need to use this class to traverse.
 * For example:
 * C++ Lime3DS log file directory will be /log/lime3ds_log.txt.
 * After DocumentsTree.resolvePath() it will become content URI.
 */
class DocumentsTree {
    private var root: DocumentsNode? = null
    private val context get() = LimeApplication.appContext

    fun setRoot(rootUri: Uri?) {
        root = null
        root = DocumentsNode()
        root!!.uri = rootUri
        root!!.isDirectory = true
    }

    @Synchronized
    fun createFile(filepath: String, name: String): Boolean {
        val node = resolvePath(filepath) ?: return false
        if (!node.isDirectory) return false
        if (!node.loaded) structTree(node)
        try {
            val filename = URLDecoder.decode(name, FileUtil.DECODE_METHOD)
            if (node.findChild(filename) != null) return true
            val createdFile = FileUtil.createFile(node.uri.toString(), name) ?: return false
            val document = DocumentsNode(createdFile, false)
            document.parent = node
            node.addChild(document)
            return true
        } catch (e: Exception) {
            error("[DocumentsTree]: Cannot create file, error: " + e.message)
        }
    }

    @Synchronized
    fun createDir(filepath: String, name: String): Boolean {
        val node = resolvePath(filepath) ?: return false
        if (!node.isDirectory) return false
        if (!node.loaded) structTree(node)
        try {
            val filename = URLDecoder.decode(name, FileUtil.DECODE_METHOD)
            if (node.findChild(filename) != null) return true
            val createdDirectory = FileUtil.createDir(node.uri.toString(), name) ?: return false
            val document = DocumentsNode(createdDirectory, true)
            document.parent = node
            node.addChild(document)
            return true
        } catch (e: Exception) {
            error("[DocumentsTree]: Cannot create file, error: " + e.message)
        }
    }

    @Synchronized
    fun openContentUri(filepath: String, openMode: String): Int {
        val node = resolvePath(filepath) ?: return -1
        return FileUtil.openContentUri(node.uri.toString(), openMode)
    }

    @Synchronized
    fun getFilename(filepath: String): String {
        val node = resolvePath(filepath) ?: return ""
        return node.name
    }

    @Synchronized
    fun getFilesName(filepath: String): Array<String?> {
        val node = resolvePath(filepath)
        if (node == null || !node.isDirectory) {
            return arrayOfNulls(0)
        }
        // If this directory has not been iterated, struct it.
        if (!node.loaded) structTree(node)
        return node.getChildNames()
    }

    @Synchronized
    fun getFileSize(filepath: String): Long {
        val node = resolvePath(filepath)
        return if (node == null || node.isDirectory) {
            0
        } else {
            FileUtil.getFileSize(node.uri.toString())
        }
    }

    @Synchronized
    fun getUri(filepath: String): Uri {
        val node = resolvePath(filepath) ?: return Uri.EMPTY
        return node.uri ?: return Uri.EMPTY
    }

    @Synchronized
    fun isDirectory(filepath: String): Boolean {
        val node = resolvePath(filepath) ?: return false
        return node.isDirectory
    }

    @Synchronized
    fun exists(filepath: String): Boolean {
        return resolvePath(filepath) != null
    }

    @Synchronized
    fun copyFile(
        sourcePath: String,
        destinationParentPath: String,
        destinationFilename: String
    ): Boolean {
        val sourceNode = resolvePath(sourcePath) ?: return false
        val destinationNode = resolvePath(destinationParentPath) ?: return false
        try {
            val destinationParent =
                DocumentFile.fromTreeUri(context, destinationNode.uri!!) ?: return false
            val filename = URLDecoder.decode(destinationFilename, "UTF-8")
            val destination = destinationParent.createFile(
                "application/octet-stream",
                filename
            ) ?: return false
            val document = DocumentsNode()
            document.uri = destination.uri
            document.parent = destinationNode
            document.name = destination.name!!
            document.isDirectory = destination.isDirectory
            document.loaded = true
            val input = context.contentResolver.openInputStream(sourceNode.uri!!)!!
            val output = context.contentResolver.openOutputStream(destination.uri, "wt")!!
            val buffer = ByteArray(1024)
            var len: Int
            while (input.read(buffer).also { len = it } != -1) {
                output.write(buffer, 0, len)
            }
            input.close()
            output.flush()
            output.close()
            destinationNode.addChild(document)
            return true
        } catch (e: Exception) {
            error("[DocumentsTree]: Cannot copy file, error: " + e.message)
        }
    }

    @Synchronized
    fun renameFile(filepath: String, destinationFilename: String?): Boolean {
        val node = resolvePath(filepath) ?: return false
        try {
            val filename = URLDecoder.decode(destinationFilename, FileUtil.DECODE_METHOD)
            DocumentsContract.renameDocument(context.contentResolver, node.uri!!, filename)
            node.rename(filename)
            return true
        } catch (e: Exception) {
            error("[DocumentsTree]: Cannot rename file, error: " + e.message)
        }
    }

    @Synchronized
    fun deleteDocument(filepath: String): Boolean {
        val node = resolvePath(filepath) ?: return false
        try {
            if (!DocumentsContract.deleteDocument(context.contentResolver, node.uri!!)) {
                return false
            }
            if (node.parent != null) {
                node.parent!!.removeChild(node)
            }
            return true
        } catch (e: Exception) {
            error("[DocumentsTree]: Cannot rename file, error: " + e.message)
        }
    }

    @Synchronized
    private fun resolvePath(filepath: String): DocumentsNode? {
        root ?: return null
        val tokens = StringTokenizer(filepath, DELIMITER, false)
        var iterator = root
        while (tokens.hasMoreTokens()) {
            val token = tokens.nextToken()
            if (token.isEmpty()) continue
            iterator = find(iterator!!, token)
            if (iterator == null) return null
        }
        return iterator
    }

    @Synchronized
    private fun find(parent: DocumentsNode, filename: String): DocumentsNode? {
        if (parent.isDirectory && !parent.loaded) {
            structTree(parent)
        }
        return parent.findChild(filename)
    }

    /**
     * Construct current level directory tree
     *
     * @param parent parent node of this level
     */
    @Synchronized
    private fun structTree(parent: DocumentsNode) {
        val documents = FileUtil.listFiles(parent.uri!!)
        for (document in documents) {
            val node = DocumentsNode(document)
            node.parent = parent
            parent.addChild(node)
        }
        parent.loaded = true
    }

    private class DocumentsNode {
        @get:Synchronized
        @set:Synchronized
        var parent: DocumentsNode? = null
        val children: MutableMap<String?, DocumentsNode?> = ConcurrentHashMap()
        lateinit var name: String

        @get:Synchronized
        @set:Synchronized
        var uri: Uri? = null

        @get:Synchronized
        @set:Synchronized
        var loaded = false
        var isDirectory = false

        constructor()
        constructor(document: CheapDocument) {
            name = document.filename
            uri = document.uri
            isDirectory = document.isDirectory
            loaded = !isDirectory
        }

        constructor(document: DocumentFile, isCreateDir: Boolean) {
            name = document.name!!
            uri = document.uri
            isDirectory = isCreateDir
            loaded = true
        }

        @Synchronized
        fun rename(name: String) {
            parent ?: return
            parent!!.removeChild(this)
            this.name = name
            parent!!.addChild(this)
        }

        fun addChild(node: DocumentsNode) {
            children[node.name.lowercase()] = node
        }

        fun removeChild(node: DocumentsNode) = children.remove(node.name.lowercase())

        fun findChild(filename: String) = children[filename.lowercase()]

        @Synchronized
        fun getChildNames(): Array<String?> =
            children.mapNotNull { it.value!!.name }.toTypedArray()
    }

    companion object {
        const val DELIMITER = "/"
    }
}

// SPDX-License-Identifier: MIT
package com.gecko.emulator.library

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File

/** A game entry shown in the library grid. */
data class GameEntry(
    val path: String,
    val title: String,
    val extension: String,
    val corePath: String?,
    val artworkPath: String?,
)

/**
 * Library model for the Android app.
 *
 * Storage layout (app-specific external storage — no permissions required):
 *   files/roms/        scanned recursively for ROMs
 *   files/cores/       libretro core libraries (.so)
 *   files/system/      BIOS / system data handed to cores
 *   files/saves/       SRAM + save states
 *   files/screenshots/ screenshots
 *
 * Files imported through the system picker (SAF) are copied into files/roms
 * so the native core can open them by real path.
 */
class LibraryRepository(private val context: Context) {

    val romsDir: File get() = File(context.getExternalFilesDir(null), "roms")
    val coresDir: File get() = File(context.getExternalFilesDir(null), "cores")
    val systemDir: File get() = File(context.getExternalFilesDir(null), "system")
    val savesDir: File get() = File(context.getExternalFilesDir(null), "saves")
    val screenshotsDir: File get() = File(context.getExternalFilesDir(null), "screenshots")

    fun ensureDirectories() {
        listOf(romsDir, coresDir, systemDir, savesDir, screenshotsDir).forEach {
            it.mkdirs()
        }
    }

    /** Extensions the scanner picks up (subset of the desktop defaults). */
    private val romExtensions = setOf(
        "smc", "sfc", "nes", "fds", "unf", "gb", "gbc", "gba", "agb",
        "md", "gen", "smd", "sms", "gg", "pce", "n64", "z64", "v64",
        "vb", "ws", "wsc", "ngp", "ngc", "a26", "a78", "col", "int",
        "zip", "7z", "cue", "chd", "iso", "bin", "pbp", "ccdl",
    )

    fun scan(): List<GameEntry> {
        val cores = scanCores()
        val games = mutableListOf<GameEntry>()
        romsDir.takeIf { it.exists() }?.walkTopDown()?.maxDepth(4)?.forEach { file ->
            if (!file.isFile) return@forEach
            val ext = file.extension.lowercase()
            if (ext !in romExtensions) return@forEach
            games += GameEntry(
                path = file.absolutePath,
                title = prettify(file.nameWithoutExtension),
                extension = ext,
                corePath = coreForExtension(cores, ext),
                artworkPath = findArtwork(file),
            )
        }
        return games.sortedBy { it.title }
    }

    data class CoreEntry(val path: String, val name: String)

    /** Lists candidate core libraries (name only; loading is done natively). */
    fun scanCores(): List<CoreEntry> =
        coresDir.listFiles { f -> f.isFile && f.extension == "so" }
            .orEmpty()
            .sortedBy { it.name }
            .map { CoreEntry(it.absolutePath, it.name.removeSuffix(".so")) }

    fun coreForExtension(cores: List<CoreEntry>, ext: String): String? {
        if (cores.isEmpty()) return null
        // Association file: one "ext=coreFileName" line per mapping.
        val mapFile = File(context.getExternalFilesDir(null), "core_map.txt")
        if (mapFile.exists()) {
            mapFile.readLines().forEach { line ->
                val parts = line.split('=', limit = 2)
                if (parts.size == 2 && parts[0].trim() == ext) {
                    val f = File(coresDir, parts[1].trim())
                    if (f.exists()) return f.absolutePath
                }
            }
        }
        return null
    }

    fun setCoreForExtension(ext: String, coreFile: File) {
        val mapFile = File(context.getExternalFilesDir(null), "core_map.txt")
        val lines = if (mapFile.exists()) mapFile.readLines().toMutableList() else mutableListOf()
        val entry = "$ext=${coreFile.name}"
        lines.removeAll { it.startsWith("$ext=") }
        lines.add(entry)
        mapFile.writeText(lines.joinToString("\n"))
    }

    /** Copies a SAF-picked document into the roms directory (real path for native access). */
    fun importRom(uri: Uri): File? {
        val resolver = context.contentResolver
        val displayName = queryDisplayName(resolver, uri) ?: return null
        ensureDirectories()
        val target = File(romsDir, displayName)
        resolver.openInputStream(uri)?.use { input ->
            target.outputStream().use { output -> input.copyTo(output) }
        } ?: return null
        return target
    }

    private fun queryDisplayName(resolver: android.content.ContentResolver, uri: Uri): String? =
        resolver.query(uri, null, null, null, null)?.use { cursor ->
            val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (nameIndex >= 0 && cursor.moveToFirst()) cursor.getString(nameIndex) else null
        } ?: uri.lastPathSegment

    fun stateFile(gameTitle: String, slot: Int): File =
        File(savesDir, "${gameTitle}.state$slot")

    fun screenshotFile(gameTitle: String): File =
        File(screenshotsDir, "$gameTitle-${System.currentTimeMillis()}.png")

    companion object {
        /** "super_mario_world_(USA)" -> "super mario world". */
        fun prettify(stem: String): String {
            // Strip (tags) and [tags].
            val stripped = buildString {
                var depth = 0
                for (c in stem) {
                    when (c) {
                        '(', '[' -> depth++
                        ')', ']' -> if (depth > 0) depth--
                        else -> if (depth == 0) append(c)
                    }
                }
            }
            return stripped.replace('_', ' ')
                .replace('.', ' ')
                .trim()
                .replace(Regex("\\s+"), " ")
        }
    }
}

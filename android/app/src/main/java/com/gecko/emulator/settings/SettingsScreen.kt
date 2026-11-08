// SPDX-License-Identifier: MIT
package com.gecko.emulator.settings

import android.content.Context
import android.graphics.Bitmap
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.gecko.emulator.library.LibraryRepository
import java.io.File

/**
 * App settings (video/audio). Persisted as simple key=value lines under
 * filesDir by [save]; the game screen reads them on launch.
 */
object SettingsRepositoryShared {
    var smoothing: Boolean by mutableStateOf(true)
    var showHud: Boolean by mutableStateOf(true)
    var volume: Int by mutableStateOf(100)
    var muteFastForward: Boolean by mutableStateOf(true)
    var filter: String by mutableStateOf("none") // none | scanlines | crt | lcd

    private lateinit var file: File

    fun init(context: Context) {
        file = File(context.filesDir, "gecko_settings.txt")
        if (!file.exists()) return
        file.readLines().forEach { line ->
            val parts = line.split('=', limit = 2)
            if (parts.size != 2) return@forEach
            when (parts[0].trim()) {
                "smoothing" -> smoothing = parts[1].trim() == "true"
                "hud" -> showHud = parts[1].trim() == "true"
                "volume" -> volume = parts[1].trim().toIntOrNull() ?: 100
                "mute_ff" -> muteFastForward = parts[1].trim() == "true"
                "filter" -> filter = parts[1].trim()
            }
        }
    }

    fun save() {
        if (!::file.isInitialized) return
        file.writeText(
            listOf(
                "smoothing=$smoothing",
                "hud=$showHud",
                "volume=$volume",
                "mute_ff=$muteFastForward",
                "filter=$filter",
            ).joinToString("\n"),
        )
    }

    fun cycleFilter() {
        filter = when (filter) {
            "none" -> "scanlines"
            "scanlines" -> "crt"
            "crt" -> "lcd"
            else -> "none"
        }
        save()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(repository: LibraryRepository, onBack: () -> Unit) {
    val settings = SettingsRepositoryShared

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Settings") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
            )
        },
    ) { padding ->
        Column(
            Modifier
                .padding(padding)
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
        ) {
            Text("Video", style = MaterialTheme.typography.titleMedium)
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("Smooth scaling")
                Switch(checked = settings.smoothing, onCheckedChange = {
                    settings.smoothing = it
                    settings.save()
                })
            }
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("Performance HUD")
                Switch(checked = settings.showHud, onCheckedChange = {
                    settings.showHud = it
                    settings.save()
                })
            }

            Spacer(Modifier.height(16.dp))
            HorizontalDivider()
            Spacer(Modifier.height(16.dp))

            Text("Audio", style = MaterialTheme.typography.titleMedium)
            Text("Volume: ${settings.volume}%")
            Slider(
                value = settings.volume.toFloat(),
                onValueChange = {
                    settings.volume = it.toInt()
                    settings.save()
                },
                valueRange = 0f..100f,
            )
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text("Mute while fast-forwarding")
                Switch(checked = settings.muteFastForward, onCheckedChange = {
                    settings.muteFastForward = it
                    settings.save()
                })
            }

            Spacer(Modifier.height(16.dp))
            HorizontalDivider()
            Spacer(Modifier.height(16.dp))

            Text("Folders", style = MaterialTheme.typography.titleMedium)
            Text(
                "ROMs: ${repository.romsDir.absolutePath}\n" +
                    "Cores: ${repository.coresDir.absolutePath}\n" +
                    "System: ${repository.systemDir.absolutePath}\n" +
                    "Saves: ${repository.savesDir.absolutePath}",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

/** Cores screen: lists core libraries found in files/cores. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CoresScreen(repository: LibraryRepository, onBack: () -> Unit) {
    val cores = remember { repository.scanCores() }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Cores") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
            )
        },
    ) { padding ->
        Column(
            Modifier
                .padding(padding)
                .fillMaxSize()
                .padding(16.dp),
        ) {
            if (cores.isEmpty()) {
                Text(
                    "No cores found.\n\nCopy libretro core libraries (.so) into\n" +
                        repository.coresDir.absolutePath,
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            } else {
                Text(
                    "${cores.size} core(s) available:",
                    style = MaterialTheme.typography.titleMedium,
                )
                Spacer(Modifier.height(8.dp))
                cores.forEach { core ->
                    Column(Modifier.padding(vertical = 6.dp)) {
                        Text(core.name, style = MaterialTheme.typography.bodyLarge)
                        Text(
                            core.path,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    HorizontalDivider()
                }
            }
            Spacer(Modifier.height(16.dp))
            Text(
                "Any libretro core works — SNES, NES, GB(A), Genesis and the " +
                    "bundled GA330 core. Place the .so file in the folder above; " +
                    "it appears here after reopening this screen.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

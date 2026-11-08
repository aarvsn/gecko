// SPDX-License-Identifier: MIT
@file:OptIn(ExperimentalMaterial3Api::class)

package com.gecko.emulator.library

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CreateNewFolder
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.painter.ColorPainter
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.io.File

/**
 * Library screen: game grid + top bar actions (add folder / import / cores / settings).
 */
@Composable
fun LibraryScreen(
    games: List<GameEntry>,
    coresCount: Int,
    onGameClick: (GameEntry) -> Unit,
    onPickRomFolder: () -> Unit,
    onImportFiles: () -> Unit,
    onOpenSettings: () -> Unit,
    onOpenCores: () -> Unit,
    repository: LibraryRepository,
) {
    var showNoCores by remember { mutableStateOf(false) }
    var pendingGame by remember { mutableStateOf<GameEntry?>(null) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Gecko") },
                actions = {
                    IconButton(onClick = onPickRomFolder) {
                        Icon(Icons.Filled.Folder, contentDescription = "Add ROM folder")
                    }
                    IconButton(onClick = onImportFiles) {
                        Icon(Icons.Filled.CreateNewFolder, contentDescription = "Import files")
                    }
                    IconButton(onClick = onOpenCores) {
                        Icon(Icons.Filled.Gamepad, contentDescription = "Cores")
                    }
                    IconButton(onClick = onOpenSettings) {
                        Icon(Icons.Filled.Settings, contentDescription = "Settings")
                    }
                },
            )
        },
    ) { padding ->
        if (games.isEmpty()) {
            Box(
                Modifier
                    .padding(padding)
                    .fillMaxSize(),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = "No games yet.\n\nAdd your ROM folder, or copy ROMs into\n" +
                        "Android/data/com.gecko.emulator/files/roms",
                    textAlign = TextAlign.Center,
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        } else {
            LazyVerticalGrid(
                columns = GridCells.Adaptive(minSize = 150.dp),
                modifier = Modifier
                    .padding(padding)
                    .fillMaxSize(),
                contentPadding = PaddingValues(12.dp),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                items(games, key = { it.path }) { game ->
                    GameCard(
                        game = game,
                        onClick = {
                            if (coresCount == 0) {
                                showNoCores = true
                            } else if (game.corePath == null) {
                                pendingGame = game
                            } else {
                                onGameClick(game)
                            }
                        },
                    )
                }
            }
        }
    }

    if (showNoCores) {
        AlertDialog(
            onDismissRequest = { showNoCores = false },
            title = { Text("No cores installed") },
            text = {
                Text(
                    "Copy libretro core libraries (.so files) into\n" +
                        "Android/data/com.gecko.emulator/files/cores\n\n" +
                        "Then open the Cores screen to verify.",
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    showNoCores = false
                    onOpenCores()
                }) { Text("Open cores") }
            },
            dismissButton = {
                TextButton(onClick = { showNoCores = false }) { Text("OK") }
            },
        )
    }

    // Ask for a core when the extension is unmapped.
    pendingGame?.let { game ->
        val cores = remember(game) { repository.scanCores() }
        AlertDialog(
            onDismissRequest = { pendingGame = null },
            title = { Text("Choose a core") },
            text = {
                Column {
                    if (cores.isEmpty()) {
                        Text("No cores found in the cores folder.")
                    } else {
                        cores.forEach { core ->
                            TextButton(onClick = {
                                repository.setCoreForExtension(game.extension, File(core.path))
                                val updated = game.copy(corePath = core.path)
                                pendingGame = null
                                onGameClick(updated)
                            }) {
                                Text(core.name)
                            }
                        }
                    }
                }
            },
            confirmButton = {},
            dismissButton = {
                TextButton(onClick = { pendingGame = null }) { Text("Cancel") }
            },
        )
    }
}

@Composable
private fun GameCard(game: GameEntry, onClick: () -> Unit) {
    Card(
        onClick = onClick,
        shape = RoundedCornerShape(12.dp),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp),
    ) {
        Column {
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(110.dp)
                    .background(brush = coverBrush(game.title)),
                contentAlignment = Alignment.Center,
            ) {
                val art = game.artworkPath
                if (art != null && File(art).exists()) {
                    Image(
                        painter = rememberArtPainter(art),
                        contentDescription = game.title,
                        contentScale = ContentScale.Crop,
                        modifier = Modifier.fillMaxSize(),
                    )
                } else {
                    Text(
                        text = initials(game.title),
                        fontSize = 34.sp,
                        color = Color.White,
                    )
                }
            }
            Column(Modifier.padding(10.dp)) {
                Text(
                    text = game.title,
                    style = MaterialTheme.typography.titleSmall,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
                Spacer(Modifier.height(2.dp))
                Text(
                    text = game.extension.uppercase(),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.primary,
                )
            }
        }
    }
}

/** Decodes box art off the UI path is out of scope for v1; decode small, cached by remember. */
@Composable
private fun rememberArtPainter(path: String): Painter {
    val bitmap = remember(path) {
        val opts = android.graphics.BitmapFactory.Options().apply { inSampleSize = 2 }
        android.graphics.BitmapFactory.decodeFile(path, opts)
    }
    return bitmap?.asImageBitmap()?.let { BitmapPainter(it) }
        ?: ColorPainter(Color(0xFF3AA657))
}

private fun initials(title: String): String =
    title.split(' ').filter { it.isNotBlank() }.take(2)
        .map { it.first().uppercaseChar() }.joinToString("")

private fun coverBrush(title: String): Brush {
    val hue = (title.hashCode().mod(360) + 360) % 360
    return Brush.verticalGradient(
        listOf(
            Color.hsl(hue.toFloat(), 0.45f, 0.55f),
            Color.hsl(hue.toFloat(), 0.45f, 0.3f),
        ),
    )
}

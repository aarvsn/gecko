// SPDX-License-Identifier: MIT
package com.gecko.emulator

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.gecko.emulator.game.GameActivity
import com.gecko.emulator.library.GameEntry
import com.gecko.emulator.library.LibraryRepository
import com.gecko.emulator.library.LibraryScreen
import com.gecko.emulator.settings.SettingsRepositoryShared
import com.gecko.emulator.ui.GeckoTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainViewModel : ViewModel() {
    private var repository: LibraryRepository? = null

    var games by mutableStateOf<List<GameEntry>>(emptyList())
    var coresCount by mutableStateOf(0)
    var screen by mutableStateOf<Screen>(Screen.Library)
        private set

    enum class Screen { Library, Cores, Settings }

    fun repository(context: android.content.Context): LibraryRepository {
        if (repository == null) {
            repository = LibraryRepository(context.applicationContext)
            repository?.ensureDirectories()
        }
        return repository!!
    }

    fun rescan(context: android.content.Context) {
        viewModelScope.launch {
            val repo = repository(context)
            withContext(Dispatchers.IO) {
                val scanned = repo.scan()
                val coreCount = repo.scanCores().size
                games = scanned
                coresCount = coreCount
            }
        }
    }

    fun open(context: android.content.Context, game: GameEntry) {
        val intent = Intent(context, GameActivity::class.java).apply {
            putExtra(GameActivity.EXTRA_GAME_PATH, game.path)
            putExtra(GameActivity.EXTRA_GAME_TITLE, game.title)
            putExtra(GameActivity.EXTRA_CORE_PATH, game.corePath)
        }
        context.startActivity(intent)
    }
}

class MainActivity : ComponentActivity() {

    private val viewModel = MainViewModel()

    private val pickRomFolder =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri: Uri? ->
            uri ?: return@registerForActivityResult
            copyTreeToRoms(uri)
        }

    private val importFiles =
        registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
            if (!uris.isNullOrEmpty()) {
                val repo = viewModel.repository(this)
                var imported = false
                uris.forEach { uri ->
                    repo.importRom(uri)?.let { imported = true }
                }
                if (imported) viewModel.rescan(this)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        SettingsRepositoryShared.init(this)
        val repo = viewModel.repository(this)
        viewModel.rescan(this)

        setContent {
            GeckoTheme {
                when (viewModel.screen) {
                    MainViewModel.Screen.Library -> {
                        LibraryScreen(
                            games = viewModel.games,
                            coresCount = viewModel.coresCount,
                            onGameClick = { viewModel.open(this, it) },
                            onPickRomFolder = { pickRomFolder.launch(null) },
                            onImportFiles = { importFiles.launch(arrayOf("*/*")) },
                            onOpenSettings = { viewModel.screen = MainViewModel.Screen.Settings },
                            onOpenCores = { viewModel.screen = MainViewModel.Screen.Cores },
                            repository = repo,
                        )
                    }
                    MainViewModel.Screen.Cores -> {
                        SettingsScreen.CoresScreen(
                            repository = repo,
                            onBack = {
                                viewModel.screen = MainViewModel.Screen.Library
                                viewModel.rescan(this)
                            },
                        )
                    }
                    MainViewModel.Screen.Settings -> {
                        SettingsScreen(
                            repository = repo,
                            onBack = { viewModel.screen = MainViewModel.Screen.Library },
                        )
                    }
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        viewModel.rescan(this)
    }

    /** Copies a picked folder tree into files/roms (bounded to 3 depth levels). */
    private fun copyTreeToRoms(root: Uri) {
        val repo = viewModel.repository(this)
        viewModel.viewModelScope.launch(Dispatchers.IO) {
            copyTreeRecursive(root, repo.romsDir, depth = 0)
            withContext(Dispatchers.Main) { viewModel.rescan(this@MainActivity) }
        }
    }

    private fun copyTreeRecursive(treeUri: Uri, targetDir: java.io.File, depth: Int) {
        if (depth > 3) return
        val childrenUri = android.provider.DocumentsContract.buildChildDocumentsUriUsingTree(
            treeUri,
            android.provider.DocumentsContract.getTreeDocumentId(treeUri),
        )
        contentResolver.query(childrenUri, null, null, null, null)?.use { cursor ->
            while (cursor.moveToNext()) {
                val docId = cursor.getString(0)
                val name = cursor.getString(
                    cursor.getColumnIndexOrThrow(android.provider.DocumentsContract.Document.COLUMN_DISPLAY_NAME),
                )
                val mime = cursor.getString(
                    cursor.getColumnIndexOrThrow(android.provider.DocumentsContract.Document.COLUMN_MIME_TYPE),
                )
                val docUri = android.provider.DocumentsContract.buildDocumentUriUsingTree(treeUri, docId)
                if (mime == android.provider.DocumentsContract.Document.MIME_TYPE_DIR) {
                    copyTreeRecursive(docUri, java.io.File(targetDir, name), depth + 1)
                } else {
                    val target = java.io.File(targetDir, name)
                    if (!target.exists()) {
                        runCatching {
                            contentResolver.openInputStream(docUri)?.use { input ->
                                target.outputStream().use { output -> input.copyTo(output) }
                            }
                        }
                    }
                }
            }
        }
    }
}

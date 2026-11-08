// SPDX-License-Identifier: MIT
package com.gecko.emulator.game

import android.os.Bundle
import android.view.KeyEvent
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.gecko.emulator.native_.RetroPadIds
import com.gecko.emulator.settings.SettingsRepositoryShared
import com.gecko.emulator.ui.GeckoTheme

/**
 * In-game activity: fullscreen SurfaceView + virtual gamepad overlay +
 * quick-menu (bottom sheet) with save states, reset, screenshot and exit.
 */
class GameActivity : ComponentActivity() {

    companion object {
        const val EXTRA_GAME_PATH = "game_path"
        const val EXTRA_GAME_TITLE = "game_title"
        const val EXTRA_CORE_PATH = "core_path"
    }

    var gameTitle: String = "game"
        private set

    private var session: EmulatorSession? = null
    private var started by mutableStateOf(false)
    private var errorMessage by mutableStateOf<String?>(null)
    private var menuOpen by mutableStateOf(false)
    private var hudFps by mutableStateOf(0.0)
    private var toast by mutableStateOf<String?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        gameTitle = intent.getStringExtra(EXTRA_GAME_TITLE) ?: "game"
        val gamePath = intent.getStringExtra(EXTRA_GAME_PATH) ?: ""
        val corePath = intent.getStringExtra(EXTRA_CORE_PATH) ?: ""

        val surfaceView = android.view.SurfaceView(this)
        session = EmulatorSession(
            context = this,
            surfaceView = surfaceView,
            corePath = corePath,
            gamePath = gamePath,
        ) { ok, message ->
            if (ok) started = true else errorMessage = message
        }

        // HUD refresh + fps.
        android.os.Handler(android.os.Looper.getMainLooper()).apply {
            val tick = object : Runnable {
                override fun run() {
                    if (started) hudFps = session?.measuredFps() ?: 0.0
                    postDelayed(this, 500)
                }
            }
            post(tick)
        }

        setContent {
            GeckoTheme {
                GameScreen(surfaceView)
            }
        }
    }

    @Composable
    private fun GameScreen(surfaceView: android.view.SurfaceView) {
        Surface(modifier = Modifier.fillMaxSize(), color = Color.Black) {
            Box(Modifier.fillMaxSize()) {
                AndroidView(
                    factory = { surfaceView },
                    modifier = Modifier.fillMaxSize(),
                )

                if (started && errorMessage == null) {
                    // HUD.
                    if (SettingsRepositoryShared.showHud) {
                        Text(
                            text = "FPS %.1f  •  %s".format(hudFps, gameTitle),
                            color = Color(0xCCFFFFFF),
                            style = MaterialTheme.typography.labelMedium,
                            modifier = Modifier
                                .align(Alignment.TopStart)
                                .padding(14.dp)
                                .background(Color(0x66000000), RoundedCornerShape(8.dp))
                                .padding(horizontal = 10.dp, vertical = 5.dp),
                        )
                    }
                    // Virtual gamepad overlay.
                    VirtualGamepad(
                        onButton = { retroId, pressed -> session?.setButton(retroId, pressed) },
                    )
                }

                if (errorMessage != null) {
                    AlertDialog(
                        onDismissRequest = { finish() },
                        title = { Text("Cannot start") },
                        text = { Text(errorMessage ?: "") },
                        confirmButton = {
                            TextButton(onClick = { finish() }) { Text("OK") }
                        },
                    )
                }

                // Menu toggle button.
                if (started) {
                    TextButton(
                        onClick = {
                            menuOpen = true
                            session?.pause(true)
                        },
                        modifier = Modifier
                            .align(Alignment.TopEnd)
                            .padding(10.dp)
                            .background(Color(0x66000000), RoundedCornerShape(8.dp)),
                    ) {
                        Text("☰", color = Color.White)
                    }
                }

                toast?.let { message ->
                    Text(
                        text = message,
                        color = Color.White,
                        modifier = Modifier
                            .align(Alignment.Center)
                            .background(Color(0xCC222222), RoundedCornerShape(10.dp))
                            .padding(horizontal = 18.dp, vertical = 10.dp),
                    )
                    androidx.compose.runtime.LaunchedEffect(message) {
                        kotlinx.coroutines.delay(1500)
                        toast = null
                    }
                }

                if (menuOpen) {
                    InGameMenu(
                        onDismiss = {
                            menuOpen = false
                            session?.pause(false)
                        },
                        onSaveState = { slot ->
                            val ok = session?.saveState(slot) == true
                            showToast(if (ok) "State saved (slot $slot)" else "Save failed")
                        },
                        onLoadState = { slot ->
                            val ok = session?.loadState(slot) == true
                            showToast(if (ok) "State loaded (slot $slot)" else "No state in slot $slot")
                        },
                        onScreenshot = {
                            val ok = session?.screenshot() == true
                            showToast(if (ok) "Screenshot saved" else "Screenshot failed")
                        },
                        onReset = {
                            session?.reset()
                            showToast("Game reset")
                        },
                        onCycleFilter = {
                            SettingsRepositoryShared.cycleFilter()
                            showToast("Filter: ${SettingsRepositoryShared.filter}")
                        },
                        onExit = { finish() },
                    )
                }
            }
        }
    }

    private fun showToast(message: String) {
        toast = message
    }

    @Composable
    private fun InGameMenu(
        onDismiss: () -> Unit,
        onSaveState: (Int) -> Unit,
        onLoadState: (Int) -> Unit,
        onScreenshot: () -> Unit,
        onReset: () -> Unit,
        onCycleFilter: () -> Unit,
        onExit: () -> Unit,
    ) {
        Box(
            Modifier
                .fillMaxSize()
                .background(Color(0x55000000)),
        ) {
            Column(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .padding(12.dp)
                    .background(
                        MaterialTheme.colorScheme.surface,
                        RoundedCornerShape(16.dp),
                    )
                    .padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                Text(
                    "Gecko — $gameTitle",
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(bottom = 6.dp),
                )
                Row {
                    Button(onClick = { onSaveState(1); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Save 1")
                    }
                    Spacer(Modifier.width(8.dp))
                    Button(onClick = { onSaveState(2); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Save 2")
                    }
                    Spacer(Modifier.width(8.dp))
                    Button(onClick = { onSaveState(3); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Save 3")
                    }
                }
                Row {
                    Button(onClick = { onLoadState(1); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Load 1")
                    }
                    Spacer(Modifier.width(8.dp))
                    Button(onClick = { onLoadState(2); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Load 2")
                    }
                    Spacer(Modifier.width(8.dp))
                    Button(onClick = { onLoadState(3); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Load 3")
                    }
                }
                Row {
                    TextButton(onClick = { onScreenshot(); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Screenshot")
                    }
                    TextButton(onClick = { onReset(); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Reset")
                    }
                    TextButton(onClick = { onCycleFilter(); onDismiss() }, modifier = Modifier.weight(1f)) {
                        Text("Filter")
                    }
                    TextButton(onClick = { onExit() }, modifier = Modifier.weight(1f)) {
                        Text("Exit")
                    }
                }
                TextButton(onClick = onDismiss, modifier = Modifier.fillMaxWidth()) {
                    Text("Resume game")
                }
            }
        }
    }

    // Physical gamepads / keyboards ------------------------------------------------
    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        val retroId = keyEventToRetroId(keyCode)
        if (retroId != null) {
            session?.setButton(retroId, true)
            return true
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        val retroId = keyEventToRetroId(keyCode)
        if (retroId != null) {
            session?.setButton(retroId, false)
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    private fun keyEventToRetroId(keyCode: Int): Int? = when (keyCode) {
        KeyEvent.KEYCODE_DPAD_UP -> RetroPadIds.UP
        KeyEvent.KEYCODE_DPAD_DOWN -> RetroPadIds.DOWN
        KeyEvent.KEYCODE_DPAD_LEFT -> RetroPadIds.LEFT
        KeyEvent.KEYCODE_DPAD_RIGHT -> RetroPadIds.RIGHT
        KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_Z -> RetroPadIds.B
        KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_X -> RetroPadIds.A
        KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_A -> RetroPadIds.Y
        KeyEvent.KEYCODE_BUTTON_X, KeyEvent.KEYCODE_S -> RetroPadIds.X
        KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ENTER -> RetroPadIds.START
        KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_SHIFT_LEFT -> RetroPadIds.SELECT
        KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_Q -> RetroPadIds.L
        KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_W -> RetroPadIds.R
        else -> null
    }

    override fun onDestroy() {
        super.onDestroy()
        session?.shutdown()
        session = null
    }
}

// SPDX-License-Identifier: MIT
package com.gecko.emulator.game

import android.content.Context
import android.graphics.Bitmap
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.view.Choreographer
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import com.gecko.emulator.native_.GeckoNative
import com.gecko.emulator.settings.SettingsRepositoryShared
import kotlin.concurrent.thread

/**
 * Drives one native session: core + content + frame loop + audio + input.
 *
 * The frame loop runs on a dedicated thread paced to the core's frame rate,
 * blitting each produced frame onto the SurfaceView's canvas.
 */
class EmulatorSession(
    context: Context,
    private val surfaceView: SurfaceView,
    corePath: String,
    gamePath: String,
    onStarted: (Boolean, String) -> Unit,
) : SurfaceHolder.Callback {

    private val appContext = context.applicationContext
    private var handle: Long = 0
    @Volatile private var running = false
    @Volatile private var surfaceReady = false
    private var audioTrack: AudioTrack? = null
    private var audioThread: Thread? = null
    private var loopThread: Thread? = null
    private var framePixels = IntArray(320 * 240)
    private val audioBuffer = ShortArray(2048 * 2)

    val repository = com.gecko.emulator.library.LibraryRepository(appContext)

    init {
        surfaceView.holder.addCallback(this)
        start(corePath, gamePath, onStarted)
    }

    private fun start(corePath: String, gamePath: String, onStarted: (Boolean, String) -> Unit) {
        handle = GeckoNative.create()
        val coreOk = GeckoNative.loadCore(handle, corePath)
        if (!coreOk) {
            onStarted(false, "Could not load core:\n$corePath")
            return
        }
        val gameOk = GeckoNative.loadGame(handle, gamePath)
        if (!gameOk) {
            onStarted(false, "Could not load game:\n$gamePath")
            return
        }

        val width = GeckoNative.frameWidth(handle).coerceAtLeast(1)
        val height = GeckoNative.frameHeight(handle).coerceAtLeast(1)
        framePixels = IntArray(width * height)

        running = true

        // Frame loop thread, paced by the native runner semantics: one frame
        // per tick with a sleep to the core's cadence.
        loopThread = thread(name = "gecko-frame-loop") {
            val frameNanos = (1_000_000_000L / 60).coerceAtLeast(1L)
            var next = System.nanoTime()
            while (running) {
                if (surfaceReady) {
                    GeckoNative.runFrame(handle)
                    blitFrame()
                }
                next += frameNanos
                val now = System.nanoTime()
                if (next > now) {
                    Thread.sleep((next - now) / 1_000_000)
                } else {
                    next = now // fell behind; catch up without sleep
                }
            }
        }

        startAudio()
        onStarted(true, GeckoNative.coreName(handle))
    }

    private fun blitFrame() {
        val surface: Surface = surfaceView.holder.surface ?: return
        if (!surface.isValid) return
        val count = GeckoNative.readFramePixels(handle, framePixels)
        if (count <= 0) return
        val width = GeckoNative.frameWidth(handle)
        val height = GeckoNative.frameHeight(handle)
        val canvas = surface.lockCanvas(null) ?: return
        try {
            val bitmap = Bitmap.createBitmap(framePixels, width, height, Bitmap.Config.ARGB_8888)
            canvas.drawBitmap(
                bitmap,
                null,
                android.graphics.RectF(0f, 0f, canvas.width.toFloat(), canvas.height.toFloat()),
                android.graphics.Paint(android.graphics.Paint.FILTER_BITMAP_FLAG).apply {
                    isFilterBitmap = SettingsRepositoryShared.smoothing
                },
            )
        } finally {
            surface.unlockCanvasAndPost(canvas)
        }
    }

    private fun startAudio() {
        val rate = GeckoNative.audioSampleRate(handle).toInt().takeIf { it > 8000 } ?: 48000
        val minBuf = AudioTrack.getMinBufferSize(
            rate,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_16BIT,
        )
        audioTrack = AudioTrack(
            AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build(),
            AudioFormat.Builder()
                .setSampleRate(rate)
                .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                .build(),
            maxOf(minBuf, rate), // >= 0.25 s
            AudioTrack.MODE_STREAM,
            AudioManager.AUDIO_SESSION_ID_GENERATE,
        )
        audioTrack?.play()
        val volume = SettingsRepositoryShared.volume
        audioTrack?.setVolume(volume / 100f)

        audioThread = thread(name = "gecko-audio") {
            while (running) {
                val frames = GeckoNative.readAudio(handle, audioBuffer, audioBuffer.size / 2)
                if (frames > 0) {
                    audioTrack?.write(audioBuffer, 0, frames * 2, AudioTrack.WRITE_BLOCKING)
                } else {
                    Thread.sleep(8)
                }
            }
        }
    }

    // --- controls ------------------------------------------------------------
    fun setButton(retroId: Int, pressed: Boolean) {
        if (handle != 0L) GeckoNative.setButton(handle, 0, retroId, pressed)
    }

    fun pause(paused: Boolean) {
        if (handle != 0L) GeckoNative.setPaused(handle, paused)
    }

    fun setSpeed(multiplier: Double) {
        if (handle != 0L) GeckoNative.setSpeed(handle, multiplier)
    }

    fun reset() {
        if (handle != 0L) GeckoNative.reset(handle)
    }

    fun measuredFps(): Double =
        if (handle != 0L) GeckoNative.measuredFps(handle) else 0.0

    fun saveState(slot: Int): Boolean =
        handle != 0L && GeckoNative.saveState(
            handle,
            repository.stateFile(currentTitle(), slot).absolutePath,
        )

    fun loadState(slot: Int): Boolean =
        handle != 0L && GeckoNative.loadState(
            handle,
            repository.stateFile(currentTitle(), slot).absolutePath,
        )

    fun screenshot(): Boolean {
        val frame = takeFrameBitmap() ?: return false
        val file = repository.screenshotFile(currentTitle())
        repository.screenshotsDir.mkdirs()
        file.outputStream().use { frame.compress(Bitmap.CompressFormat.PNG, 100, it) }
        return true
    }

    private fun takeFrameBitmap(): Bitmap? {
        val w = GeckoNative.frameWidth(handle)
        val h = GeckoNative.frameHeight(handle)
        if (w <= 0 || h <= 0) return null
        val pixels = IntArray(w * h)
        val count = GeckoNative.readFramePixels(handle, pixels)
        if (count <= 0) return null
        return Bitmap.createBitmap(pixels, w, h, Bitmap.Config.ARGB_8888)
    }

    private fun currentTitle(): String {
        val activity = surfaceView.context as? GameActivity
        return activity?.gameTitle ?: "game"
    }

    // --- SurfaceHolder.Callback ------------------------------------------------
    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceReady = true
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        surfaceReady = true
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        surfaceReady = false
    }

    fun shutdown() {
        running = false
        loopThread?.join(1500)
        audioThread?.join(1500)
        audioTrack?.stop()
        audioTrack?.release()
        if (handle != 0L) {
            GeckoNative.unloadGame(handle)
            GeckoNative.destroy(handle)
            handle = 0
        }
    }
}

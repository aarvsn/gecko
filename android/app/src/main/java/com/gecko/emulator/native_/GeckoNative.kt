// SPDX-License-Identifier: MIT
package com.gecko.emulator.native_

/**
 * JNI bindings to the Gecko native core (libgecko_native.so), which wraps the
 * platform-agnostic gecko-core libretro host library.
 *
 * Threading: [runFrame] is called from the game loop; audio is pulled from
 * the AudioTrack writer thread via [readAudio].
 */
object GeckoNative {

    init {
        System.loadLibrary("gecko_native")
    }

    // Lifecycle ---------------------------------------------------------------
    external fun create(): Long
    external fun destroy(handle: Long)

    // Core / content ------------------------------------------------------------
    external fun loadCore(handle: Long, path: String): Boolean
    external fun loadGame(handle: Long, path: String): Boolean
    external fun unloadGame(handle: Long)
    external fun coreName(handle: Long): String

    // Frame loop ------------------------------------------------------------------
    external fun runFrame(handle: Long)
    external fun frameWidth(handle: Long): Int
    external fun frameHeight(handle: Long): Int
    external fun readFramePixels(handle: Long, pixels: IntArray): Int
    external fun measuredFps(handle: Long): Double

    // Audio -----------------------------------------------------------------------
    external fun audioSampleRate(handle: Long): Double
    external fun readAudio(handle: Long, out: ShortArray, maxFrames: Int): Int

    // Input ------------------------------------------------------------------------
    external fun setButton(handle: Long, port: Int, retroId: Int, pressed: Boolean)

    // States ------------------------------------------------------------------------
    external fun saveState(handle: Long, path: String): Boolean
    external fun loadState(handle: Long, path: String): Boolean
    external fun reset(handle: Long)
    external fun setPaused(handle: Long, paused: Boolean)
    external fun setSpeed(handle: Long, multiplier: Double)
}

/** libretro joypad button ids (mirrors core/include/gecko/libretro.h). */
object RetroPadIds {
    const val B = 0
    const val Y = 1
    const val SELECT = 2
    const val START = 3
    const val UP = 4
    const val DOWN = 5
    const val LEFT = 6
    const val RIGHT = 7
    const val A = 8
    const val X = 9
    const val L = 10
    const val R = 11
    const val L2 = 12
    const val R2 = 13
    const val L3 = 14
    const val R3 = 15
}

// SPDX-License-Identifier: MIT
package com.gecko.emulator.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Gecko brand greens.
val GeckoGreen = Color(0xFF3AA657)
val GeckoGreenDark = Color(0xFF1B5E20)
val GeckoGreenContainer = Color(0xFFB7EFC0)
val GeckoSurfaceDark = Color(0xFF121511)

private val LightColors = lightColorScheme(
    primary = GeckoGreenDark,
    onPrimary = Color.White,
    primaryContainer = GeckoGreenContainer,
    onPrimaryContainer = Color(0xFF00210A),
    secondary = Color(0xFF52634F),
)

private val DarkColors = darkColorScheme(
    primary = Color(0xFF8CE596),
    onPrimary = Color(0xFF003912),
    primaryContainer = GeckoGreenDark,
    onPrimaryContainer = GeckoGreenContainer,
    secondary = Color(0xFFBBC9B4),
    surface = GeckoSurfaceDark,
)

@Composable
fun GeckoTheme(content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    MaterialTheme(
        colorScheme = if (dark) DarkColors else LightColors,
        content = content,
    )
}

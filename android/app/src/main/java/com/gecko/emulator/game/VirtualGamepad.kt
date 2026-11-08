// SPDX-License-Identifier: MIT
package com.gecko.emulator.game

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.gecko.emulator.native_.RetroPadIds

/**
 * Translucent virtual gamepad: D-pad (left), Start/Select (center),
 * A/B/X/Y (right). Press state is fed straight into the native input.
 */
@Composable
fun VirtualGamepad(
    modifier: Modifier = Modifier,
    onButton: (retroId: Int, pressed: Boolean) -> Unit,
) {
    Row(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 18.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        // D-pad.
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            PadButton(label = "▲", retroId = RetroPadIds.UP, onButton = onButton)
            Row {
                PadButton(label = "◀", retroId = RetroPadIds.LEFT, onButton = onButton)
                Spacer(Modifier.width(64.dp))
                PadButton(label = "▶", retroId = RetroPadIds.RIGHT, onButton = onButton)
            }
            PadButton(label = "▼", retroId = RetroPadIds.DOWN, onButton = onButton)
        }

        Spacer(Modifier.weight(1f))

        // Select / Start.
        Row {
            PadButton(label = "SEL", retroId = RetroPadIds.SELECT, onButton = onButton, small = true)
            Spacer(Modifier.width(14.dp))
            PadButton(label = "STA", retroId = RetroPadIds.START, onButton = onButton, small = true)
        }

        Spacer(Modifier.weight(1f))

        // A / B / X / Y diamond.
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            PadButton(label = "X", retroId = RetroPadIds.X, onButton = onButton)
            Row {
                PadButton(label = "Y", retroId = RetroPadIds.Y, onButton = onButton)
                Spacer(Modifier.width(64.dp))
                PadButton(label = "A", retroId = RetroPadIds.A, onButton = onButton)
            }
            PadButton(label = "B", retroId = RetroPadIds.B, onButton = onButton)
        }
    }
}

@Composable
private fun PadButton(
    label: String,
    retroId: Int,
    onButton: (retroId: Int, pressed: Boolean) -> Unit,
    small: Boolean = false,
) {
    val interaction = remember { MutableInteractionSource() }
    val pressed by interaction.collectIsPressedAsState()

    // Report press transitions to the native input layer.
    LaunchedEffect(pressed) {
        onButton(retroId, pressed)
    }

    val size = if (small) 44.dp else 56.dp
    Box(
        modifier = Modifier
            .size(size)
            .background(
                color = if (pressed) Color(0xCC8CE596) else Color(0x66000000),
                shape = CircleShape,
            )
            .clickable(
                interactionSource = interaction,
                indication = null,
                onClick = {},
            ),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = label,
            color = if (pressed) Color(0xFF10240F) else Color(0xE6FFFFFF),
            style = MaterialTheme.typography.labelMedium,
        )
    }
}

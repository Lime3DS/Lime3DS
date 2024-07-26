// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.graphics.Bitmap
import android.widget.ImageView
import androidx.core.graphics.drawable.toDrawable
import androidx.fragment.app.FragmentActivity
import coil.ImageLoader
import coil.decode.DataSource
import coil.fetch.DrawableResult
import coil.fetch.FetchResult
import coil.fetch.Fetcher
import coil.key.Keyer
import coil.memory.MemoryCache
import coil.request.ImageRequest
import coil.request.Options
import coil.transform.RoundedCornersTransformation
import io.github.lime3ds.android.R
import io.github.lime3ds.android.model.Game
import java.nio.IntBuffer

class GameIconFetcher(
    private val game: Game,
    private val options: Options
) : Fetcher {
    override suspend fun fetch(): FetchResult {
        return DrawableResult(
            drawable = getGameIcon(game.icon)!!.toDrawable(options.context.resources),
            isSampled = false,
            dataSource = DataSource.DISK
        )
    }

    private fun getGameIcon(vector: IntArray?): Bitmap? {
        val bitmap = Bitmap.createBitmap(48, 48, Bitmap.Config.RGB_565)
        bitmap.copyPixelsFromBuffer(IntBuffer.wrap(vector))
        return bitmap
    }

    class Factory : Fetcher.Factory<Game> {
        override fun create(data: Game, options: Options, imageLoader: ImageLoader): Fetcher =
            GameIconFetcher(data, options)
    }
}

class GameIconKeyer : Keyer<Game> {
    override fun key(data: Game, options: Options): String = data.path
}

object GameIconUtils {
    fun loadGameIcon(activity: FragmentActivity, game: Game, imageView: ImageView) {
        val imageLoader = ImageLoader.Builder(activity)
            .components {
                add(GameIconKeyer())
                add(GameIconFetcher.Factory())
            }
            .memoryCache {
                MemoryCache.Builder(activity)
                    .maxSizePercent(0.25)
                    .build()
            }
            .build()

        val request = ImageRequest.Builder(activity)
            .data(game)
            .target(imageView)
            .error(R.drawable.no_icon)
            .transformations(
                RoundedCornersTransformation(
                    activity.resources.getDimensionPixelSize(R.dimen.spacing_med).toFloat()
                )
            )
            .build()
        imageLoader.enqueue(request)
    }
}

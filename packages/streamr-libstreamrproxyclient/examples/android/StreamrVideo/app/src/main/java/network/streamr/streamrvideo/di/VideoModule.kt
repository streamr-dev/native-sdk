package network.streamr.streamrvideo.di

import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import network.streamr.streamrvideo.data.encoder.VideoEncoder
import network.streamr.streamrvideo.data.encoder.VideoEncoderImpl
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class VideoModule {
    @Binds
    @Singleton
    abstract fun bindVideoEncoder(impl: VideoEncoderImpl): VideoEncoder
} 
package network.streamr.streamrvideo.di

import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import network.streamr.streamrvideo.data.repository.builder.MediaPacketBuilder
import network.streamr.streamrvideo.data.repository.builder.MediaPacketFragmentBuilder
import network.streamr.streamrvideo.data.repository.builder.VideoFrameBuilder
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object BuilderModule {
    
    @Provides
    @Singleton
    fun provideVideoFrameBuilder(): VideoFrameBuilder {
        return VideoFrameBuilder()
    }

    @Provides
    @Singleton
    fun provideMediaPacketBuilder(): MediaPacketBuilder {
        return MediaPacketBuilder()
    }

    @Provides
    @Singleton
    fun provideMediaPacketFragmentBuilder(): MediaPacketFragmentBuilder {
        return MediaPacketFragmentBuilder()
    }
} 
package network.streamr.streamrvideo.di

import android.content.Context
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import network.streamr.streamrvideo.data.camera.AndroidCameraManagerImpl
import network.streamr.streamrvideo.data.camera.CameraManager
import network.streamr.streamrvideo.data.repository.VideoRepository
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object CameraModule {
    @Provides
    @Singleton
    fun provideCameraManager(
        context: Context,
        videoRepository: VideoRepository
    ): CameraManager {
        return AndroidCameraManagerImpl(context, videoRepository)
    }
}
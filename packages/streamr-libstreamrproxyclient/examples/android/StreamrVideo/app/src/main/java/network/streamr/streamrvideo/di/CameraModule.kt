package network.streamr.streamrvideo.di

import dagger.Binds
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import network.streamr.streamrvideo.data.camera.CameraManager
import network.streamr.streamrvideo.data.camera.CameraManagerImpl
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class CameraModule {
    @Binds
    @Singleton
    abstract fun bindCameraManager(
        cameraManagerImpl: CameraManagerImpl
    ): CameraManager
}
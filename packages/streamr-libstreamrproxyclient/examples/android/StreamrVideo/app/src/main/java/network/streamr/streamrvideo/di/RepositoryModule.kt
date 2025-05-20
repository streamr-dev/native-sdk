package network.streamr.streamrvideo.di

import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import network.streamr.streamrvideo.data.repository.VideoRepository
import network.streamr.streamrvideo.data.repository.VideoRepositoryImpl
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class RepositoryModule {
    
    @Binds
    @Singleton
    abstract fun bindVideoRepository(
        videoRepositoryImpl: VideoRepositoryImpl
    ): VideoRepository
}
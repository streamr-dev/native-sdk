package network.streamr.streamrvideo.di

import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import network.streamr.proxyclient.StreamrProxyClientCoro
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object ProxyClientModule {
    @Provides
    @Singleton
    fun provideProxyClient(): StreamrProxyClientCoro {
        return StreamrProxyClientCoro()
    }
} 
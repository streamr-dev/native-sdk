package network.streamr.streamrvideo.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.util.Log
import androidx.core.app.NotificationCompat
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import network.streamr.proxyclient.StreamrProxyAddress
import network.streamr.proxyclient.StreamrProxyClientCoro
import network.streamr.streamrvideo.data.repository.VideoRepository
import network.streamr.streamrvideo.feature.videostream.VideoStreamActivity
import network.streamr.streamrvideo.R
import network.streamr.streamrvideo.data.repository.SettingsRepository
import javax.inject.Inject

@AndroidEntryPoint
class VideoStreamService : Service() {
    @Inject
    lateinit var videoRepository: VideoRepository
    
    @Inject
    lateinit var proxyClient: StreamrProxyClientCoro

    @Inject
    lateinit var settingsRepository: SettingsRepository

    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    
    private val wakeLock: PowerManager.WakeLock by lazy {
        (getSystemService(Context.POWER_SERVICE) as PowerManager).run {
            newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "StreamrVideo::VideoStreamWakeLock")
        }
    }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    
    override fun onCreate() {
        Log.d(TAG, "onCreate called")
        super.onCreate()
        createNotificationChannel()
        startForeground(NOTIFICATION_ID, createNotification())
        wakeLock.acquire()

        scope.launch {
            // Collect all settings
            settingsRepository.privateKey.collect { privateKey ->
                // Store for use in onStartCommand
                currentPrivateKey = privateKey
            }
        }
    }

    private var currentPrivateKey: String = ""

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> {
                serviceScope.launch {
                    proxyClient.initialize(settingsRepository.localAddress.value,
                        settingsRepository.streamPartId.value)
                    val connectResult = proxyClient.connect(listOf(StreamrProxyAddress(settingsRepository.proxyId.value, settingsRepository.proxyAddress.value)))
                    Log.d(TAG, "proxyClient.connect result: ${connectResult}")

                    var lastPacketNumber = -1L
                    var lastFragmentNumber = -1
                    
                    videoRepository.getMediaPacketFragmentFlow().collect { fragment ->
                        // Check for skipped fragments

                        if (fragment.packetNumber == lastPacketNumber && 
                            fragment.fragmentNumber > lastFragmentNumber + 1) {
                            Log.w(TAG, "Skipped fragments in packet ${fragment.packetNumber}: " +
                                "from ${lastFragmentNumber + 1} to ${fragment.fragmentNumber - 1}")
                        } else if (fragment.packetNumber > lastPacketNumber + 1) {
                            Log.w(TAG, "Skipped packets: from ${lastPacketNumber + 1} to ${fragment.packetNumber - 1}")
                        }

                        // Update tracking variables
                        lastPacketNumber = fragment.packetNumber
                        lastFragmentNumber = fragment.fragmentNumber

                        // Process fragment
                        val publishResult = proxyClient.publish(fragment.toByteArray(), settingsRepository.privateKey.value)
                        if (publishResult.failed.size > 0) {
                            Log.w(TAG, "Failed to publish fragment: packetNumber=${fragment.packetNumber}, fragmentNumber=${fragment.fragmentNumber}")
                        }
                        
                    }
                }
            }
            ACTION_STOP -> {
                proxyClient.close()
                stopSelf()
            }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        super.onDestroy()
        serviceScope.cancel()
        wakeLock.release()
        videoRepository.release()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Video Stream Service",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Running video stream service"
            setShowBadge(false)
        }
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager.createNotificationChannel(channel)
    }

    private fun createNotification(): Notification {
        val pendingIntent = Intent(this, VideoStreamActivity::class.java).let { notificationIntent ->
            PendingIntent.getActivity(
                this, 0, notificationIntent,
                PendingIntent.FLAG_IMMUTABLE
            )
        }

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Streaming Video")
            .setContentText("Video stream is active")
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setContentIntent(pendingIntent)
            .setSilent(true)
            .setOngoing(true)
            .build()
    }

    companion object {
        private const val TAG = "VideoStreamService"
        private const val NOTIFICATION_ID = 1
        private const val CHANNEL_ID = "video_stream_channel"
        const val ACTION_START = "action_start"
        const val ACTION_STOP = "action_stop"

        fun startService(context: Context) {
            val intent = Intent(context, VideoStreamService::class.java).apply {
                action = ACTION_START
            }
            context.startForegroundService(intent)
        }

        fun stopService(context: Context) {
            val intent = Intent(context, VideoStreamService::class.java).apply {
                action = ACTION_STOP
            }
            context.stopService(intent)
        }
    }
} 

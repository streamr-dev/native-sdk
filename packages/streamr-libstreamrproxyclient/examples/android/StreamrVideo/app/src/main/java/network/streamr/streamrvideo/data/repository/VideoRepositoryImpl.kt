package network.streamr.streamrvideo.data.repository

import android.util.Log
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.withContext
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import network.streamr.streamrvideo.data.encoder.VideoEncoder
import network.streamr.streamrvideo.data.model.proto.MediaPacketFragment
import network.streamr.streamrvideo.data.repository.builder.VideoFrameBuilder
import network.streamr.streamrvideo.data.repository.builder.MediaPacketBuilder
import network.streamr.streamrvideo.data.repository.builder.MediaPacketFragmentBuilder
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class VideoRepositoryImpl @Inject constructor(
    private val mediaCodecWrapper: VideoEncoder,
    private val mediaPacketFragmentBuilder: MediaPacketFragmentBuilder,
    private val videoFrameBuilder: VideoFrameBuilder,
    private val mediaPacketBuilder: MediaPacketBuilder
) : VideoRepository {
    
    private val fragmentFlow = MutableSharedFlow<MediaPacketFragment>(
        replay = 0,
        extraBufferCapacity = 1000,
        onBufferOverflow = BufferOverflow.SUSPEND
    )

    // Single-threaded context for all encoding operations
    private val encodingContext = Dispatchers.Default.limitedParallelism(1)
    private val repositoryScope = CoroutineScope(SupervisorJob() + encodingContext)

    companion object {
        private const val TAG = "VideoRepositoryImpl"
    }

    override fun initializeEncoder(width: Int, height: Int) {
        // Initialize encoder on the encoding thread
        repositoryScope.launch {
            mediaCodecWrapper.initializeEncoder(width, height) { encodedData, presentationTimeUs, flags ->
                repositoryScope.launch {
                    handleEncodedFrame(encodedData, presentationTimeUs, flags)
                }
            }
        }
    }

    override suspend fun encodeFrame(nv21Data: ByteArray) {
        try {
            // Switch to encoding thread
            withContext(encodingContext) {
                mediaCodecWrapper.encodeFrame(nv21Data)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error encoding frame", e)
        }
    }

    override fun getMediaPacketFragmentFlow(): Flow<MediaPacketFragment> = fragmentFlow

    private suspend fun handleEncodedFrame(
        encodedData: ByteArray,
        presentationTimeUs: Long,
        flags: Int
    ) {
        val extradata = mediaCodecWrapper.getExtraData() ?: ByteArray(0)
        val vFrame = videoFrameBuilder.build(
            flags = flags,
            presentationTimeUs = presentationTimeUs,
            encodedData = encodedData,
            extradataBytes = extradata,
            width = mediaCodecWrapper.getWidth(),
            height = mediaCodecWrapper.getHeight(),
            codecString = mediaCodecWrapper.getCodecString()
        )


        Log.d(TAG, "VideoFrame: number=${vFrame.frameNumber}, " +
            "timestamp=${vFrame.timestamp}, " +
            "duration=${vFrame.duration}, " +
            "isKeyFrame=${vFrame.isKeyFrame}, " +
            "codec=${vFrame.codec}, " +
            "size=${vFrame.data.size()} bytes")

        val mediaPacket = mediaPacketBuilder.build(
            videoFrame = vFrame
        )

        // Process fragments sequentially with suspending emit
        for (fragment in mediaPacketFragmentBuilder.build(mediaPacket)) {
            try {
                fragmentFlow.emit(fragment)  // Will suspend if buffer is full
            } catch (e: Exception) {
                Log.e(TAG, "Failed to emit fragment ${fragment.fragmentNumber}", e)
            }
        }
    }

    override fun release() {
        repositoryScope.launch {
            mediaCodecWrapper.release()
        }
        repositoryScope.cancel()
    }
}

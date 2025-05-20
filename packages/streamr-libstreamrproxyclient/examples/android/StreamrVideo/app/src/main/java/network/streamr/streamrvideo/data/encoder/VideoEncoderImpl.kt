package network.streamr.streamrvideo.data.encoder

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.util.Log
import javax.inject.Inject
import javax.inject.Singleton
import java.nio.ByteBuffer

@Singleton
class VideoEncoderImpl @Inject constructor() : VideoEncoder {
    private lateinit var encodedFrameCallback: ((ByteArray, Long, Int) -> Unit)
    private var mediaCodec: MediaCodec? = null
    private var mediaFormat: MediaFormat? = null
    private val bufferInfo = MediaCodec.BufferInfo()
    private var startTimeNs = -1L
    private var extraData: ByteArray? = null
    private var width: Int = 0
    private var height: Int = 0
    private var isReleased = false

    companion object {
        private const val TAG = "VideoEncoderImpl"
        private const val TIMEOUT_USEC = 10_000L
        private const val NANOS_PER_SECOND = 1_000_000_000L
        private const val MICROS_PER_SECOND = 1_000_000L
        
        // Switch to HEVC/H.265
        private const val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_HEVC
        private const val PROFILE = MediaCodecInfo.CodecProfileLevel.  // Main profile
        private const val LEVEL = MediaCodecInfo.CodecProfileLevel.HEVCMainTierLevel31

        private fun convertToWebCodecsCodecString(mimeType: String, profile: Int, level: Int): String {
            return when (mimeType) {
                MediaFormat.MIMETYPE_VIDEO_HEVC -> {
                    // H.265/HEVC
                    String.format("hev1.%d.%d.L%d.B%d",
                        (profile shr 16) and 0xFF,  // profile_space
                        profile and 0xFF,           // profile_idc
                        level,                      // level_idc
                        (profile shr 8) and 0xFF    // compatibility_flags
                    )
                }
                MediaFormat.MIMETYPE_VIDEO_VP8 -> "vp8"
                MediaFormat.MIMETYPE_VIDEO_VP9 -> "vp9"
                MediaFormat.MIMETYPE_VIDEO_AV1 -> "av1"
                else -> throw IllegalArgumentException("Unsupported mime type: $mimeType")
            }
        }
    }

    private fun computePresentationTimeUs(): Long {
        if (startTimeNs == -1L) {
            startTimeNs = System.nanoTime()
            return 0
        }
        
        val nowNs = System.nanoTime()
        val durationNs = nowNs - startTimeNs
        return (durationNs * MICROS_PER_SECOND) / NANOS_PER_SECOND
    }

    private fun createExtraData(mediaFormat: MediaFormat): ByteArray {
        // Get VPS, SPS, PPS buffers
        val vps = mediaFormat.getByteBuffer("csd-0")?.let { buffer ->
            ByteArray(buffer.remaining()).also { buffer.get(it) }
        } ?: ByteArray(0)
        
        val sps = mediaFormat.getByteBuffer("csd-1")?.let { buffer ->
            ByteArray(buffer.remaining()).also { buffer.get(it) }
        } ?: ByteArray(0)
        
        val pps = mediaFormat.getByteBuffer("csd-2")?.let { buffer ->
            ByteArray(buffer.remaining()).also { buffer.get(it) }
        } ?: ByteArray(0)

        return ByteBuffer.allocate(4 + vps.size + 4 + sps.size + 4 + pps.size).apply {
            putInt(vps.size)
            put(vps)
            putInt(sps.size)
            put(sps)
            putInt(pps.size)
            put(pps)
        }.array()
    }

    override fun initializeEncoder(
        width: Int, 
        height: Int,
        callback: (ByteArray, Long, Int) -> Unit
    ) {
        Log.d(TAG, "Initializing encoder with width=$width, height=$height")
        this.width = width
        this.height = height
        encodedFrameCallback = callback
        try {
            val format = MediaFormat.createVideoFormat(MIME_TYPE, width, height).apply {
                setInteger(MediaFormat.KEY_BIT_RATE, 2_000_000)
                setInteger(MediaFormat.KEY_FRAME_RATE, 30)
                setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
                setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar)
                setInteger(MediaFormat.KEY_PROFILE, PROFILE)
                setInteger(MediaFormat.KEY_LEVEL, LEVEL)
            }

            Log.d(TAG, "Created format: $format")
            
            val codec = MediaCodec.createEncoderByType(MIME_TYPE)
            Log.d(TAG, "Created encoder for MIME type: $MIME_TYPE")
            
            codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            Log.d(TAG, "Configured encoder")
            
            codec.start()
            Log.d(TAG, "Started encoder")
            
            mediaCodec = codec
            mediaFormat = format

            // Get and store extra data after configuration
            extraData = createExtraData(format)
            Log.d(TAG, "Extra data size: ${extraData?.size}")
            
            // Log the codec string
            Log.d(TAG, "Codec string: ${getCodecString()}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize encoder", e)
            mediaCodec = null
            mediaFormat = null
        }
    }

    override suspend fun encodeFrame(nv21Data: ByteArray) {
        if (isReleased) {
            Log.w(TAG, "Encoder is released, skipping frame")
            return
        }

        try {
            val codec = mediaCodec ?: run {
                Log.w(TAG, "MediaCodec is null, skipping frame")
                return
            }

            // Queue input buffer
            val inputBufferIndex = codec.dequeueInputBuffer(TIMEOUT_USEC)
            if (inputBufferIndex >= 0) {
                val inputBuffer = codec.getInputBuffer(inputBufferIndex)
                inputBuffer?.put(nv21Data)
                codec.queueInputBuffer(inputBufferIndex, 0, nv21Data.size,
                    computePresentationTimeUs(), 0)
            }

            // Get output buffer
            var outputBufferIndex: Int
            do {
                outputBufferIndex = codec.dequeueOutputBuffer(bufferInfo, TIMEOUT_USEC)
                when {
                    outputBufferIndex >= 0 -> {
                        val outputBuffer = codec.getOutputBuffer(outputBufferIndex)
                        outputBuffer?.let {
                            val encodedData = ByteArray(bufferInfo.size)
                            it.get(encodedData)
                            encodedFrameCallback(encodedData, bufferInfo.presentationTimeUs, bufferInfo.flags)
                            codec.releaseOutputBuffer(outputBufferIndex, false)
                        }
                    }
                    outputBufferIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                        // Store updated format
                        mediaFormat = codec.outputFormat
                        Log.d(TAG, "Output format changed to: ${codec.outputFormat}")
                    }
                    outputBufferIndex == MediaCodec.INFO_TRY_AGAIN_LATER -> {
                        // No output available yet
                        break
                    }
                }
            } while (outputBufferIndex >= 0)

        } catch (e: Exception) {
            Log.e(TAG, "Error encoding frame", e)
        }
    }

    override fun release() {
        isReleased = true
        try {
            mediaCodec?.apply {
                stop()
                release()
            }
            mediaCodec = null
            mediaFormat = null
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing encoder", e)
        }
    }

    override fun getExtraData(): ByteArray? = extraData
    override fun getWidth(): Int = width
    override fun getHeight(): Int = height
    override fun getCodecString(): String {
        return mediaFormat?.let { format ->
            val mimeType = format.getString(MediaFormat.KEY_MIME)
            // Get the profile and level if available
            val profile = try {
                format.getInteger(MediaFormat.KEY_PROFILE)
            } catch (e: Exception) {
                PROFILE // Fall back to default profile
            }
            val level = try {
                format.getInteger(MediaFormat.KEY_LEVEL)
            } catch (e: Exception) {
                LEVEL // Fall back to default level
            }
            convertToWebCodecsCodecString(mimeType!!, profile, level)
        } ?: "hev1.1.6.L93.B0" // Default fallback
    }
}

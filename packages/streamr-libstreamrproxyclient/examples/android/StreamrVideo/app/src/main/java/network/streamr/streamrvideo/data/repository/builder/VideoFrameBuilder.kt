package network.streamr.streamrvideo.data.repository.builder

import android.media.MediaCodec
import com.google.protobuf.ByteString
import network.streamr.streamrvideo.data.model.proto.VideoFrame
import javax.inject.Singleton

@Singleton
class VideoFrameBuilder {
    var frameNumberCounter = 0L
    private var lastTimestampUs = 0L

    companion object {
        private const val MIME_TYPE = "video/avc" // H.264/AVC
    }

    fun build(
        flags: Int,
        presentationTimeUs: Long,
        encodedData: ByteArray,
        extradataBytes: ByteArray,
        width: Int,
        height: Int,
        codecString: String
    ): VideoFrame {
        // Calculate frame duration in microseconds
        val durationUs = if (lastTimestampUs > 0) {
            presentationTimeUs - lastTimestampUs
        } else {
            0
        }
        lastTimestampUs = presentationTimeUs

        return VideoFrame.newBuilder().apply {
            this.frameNumber = frameNumberCounter++
            timestamp = presentationTimeUs
            this.isKeyFrame = (flags and MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0
            data = ByteString.copyFrom(encodedData)
            codec = MIME_TYPE
            duration = durationUs
            screenOrientation = 0
            extradata = ByteString.copyFrom(extradataBytes)
            codedWidth = width
            codedHeight = height
            codec = codecString
            isScreenSharing = false
            videoOrientation = 0
        }.build()
    }

} 
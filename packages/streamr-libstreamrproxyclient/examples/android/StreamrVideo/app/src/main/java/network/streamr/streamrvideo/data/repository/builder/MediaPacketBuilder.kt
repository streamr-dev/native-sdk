package network.streamr.streamrvideo.data.repository.builder

import network.streamr.streamrvideo.data.model.proto.MediaPacket
import network.streamr.streamrvideo.data.model.proto.VideoFrame
import javax.inject.Singleton

@Singleton
class MediaPacketBuilder {
    var packetNumberCounter = 0L
    private val mStreamId: String = generateUniqueStreamId()

    companion object {
        private fun generateUniqueStreamId(): String {
            return System.currentTimeMillis().toString(16) + // Timestamp in hex
                   (0..0xFFFF).random().toString(16) // Random suffix in hex
        }
    }

    fun build(
        videoFrame: VideoFrame
    ): MediaPacket = MediaPacket.newBuilder().apply {
        this.packetNumber = packetNumberCounter++
        addVideoFrames(videoFrame)
        this.mediaStreamId = mStreamId
    }.build()
} 
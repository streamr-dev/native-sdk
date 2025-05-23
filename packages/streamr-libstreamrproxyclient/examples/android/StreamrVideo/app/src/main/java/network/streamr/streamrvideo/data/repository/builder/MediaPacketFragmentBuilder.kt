package network.streamr.streamrvideo.data.repository.builder

import network.streamr.streamrvideo.data.model.proto.MediaPacket
import network.streamr.streamrvideo.data.model.proto.MediaPacketFragment
import javax.inject.Inject
import javax.inject.Singleton
import com.google.protobuf.ByteString

@Singleton
class MediaPacketFragmentBuilder @Inject constructor() {
    private var packetNumberCounter: Long = 0

    companion object {
        private const val MAX_FRAGMENT_SIZE = 64000
    }

    fun build(mediaPacket: MediaPacket): List<MediaPacketFragment> {
        val packetData = mediaPacket.toByteArray()
        val packetNumber = packetNumberCounter++

        // If the packet is small enough, return it as a single fragment
        if (packetData.size <= MAX_FRAGMENT_SIZE) {
            return listOf(
                MediaPacketFragment.newBuilder().apply {
                    this.packetNumber = packetNumber
                    fragmentNumber = 0
                    numberOfFragments = 1
                    data = com.google.protobuf.ByteString.copyFrom(packetData)
                }.build()
            )
        }

        // Calculate number of fragments needed
        val numberOfFragments = kotlin.math.ceil(packetData.size.toDouble() / MAX_FRAGMENT_SIZE).toInt()

        // Split the packet data into fragments
        return (0 until numberOfFragments).map { fragmentNumber ->
            val start = fragmentNumber * MAX_FRAGMENT_SIZE
            val end = minOf(start + MAX_FRAGMENT_SIZE, packetData.size)
            val fragmentData = packetData.copyOfRange(start, end)

            MediaPacketFragment.newBuilder()
                .setPacketNumber(packetNumber)
                .setFragmentNumber(fragmentNumber)
                .setNumberOfFragments(numberOfFragments)
                .setData(ByteString.copyFrom(fragmentData))
                .build()
        }
    }
} 
package network.streamr.streamrvideo.data.repository

import kotlinx.coroutines.flow.Flow
import network.streamr.streamrvideo.data.model.proto.MediaPacketFragment
import com.serenegiant.usb.widget.CameraViewInterface
import android.app.Activity

interface VideoRepository {
    fun getMediaPacketFragmentFlow(): Flow<MediaPacketFragment>
    suspend fun encodeFrame(nv21Data: ByteArray)
    fun initializeEncoder(width: Int, height: Int)
    fun release()
}
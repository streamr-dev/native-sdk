package network.streamr.streamrvideo.data.encoder

interface VideoEncoder {
    fun initializeEncoder(width: Int, height: Int, callback: (ByteArray, Long, Int) -> Unit)
    suspend fun encodeFrame(nv21Data: ByteArray)
    fun release()
    fun getExtraData(): ByteArray?
    fun getWidth(): Int
    fun getHeight(): Int
    fun getCodecString(): String
} 
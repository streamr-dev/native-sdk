package network.streamr.streamrvideo.data.encoder

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.util.Log
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import kotlinx.coroutines.test.runTest
import org.junit.Before
import org.junit.Test
import java.nio.ByteBuffer
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class VideoEncoderImplTest {
    
    private lateinit var encoder: VideoEncoderImpl
    private lateinit var mockMediaCodec: MediaCodec
    private lateinit var mockFormat: MediaFormat
    
    @Before
    fun setup() {
        // Mock Android Log
        mockkStatic(Log::class)
        every { Log.d(any(), any()) } returns 0
        every { Log.e(any(), any()) } returns 0
        every { Log.e(any(), any(), any()) } returns 0
        
        // Mock MediaFormat
        mockkStatic(MediaFormat::class)
        mockFormat = mockk(relaxed = true)
        every { MediaFormat.createVideoFormat(any(), any(), any()) } returns mockFormat
        
        // Mock MediaCodec
        mockkStatic(MediaCodec::class)
        mockMediaCodec = mockk(relaxed = true)
        every { MediaCodec.createEncoderByType(any()) } returns mockMediaCodec
        
        // Mock format configuration
        every { mockFormat.setInteger(MediaFormat.KEY_BIT_RATE, any()) } returns Unit
        every { mockFormat.setInteger(MediaFormat.KEY_FRAME_RATE, any()) } returns Unit
        every { mockFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, any()) } returns Unit
        every { mockFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT, any()) } returns Unit
        every { mockFormat.setInteger(MediaFormat.KEY_PROFILE, MediaCodecInfo.CodecProfileLevel.AVCProfileBaseline) } returns Unit
        every { mockFormat.setInteger(MediaFormat.KEY_LEVEL, MediaCodecInfo.CodecProfileLevel.AVCLevel31) } returns Unit
        
        // Mock codec configuration
        every { mockMediaCodec.configure(any(), null, null, MediaCodec.CONFIGURE_FLAG_ENCODE) } returns Unit
        every { mockMediaCodec.start() } returns Unit
        
        // Mock buffers
        val mockInputBuffer = ByteBuffer.allocate(1024)
        val mockOutputBuffer = ByteBuffer.allocate(1024).apply {
            put(ByteArray(10) { 1 }) // Add some test data
            flip()
        }
        every { mockMediaCodec.getInputBuffer(any()) } returns mockInputBuffer
        every { mockMediaCodec.getOutputBuffer(any()) } returns mockOutputBuffer
        every { mockMediaCodec.dequeueInputBuffer(any()) } returns 0
        every { mockMediaCodec.dequeueOutputBuffer(any(), any()) } returns 0
        
        // Mock codec specific data (CSD buffers)
        val csd0 = ByteBuffer.allocate(10).apply {
            put(ByteArray(10) { 1 })
            flip()
        }
        val csd1 = ByteBuffer.allocate(10).apply {
            put(ByteArray(10) { 2 })
            flip()
        }
        every { mockFormat.getByteBuffer("csd-0") } returns csd0
        every { mockFormat.getByteBuffer("csd-1") } returns csd1
        
        encoder = VideoEncoderImpl()
    }

    @Test
    fun `test codec string generation`() {
        // Given
        val width = 640
        val height = 480
        
        // When
        encoder.initializeEncoder(width, height) { _, _, _ -> }
        val codecString = encoder.getCodecString()
        
        // Then
        assertEquals("hev1.1.6.L93.B0", codecString)
    }

    @Test
    fun `test frame encoding`() = runTest {
        // Given
        val width = 640
        val height = 480
        val testFrame = ByteArray(width * height * 3 / 2) // NV21 format
        var capturedData: ByteArray? = null
        var capturedTime: Long = 0
        var capturedFlags: Int = 0
        
        // When
        encoder.initializeEncoder(width, height) { data, time, flags ->
            capturedData = data
            capturedTime = time
            capturedFlags = flags
        }
        
        encoder.encodeFrame(testFrame)
        
        // Then
        assertNotNull(capturedData)
        assert(capturedTime >= 0)
    }

    @Test
    fun `test extradata generation`() {
        // Given
        val width = 640
        val height = 480
        val mockFormat = mockk<MediaFormat>(relaxed = true)
        val mockCsd0 = ByteBuffer.allocate(10)
        val mockCsd1 = ByteBuffer.allocate(10)
        
        every { mockFormat.getByteBuffer("csd-0") } returns mockCsd0
        every { mockFormat.getByteBuffer("csd-1") } returns mockCsd1
        
        // When
        encoder.initializeEncoder(width, height) { _, _, _ -> }
        val extraData = encoder.getExtraData()
        
        // Then
        assertNotNull(extraData)
    }

    @Test
    fun `test encoder dimensions`() {
        // Given
        val width = 640
        val height = 480
        
        // When
        encoder.initializeEncoder(width, height) { _, _, _ -> }
        
        // Then
        assertEquals(width, encoder.getWidth())
        assertEquals(height, encoder.getHeight())
    }
} 
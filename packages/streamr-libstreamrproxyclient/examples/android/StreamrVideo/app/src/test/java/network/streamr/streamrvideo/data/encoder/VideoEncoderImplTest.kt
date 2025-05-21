package network.streamr.streamrvideo.data.encoder

import android.media.MediaCodec
import android.media.MediaCodec.MetricsConstants.MIME_TYPE
import android.media.MediaCodecInfo
import android.media.MediaCrypto
import android.media.MediaFormat
import android.util.Log
import android.view.Surface
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
        // Mock Android Log with specific matchers
        mockkStatic(Log::class)
        every { Log.d(any<String>(), any<String>()) } returns 0
        every { Log.e(any<String>(), any<String>()) } returns 0
        every { Log.e(any<String>(), any<String>(), any<Throwable>()) } returns 0
        
        mockFormat = mockk(relaxed = true)
        mockMediaCodec = mockk(relaxed = true)
        
        mockkStatic(MediaFormat::class)
        mockkStatic(MediaCodec::class)
        
        // Mock MediaFormat creation and properties
        every { MediaFormat.createVideoFormat(any(), any(), any()) } returns mockFormat
        every { mockFormat.getString(MediaFormat.KEY_MIME) } returns MediaFormat.MIMETYPE_VIDEO_HEVC
        every { mockFormat.getInteger(MediaFormat.KEY_PROFILE) } returns MediaCodecInfo.CodecProfileLevel.HEVCProfileMain
        every { mockFormat.getInteger(MediaFormat.KEY_LEVEL) } returns MediaCodecInfo.CodecProfileLevel.HEVCMainTierLevel41
        
        // Mock MediaCodec
        every { MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_HEVC) } returns mockMediaCodec
        
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
        assertEquals("hev1.0.1.L4096.B0", codecString)  // Level 4.1 = 4096 in HEVC
    }

    @Test
    fun `test frame encoding`() = runTest {
        // Given
        val width = 16
        val height = 16
        val frameSize = width * height * 3 / 2
        val testFrame = ByteArray(frameSize) { 0 }
        
        val mockInputBuffer = ByteBuffer.allocate(frameSize)
        val mockOutputBuffer = ByteBuffer.allocate(10)
        
        // Track number of calls to dequeueOutputBuffer
        var outputBufferCalls = 0
        
        every { mockMediaCodec.getInputBuffer(any()) } returns mockInputBuffer
        every { mockMediaCodec.getOutputBuffer(any()) } returns mockOutputBuffer
        every { mockMediaCodec.dequeueInputBuffer(any()) } returns 0
        every { mockMediaCodec.dequeueOutputBuffer(any<MediaCodec.BufferInfo>(), any<Long>()) } answers {
            outputBufferCalls++
            if (outputBufferCalls == 1) 0 else MediaCodec.INFO_TRY_AGAIN_LATER
        }
        
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
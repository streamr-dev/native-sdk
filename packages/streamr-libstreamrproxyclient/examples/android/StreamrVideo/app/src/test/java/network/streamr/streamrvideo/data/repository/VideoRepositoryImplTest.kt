package network.streamr.streamrvideo.data.repository

import android.util.Log
import io.mockk.*
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import network.streamr.streamrvideo.data.encoder.VideoEncoder
import network.streamr.streamrvideo.data.model.proto.MediaPacket
import network.streamr.streamrvideo.data.model.proto.MediaPacketFragment
import network.streamr.streamrvideo.data.model.proto.VideoFrame
import network.streamr.streamrvideo.data.repository.builder.MediaPacketBuilder
import network.streamr.streamrvideo.data.repository.builder.MediaPacketFragmentBuilder
import network.streamr.streamrvideo.data.repository.builder.VideoFrameBuilder
import org.junit.After
import org.junit.Before
import org.junit.Test
import java.nio.ByteBuffer
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import com.google.protobuf.ByteString

class VideoRepositoryImplTest {
    private lateinit var repository: VideoRepositoryImpl
    private lateinit var mockEncoder: VideoEncoder
    private lateinit var mockFragmentBuilder: MediaPacketFragmentBuilder
    private lateinit var mockFrameBuilder: VideoFrameBuilder
    private lateinit var mockPacketBuilder: MediaPacketBuilder
    
    @Before
    fun setup() {
        // Clear all mocks before each test
        clearAllMocks()
        
        // Mock dependencies
        mockEncoder = mockk(relaxed = true)
        mockFragmentBuilder = mockk()
        mockFrameBuilder = mockk()
        mockPacketBuilder = mockk()
        
        // Mock Android Log with specific types
        mockkStatic(Log::class)
        every { Log.v(any<String>(), any<String>()) } returns 0
        every { Log.d(any<String>(), any<String>()) } returns 0
        every { Log.i(any<String>(), any<String>()) } returns 0
        every { Log.w(any<String>(), any<String>()) } returns 0
        every { Log.w(any<String>(), any<Throwable>()) } returns 0
        every { Log.e(any<String>(), any<String>()) } returns 0
        every { Log.e(any<String>(), any<String>(), any<Throwable>()) } returns 0
        every { Log.wtf(any<String>(), any<String>()) } returns 0
        every { Log.println(any<Int>(), any<String>(), any<String>()) } returns 0
        
        repository = VideoRepositoryImpl(
            mockEncoder,
            mockFragmentBuilder,
            mockFrameBuilder,
            mockPacketBuilder
        )
    }
    
    @After
    fun tearDown() {
        // Clean up after each test
        unmockkAll()
    }
    
    @Test
    fun `test initialize encoder`() = runTest {
        // Given
        val width = 640
        val height = 480
        val encoderCallback = slot<(ByteArray, Long, Int) -> Unit>()
        
        every { mockEncoder.initializeEncoder(width, height, capture(encoderCallback)) } returns Unit
        
        // When
        repository.initializeEncoder(width, height)
        
        // Then
        coVerify { mockEncoder.initializeEncoder(width, height, any()) }
    }
    
    @Test
    fun `test encode frame`() = runTest {
        // Given
        val testFrame = ByteArray(100) { 0 }
        
        // When
        repository.encodeFrame(testFrame)
        
        // Then
        coVerify { mockEncoder.encodeFrame(testFrame) }
    }
    
    @Test
    fun `test handle encoded frame`() = runTest {
        // Given
        val encodedData = ByteArray(10)
        val presentationTime = 1234L
        val flags = 1
        val extraData = ByteArray(5)
        val width = 640
        val height = 480
        val codecString = "test_codec"
        
        val mockVideoFrame = mockk<VideoFrame>() {
            every { frameNumber } returns 1
            every { timestamp } returns presentationTime
            every { duration } returns 0L
            every { isKeyFrame } returns false
            every { codec } returns codecString
            every { data } returns ByteString.copyFrom(encodedData)
        }
        val mockMediaPacket = mockk<MediaPacket>()
        val mockFragment = mockk<MediaPacketFragment>()
        
        every { mockEncoder.getExtraData() } returns extraData
        every { mockEncoder.getWidth() } returns width
        every { mockEncoder.getHeight() } returns height
        every { mockEncoder.getCodecString() } returns codecString
        
        coEvery { mockFrameBuilder.build(any(), any(), any(), any(), any(), any(), any()) } returns mockVideoFrame
        coEvery { mockPacketBuilder.build(mockVideoFrame) } returns mockMediaPacket
        coEvery { mockFragmentBuilder.build(mockMediaPacket) } returns listOf(mockFragment)
        
        // Capture and execute the callback
        val callbackSlot = slot<(ByteArray, Long, Int) -> Unit>()
        every { mockEncoder.initializeEncoder(any(), any(), capture(callbackSlot)) } coAnswers {
            // Execute callback in a coroutine
            callbackSlot.captured(encodedData, presentationTime, flags)
            Unit
        }
        
        // When
        repository.initializeEncoder(width, height)
        
        // Wait for all coroutines to complete
        advanceUntilIdle()
        
        // Then
        coVerify(timeout = 5000) {
            mockFrameBuilder.build(
                flags = flags,
                presentationTimeUs = presentationTime,
                encodedData = encodedData,
                extradataBytes = extraData,
                width = width,
                height = height,
                codecString = codecString
            )
        }
    }

    @Test
    fun `test error handling during frame encoding`() = runTest {
        // Given
        val testFrame = ByteArray(100) { 0 }
        coEvery { mockEncoder.encodeFrame(any()) } throws RuntimeException("Test error")
        
        // When
        repository.encodeFrame(testFrame)
        
        // Then
        coVerify { 
            mockEncoder.encodeFrame(testFrame)
            Log.e(any(), "Error encoding frame", any())
        }
    }

    @Test
    fun `test release`() = runTest {
        // Given
        coEvery { mockEncoder.release() } returns Unit

        // When
        repository.release()
        
        // Wait for coroutines to complete
        advanceUntilIdle()
        
        // Then
        coVerify(timeout = 5000) { mockEncoder.release() }
    }
} 
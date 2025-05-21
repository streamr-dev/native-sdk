package network.streamr.streamrvideo.data.encoder

import android.media.MediaCodecInfo
import android.media.MediaFormat
import org.junit.Test
import kotlin.test.assertEquals

class CodecStringConverterTest {
    @Test
    fun `test HEVC codec string conversion`() {
        // Given
        val mimeType = MediaFormat.MIMETYPE_VIDEO_HEVC
        val profile = MediaCodecInfo.CodecProfileLevel.HEVCProfileMain  // This is 1
        val level = MediaCodecInfo.CodecProfileLevel.HEVCMainTierLevel41  // This is 4096

        // When
        val codecString = CodecStringConverter.convertToWebCodecsCodecString(mimeType, profile, level)

        // Then
        assertEquals("hev1.0.1.L4096.B0", codecString)  // profile_space=0, profile_idc=1, level=4096
    }

    @Test
    fun `test VP8 codec string conversion`() {
        assertEquals("vp8", CodecStringConverter.convertToWebCodecsCodecString(
            MediaFormat.MIMETYPE_VIDEO_VP8, 0, 0))
    }

    @Test
    fun `test VP9 codec string conversion`() {
        assertEquals("vp9", CodecStringConverter.convertToWebCodecsCodecString(
            MediaFormat.MIMETYPE_VIDEO_VP9, 0, 0))
    }

    @Test
    fun `test AV1 codec string conversion`() {
        assertEquals("av1", CodecStringConverter.convertToWebCodecsCodecString(
            MediaFormat.MIMETYPE_VIDEO_AV1, 0, 0))
    }

    @Test(expected = IllegalArgumentException::class)
    fun `test unsupported mime type throws exception`() {
        CodecStringConverter.convertToWebCodecsCodecString("unsupported/mime", 0, 0)
    }

    @Test
    fun `test HEVC Main10 profile codec string conversion`() {
        val mimeType = MediaFormat.MIMETYPE_VIDEO_HEVC
        val profile = MediaCodecInfo.CodecProfileLevel.HEVCProfileMain10
        val level = MediaCodecInfo.CodecProfileLevel.HEVCMainTierLevel51

        val codecString = CodecStringConverter.convertToWebCodecsCodecString(mimeType, profile, level)

        assertEquals("hev1.0.2.L5120.B0", codecString)  // Level 5.1 = 5120 in HEVC
    }

    @Test
    fun `test AVC codec string conversion`() {
        val mimeType = MediaFormat.MIMETYPE_VIDEO_AVC
        val profile = MediaCodecInfo.CodecProfileLevel.AVCProfileBaseline
        val level = MediaCodecInfo.CodecProfileLevel.AVCLevel41

        val codecString = CodecStringConverter.convertToWebCodecsCodecString(mimeType, profile, level)

        assertEquals("avc1.42E029", codecString)  // Baseline profile, Level 4.1
    }
} 
package network.streamr.streamrvideo.data.encoder

import android.media.MediaFormat
import android.media.MediaCodecInfo

object CodecStringConverter {
    /**
     * Converts Android MediaFormat codec info to WebCodecs compatible codec string
     *
     * @param mimeType The MIME type of the codec (e.g. MediaFormat.MIMETYPE_VIDEO_HEVC)
     * @param profile The codec profile (e.g. MediaCodecInfo.CodecProfileLevel.HEVCProfileMain)
     * @param level The codec level (e.g. MediaCodecInfo.CodecProfileLevel.HEVCMainTierLevel41)
     * @return WebCodecs compatible codec string
     */
    fun convertToWebCodecsCodecString(mimeType: String, profile: Int, level: Int): String {
        return when (mimeType) {
            MediaFormat.MIMETYPE_VIDEO_HEVC -> {
                // H.265/HEVC
                val profileSpace = (profile shr 16) and 0xFF  // Usually 0
                val profileIdc = profile and 0xFF             // 1=Main, 2=Main10
                val compatFlags = (profile shr 8) and 0xFF    // Usually 0
                
                // Convert level to the correct format (e.g., HEVCMainTierLevel51 = 153 -> 5120)
                val levelValue = when (level) {
                    MediaCodecInfo.CodecProfileLevel.HEVCMainTierLevel51 -> 5120
                    MediaCodecInfo.CodecProfileLevel.HEVCMainTierLevel41 -> 4096
                    else -> level * 30  // Default conversion
                }
                
                String.format("hev1.%d.%d.L%d.B%d",
                    profileSpace,
                    profileIdc,
                    levelValue,
                    compatFlags
                )
            }
            MediaFormat.MIMETYPE_VIDEO_AVC -> {
                // H.264/AVC
                // AVCProfileBaseline = 0x01
                // Constraint set flags = 0xE0 for Baseline
                // Level 4.1 = 29 (0x1D)
                when (profile) {
                    MediaCodecInfo.CodecProfileLevel.AVCProfileBaseline -> "avc1.42E029"
                    MediaCodecInfo.CodecProfileLevel.AVCProfileMain -> "avc1.4D401F"
                    MediaCodecInfo.CodecProfileLevel.AVCProfileHigh -> "avc1.640029"
                    else -> String.format("avc1.%02X%02X%02X", 
                        profile and 0xFF,
                        (profile shr 8) and 0xFF,
                        level and 0xFF
                    )
                }
            }
            MediaFormat.MIMETYPE_VIDEO_VP8 -> "vp8"
            MediaFormat.MIMETYPE_VIDEO_VP9 -> "vp9"
            MediaFormat.MIMETYPE_VIDEO_AV1 -> "av1"
            else -> throw IllegalArgumentException("Unsupported mime type: $mimeType")
        }
    }
} 
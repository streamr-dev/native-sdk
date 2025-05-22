package network.streamr.streamrvideo.data.camera

import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CameraCaptureSession
import android.os.Handler
import android.view.Surface
import io.mockk.*
import kotlinx.coroutines.test.runTest
import org.junit.Before
import org.junit.Test
import kotlin.test.assertEquals

class AndroidCameraManagerImplTest {
    private lateinit var androidCameraManager: AndroidCameraManagerImpl
    private lateinit var mockContext: Context
    private lateinit var mockCameraManager: CameraManager
    private lateinit var mockActivity: Activity
    private lateinit var mockSurface: Surface
    
    @Before
    fun setup() {
        mockContext = mockk(relaxed = true)
        mockCameraManager = mockk(relaxed = true)
        mockActivity = mockk(relaxed = true)
        mockSurface = mockk(relaxed = true)
        
        every { mockContext.getSystemService(Context.CAMERA_SERVICE) } returns mockCameraManager
        
        androidCameraManager = AndroidCameraManagerImpl(mockContext)
    }
    
    @Test
    fun `test initialize with camera permission granted`() = runTest {
        // Given
        every { mockActivity.checkSelfPermission(any()) } returns PackageManager.PERMISSION_GRANTED
        every { mockCameraManager.cameraIdList } returns arrayOf("0")
        
        // When
        androidCameraManager.initialize(mockActivity)
        
        // Then
        assertEquals(CameraManager.CameraState.Connected, androidCameraManager.cameraState.value)
        verify { mockActivity.checkSelfPermission(android.Manifest.permission.CAMERA) }
    }
    
    @Test
    fun `test initialize without camera permission`() = runTest {
        // Given
        every { mockActivity.checkSelfPermission(any()) } returns PackageManager.PERMISSION_DENIED
        
        // When
        androidCameraManager.initialize(mockActivity)
        
        // Then
        assert(androidCameraManager.cameraState.value is CameraManager.CameraState.Error)
        verify { mockActivity.requestPermissions(any(), any()) }
    }
    
    @Test
    fun `test start preview success`() = runTest {
        // Given
        val mockCameraDevice = mockk<CameraDevice>(relaxed = true)
        val mockCaptureSession = mockk<CameraCaptureSession>(relaxed = true)
        val cameraDeviceCallback = slot<CameraDevice.StateCallback>()
        val captureSessionCallback = slot<CameraCaptureSession.StateCallback>()
        
        every { mockCameraManager.openCamera(any(), capture(cameraDeviceCallback), any<Handler>()) } answers {
            cameraDeviceCallback.captured.onOpened(mockCameraDevice)
            Unit
        }
        
        every { mockCameraDevice.createCaptureSession(any(), capture(captureSessionCallback), any<Handler>()) } answers {
            captureSessionCallback.captured.onConfigured(mockCaptureSession)
            Unit
        }
        
        // When
        androidCameraManager.startPreview(mockSurface)
        
        // Then
        assertEquals(CameraManager.CameraState.Preview, androidCameraManager.cameraState.value)
        verify { 
            mockCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
            mockCaptureSession.setRepeatingRequest(any(), null, any())
        }
    }
    
    @Test
    fun `test stop preview`() = runTest {
        // Given
        val mockCaptureSession = mockk<CameraCaptureSession>(relaxed = true)
        androidCameraManager.startPreview(mockSurface)
        
        // When
        androidCameraManager.stopPreview()
        
        // Then
        assertEquals(CameraManager.CameraState.Connected, androidCameraManager.cameraState.value)
        verify { 
            mockCaptureSession.stopRepeating()
            mockCaptureSession.close()
        }
    }
} 
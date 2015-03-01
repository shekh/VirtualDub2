#ifndef f_VD2_VDCAPTURE_SCREENGRABBERGL_H
#define f_VD2_VDCAPTURE_SCREENGRABBERGL_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/error.h>
#include <vd2/system/profile.h>
#include <vd2/system/vectors.h>
#include <windows.h>
#include <vd2/VDCapture/ScreenGrabber.h>
#include <vd2/Riza/opengl.h>

class VDScreenGrabberGL : public IVDScreenGrabber {
	VDScreenGrabberGL(const VDScreenGrabberGL&);
	VDScreenGrabberGL& operator=(const VDScreenGrabberGL&);
public:
	VDScreenGrabberGL();
	~VDScreenGrabberGL();

	bool	Init(IVDScreenGrabberCallback *cb);
	void	Shutdown();

	bool	InitCapture(uint32 srcw, uint32 srch, uint32 dstw, uint32 dsth, VDScreenGrabberFormat format);
	void	ShutdownCapture();

	void	SetCaptureOffset(int x, int y);
	void	SetCapturePointer(bool enable);

	uint64	GetCurrentTimestamp();
	sint64	ConvertTimestampDelta(uint64 t, uint64 base);

	bool	AcquireFrame(bool dispatch);

	bool	InitDisplay(HWND hwndParent, bool preview);
	void	ShutdownDisplay();

	void	SetDisplayArea(const vdrect32& r);
	void	SetDisplayVisible(bool vis);

	void	SetFramePeriod(sint32 ms);

	bool	SetVideoFormat(uint32 w, uint32 h, VDScreenGrabberFormat format);

	bool	CaptureStart();

protected:
	bool	InitVideoBuffer();
	void	ShutdownVideoBuffer();
	void	FlushFrameQueue();
	sint64	ComputeGlobalTime();
	bool	UpdateCachedCursor(const CURSORINFO& ci);
	void	DispatchFrame(const void *data, ptrdiff_t size, sint64 timestamp);

	void	ConvertToYV12_GL_NV1x(int w, int h, float u, float v);
	void	ConvertToYV12_GL_NV2x(int w, int h, float u, float v);
	void	ConvertToYV12_GL_ATIFS(int w, int h, float u, float v);
	void	ConvertToYUY2_GL_NV1x(int w, int h, float u, float v);
	void	ConvertToYUY2_GL_NV2x_ATIFS(int w, int h, float u, float v, bool atifs);
	GLuint	GetOcclusionQueryPixelCountSafe(GLuint query);

	static LRESULT CALLBACK StaticWndProcGL(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND	mhwndParent;
	HWND	mhwndGL;
	HWND	mhwndGLDraw;

	bool	mbCapBuffersInited;
	uint32	mOffscreenSize;
	ptrdiff_t	mOffscreenPitch;

	VDOpenGLBinding		mGL;
	GLuint	mGLBuffers[2];
	GLuint	mGLShaderBase;
	GLuint	mGLTextures[2];
	int		mGLTextureW;
	int		mGLTextureH;
	vdblock<uint32> mGLReadBuffer;
	GLuint	mGLOcclusionQueries[2];
	bool	mbFrameValid[2];
	bool	mbGLOcclusionValid[2];
	bool	mbGLOcclusionPrevFrameValid;
	sint64	mTimestampQueue[4];
	int		mTimestampDelay;
	int		mTimestampIndex;

	GLuint	mGLCursorCacheTexture;
	float	mGLCursorCacheTextureInvW;
	float	mGLCursorCacheTextureInvH;
	HCURSOR	mCachedCursor;
	int		mCachedCursorWidth;
	int		mCachedCursorHeight;
	int		mCachedCursorHotspotX;
	int		mCachedCursorHotspotY;
	bool	mbCachedCursorXORMode;

	HDC		mhdcCursorBuffer;
	HBITMAP	mhbmCursorBuffer;
	HGDIOBJ	mhbmCursorBufferOld;
	uint32	*mpCursorBuffer;

	VDAtomicInt	mbCaptureFramePending;
	bool	mbVisible;
	bool	mbDisplayPreview;
	vdrect32	mDisplayArea;

	int		mTrackX;
	int		mTrackY;

	bool	mbRescaleImage;

	bool	mbDrawMousePointer;
	bool	mbRemoveDuplicates;

	uint32	mFramePeriod;

	uint32	mCaptureSrcWidth;
	uint32	mCaptureSrcHeight;
	uint32	mCaptureDstWidth;
	uint32	mCaptureDstHeight;
	VDScreenGrabberFormat	mCaptureFormat;

	IVDScreenGrabberCallback	*mpCB;

	VDRTProfileChannel mProfileChannel;

	ATOM	mWndClassGL;
};

#endif

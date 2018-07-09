#ifndef f_VD2_VDCAPTURE_SCREENGRABBERGDI_H
#define f_VD2_VDCAPTURE_SCREENGRABBERGDI_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/error.h>
#include <vd2/system/profile.h>
#include <vd2/system/vectors.h>
#include <windows.h>
#include <vd2/VDCapture/ScreenGrabber.h>

class VDScreenGrabberGDI : public IVDScreenGrabber {
	VDScreenGrabberGDI(const VDScreenGrabberGDI&);
	VDScreenGrabberGDI& operator=(const VDScreenGrabberGDI&);
public:
	VDScreenGrabberGDI();
	~VDScreenGrabberGDI();

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

protected:
	sint64	ComputeGlobalTime();
	void	DispatchFrame(const void *data, uint32 pitch, sint64 timestamp);

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND	mhwnd;
	ATOM	mWndClass;
	IVDScreenGrabberCallback *mpCB;

	bool	mbCapBuffersInited;
	HDC		mhdcOffscreen;
	HBITMAP	mhbmOffscreen;
	void	*mpOffscreenData;
	uint32	mOffscreenSize;
	uint32	mOffscreenPitch;

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
	HCURSOR cap_cursor;

	bool	mbVisible;
	bool	mbDisplayPreview;
	bool	mbExcludeSelf;
	vdrect32	mDisplayArea;

	int		mTrackX;
	int		mTrackY;

	bool	mbDrawMousePointer;

	uint32	mCaptureWidth;
	uint32	mCaptureHeight;
};

#endif

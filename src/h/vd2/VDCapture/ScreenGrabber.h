#ifndef f_VD2_VDCAPTURE_SCREENGRABBER_H
#define f_VD2_VDCAPTURE_SCREENGRABBER_H

#include <vd2/system/vectors.h>
#include <vd2/system/win32/miniwindows.h>

enum VDScreenGrabberFormat {
	kVDScreenGrabberFormat_XRGB32,
	kVDScreenGrabberFormat_YUY2,
	kVDScreenGrabberFormat_YV12
};

class IVDScreenGrabberCallback {
public:
	virtual void ReceiveFrame(uint64 timestamp, const void *data, ptrdiff_t rowpitch, uint32 rowlen, uint32 rowcnt) = 0;
};

class IVDScreenGrabber {
public:
	virtual ~IVDScreenGrabber() {}

	virtual bool Init(IVDScreenGrabberCallback *cb) = 0;
	virtual void Shutdown() = 0;

	virtual bool InitCapture(uint32 srcw, uint32 srch, uint32 dstw, uint32 dsth, VDScreenGrabberFormat format) = 0;
	virtual void ShutdownCapture() = 0;

	virtual void SetCaptureOffset(int x, int y) = 0;
	virtual void SetCapturePointer(bool enable) = 0;

	virtual uint64 GetCurrentTimestamp() = 0;
	virtual sint64 ConvertTimestampDelta(uint64 t, uint64 base) = 0;

	virtual bool AcquireFrame(bool dispatch) = 0;

	virtual bool InitDisplay(VDZHWND hwndParent, bool preview) = 0;
	virtual void ShutdownDisplay() = 0;

	virtual void SetDisplayVisible(bool visible) = 0;
	virtual void SetDisplayArea(const vdrect32& area) = 0;
};

IVDScreenGrabber *VDCreateScreenGrabberGDI();
IVDScreenGrabber *VDCreateScreenGrabberGL();
IVDScreenGrabber *VDCreateScreenGrabberDXGI12();

#endif	// f_VD2_VDCAPTURE_SCREENGRABBER_H

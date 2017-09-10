#ifndef f_VD2_RIZA_DISPLAY_H
#define f_VD2_RIZA_DISPLAY_H

#include <vd2/system/vectors.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/refcount.h>
#include <vd2/system/atomic.h>
#include <vd2/Kasumi/pixmap.h>

VDGUIHandle VDCreateDisplayWindowW32(uint32 dwExFlags, uint32 dwFlags, int x, int y, int width, int height, VDGUIHandle hwndParent);

class IVDVideoDisplay;
class IVDDisplayCompositor;
class IVDVideoDisplayMinidriver;

class VDVideoDisplayFrame : public vdlist_node, public IVDRefCount {
public:
	VDVideoDisplayFrame();
	virtual ~VDVideoDisplayFrame();

	virtual int AddRef();
	virtual int Release();

	VDPixmap	mPixmap;
	uint32		mFlags;
	bool		mbInterlaced;
	bool		mbAllowConversion;

protected:
	VDAtomicInt	mRefCount;
};

class VDINTERFACE IVDVideoDisplayCallback {
public:
	virtual void DisplayRequestUpdate(IVDVideoDisplay *pDisp) = 0;
};

class VDINTERFACE IVDVideoDisplayDrawMode {
public:
	virtual void PreparePaint(IVDVideoDisplayMinidriver* driver) = 0;
	virtual void Paint(HDC dc) = 0;
	virtual void PaintZoom(HDC dc, int xDest, int yDest, int destW, int destH, int xSrc, int ySrc, int srcW, int srcH) = 0;
	virtual void SetDisplayPos(int x, int y, int w, int h) = 0;
};

class VDINTERFACE IVDVideoDisplay {
public:
	enum {
		kFormatPal8			= nsVDPixmap::kPixFormat_Pal8,
		kFormatRGB1555		= nsVDPixmap::kPixFormat_XRGB1555,
		kFormatRGB565		= nsVDPixmap::kPixFormat_RGB565,
		kFormatRGB888		= nsVDPixmap::kPixFormat_RGB888,
		kFormatRGB8888		= nsVDPixmap::kPixFormat_XRGB8888,
		kFormatYUV422_YUYV	= nsVDPixmap::kPixFormat_YUV422_YUYV,
		kFormatYUV422_UYVY	= nsVDPixmap::kPixFormat_YUV422_UYVY
	};

	enum FieldMode {
		kEvenFieldOnly		= 0x0001,
		kOddFieldOnly		= 0x0002,
		kAllFields			= 0x0003,
		kVSync				= 0x0004,
		kFirstField			= 0x0008,

		kDoNotCache			= 0x0020,
		kVisibleOnly		= 0x0040,
		kAutoFlipFields		= 0x0080,
		kBobEven			= 0x0100,
		kBobOdd				= 0x0200,
		kSequentialFields	= 0x0400,
		kDoNotWait			= 0x0800,

		kFieldModeMax		= 0xffff,
	};

	enum FilterMode {
		kFilterAnySuitable,
		kFilterPoint,
		kFilterBilinear,
		kFilterBicubic
	};

	enum DisplayMode {
		kDisplayDefault,
		kDisplayColor,
		kDisplayAlpha,
		kDisplayBlendChecker,
		kDisplayBlend0,
		kDisplayBlend1,
	};

	virtual void Destroy() = 0;
	virtual void Reset() = 0;
	virtual void SetSourceMessage(const wchar_t *msg) = 0;
	virtual bool SetSource(bool bAutoUpdate, const VDPixmap& src, void *pSharedObject = 0, ptrdiff_t sharedOffset = 0, bool bAllowConversion = true, bool bInterlaced = false) = 0;
	virtual bool SetSourcePersistent(bool bAutoUpdate, const VDPixmap& src, bool bAllowConversion = true, bool bInterlaced = false) = 0;
	virtual void SetSourceSubrect(const vdrect32 *r) = 0;
	virtual void SetSourceSolidColor(uint32 color) = 0;

	virtual void SetReturnFocus(bool enable) = 0;
	virtual void SetFullScreen(bool fs, uint32 width = 0, uint32 height = 0, uint32 refresh = 0) = 0;
	virtual void SetDestRect(const vdrect32 *r, uint32 backgroundColor) = 0;
	virtual void SetPixelSharpness(float xfactor, float yfactor) = 0;
	virtual void SetCompositor(IVDDisplayCompositor *compositor) = 0;

	virtual void PostBuffer(VDVideoDisplayFrame *) = 0;
	virtual bool RevokeBuffer(bool allowFrameSkip, VDVideoDisplayFrame **ppFrame) = 0;
	virtual void FlushBuffers() = 0;

	virtual void Update(int mode = kAllFields) = 0;
	virtual void Cache() = 0;
	virtual void SetCallback(IVDVideoDisplayCallback *p) = 0;
	virtual void SetDrawMode(IVDVideoDisplayDrawMode *p) = 0;
	virtual void DrawInvalidate(RECT* r) = 0;

	enum AccelerationMode {
		kAccelOnlyInForeground,
		kAccelResetInForeground,
		kAccelAlways
	};

	virtual void SetAccelerationMode(AccelerationMode mode) = 0;

	virtual FilterMode GetFilterMode() = 0;
	virtual void SetFilterMode(FilterMode) = 0;
	virtual float GetSyncDelta() const = 0;
	virtual DisplayMode GetDisplayMode() = 0;
	virtual void SetDisplayMode(DisplayMode) = 0;
	virtual bool IsFramePending() = 0;
};

void VDVideoDisplaySetFeatures(bool enableDirectX, bool enableOverlays, bool enableTermServ, bool enableOpenGL, bool enableDirect3D, bool enableD3DFX, bool enableHighPrecision);
void VDVideoDisplaySetD3D9ExEnabled(bool enable);
void VDVideoDisplaySetDDrawEnabled(bool enable);
void VDVideoDisplaySet3DEnabled(bool enable);
void VDVideoDisplaySetD3DFXFileName(const wchar_t *path);
void VDVideoDisplaySetDebugInfoEnabled(bool enable);
void VDVideoDisplaySetBackgroundFallbackEnabled(bool enable);
void VDVideoDisplaySetSecondaryDXEnabled(bool enable);
void VDVideoDisplaySetMonitorSwitchingDXEnabled(bool enable);
void VDVideoDisplaySetTermServ3DEnabled(bool enable);

IVDVideoDisplay *VDGetIVideoDisplay(VDGUIHandle hwnd);
bool VDRegisterVideoDisplayControl();

#endif

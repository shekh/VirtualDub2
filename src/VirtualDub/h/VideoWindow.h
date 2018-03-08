#ifndef f_VIDEOWINDOW_H
#define f_VIDEOWINDOW_H

#include <windows.h>

extern const char g_szVideoWindowClass[];

#define VIDEOWINDOWCLASS (g_szVideoWindowClass)

ATOM RegisterVideoWindow();

class VDFraction;
class IVDVideoDisplay;

enum PanCenteringMode {
	kPanCenter,
	kPanTopLeft,
};

class VDINTERFACE IVDVideoWindow {
public:
	virtual void SetMouseTransparent(bool) = 0;
	virtual void GetSourceSize(int& w, int& h) = 0;
	virtual void SetSourceSize(int w, int h) = 0;
	virtual void GetDisplayRect(int& x0, int& y0, int& w, int& h) = 0;
	virtual void Move(int x, int y) = 0;
	virtual void Resize(bool useWorkArea=true) = 0;
	virtual void SetChild(HWND hwnd) = 0;
	virtual void SetMaxDisplayHost(HWND hwnd) = 0;
	virtual void SetDisplay(IVDVideoDisplay *) = 0;
	virtual const VDFraction GetSourcePAR() = 0;
	virtual void SetSourcePAR(const VDFraction& fr) = 0;
	virtual void SetZoom(double zoom, bool useWorkArea=true) = 0;
	virtual double GetMaxZoomForArea(int w, int h) = 0;
	virtual void SetBorder(int v=4, int ht=-1) = 0;
	virtual bool GetAutoSize() = 0;
	virtual void SetAutoSize(bool) = 0;
	virtual void InitSourcePAR() = 0;
	virtual void SetWorkArea(RECT& r, bool auto_border=false) = 0;
	virtual void SetDrawMode(IVDVideoDisplayDrawMode *p) = 0;
	virtual void ToggleFullscreen() = 0;
	virtual bool IsFullscreen() = 0;
	virtual void SetPanCentering(PanCenteringMode mode) = 0;
};

IVDVideoWindow *VDGetIVideoWindow(HWND hwnd);

enum {
	VWN_RESIZED		= 16,				// WM_NOTIFY: window rect was changed
	VWN_REQUPDATE	= 17
};

#endif

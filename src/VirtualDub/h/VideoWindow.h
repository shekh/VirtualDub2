#ifndef f_VIDEOWINDOW_H
#define f_VIDEOWINDOW_H

#include <windows.h>

extern const char g_szVideoWindowClass[];

#define VIDEOWINDOWCLASS (g_szVideoWindowClass)

ATOM RegisterVideoWindow();

class VDFraction;
class IVDVideoDisplay;

class VDINTERFACE IVDVideoWindow {
public:
	virtual void SetMouseTransparent(bool) = 0;
	virtual void GetSourceSize(int& w, int& h) = 0;
	virtual void SetSourceSize(int w, int h) = 0;
	virtual void GetFrameSize(int& w, int& h) = 0;
	virtual void Move(int x, int y) = 0;
	virtual void Resize() = 0;
	virtual void SetChild(HWND hwnd) = 0;
	virtual void SetDisplay(IVDVideoDisplay *) = 0;
	virtual const VDFraction GetSourcePAR() = 0;
	virtual void SetSourcePAR(const VDFraction& fr) = 0;
	virtual void SetZoom(double zoom) = 0;
	virtual double GetMaxZoomForArea(int w, int h) = 0;
	virtual void SetBorderless(bool) = 0;
	virtual bool GetAutoSize() = 0;
	virtual void SetAutoSize(bool) = 0;
	virtual void InitSourcePAR() = 0;
};

IVDVideoWindow *VDGetIVideoWindow(HWND hwnd);

enum {
	VWN_RESIZED		= 16,				// WM_NOTIFY: window rect was changed
	VWN_REQUPDATE	= 17
};

#endif

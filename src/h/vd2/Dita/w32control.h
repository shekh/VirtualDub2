#ifndef f_VD2_DITA_W32CONTROL_H
#define f_VD2_DITA_W32CONTROL_H

#include <vd2/Dita/w32peer.h>

class VDUIControlW32 : public VDUIPeerW32 {
public:
	~VDUIControlW32();

protected:
	bool CreateW32(IVDUIParameters *pParms, const char *pClass, DWORD style);
	void Destroy();

	virtual void PreLayoutBase(const VDUILayoutSpecs&);
	virtual void PreLayoutBaseW32(const VDUILayoutSpecs&) {}

	SIZE SizeText(int nMaxWidth, int nPadWidth, int nPadHeight);
};

class VDUICustomControlW32 : public VDUIControlW32 {
public:
	bool Create(IVDUIParameters *pParameters, bool forceNonChild = false, DWORD flags = 0);
	void Destroy();

protected:
	virtual bool IsOwnerW32() const { return true; }

	static INT_PTR CALLBACK StaticDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	virtual LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	static ATOM sWindowClass;
};

#endif

#ifndef f_VD2_DITA_W32PEER_H
#define f_VD2_DITA_W32PEER_H

#include <windows.h>
#include <vd2/Dita/basetypes.h>
#include <vd2/Dita/w32interface.h>

class VDUIPeerW32 : public VDUIWindow, public IVDUIWindowW32 {
public:
	enum { kTypeID = 'uipr' };

	VDUIPeerW32();
	VDUIPeerW32(HWND hwnd);

	void		Attach(HWND hwnd);
	void		Detach();

	void *AsInterface(uint32 id);

	void		RelayoutChildren();

	void		SetFocus();

	void		SetCaption(const wchar_t *caption);

	vduirect	GetArea();
	void		SetArea(const vduirect& pos);

	vduirect	GetClientArea() const;

	void		PropagateVisible(bool vis);
	void		PropagateEnabled(bool ena);

public:
	VDUIPeerW32 *GetParentPeerW32() const;
	HWND GetParentW32() const;
	HWND GetHandleW32() const { return mhwnd; }
	virtual bool IsOwnerW32() const;

	void RegisterCallbackW32(VDUIPeerW32 *pChild);
	void UnregisterCallbackW32(VDUIPeerW32 *pChild);

	void UpdateCaptionW32();

	virtual void OnCommandCallback(UINT code) {}
	virtual void OnNotifyCallback(const NMHDR *pHdr) {}
	virtual void OnScrollCallback(UINT code) {}

	HWND mhwnd;

	typedef std::map<HWND, VDUIPeerW32 *> tCallbacks;
	tCallbacks mCallbacks;
};

#endif

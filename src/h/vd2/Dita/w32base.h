#ifndef f_VD2_DITA_W32BASE_H
#define f_VD2_DITA_W32BASE_H

#include <vd2/system/refcount.h>
#include <vd2/Dita/w32control.h>
#include <map>
#include <vector>

class VDUIBaseWindowW32 : public VDUICustomControlW32, public IVDUIBase {
public:
	VDUIBaseWindowW32();
	~VDUIBaseWindowW32();

	void *AsInterface(uint32 id);

	void Shutdown();

	bool Create(IVDUIParameters *pParams);
	void Destroy();

	void SetAutoDestroy(bool);
	void SetTickEnable(bool);

	void AddControl(IVDUIWindow *);
	void RemoveControl(IVDUIWindow *);
	IVDUIWindow *GetControl(uint32 id);

	vduirect MapUnitsToPixels(vduirect r);
	vduisize MapUnitsToPixels(vduisize s);

	uint32 GetNextNativeID();

	void SetCallback(IVDUICallback *, bool autoDelete);

	void FinalizeDialog();

	int DoModal();
	void EndModal(int value);

	void Link(uint32 id, nsVDUI::LinkTarget target, const uint8 *src, size_t len);
	void ExecuteAllLinks();

	void ProcessActivation(IVDUIWindow *pWin, uint32 id);
	void ProcessValueChange(IVDUIWindow *pWin, uint32 id);
	bool DispatchEvent(IVDUIWindow *pWin, uint32 id, IVDUICallback::eEventType, int item);

	void Relayout();
	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const vduirect& target);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void RebuildLinkUpdateMap();
	void ExecuteLinks(uint32 id);
	void ExecuteLinks(std::list<uint32>& queue);

	typedef std::multimap<uint32, IVDUIWindow *> tControls;
	tControls	mControls;

	int			mPadding;

	RECT		mInsets;

	IVDUICallback	*mpCB;
	bool		mbCBAutoDelete;

	uint32		mNextNativeID;

	bool		mbAutoDestroy;
	bool		mbTick;

	struct ModalData;

	ModalData	*volatile mpModal;		// needs to be volatile or else it gets cached in the message loop

	// link map: holds link entries
	struct LinkEntry {
		nsVDUI::LinkTarget		mTarget;
		std::vector<uint32>		mLinkSources;
		std::vector<uint8>		mByteCode;
	};

	typedef std::multimap<uint32, LinkEntry> tLinkList;
	tLinkList		mLinkList;

	// link update map: says which link entries to execute on a change
	typedef std::multimap<uint32, const tLinkList::value_type *> tLinkUpdateMap;
	tLinkUpdateMap	mLinkUpdateMap;
	bool			mbLinkUpdateMapDirty;
};

extern IVDUIWindow *VDCreateUIBaseWindow() { return new VDUIBaseWindowW32; }

#endif

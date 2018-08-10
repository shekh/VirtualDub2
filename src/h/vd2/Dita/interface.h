#ifndef f_VD2_DITA_INTERFACE_H
#define f_VD2_DITA_INTERFACE_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vectors.h>
#include <vd2/system/unknown.h>
#include <vd2/system/VDString.h>

namespace nsVDUI {
	// !!WARNING!!
	//
	// These constants cannot be changed without invalidating existing resource files!

	enum Alignment {
		kAlignDefault	= 0,
		kLeft			= 1,
		kTop			= 1,
		kCenter			= 2,
		kRight			= 3,
		kBottom			= 3,
		kFill			= 4,

		kAlignTypeMask	= 0x0FF,
		kExpandFlag		= 0x100,		// allow this axis to expand to meet AR

		kAlignmentLimit = 0xFFFFFFFFUL
	};

	enum CompressType {
		kCompressNone,
		kCompressCheckbox,
		kCompressOption,
		kCompressTypes
	};

	enum ParameterID {
		kUIParam_None,
		kUIParam_Raised,
		kUIParam_Sunken,
		kUIParam_IsVertical,
		kUIParam_Child,
		kUIParam_Multiline,
		kUIParam_Readonly,
		kUIParam_Checkable,
		kUIParam_NoHeader,
		kUIParam_Default,

		kUIParam_Spacing,

		kUIParam_MarginL,
		kUIParam_MarginT,
		kUIParam_MarginR,
		kUIParam_MarginB,
		kUIParam_PadL,
		kUIParam_PadT,
		kUIParam_PadR,
		kUIParam_PadB,
		kUIParam_MinW,
		kUIParam_MinH,
		kUIParam_MaxW,
		kUIParam_MaxH,

		kUIParam_Col,
		kUIParam_Row,
		kUIParam_ColSpan,
		kUIParam_RowSpan,

		kUIParam_Align,
		kUIParam_VAlign,
		kUIParam_Aspect,

		kUIParam_EnableLinkExpr,
		kUIParam_ValueLinkExpr,

		kUIParam_Affinity,

		kUIParam_SystemMenus,

		kUIParam_Count
	};

	enum LinkTarget {
		kLinkTarget_Enable,
		kLinkTarget_Value
	};
};

template<class T>
class VDRectInsets {
public:
	VDRectInsets() {}
	VDRectInsets(T w) : left(w), top(w), right(w), bottom(w) {}
	VDRectInsets(T w, T h) : left(w), top(h), right(w), bottom(h) {}
	VDRectInsets(T l, T t, T r, T b) : left(l), top(t), right(r), bottom(b) {}

	T width() const { return left+right; }
	T height() const { return top+bottom; }

	T left, top, right, bottom;
};

typedef VDRect<int> vduirect;
typedef VDRectInsets<int> vduirectinsets;
typedef VDSize<int> vduisize;


struct VDUILayoutSpecs {
	vduisize minsize;			// we can't use min because some moron #define'd it :(
};


class IVDUIBase;

class VDINTERFACE IVDUIParameters {
public:
	virtual bool	GetB(uint32 id, bool defaultVal) = 0;
	virtual int		GetI(uint32 id, int defaultVal) = 0;
	virtual float	GetF(uint32 id, float defaultVal) = 0;

	virtual bool	GetOptB(uint32 id, bool& v) = 0;
	virtual bool	GetOptI(uint32 id, int& v) = 0;
	virtual bool	GetOptF(uint32 id, float& v) = 0;
};


class VDINTERFACE IVDUIWindow : public IVDRefUnknown {
public:
	enum { kTypeID = 'wind' };

	virtual ~IVDUIWindow() {}

	virtual void		Shutdown() = 0;

	virtual IVDUIWindow *GetParent() const = 0;
	virtual void		SetParent(IVDUIWindow *) = 0;

	virtual IVDUIBase *GetBase() const = 0;

	virtual bool		Create(IVDUIParameters *) = 0;
	virtual void		Destroy() = 0;

	virtual void AddChild(IVDUIWindow *pWindow) = 0;
	virtual void RemoveChild(IVDUIWindow *pWindow) = 0;
	virtual IVDUIWindow *GetStartingChild() = 0;
	virtual IVDUIWindow *GetPreviousChild(IVDUIWindow *pWindow) = 0;
	virtual IVDUIWindow *GetNextChild(IVDUIWindow *pWindow) = 0;

	virtual void		SetVisible(bool vis) = 0;
	virtual void		SetEnabled(bool ena) = 0;
	virtual bool		IsVisible() = 0;
	virtual bool		IsEnabled() = 0;
	virtual bool		IsActuallyVisible() = 0;
	virtual bool		IsActuallyEnabled() = 0;

	virtual void		PropagateVisible(bool vis) = 0;
	virtual void		PropagateEnabled(bool ena) = 0;

	virtual void		SetFocus() = 0;

	virtual uint32		GetID() const = 0;
	virtual void		SetID(uint32 id) = 0;

	virtual VDStringW	GetCaption() = 0;
	virtual void		SetCaption(const wchar_t *s) = 0;

	virtual vduirect	GetArea() const = 0;
	virtual void		SetArea(const vduirect& pos) = 0;

	virtual vduirect	GetClientArea() const = 0;

	virtual void		GetAlignment(nsVDUI::Alignment& x, nsVDUI::Alignment& y) = 0;
	virtual void		SetAlignment(nsVDUI::Alignment x, nsVDUI::Alignment y) = 0;

	virtual void		GetMinimumSize(vduisize&) = 0;
	virtual void		SetMinimumSize(const vduisize&)=0;
	virtual void		GetMaximumSize(vduisize&) = 0;
	virtual void		SetMaximumSize(const vduisize&)=0;

	virtual void		SetMargins(const vduirectinsets&) = 0;
	virtual vduirectinsets GetMargins() const = 0;
	virtual void		SetPadding(const vduirectinsets&) = 0;
	virtual vduirectinsets GetPadding() const = 0;

	virtual nsVDUI::CompressType GetCompressType() = 0;

	virtual float		GetDesiredAspectRatio() = 0;
	virtual void		SetDesiredAspectRatio(float rAspect) = 0;

	virtual const VDUILayoutSpecs& GetLayoutSpecs() = 0;

	virtual void		Layout(const vduirect& target) = 0;
	virtual void		PreLayout(const VDUILayoutSpecs& parentConstraints) = 0;
	virtual void		PostLayout(const vduirect& target) = 0;

	virtual int			GetValue() = 0;
	virtual void		SetValue(int) = 0;
};

class VDINTERFACE IVDUICallback {
public:
	enum { kTypeID = 'uicb' };

	enum eEventType {
		kEventNone,
		kEventAttach,
		kEventDetach,
		kEventSelect,
		kEventDoubleClick,
		kEventClose,
		kEventCreate,
		kEventDestroy,
		kEventSync,
		kEventTick
	};

	virtual ~IVDUICallback() {}

	virtual bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) = 0;
};

class VDINTERFACE IVDUIBase : public IVDUnknown {
public:
	enum { kTypeID = 'base' };

	virtual void SetTickEnable(bool) = 0;
	virtual void SetAutoDestroy(bool bAutoDestroy) = 0;

	virtual vduirect MapUnitsToPixels(vduirect r) = 0;
	virtual vduisize MapUnitsToPixels(vduisize r) = 0;

	virtual void AddControl(IVDUIWindow *) = 0;
	virtual void RemoveControl(IVDUIWindow *) = 0;
	virtual IVDUIWindow *GetControl(uint32 id) = 0;

	virtual uint32 GetNextNativeID() = 0;

	virtual void SetCallback(IVDUICallback *pCB, bool autoDelete) = 0;
	virtual void FinalizeDialog() = 0;

	virtual int DoModal() = 0;
	virtual void EndModal(int rv) = 0;

	virtual void Link(uint32 id, nsVDUI::LinkTarget target, const uint8 *src, size_t len) = 0;
	virtual void ExecuteAllLinks() = 0;

	virtual void ProcessActivation(IVDUIWindow *pWin, uint32 id) = 0;
	virtual void ProcessValueChange(IVDUIWindow *pWin, uint32 id) = 0;

	virtual bool DispatchEvent(IVDUIWindow *pWin, uint32 id, IVDUICallback::eEventType, int item) = 0;

	virtual void Relayout() = 0;
};

class VDINTERFACE IVDUIListCallback {
public:
	virtual bool GetListText(int index, int subitem, VDStringW& text) = 0;
};

class VDINTERFACE IVDUIList : public IVDUnknown {
public:
	enum { kTypeID = 'list' };

	virtual void Clear() = 0;
	virtual int GetItemCount() = 0;
	virtual uintptr GetItemData(int index) = 0;
	virtual void SetItemData(int index, uintptr value) = 0;
	virtual int AddItem(const wchar_t *text, uintptr data = 0) = 0;
};

class VDINTERFACE IVDUIListView : public IVDUnknown {
public:
	enum { kTypeID = 'lsvw' };

	virtual void SetListCallback(IVDUIListCallback *cb) = 0;
	virtual void SetItemText(int item, int subitem, const wchar_t *text) = 0;

	virtual void AddColumn(const wchar_t *name, int width, int affinity) = 0;
	virtual void SetItemChecked(int item, bool checked) = 0;
	virtual bool IsItemChecked(int item) = 0;
};

class VDINTERFACE IVDUIPageSet : public IVDUnknown {
public:
	enum { kTypeID = 'pgst' };

	virtual void AddPage(int dialogID) = 0;
};

class VDINTERFACE IVDUITrackbar : public IVDUnknown {
public:
	enum { kTypeID = 'trck' };

	virtual void SetRange(sint32 mn, sint32 mx) = 0;
	virtual void SetRangeStep(sint32 mn, sint32 mx, sint32 step) = 0;
};

class VDINTERFACE IVDUIGrid : public IVDUnknown {
public:
	enum { kTypeID = 'grid' };

	virtual void AddChild(IVDUIWindow *pWin, int col, int row, int colspan, int rowspan) = 0;
	virtual void SetRow(int row, int minsize=-1, int maxsize=-1, int affinity=-1) = 0;
	virtual void SetColumn(int col, int minsize=-1, int maxsize=-1, int affinity=-1) = 0;
	virtual void NextRow() = 0;
};

IVDUIWindow *VDCreateDialogFromResource(int dialogID, IVDUIWindow *pParent);
uint32 VDUIExecuteRuntimeExpression(const uint8 *expr, IVDUIWindow *const *pSrcWindows);

IVDUIWindow *VDUICreatePeer(VDGUIHandle h);

void VDUIRegisterWindowClass(uint32 classID, IVDUIWindow *(*pCreator)());
IVDUIWindow *VDUICreateWindowClass(uint32 classID);

#endif

#ifndef f_VD2_DITA_BASETYPES_H
#define f_VD2_DITA_BASETYPES_H

#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

#include <math.h>

#include <list>
#include <map>

#include <vd2/system/atomic.h>
#include <vd2/Dita/interface.h>

class VDUIParameters : public IVDUIParameters {
public:
	void	Clear();

	bool	GetB(uint32 id, bool defaultVal);
	int		GetI(uint32 id, int defaultVal);
	float	GetF(uint32 id, float defaultVal);

	bool	GetOptB(uint32 id, bool& v);
	bool	GetOptI(uint32 id, int& v);
	bool	GetOptF(uint32 id, float& v);

	void SetB(uint32 id, bool val) { mParams[id].b = val; }
	void SetI(uint32 id, int val) { mParams[id].i = val; }
	void SetF(uint32 id, float val) { mParams[id].f = val; }

protected:
	union Variant {
		bool b;
		int i;
		float f;
	};

	const Variant *Lookup(uint32 id) const;

	typedef std::map<uint32, Variant> tParams;
	tParams mParams;
};

class VDUIWindow : public IVDUIWindow {
public:
	VDUIWindow();
	~VDUIWindow();

	int AddRef();
	int Release();
	void *AsInterface(uint32 id);

	void Shutdown();

	IVDUIWindow *GetParent() const { return mpParent; }
	void SetParent(IVDUIWindow *);

	IVDUIBase *GetBase() const { return mpBase; }

	bool Create(IVDUIParameters *);
	void Destroy();

	void AddChild(IVDUIWindow *pWindow);
	void RemoveChild(IVDUIWindow *pWindow);
	IVDUIWindow *GetStartingChild();
	IVDUIWindow *GetPreviousChild(IVDUIWindow *pWindow);
	IVDUIWindow *GetNextChild(IVDUIWindow *pWindow);

	void SetVisible(bool vis) {
		if (mbVisible != vis) {
			mbVisible = vis;
			PropagateVisible(mpParent->IsActuallyVisible());
		}
	}

	void SetEnabled(bool ena) {
		if (mbEnabled != ena) {
			mbEnabled = ena;
			PropagateEnabled(mpParent->IsActuallyEnabled());
		}
	}

	bool IsVisible() {
		return mbVisible;
	}

	bool IsEnabled() {
		return mbEnabled;
	}

	bool IsActuallyVisible();
	bool IsActuallyEnabled();

	void PropagateVisible(bool vis);
	void PropagateEnabled(bool ena);

	void SetFocus();

	uint32 GetID() const;
	void SetID(uint32 id);

	VDStringW GetCaption() {
		return mCaption;
	}

	void SetCaption(const wchar_t *caption) {
		mCaption = caption;
	}

	vduirect GetArea() const {
		return mArea;
	}

	void SetArea(const vduirect& pos) {
		mArea = pos;

		OnResize();
	}

	vduirect GetClientArea() const {
		return vduirect(0, 0, mArea.width(), mArea.height());
	}

	void GetAlignment(nsVDUI::Alignment& x, nsVDUI::Alignment& y) {
		x = mAlignX;
		y = mAlignY;
	}

	void SetAlignment(nsVDUI::Alignment x, nsVDUI::Alignment y) {
		if (x)
			mAlignX = x;

		if (y)
			mAlignY = y;
	}

   	void GetMinimumSize(vduisize& s) {
   		s = mMinSize;
   	}
   
   	void SetMinimumSize(const vduisize& s) {
   		mMinSize = s;
   	}
   
   	void GetMaximumSize(vduisize& s) {
   		s = mMaxSize;
   	}
   
   	void SetMaximumSize(const vduisize& s) {
   		mMaxSize = s;
   	}
   
	nsVDUI::CompressType GetCompressType() {
		return nsVDUI::kCompressNone;
	}

   	float GetDesiredAspectRatio() {
   		return mDesiredAspectRatio;
   	}
   
   	void SetDesiredAspectRatio(float rAspect) {
   		mDesiredAspectRatio = rAspect;
   	}

	void SetMargins(const vduirectinsets& insets) {
		mMargins = insets;
	}

	vduirectinsets GetMargins() const {
		return mMargins;
	}

	void SetPadding(const vduirectinsets& insets) {
		mPadding = insets;
	}

	vduirectinsets GetPadding() const {
		return mPadding;
	}

	const VDUILayoutSpecs& GetLayoutSpecs() { return mLayoutSpecs; }
	void Layout(const vduirect& target);
	void PreLayout(const VDUILayoutSpecs& parentConstraints);
	void PreLayoutAttempt(const VDUILayoutSpecs& parentConstraints);
	virtual void PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {}
	void PostLayout(const vduirect& target);
	virtual void PostLayoutBase(const vduirect& target) {
		SetArea(target);
	}

	int GetValue();
	void SetValue(int);

protected:
	virtual void OnResize() {}

	IVDUIWindow	*mpParent;
	IVDUIBase *mpBase;

	uint32			mID;

	vduirect		mArea;
	vduisize		mMinSize;
	vduisize		mMaxSize;
	vduirectinsets	mMargins;
	vduirectinsets	mPadding;
	VDUILayoutSpecs	mLayoutSpecs;
	nsVDUI::Alignment	mAlignX;
	nsVDUI::Alignment	mAlignY;
	float			mDesiredAspectRatio;
	bool			mbVisible;
	bool			mbEnabled;

	VDStringW	mCaption;

	typedef std::list<IVDUIWindow *> tChildren;
	tChildren	mChildren;

	VDAtomicInt		mRefCount;
};

#endif

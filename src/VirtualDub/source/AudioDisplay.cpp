//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include <windows.h>
#include <math.h>
#include <vd2/system/binary.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/Dita/interface.h>
#include <vd2/VDLib/fft.h>

#include "oshelper.h"
#include "resource.h"

#include "AudioDisplay.h"

#include "AVIOutputWAV.h"

extern HINSTANCE g_hInst;

extern const char g_szAudioDisplayControlName[]="birdyAudioDisplayControl";

/////////////////////////////////////////////////////////////////////////////

class IVDUIDrawContext {
public:
	enum {
		kTA_Left	= 0,
		kTA_Center	= 1,
		kTA_Right	= 2,
		kTA_HMask	= 3,

		kTA_Top			= 0x00,
		kTA_Baseline	= 0x04,
		kTA_Bottom		= 0x08,
		kTA_VMask		= 0x0C
	};

	virtual void SetColor(uint32 color) = 0;
	virtual void SetTextBackColor(uint32 color) = 0;
	virtual void SetTextBackTransparent() = 0;
	virtual void SetTextAlignment(int alignment) = 0;

	virtual void SetOffset(int x, int y) = 0;

	virtual void DrawLine(int x1, int y1, int x2, int y2) = 0;
	virtual void FillRect(int x, int y, int w, int h) = 0;

	virtual void DrawTextLine(int x, int y, const char *text) = 0;

	virtual vduisize MeasureText(const char *text) = 0;
};

class VDUIDrawContextGDI : public IVDUIDrawContext {
public:
	VDUIDrawContextGDI();
	~VDUIDrawContextGDI();

	bool Init(HDC hdc);
	void Shutdown();

	HDC GetHDC() const { return mhdc; }

	void Reset();
	void SetColor(uint32 color);
	void SetTextBackColor(uint32 color);
	void SetTextBackTransparent();
	void SetTextAlignment(int alignment);

	void SetOffset(int x, int y);

	void DrawLine(int x1, int y1, int x2, int y2);
	void FillRect(int x, int y, int w, int h);

	void DrawTextLine(int x, int y, const char *text);

	vduisize MeasureText(const char *text);

protected:
	void UpdatePen();

	HDC mhdc;
	int mSavedDC;
	HPEN mpen;
	uint32 mColor;
	uint32 mLastPenColor;
	uint32 mLastTextColor;
	int mOffsetX;
	int mOffsetY;
};

VDUIDrawContextGDI::VDUIDrawContextGDI()
	: mhdc(NULL)
	, mpen((HPEN)GetStockObject(BLACK_PEN))
	, mColor(0xFF000000)
	, mLastPenColor(0xFF000000)
	, mLastTextColor(0xFF000000)
	, mOffsetX(0)
	, mOffsetY(0)
{
}

VDUIDrawContextGDI::~VDUIDrawContextGDI() {
}

bool VDUIDrawContextGDI::Init(HDC hdc) {
	mSavedDC = SaveDC(hdc);
	if (!mSavedDC)
		return false;

	mhdc = hdc;
	SelectObject(mhdc, mpen);
	SelectObject(mhdc, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
	SetBkMode(mhdc, TRANSPARENT);
	SetBkColor(mhdc, RGB(0, 0, 0));
	SetTextAlign(mhdc, TA_TOP | TA_LEFT);
	return true;
}

void VDUIDrawContextGDI::Shutdown() {
	if (mhdc) {
		RestoreDC(mhdc, mSavedDC);
		mhdc = NULL;
	}
}

void VDUIDrawContextGDI::Reset() {
	SetColor(0xFF000000);
	SetBkMode(mhdc, TRANSPARENT);
	SetBkColor(mhdc, RGB(0, 0, 0));
}

void VDUIDrawContextGDI::SetColor(uint32 color) {
	mColor = color;
}

void VDUIDrawContextGDI::SetTextBackColor(uint32 color) {
	SetBkMode(mhdc, OPAQUE);
	SetBkColor(mhdc, VDSwizzleU32(color) >> 8);
}

void VDUIDrawContextGDI::SetTextBackTransparent() {
	SetBkMode(mhdc, TRANSPARENT);
}

void VDUIDrawContextGDI::SetTextAlignment(int alignment) {
	static const UINT kAlignmentMap[16]={
		TA_LEFT | TA_TOP,		TA_CENTER | TA_TOP,			TA_RIGHT | TA_TOP,			TA_LEFT | TA_TOP,
		TA_LEFT | TA_BASELINE,	TA_CENTER | TA_BASELINE,	TA_RIGHT | TA_BASELINE,		TA_LEFT | TA_BASELINE,
		TA_LEFT | TA_BOTTOM,	TA_CENTER | TA_BOTTOM,		TA_RIGHT | TA_BOTTOM,		TA_LEFT | TA_BOTTOM,
		TA_LEFT | TA_TOP,		TA_CENTER | TA_TOP,			TA_RIGHT | TA_TOP,			TA_LEFT | TA_TOP,
	};

	SetTextAlign(mhdc, kAlignmentMap[alignment & 15]);
}

void VDUIDrawContextGDI::SetOffset(int x, int y) {
	mOffsetX = x;
	mOffsetY = y;
}

void VDUIDrawContextGDI::DrawLine(int x1, int y1, int x2, int y2) {
	UpdatePen();
	MoveToEx(mhdc, mOffsetX + x1, mOffsetY + y1, NULL);
	LineTo(mhdc, mOffsetX + x2, mOffsetY + y2);
}

void VDUIDrawContextGDI::FillRect(int x, int y, int w, int h) {
}

void VDUIDrawContextGDI::DrawTextLine(int x, int y, const char *text) {
	TextOutA(mhdc, mOffsetX + x, mOffsetY + y, text, strlen(text));
}

vduisize VDUIDrawContextGDI::MeasureText(const char *text) {
	SIZE siz = {0,0};
	GetTextExtentPoint32(mhdc, text, strlen(text), &siz);

	return vduisize(siz.cx, siz.cy);
}

void VDUIDrawContextGDI::UpdatePen() {
	if (mLastPenColor != mColor) {
		mLastPenColor = mColor;

		COLORREF c = VDSwizzleU32(mColor) >> 8;

		HPEN pen = CreatePen(PS_SOLID, 0, c);

		if (pen) {
			if (SelectObject(mhdc, pen)) {
				if (mpen)
					DeleteObject(mpen);

				mpen = pen;
			} else {
				DeleteObject(pen);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

class VDUISprite;

class IVDUISpriteContext : public IVDUnknown {
public:
	virtual IVDUIDrawContext *GetIC() = 0;
	virtual void ReleaseIC(IVDUIDrawContext *) = 0;
	virtual void InvalidateCanvas(const vduirect& r) = 0;
};

class VDUISprite : public vdrefcounted<IVDRefCount> {
public:
	VDUISprite();
	~VDUISprite();

	virtual void Attach(IVDUISpriteContext *, VDUISprite *parent);
	virtual void Detach();

	virtual void AddChild(VDUISprite *sprite);
	virtual void RemoveChild(VDUISprite *sprite);

	virtual void Render(IVDUIDrawContext& dc, int x, int y);
	virtual void RenderLocal(IVDUIDrawContext& dc) {}
	virtual bool HitTest(int x, int y) { return true; }

	const vduirect& GetBounds() const { return mBounds; }
	void SetBounds(const vduirect& r) { mBounds = r; }

	const vduirect& GetLastRenderedBounds() const { return mLastRenderedBounds; }
	const vduirect& GetCumulativeBounds() const { return mCumulativeBounds; }

	void Invalidate();

protected:
	void Invalidate(int xoffset, int yoffset);

	vduirect	mLastRenderedBounds;		// global
	vduirect	mBounds;					// local
	vduirect	mCumulativeBounds;			// global

	IVDUISpriteContext *mpContext;
	VDUISprite *mpParent;

	typedef vdfastvector<VDUISprite *> Sprites;
	Sprites mSprites;
};

VDUISprite::VDUISprite()
	: mBounds(0,0,0,0)
	, mpContext(NULL)
	, mpParent(NULL)
{
}

VDUISprite::~VDUISprite() {
	while(!mSprites.empty())
		RemoveChild(mSprites.back());
}

void VDUISprite::Attach(IVDUISpriteContext *context, VDUISprite *parent) {
	mpContext = context;
	mpParent = parent;
	mLastRenderedBounds.set(0, 0, 0, 0);

	Sprites::const_iterator it(mSprites.begin()), itEnd(mSprites.end());
	for(; it!=itEnd; ++it) {
		VDUISprite& sprite = **it;

		sprite.Attach(context, this);
	}
}

void VDUISprite::Detach() {
	if (mpContext) {
		Sprites::const_iterator it(mSprites.begin()), itEnd(mSprites.end());
		for(; it!=itEnd; ++it) {
			VDUISprite& sprite = **it;

			sprite.Detach();
		}

		mpContext = NULL;
		mpParent = NULL;
	}
}

void VDUISprite::AddChild(VDUISprite *sprite) {
	sprite->AddRef();
	mSprites.push_back(sprite);

	if (mpContext)
		sprite->Attach(mpContext, this);
}

void VDUISprite::RemoveChild(VDUISprite *sprite) {
	Sprites::iterator it(std::find(mSprites.begin(), mSprites.end(), sprite));

	if (it != mSprites.end()) {
		if (mpContext)
			sprite->Invalidate();
		sprite->Detach();
		mSprites.erase(it);
		sprite->Release();
	}
}

void VDUISprite::Render(IVDUIDrawContext& dc, int x, int y) {
	x += mBounds.left;
	y += mBounds.top;

	dc.SetOffset(x, y);
	RenderLocal(dc);

	Sprites::const_iterator it(mSprites.begin()), itEnd(mSprites.end());
	for(; it!=itEnd; ++it) {
		VDUISprite& sprite = **it;

		sprite.Render(dc, x, y);
	}
}

void VDUISprite::Invalidate() {
	int x = 0;
	int y = 0;

	for(VDUISprite *p = mpParent; p; p = p->mpParent) {
		x += p->mBounds.left;
		y += p->mBounds.top;
	}

	Invalidate(x, y);
}

void VDUISprite::Invalidate(int xoffset, int yoffset) {
	mpContext->InvalidateCanvas(mLastRenderedBounds);
	mLastRenderedBounds = mBounds;
	mLastRenderedBounds.left += xoffset;
	mLastRenderedBounds.top += yoffset;
	mLastRenderedBounds.right += xoffset;
	mLastRenderedBounds.bottom += yoffset;
	mpContext->InvalidateCanvas(mLastRenderedBounds);

	Sprites::const_iterator it(mSprites.begin()), itEnd(mSprites.end());
	for(; it!=itEnd; ++it) {
		VDUISprite& sprite = **it;

		sprite.Invalidate(mLastRenderedBounds.left, mLastRenderedBounds.top);
	}
}

/////////////////////////////////////////////////////////////////////////////

class VDUISpriteBasedControlW32 : public IVDUISpriteContext {
public:
	VDUISpriteBasedControlW32(HWND hwnd);
	virtual ~VDUISpriteBasedControlW32();

	int AddRef();
	int Release();

	void AddSprite(VDUISprite *sprite);
	void RemoveSprite(VDUISprite *sprite);
	void Render(IVDUIDrawContext& dc);

public:
	void InvalidateCanvas(const vduirect& r);
	IVDUIDrawContext *GetIC();
	void ReleaseIC(IVDUIDrawContext *);

public:
	template<class T>
	static LRESULT APIENTRY StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	virtual LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	const HWND mhwnd;

	vdrefptr<VDUISprite> mpRootSprite;

	VDUIDrawContextGDI	mDC;

	VDAtomicInt	mRefCount;
};

VDUISpriteBasedControlW32::VDUISpriteBasedControlW32(HWND hwnd)
	: mhwnd(hwnd)
	, mpRootSprite(new VDUISprite)
	, mRefCount(1)
{
	mpRootSprite->Attach(this, NULL);
}

VDUISpriteBasedControlW32::~VDUISpriteBasedControlW32() {
}

int VDUISpriteBasedControlW32::AddRef() {
	return ++mRefCount;
}

int VDUISpriteBasedControlW32::Release() {
	int rc = --mRefCount;

	if (!rc)
		delete this;

	return rc;
}

void VDUISpriteBasedControlW32::AddSprite(VDUISprite *sprite) {
	mpRootSprite->AddChild(sprite);
}

void VDUISpriteBasedControlW32::RemoveSprite(VDUISprite *sprite) {
	mpRootSprite->RemoveChild(sprite);
}

void VDUISpriteBasedControlW32::Render(IVDUIDrawContext& dc) {
	mpRootSprite->Render(dc, 0, 0);
}

void VDUISpriteBasedControlW32::InvalidateCanvas(const vduirect& r) {
	// We dilate by one pixel to work around antialiasing related glitches, particularly
	// ClearType text rendering slightly out of bounds.
	RECT r2 = { r.left - 1, r.top - 1, r.right + 1, r.bottom + 1 };
	InvalidateRect(mhwnd, &r2, TRUE);
}

IVDUIDrawContext *VDUISpriteBasedControlW32::GetIC() {
	HDC hdc = GetDC(mhwnd);
	if (!hdc)
		return NULL;
	if (!mDC.Init(hdc)) {
		ReleaseDC(mhwnd, hdc);
		return NULL;
	}

	return &mDC;
}

void VDUISpriteBasedControlW32::ReleaseIC(IVDUIDrawContext *pdc) {
	HDC hdc = mDC.GetHDC();
	mDC.Shutdown();
	ReleaseDC(mhwnd, hdc);
}

template<class T>
LRESULT APIENTRY VDUISpriteBasedControlW32::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDUISpriteBasedControlW32 *pThis = (VDUISpriteBasedControlW32 *)GetWindowLongPtr(hwnd, 0);

	if (msg == WM_NCCREATE) {
		pThis = new T(hwnd);

		if (!pThis)
			return FALSE;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
	} else if (msg == WM_NCDESTROY) {
		pThis->Release();
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return pThis->WndProc(msg, wParam, lParam);
}

LRESULT VDUISpriteBasedControlW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////

class VDUITextSprite : public VDUISprite {
public:
	void RenderLocal(IVDUIDrawContext& dc);

	void SetCenteredText(int cx, int cy, const char *text);

protected:
	VDStringA	mText;
};

void VDUITextSprite::RenderLocal(IVDUIDrawContext& dc) {
	dc.SetColor(0xFFFFFFFF);
	dc.SetTextBackColor(0xFF000000);
	dc.SetTextAlignment(dc.kTA_Top | dc.kTA_Left);
	dc.DrawTextLine(0, 0, mText.c_str());
}

void VDUITextSprite::SetCenteredText(int cx, int cy, const char *text) {
	IVDUIDrawContext *ic = mpContext->GetIC();

	const vduisize siz(ic->MeasureText(text));

	mpContext->ReleaseIC(ic);

	mText = text;

	int x = cx - (siz.w >> 1);
	int y = cy - (siz.h >> 1);
	SetBounds(vduirect(x, y, x+siz.w, y+siz.h));
	Invalidate();
}

/////////////////////////////////////////////////////////////////////////////

class VDUIDimensionSprite : public VDUISprite {
public:
	VDUIDimensionSprite();
	~VDUIDimensionSprite();

	void RenderLocal(IVDUIDrawContext& dc);

	void SetLine(int x1, int y1, int x2, int ht, const char *text);

protected:
	VDStringA	mText;
	int mYOffset;

	vdrefptr<VDUITextSprite> mpTextSprite;
};

VDUIDimensionSprite::VDUIDimensionSprite()
	: mpTextSprite(new VDUITextSprite)
{
	AddChild(mpTextSprite);
}

VDUIDimensionSprite::~VDUIDimensionSprite() {
}

void VDUIDimensionSprite::RenderLocal(IVDUIDrawContext& dc) {
	dc.SetColor(0xFFFFFFFF);

	int w = mBounds.right - mBounds.left;
	int h = mBounds.bottom - mBounds.top;

	dc.DrawLine(0, mYOffset, w, mYOffset);
	dc.DrawLine(0, 0, 0, h);
	dc.DrawLine(w-1, 0, w-1, h);
}

void VDUIDimensionSprite::SetLine(int x1, int y1, int x2, int ht, const char *text) {
	y1 -= ht >> 1;
	mYOffset = ht >> 1;

	if (x2 < x1)
		std::swap(x1, x2);

	SetBounds(vduirect(x1, y1, x2+1, y1 + ht));
	Invalidate();

	mpTextSprite->SetCenteredText((x2 - x1) >> 1, mYOffset + 4, text);
}

/////////////////////////////////////////////////////////////////////////////
//
//	VDAudioDisplayControl
//
/////////////////////////////////////////////////////////////////////////////

class VDAudioDisplayControl : public VDUISpriteBasedControlW32, public IVDUIAudioDisplayControl {
public:
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	VDAudioDisplayControl(HWND hwnd);
	~VDAudioDisplayControl();

	int AddRef() { return VDUISpriteBasedControlW32::AddRef(); }
	int Release() { return VDUISpriteBasedControlW32::Release(); }
	void *AsInterface(uint32 id);

	void SetSpectralPaletteDefault();
	void SetSpectralBoost(int boost);

	Mode GetMode();
	void SetMode(Mode mode);

	int GetZoom();
	void SetZoom(int samplesPerPixel);

	void ClearFailureMessage();
	void SetFailureMessage(const wchar_t *s);

	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnCommand(int command);
	void OnInitMenu(HMENU hmenu);
	void OnMouseMove(int x, int y, uint32 modifiers);
	void OnLButtonDown(int x, int y, uint32 modifiers);
	void OnLButtonUp(int x, int y, uint32 modifiers);
	void OnRButtonDown(int x, int y, uint32 modifiers);
	void OnPaint(HDC hdc, const PAINTSTRUCT& ps);
	void OnSize();
	void OnTimer();
	void PaintChannel(HDC hdc, int ch);

	void SetFormat(double samplingRate, int channels);
	void SetFrameMarkers(sint64 mn, sint64 mx, double start, double rate);
	void SetHighlightedFrameMarker(VDPosition pos);
	void SetSelectedFrameRange(VDPosition start, VDPosition end);
	void ClearSelectedFrameRange();
	void SetPosition(VDPosition pos);
	void Rescan();
	VDPosition GetReadPosition();
	bool ProcessAudio8U(const uint8 *src, int count, int chanStride, int sampleStride);
	bool ProcessAudio16S(const sint16 *src, int count, int chanStride, int sampleStride);
	VDEvent<IVDUIAudioDisplayControl, VDPosition>& AudioRequiredEvent();
	VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& SetSelectStartEvent();
	VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& SetSelectTrackEvent();
	VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& SetSelectEndEvent();
	VDEvent<IVDUIAudioDisplayControl, sint32>& TrackAudioOffsetEvent();
	VDEvent<IVDUIAudioDisplayControl, sint32>& SetAudioOffsetEvent();

protected:
	bool ProcessAudio(const void *src, int count, int chanStride, int sampleStride, bool bit16);
	void RecomputeMarkerSteps();
	void InvalidateRange(int x1, int x2);

	static void FastFill(HDC hdc, int x1, int y1, int x2, int y2, DWORD c);

	HFONT		mhfont;
	int			mFontDigitWidth;
	int			mFontHeight;

	HMENU		mhmenuPopup;

	int			mWidth;
	int			mHeight;
	int			mChanHeight;
	int			mChanWidth;
	int			mChanCount;
	VDPosition	mHighlightedMarker;
	VDPosition	mSelectedMarkerRangeStart;
	VDPosition	mSelectedMarkerRangeEnd;
	VDPosition	mAudioOffsetDragAnchor;
	VDPosition	mAudioOffsetDragEndPoint;
	uint32		mAccumulatedSamples;
	uint32		mBufferedWindowSamples;
	double		mSamplingRate;
	sint64		mMarkerRangeMin;
	sint64		mMarkerRangeMax;
	double		mMarkerStart;
	double		mMarkerRate;
	double		mMarkerInvRate;
	double		mMarkerMinorRate;
	double		mMarkerMinorInvRate;
	uint32		mMarkerMinorStep;
	uint32		mMarkerMajorStep;
	sint32		mSamplesPerPixel;
	double		mPixelsPerSample;
	sint64		mPosition;
	sint64		mWindowPosition;
	sint64		mReadPosition;
	bool		mbSpectrumMode;
	bool		mbPointsDirty;
	bool		mbSolidWaveform;
	int			mSpectralBoost;

	int			mUpdateX1;
	int			mUpdateX2;
	UINT		mUpdateTimer;

	enum DragMode {
		kDragModeNone,
		kDragModeSelect,
		kDragModeAudioOffset,
		kDragModeCount
	};

	DragMode	mDragMode;
	int			mDragAnchorX;
	int			mDragAnchorY;

	vdrefptr<VDUIDimensionSprite> mpDimensionSprite;

	vdfastvector<uint8>	mImage;
	vdfastvector<POINT> mPoints;

	VDStringW	mFailureMessage;

	VDEvent<IVDUIAudioDisplayControl, VDPosition> mAudioRequiredEvent;
	VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange> mSetSelectStartEvent;
	VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange> mSetSelectTrackEvent;
	VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange> mSetSelectEndEvent;
	VDEvent<IVDUIAudioDisplayControl, sint32> mTrackAudioOffsetEvent;
	VDEvent<IVDUIAudioDisplayControl, sint32> mSetAudioOffsetEvent;

	struct {
		BITMAPINFOHEADER hdr;
		RGBQUAD pal[256];
	} mbihSpectrum, mbihSpectrumHighlight, mbihSpectrumMinorMarker, mbihSpectrumMajorMarker;

	typedef std::vector<VDRollingRealFFT> Transforms;
	Transforms mTransforms;
};

ATOM RegisterAudioDisplayControl() {
	WNDCLASS wc;

	wc.style		= CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc	= VDUISpriteBasedControlW32::StaticWndProc<VDAudioDisplayControl>;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(IVDUnknown *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);		//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= AUDIODISPLAYCONTROLCLASS;

	return RegisterClass(&wc);
}

IVDUIAudioDisplayControl *VDGetIUIAudioDisplayControl(VDGUIHandle h) {
	return vdpoly_cast<IVDUIAudioDisplayControl *>((IVDUnknown *)GetWindowLongPtr((HWND)h, 0));
}

VDAudioDisplayControl::VDAudioDisplayControl(HWND hwnd)
	: VDUISpriteBasedControlW32(hwnd)
	, mWidth(0)
	, mHeight(0)
	, mChanWidth(0)
	, mChanHeight(0)
	, mChanCount(0)
	, mAccumulatedSamples(0)
	, mBufferedWindowSamples(0)
	, mSamplingRate(44100.0)
	, mHighlightedMarker(-1)
	, mSelectedMarkerRangeStart(-1)
	, mSelectedMarkerRangeEnd(-1)
	, mMarkerRangeMin(0)
	, mMarkerRangeMax(0)
	, mMarkerStart(0.0)
	, mMarkerRate(1e+6)
	, mMarkerInvRate(1e-6)
	, mMarkerMinorRate(1e+6)
	, mMarkerMinorInvRate(1e-6)
	, mMarkerMinorStep(1)
	, mMarkerMajorStep(1)
	, mSamplesPerPixel(32)
	, mPixelsPerSample(1.0 / 32.0)
	, mPosition(-1)
	, mWindowPosition(0)
	, mReadPosition(-1)
	, mbSpectrumMode(false)
	, mbPointsDirty(false)
	, mbSolidWaveform(false)
	, mSpectralBoost(0)
	, mUpdateX1(INT_MAX)
	, mUpdateX2(INT_MIN)
	, mUpdateTimer(0)
	, mDragMode(kDragModeNone)
{
	SetSpectralPaletteDefault();

	mbihSpectrum.hdr.biSize				= sizeof(BITMAPINFOHEADER);
	mbihSpectrum.hdr.biWidth			= 0;
	mbihSpectrum.hdr.biHeight			= 256;
	mbihSpectrum.hdr.biPlanes			= 1;
	mbihSpectrum.hdr.biCompression		= BI_RGB;
	mbihSpectrum.hdr.biBitCount			= 8;
	mbihSpectrum.hdr.biSizeImage		= 0;
	mbihSpectrum.hdr.biXPelsPerMeter	= 0;
	mbihSpectrum.hdr.biYPelsPerMeter	= 0;
	mbihSpectrum.hdr.biClrUsed			= 256;
	mbihSpectrum.hdr.biClrImportant		= 256;
	mbihSpectrumHighlight.hdr = mbihSpectrum.hdr;
	mbihSpectrumMinorMarker.hdr = mbihSpectrum.hdr;
	mbihSpectrumMajorMarker.hdr = mbihSpectrum.hdr;

	mhfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	mFontDigitWidth = 12;
	mFontHeight = 16;
	if (HDC hdc = GetDC(hwnd)) {
		TEXTMETRIC tm;
		if (SelectObject(hdc, mhfont) && GetTextMetrics(hdc, &tm)) {
			mFontHeight = tm.tmHeight;

			SIZE siz;
			if (GetTextExtentPoint32(hdc, "0123456789", 10, &siz))
				mFontDigitWidth = (siz.cx + 9) / 10;
		}
		ReleaseDC(hwnd, hdc);
	}

	mhmenuPopup = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_AUDIODISPLAY_MENU));
	if (mhmenuPopup)
		mhmenuPopup = GetSubMenu(mhmenuPopup, 0);
}

VDAudioDisplayControl::~VDAudioDisplayControl() {
	if (mhfont)
		DeleteObject(mhfont);
}

void *VDAudioDisplayControl::AsInterface(uint32 id) {
	if (id == IVDUIAudioDisplayControl::kTypeID)
		return static_cast<IVDUIAudioDisplayControl *>(this);

	return NULL;
}

void VDAudioDisplayControl::SetSpectralPaletteDefault() {
	static const uint32 kDefaultPalette[]={
		0x000000,0x000000,0x000000,0x000000,0x000001,0x000001,0x000001,0x000002,
		0x010002,0x010003,0x010003,0x010004,0x010005,0x010006,0x010006,0x020007,
		0x020008,0x020009,0x02000a,0x03000b,0x03000c,0x03000d,0x03000e,0x04000f,
		0x040010,0x040011,0x050012,0x050114,0x050115,0x060116,0x060117,0x070119,
		0x07011a,0x07011b,0x08011c,0x08011e,0x09011f,0x090120,0x0a0121,0x0a0223,
		0x0b0224,0x0c0225,0x0c0226,0x0d0228,0x0d0229,0x0e022a,0x0f032b,0x0f032d,
		0x10032e,0x11032f,0x110330,0x120332,0x130433,0x140434,0x140435,0x150436,
		0x160537,0x170539,0x18053a,0x19053b,0x19063c,0x1a063d,0x1b063e,0x1c063f,
		0x1d0740,0x1e0741,0x1f0742,0x200843,0x210844,0x220845,0x230946,0x240946,
		0x250947,0x260a48,0x270a49,0x280b4a,0x290b4a,0x2a0c4b,0x2c0c4c,0x2d0c4d,
		0x2e0d4d,0x2f0d4e,0x300e4e,0x310e4f,0x330f50,0x340f50,0x351051,0x361051,
		0x381152,0x391152,0x3a1252,0x3b1353,0x3d1353,0x3e1453,0x3f1454,0x411554,
		0x421654,0x431655,0x451755,0x461855,0x471855,0x491955,0x4a1a55,0x4c1b55,
		0x4d1b55,0x4e1c56,0x501d56,0x511e56,0x531e56,0x541f55,0x562055,0x572155,
		0x582255,0x5a2255,0x5b2355,0x5d2455,0x5e2555,0x602654,0x612754,0x632854,
		0x642953,0x662a53,0x672b53,0x692c53,0x6a2c52,0x6c2d52,0x6d2e51,0x6f3051,
		0x703151,0x723250,0x733350,0x75344f,0x76354f,0x78364e,0x79374e,0x7b384d,
		0x7c394c,0x7d3a4c,0x7f3b4b,0x803d4b,0x823e4a,0x833f49,0x854049,0x864148,
		0x884347,0x894447,0x8a4546,0x8c4645,0x8d4845,0x8f4944,0x904a43,0x914c42,
		0x934d41,0x944e41,0x955040,0x97513f,0x98523e,0x99543e,0x9b553d,0x9c563c,
		0x9d583b,0x9f593a,0xa05b39,0xa15c39,0xa25e38,0xa45f37,0xa56036,0xa66235,
		0xa76334,0xa86533,0xa96633,0xab6832,0xac6931,0xad6b30,0xae6d2f,0xaf6e2e,
		0xb0702d,0xb1712c,0xb2732b,0xb3742b,0xb4762a,0xb57829,0xb67928,0xb77b27,
		0xb87d26,0xb97e25,0xb98024,0xba8224,0xbb8323,0xbc8522,0xbd8721,0xbd8820,
		0xbe8a1f,0xbf8c1e,0xbf8d1e,0xc08f1d,0xc1911c,0xc1931b,0xc2941a,0xc2961a,
		0xc39819,0xc39a18,0xc49b17,0xc49d16,0xc59f16,0xc5a115,0xc6a214,0xc6a413,
		0xc6a613,0xc7a812,0xc7aa11,0xc7ab11,0xc7ad10,0xc8af0f,0xc8b10f,0xc8b30e,
		0xc8b50d,0xc8b60d,0xc8b80c,0xc8ba0b,0xc8bc0b,0xc8be0a,0xc8c00a,0xc8c109,
		0xc8c309,0xc8c508,0xc8c708,0xc8c907,0xc7cb07,0xc7cd06,0xc7ce06,0xc7d005,
		0xc6d205,0xc6d404,0xc6d604,0xc5d804,0xc5da03,0xc4db03,0xc4dd03,0xc3df02,
		0xc3e102,0xc2e302,0xc2e502,0xc1e701,0xc0e801,0xc0ea01,0xbfec01,0xbeee01,
		0xbef000,0xbdf200,0xbcf300,0xbbf500,0xbaf700,0xb9f900,0xb8fb00,0xb8fd00,
	};

	memcpy(mbihSpectrum.pal, kDefaultPalette, sizeof mbihSpectrum.pal);

	for(uint32 i=0; i<256; ++i) {
		uint32 r = mbihSpectrum.pal[i].rgbRed;
		uint32 g = mbihSpectrum.pal[i].rgbGreen;
		uint32 b = mbihSpectrum.pal[i].rgbBlue;

		mbihSpectrumHighlight.pal[i].rgbRed = (BYTE)r;
		mbihSpectrumHighlight.pal[i].rgbGreen = (BYTE)((g * (255-32) + 128) / 255 + 32);
		mbihSpectrumHighlight.pal[i].rgbBlue = (BYTE)((b * (255-24) + 128) / 255 + 24);
		mbihSpectrumHighlight.pal[i].rgbReserved = 0;

		mbihSpectrumMinorMarker.pal[i].rgbRed = (BYTE)((r * (255-32) + 128) / 255 + 32);
		mbihSpectrumMinorMarker.pal[i].rgbGreen = (BYTE)((g * (255-32) + 128) / 255 + 32);
		mbihSpectrumMinorMarker.pal[i].rgbBlue = (BYTE)((b * (255-32) + 128) / 255 + 32);
		mbihSpectrumMinorMarker.pal[i].rgbReserved = 0;

		mbihSpectrumMajorMarker.pal[i].rgbRed = (BYTE)((r * (255-64) + 128) / 255 + 64);
		mbihSpectrumMajorMarker.pal[i].rgbGreen = (BYTE)((g * (255-64) + 128) / 255 + 64);
		mbihSpectrumMajorMarker.pal[i].rgbBlue = (BYTE)((b * (255-64) + 128) / 255 + 64);
		mbihSpectrumMajorMarker.pal[i].rgbReserved = 0;
	}
}

void VDAudioDisplayControl::SetSpectralBoost(int boost) {
	if (mSpectralBoost != boost) {
		mSpectralBoost = boost;
		Rescan();
	}
}

IVDUIAudioDisplayControl::Mode VDAudioDisplayControl::GetMode() {
	return mbSpectrumMode ? kModeSpectrogram : kModeWaveform;
}

void VDAudioDisplayControl::SetMode(Mode mode) {
	bool spectrumMode = (mode == kModeSpectrogram);

	if (spectrumMode != mbSpectrumMode) {
		mbSpectrumMode = spectrumMode;
		Rescan();
	}
}

int VDAudioDisplayControl::GetZoom() {
	return mSamplesPerPixel;
}

void VDAudioDisplayControl::SetZoom(int samplesPerPixel) {
	while(int t = samplesPerPixel & (samplesPerPixel - 1))
		samplesPerPixel = t;

	if (samplesPerPixel < 1)
		samplesPerPixel = 1;
	else if (samplesPerPixel > 1024)
		samplesPerPixel = 1024;

	if (mSamplesPerPixel != samplesPerPixel) {
		mSamplesPerPixel = samplesPerPixel;
		mPixelsPerSample = 1.0 / (double)mSamplesPerPixel;

		RecomputeMarkerSteps();

		VDPosition pos = mPosition;
		mPosition = -1;
		SetPosition(pos);
	}
}

void VDAudioDisplayControl::ClearFailureMessage() {
	mFailureMessage.clear();
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDAudioDisplayControl::SetFailureMessage(const wchar_t *s) {
	mFailureMessage = s;
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDAudioDisplayControl::SetFormat(double samplingRate, int channels) {
	if (mSamplingRate != samplingRate || mChanCount != channels) {
		mSamplingRate = samplingRate;
		mChanCount = channels;

		mTransforms.resize(channels);

		for(Transforms::iterator it(mTransforms.begin()), itEnd(mTransforms.end()); it!=itEnd; ++it) {
			it->Init(13);
		}

		--mHeight;
		OnSize();
		Rescan();
	}
}

void VDAudioDisplayControl::SetFrameMarkers(sint64 mn, sint64 mx, double start, double rate) {
	mMarkerRangeMin = mn;
	mMarkerRangeMax = mx;
	mMarkerStart = start;
	mMarkerRate = rate;
	mMarkerInvRate = 1.0 / rate;

	RecomputeMarkerSteps();
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDAudioDisplayControl::SetHighlightedFrameMarker(VDPosition pos) {
	if (mHighlightedMarker != pos) {
		mHighlightedMarker = pos;

		InvalidateRect(mhwnd, NULL, TRUE);
	}
}

void VDAudioDisplayControl::SetSelectedFrameRange(VDPosition start, VDPosition end) {
	if (mSelectedMarkerRangeStart != start || mSelectedMarkerRangeEnd != end) {
		mSelectedMarkerRangeStart = start;
		mSelectedMarkerRangeEnd = end;

		RECT r = {0, mChanHeight * mChanCount, mChanWidth, mHeight};
		InvalidateRect(mhwnd, &r, TRUE);
	}
}

void VDAudioDisplayControl::ClearSelectedFrameRange() {
	if (mSelectedMarkerRangeStart >= 0) {
		mSelectedMarkerRangeStart = -1;
		mSelectedMarkerRangeEnd = -1;

		RECT r = {0, mChanHeight * mChanCount, mChanWidth, mHeight};
		InvalidateRect(mhwnd, &r, TRUE);
	}
}

void VDAudioDisplayControl::SetPosition(VDPosition pos) {
	// round off position
	pos -= pos % mSamplesPerPixel;

	// check for null move
	if (mPosition == pos)
		return;

	sint64 newWindowPos = pos - ((mChanWidth * mSamplesPerPixel) >> 1);

	// check for forward move within window
	if (pos > mPosition && mPosition >= 0) {
		uint64 delta64 = (uint64)(pos - mPosition) / mSamplesPerPixel;

		if (delta64 < mChanWidth) {
			uint32 delta = (uint32)delta64;

			if (mbSpectrumMode) {
				if (mAccumulatedSamples >= delta) {
					mAccumulatedSamples -= delta;
					uint32 pitch = (mChanWidth + 3) & ~3;
					for(int y=0; y<256*mChanCount; ++y)
						memmove(&mImage[pitch*y], &mImage[pitch*y + delta], mAccumulatedSamples);

					mPosition = pos;
					mWindowPosition = newWindowPos;
					if (mReadPosition < 0) {
						mReadPosition = newWindowPos - (8192 >> 1) + mSamplesPerPixel * mAccumulatedSamples;
						mBufferedWindowSamples = 0;
						mAudioRequiredEvent.Raise(this, mReadPosition);
					}

					InvalidateRect(mhwnd, NULL, TRUE);
					return;
				}
			} else {
				if (mAccumulatedSamples >= delta * mSamplesPerPixel) {
					mAccumulatedSamples -= delta * mSamplesPerPixel;
					mImage.erase(mImage.begin(), mImage.begin() + delta * mSamplesPerPixel * mChanCount);
					mbPointsDirty = true;
					VDASSERT(mImage.size() == mAccumulatedSamples * mChanCount);

					mPosition = pos;
					mWindowPosition = newWindowPos;
					if (mReadPosition < 0) {
						mReadPosition = newWindowPos + mAccumulatedSamples;
						mAudioRequiredEvent.Raise(this, mReadPosition);
					}
					InvalidateRect(mhwnd, NULL, TRUE);
					return;
				}
			}
		}
	}

	mPosition = pos;
	mWindowPosition = newWindowPos;

	Rescan();
}

void VDAudioDisplayControl::Rescan() {
	mImage.clear();
	mAccumulatedSamples = 0;
	mBufferedWindowSamples = 0;
	mbPointsDirty = true;
	mReadPosition = mWindowPosition;

	if (mbSpectrumMode) {
		unsigned pitch = (mChanWidth + 3) & ~3;
		mImage.resize(256*pitch*mChanCount, 0);

		mbihSpectrum.hdr.biWidth		= mChanWidth;
		mbihSpectrum.hdr.biHeight		= 256 * mChanCount;
		mbihSpectrum.hdr.biSizeImage	= mImage.size();
		mbihSpectrumHighlight.hdr = mbihSpectrum.hdr;
		mbihSpectrumMinorMarker.hdr = mbihSpectrum.hdr;
		mbihSpectrumMajorMarker.hdr = mbihSpectrum.hdr;

		for(Transforms::iterator it(mTransforms.begin()), itEnd(mTransforms.end()); it!=itEnd; ++it) {
			it->Clear();
		}

		mReadPosition -= 8192 >> 1;

		if (mReadPosition < 0 && mReadPosition >= -(mChanWidth*mSamplesPerPixel + (8192-mSamplesPerPixel))) {
			mBufferedWindowSamples -= (uint32)mReadPosition;
			mReadPosition = 0;

			int preload = (sint32)(mBufferedWindowSamples - (8192 - mSamplesPerPixel)) / (sint32)mSamplesPerPixel;
			if (preload > 0) {
				mAccumulatedSamples = preload;
				mBufferedWindowSamples -= preload * mSamplesPerPixel;
			}

			VDASSERT(mBufferedWindowSamples <= 8192);
		}
	} else {
		if (mReadPosition < 0) {
			sint32 limit = (sint32)mChanWidth * (sint32)mSamplesPerPixel;
			sint32 preload = -(sint32)mReadPosition;

			if (mReadPosition <= -limit)
				preload = limit;

			mImage.resize(preload * mChanCount, 0);
			mReadPosition = 0;
			mAccumulatedSamples = preload;
		}
	}

	if (mReadPosition >= 0)
		mAudioRequiredEvent.Raise(this, mReadPosition);

	InvalidateRect(mhwnd, NULL, TRUE);
}

VDPosition VDAudioDisplayControl::GetReadPosition() {
	return mReadPosition;
}

bool VDAudioDisplayControl::ProcessAudio8U(const uint8 *src, int count, int chanStride, int sampleStride) {
	return ProcessAudio(src, count, chanStride, sampleStride, false);
}

bool VDAudioDisplayControl::ProcessAudio16S(const sint16 *src, int count, int chanStride, int sampleStride) {
	return ProcessAudio(src, count, chanStride, sampleStride, true);
}

VDEvent<IVDUIAudioDisplayControl, VDPosition>& VDAudioDisplayControl::AudioRequiredEvent() {
	return mAudioRequiredEvent;
}

VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& VDAudioDisplayControl::SetSelectStartEvent() {
	return mSetSelectStartEvent;
}

VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& VDAudioDisplayControl::SetSelectTrackEvent() {
	return mSetSelectTrackEvent;
}

VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& VDAudioDisplayControl::SetSelectEndEvent() {
	return mSetSelectEndEvent;
}

VDEvent<IVDUIAudioDisplayControl, sint32>& VDAudioDisplayControl::TrackAudioOffsetEvent() {
	return mTrackAudioOffsetEvent;
}

VDEvent<IVDUIAudioDisplayControl, sint32>& VDAudioDisplayControl::SetAudioOffsetEvent() {
	return mSetAudioOffsetEvent;
}

LRESULT VDAudioDisplayControl::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {

	case WM_CREATE:
		OnSize();
		break;

	case WM_SIZE:
		OnSize();
		return 0;

	case WM_ERASEBKGND:
		return FALSE;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc;

			hdc = BeginPaint(mhwnd, &ps);
			OnPaint(hdc, ps);
			mDC.Init(hdc);
			Render(mDC);
			mDC.Shutdown();
			EndPaint(mhwnd, &ps);
		}
		return 0;

	case WM_MOUSEMOVE:
		OnMouseMove((short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
		break;

	case WM_LBUTTONDOWN:
		OnLButtonDown((short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
		break;

	case WM_LBUTTONUP:
		OnLButtonUp((short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
		break;

	case WM_RBUTTONDOWN:
		OnRButtonDown((short)LOWORD(lParam), (short)HIWORD(lParam), wParam);
		break;

	case WM_COMMAND:
		OnCommand((int)wParam);
		return 0;

	case WM_INITMENU:
		OnInitMenu((HMENU)wParam);
		return 0;

	case WM_TIMER:
		OnTimer();
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDAudioDisplayControl::OnCommand(int command) {
	switch(command) {
		case ID_AUDIODISPLAY_SPECTROGRAM:
			SetMode(kModeSpectrogram);
			break;

		case ID_AUDIODISPLAY_WAVEFORM:
			SetMode(kModeWaveform);
			break;

		case ID_AUDIODISPLAY_ZOOMIN:
			SetZoom(GetZoom() >> 1);
			break;

		case ID_AUDIODISPLAY_ZOOMOUT:
			SetZoom(GetZoom() << 1);
			break;

		case ID_AUDIODISPLAY_BOOST_20DB:
			SetSpectralBoost(3);
			break;

		case ID_AUDIODISPLAY_BOOST_12DB:
			SetSpectralBoost(2);
			break;

		case ID_AUDIODISPLAY_BOOST_6DB:
			SetSpectralBoost(1);
			break;

		case ID_AUDIODISPLAY_BOOST_0DB:
			SetSpectralBoost(0);
			break;
	}
}

void VDAudioDisplayControl::OnInitMenu(HMENU hmenu) {
	VDCheckRadioMenuItemByCommandW32(hmenu, ID_AUDIODISPLAY_BOOST_0DB, mSpectralBoost == 0);
	VDCheckRadioMenuItemByCommandW32(hmenu, ID_AUDIODISPLAY_BOOST_6DB, mSpectralBoost == 1);
	VDCheckRadioMenuItemByCommandW32(hmenu, ID_AUDIODISPLAY_BOOST_12DB, mSpectralBoost == 2);
	VDCheckRadioMenuItemByCommandW32(hmenu, ID_AUDIODISPLAY_BOOST_20DB, mSpectralBoost == 3);
	VDCheckRadioMenuItemByCommandW32(hmenu, ID_AUDIODISPLAY_WAVEFORM, !mbSpectrumMode);
	VDCheckRadioMenuItemByCommandW32(hmenu, ID_AUDIODISPLAY_SPECTROGRAM, mbSpectrumMode);
	VDEnableMenuItemByCommandW32(hmenu, ID_AUDIODISPLAY_ZOOMIN, mSamplesPerPixel > 1);
	VDEnableMenuItemByCommandW32(hmenu, ID_AUDIODISPLAY_ZOOMOUT, mSamplesPerPixel < 1024);
}

void VDAudioDisplayControl::OnSize() {
	RECT r;
	GetClientRect(mhwnd, &r);

	if (r.bottom != mHeight) {
		mHeight			= r.bottom;
		mChanHeight		= (mHeight - mFontHeight - 4);
		if (mChanCount > 0)
			mChanHeight		= mChanHeight / mChanCount;
		mbPointsDirty	= true;
	}

	if (r.right != mWidth) {
		mWidth			= r.right;
		mChanWidth		= r.right;

		RecomputeMarkerSteps();
		Rescan();
	}
}

void VDAudioDisplayControl::OnTimer() {
	if (mUpdateX2 > mUpdateX1) {
		RECT r = {mUpdateX1, 0, mUpdateX2, mChanHeight * mChanCount};
		mUpdateX1 = INT_MAX;
		mUpdateX2 = INT_MIN;
		InvalidateRect(mhwnd, &r, TRUE);
	}

	if (mUpdateTimer) {
		KillTimer(mhwnd, mUpdateTimer);
		mUpdateTimer = 0;
	}
}

void VDAudioDisplayControl::OnMouseMove(int x, int y, uint32 modifiers) {
	if (modifiers & MK_LBUTTON) {
		VDPosition pos = mWindowPosition + x * mSamplesPerPixel;

		switch(mDragMode) {
			case kDragModeSelect:
				{
					VDUIAudioDisplaySelectionRange range = {mAudioOffsetDragAnchor, pos};
					mSetSelectTrackEvent.Raise(this, range);
				}
				break;
			case kDragModeAudioOffset:
				mAudioOffsetDragEndPoint = pos;
				mTrackAudioOffsetEvent.Raise(this, VDClampToSint32(pos - mAudioOffsetDragAnchor));
				
				{
					char buf[64];

					sprintf(buf, "Shift audio by %+.0fms", (pos - mAudioOffsetDragAnchor) / mSamplingRate * 1000.0f);
					mpDimensionSprite->SetLine(mDragAnchorX, mDragAnchorY, x, 24, buf);
				}

				break;
		}
	}
}

void VDAudioDisplayControl::OnLButtonDown(int x, int y, uint32 modifiers) {
	if (!mFailureMessage.empty())
		return;

	VDPosition pos = mWindowPosition + x * mSamplesPerPixel;

	mDragAnchorX = x;
	mDragAnchorY = y;

	SetCapture(mhwnd);

	if (modifiers & MK_CONTROL) {
		mTrackAudioOffsetEvent.Raise(this, 0);
		mAudioOffsetDragAnchor = pos;
		mAudioOffsetDragEndPoint = pos;
		mDragMode = kDragModeAudioOffset;

		mpDimensionSprite = new VDUIDimensionSprite;
		AddSprite(mpDimensionSprite);
		mpDimensionSprite->SetLine(x, mDragAnchorY, x, 24, "+0 ms");
	} else if (modifiers & MK_SHIFT) {
		mAudioOffsetDragAnchor = pos;
		VDUIAudioDisplaySelectionRange range = {pos, pos};
		mSetSelectEndEvent.Raise(this, range);
		mDragMode = kDragModeSelect;
	}
}

void VDAudioDisplayControl::OnLButtonUp(int x, int y, uint32 modifiers) {
	VDPosition pos = mWindowPosition + x * mSamplesPerPixel;
	switch(mDragMode) {
		case kDragModeSelect:
			{
				VDUIAudioDisplaySelectionRange range = {mAudioOffsetDragAnchor, pos};
				mSetSelectEndEvent.Raise(this, range);
			}
			break;
		case kDragModeAudioOffset:
			mSetAudioOffsetEvent.Raise(this, VDClampToSint32(pos - mAudioOffsetDragAnchor));
			RemoveSprite(mpDimensionSprite);
			mpDimensionSprite = NULL;
			break;
	}
	mDragMode = kDragModeNone;

	ReleaseCapture();
}

void VDAudioDisplayControl::OnRButtonDown(int x, int y, uint32 modifiers) {
	POINT pt = {x,y};
	ClientToScreen(mhwnd, &pt);
	TrackPopupMenu(mhmenuPopup, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, mhwnd, NULL);
	PostMessage(mhwnd, WM_NULL, 0, 0);
}

void VDAudioDisplayControl::OnPaint(HDC hdc, const PAINTSTRUCT& ps) {
	if (!mFailureMessage.empty() || !mChanCount) {
		RECT r;
		if (GetClientRect(mhwnd, &r)) {
			SelectObject(hdc, mhfont);
			SetBkMode(hdc, OPAQUE);
			SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
			FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE + 1));
			VDDrawTextW32(hdc, mFailureMessage.data(), mFailureMessage.size(), &r, DT_CENTER | DT_WORDBREAK | DT_VCENTER | DT_NOPREFIX);
		}
		return;
	}

	SetStretchBltMode(hdc, COLORONCOLOR);
	
	if (mbSpectrumMode) {
		int y0 = 0;
		if (!mImage.empty()) {
//			SetDIBitsToDevice(hdc, 0, 0, mbihSpectrum.hdr.biWidth, mbihSpectrum.hdr.biHeight, 0, 0, 0, mbihSpectrum.hdr.biHeight, &mImage[0], (const BITMAPINFO *)&mbihSpectrum, DIB_RGB_COLORS);
			StretchDIBits(hdc, 0, 0, mbihSpectrum.hdr.biWidth, mChanHeight * mChanCount, 0, 0, mbihSpectrum.hdr.biWidth, mbihSpectrum.hdr.biHeight, &mImage[0], (const BITMAPINFO *)&mbihSpectrum, DIB_RGB_COLORS, SRCCOPY);
			y0 = mChanHeight * mChanCount;
		}
		FastFill(hdc, 0, y0, mChanWidth, mChanHeight * mChanCount, RGB(0,0,0));
	} else {
		FastFill(hdc, 0, 0, mChanWidth, mChanHeight * mChanCount, RGB(0,0,0));
	}

	int xselstart32 = ps.rcPaint.left;
	int xselend32 = ps.rcPaint.left;

	if (mSelectedMarkerRangeStart >= 0) {
		VDPosition xselstart = VDFloorToInt((mSelectedMarkerRangeStart*mMarkerRate + mMarkerStart - mWindowPosition) * mPixelsPerSample);
		VDPosition xselend = VDFloorToInt((mSelectedMarkerRangeEnd*mMarkerRate + mMarkerStart - mWindowPosition) * mPixelsPerSample);

		if (xselstart < ps.rcPaint.left)
			xselstart = ps.rcPaint.left;
		if (xselend > ps.rcPaint.right)
			xselend = ps.rcPaint.right;

		xselstart32 = (int)xselstart;
		xselend32 = (int)xselend;

		if (xselend32 == xselstart32)
			++xselend32;
		else if (xselend32 < xselstart32)
			xselstart32 = xselend32 = 0;
	}

	if (xselstart32 > ps.rcPaint.left)
		FastFill(hdc, 0, mChanHeight * mChanCount, xselstart32, mHeight, RGB(0,0,0));
	if (xselend32 > xselstart32)
		FastFill(hdc, xselstart32, mChanHeight * mChanCount, xselend32, mHeight, RGB(96,112,127));
	if (ps.rcPaint.right > xselend32)
		FastFill(hdc, xselend32, mChanHeight * mChanCount, ps.rcPaint.right, mHeight, RGB(0,0,0));

	if (!mbSpectrumMode) {
		for(int i=0; i<mChanCount; ++i) {
			int y = (int)((mChanHeight * (2*i+1))>>1);
			FastFill(hdc, 0, y, mChanWidth, y+1, RGB(0,128,96));
		}
	}

	int x1 = ps.rcPaint.left;
	int x2 = ps.rcPaint.right;

	sint64 marker1 = VDCeilToInt64((x1 * mSamplesPerPixel + mWindowPosition - mMarkerStart) * mMarkerMinorInvRate);
	sint64 marker2 = VDCeilToInt64((x2 * mSamplesPerPixel + mWindowPosition - mMarkerStart) * mMarkerMinorInvRate);

	// draw the previous marker so text is displayed.
	--marker1;

	if (marker1 < mMarkerRangeMin)
		marker1 = mMarkerRangeMin;

	if (marker2 > mMarkerRangeMax)
		marker2 = mMarkerRangeMax;

	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(255, 255, 255));
	SetTextAlign(hdc, TA_BOTTOM | TA_LEFT);
	SelectObject(hdc, mhfont);

	for(sint64 marker = marker1; marker < marker2; ++marker) {
		sint32 x = VDFloorToInt((marker*mMarkerMinorRate + mMarkerStart - mWindowPosition) * mPixelsPerSample);
		bool isMajor = (marker % mMarkerMajorStep) == 0;

		if (isMajor) {
			if (mbSpectrumMode) {
				StretchDIBits(hdc, x, 0, 1, mChanHeight * mChanCount, x, 0, 1, mbihSpectrum.hdr.biHeight, &mImage[0], (const BITMAPINFO *)&mbihSpectrumMajorMarker, DIB_RGB_COLORS, SRCCOPY);
			} else {
				FastFill(hdc, x, ps.rcPaint.top, x+1, ps.rcPaint.bottom, RGB(64, 64, 64));
			}
		} else if (ps.rcPaint.top < mChanHeight * mChanCount) {
			if (mbSpectrumMode) {
				StretchDIBits(hdc, x, 0, 1, mChanHeight * mChanCount, x, 0, 1, mbihSpectrum.hdr.biHeight, &mImage[0], (const BITMAPINFO *)&mbihSpectrumMinorMarker, DIB_RGB_COLORS, SRCCOPY);
			} else {
				FastFill(hdc, x, ps.rcPaint.top, x+1, mChanHeight*mChanCount, RGB(32, 32, 32));
			}
		}

		if (isMajor) {
			char buf[64];
			sprintf(buf, "%I64d", marker*mMarkerMinorStep);
			TextOut(hdc, x, mHeight - 4, buf, strlen(buf));
		}
	}

	if (ps.rcPaint.top < mChanHeight * mChanCount && mHighlightedMarker >= marker1*mMarkerMinorStep && mHighlightedMarker < marker2*mMarkerMinorStep) {
		sint32 xh1 = VDFloorToInt((mHighlightedMarker*mMarkerRate + mMarkerStart - mWindowPosition) * mPixelsPerSample);
		sint32 xh2 = VDFloorToInt(((mHighlightedMarker+1)*mMarkerRate + mMarkerStart - mWindowPosition) * mPixelsPerSample);

		if (xh1 == xh2)
			++xh2;

		if (mbSpectrumMode)
			StretchDIBits(hdc, xh1, 0, xh2 - xh1, mChanHeight * mChanCount, xh1, 0, xh2 - xh1, mbihSpectrumHighlight.hdr.biHeight, &mImage[0], (const BITMAPINFO *)&mbihSpectrumHighlight, DIB_RGB_COLORS, SRCCOPY);
		else
			FastFill(hdc, xh1, ps.rcPaint.top, xh2, mChanHeight * mChanCount, RGB(0, 32, 64));
	}

	if (mbSpectrumMode) {
		double range = mSamplingRate / 8192.0 * 256.0;
		double division = range / (double)mChanHeight * (double)mFontHeight * 1.5f;
		double divexp = floor(log10(division));
		double divbase = pow(10.0, divexp);

		division /= divbase;

		if (division > 5.0)
			division = 10.0;
		else if (division > 2.0)
			division = 5.0;
		else if (division > 1.0)
			division = 2.0;

		division *= divbase;

		double divunits = division / (44100.0 / 8192.0 * 256.0 / 1.0);
		double divlimit = 1.0 - (double)mFontHeight * 2.0 / (double)mChanHeight;

		int ybase = 0;
		for(int i=0; i<mChanCount; ++i) {
			char buf[64];
			SetTextAlign(hdc, TA_BOTTOM | TA_LEFT);
			for(double d=0; d<divlimit; d += divunits) {
				sprintf(buf, "%.0f Hz", (44100.0 / 8192.0 * 256.0 / 1.0) * d);
				TextOut(hdc, 4, ybase + VDRoundToInt(mChanHeight*(1.0 - d/1.0)), buf, strlen(buf));
			}

			SetTextAlign(hdc, TA_TOP | TA_LEFT);
			sprintf(buf, "%.0f Hz", range);
			TextOut(hdc, 4, ybase + 4, buf, strlen(buf));
			ybase += mChanHeight;

			FastFill(hdc, 0, ybase-1, mChanWidth, ybase, RGB(0,128,96));
		}
	} else {
		if (HPEN hpen = CreatePen(PS_SOLID, 0, RGB(255, 0, 0))) {
			if (HGDIOBJ hOldPen = SelectObject(hdc, hpen)) {
				float yscale = (float)(mChanHeight - 1) / 256.0f;

				if (mbSolidWaveform) {
					if (mbPointsDirty) {
						mbPointsDirty = false;

						size_t count = mImage.size() / mChanCount;

						count &= ~(mSamplesPerPixel - 1);

						mPoints.resize((count / mSamplesPerPixel) * 2 * mChanCount);

						if (count) {
							uint32 j = 0;
							for(uint32 ch=0; ch<mChanCount; ++ch) {
								uint32 x = 0;
								const uint8 *src = &mImage[ch];
								int yoffset = mChanHeight * ch;

								uint32 basePtIdx = j;
								for(uint32 i=0; i<count; i += mSamplesPerPixel) {
									uint8 minval = 0xff;
									uint8 maxval = 0x00;
									for(uint32 k=0; k<mSamplesPerPixel; ++k) {
										uint8 v = *src;
										src += mChanCount;

										if (minval > v)
											minval = v;
										if (maxval < v)
											maxval = v;
									}

									mPoints[j+0].x = x;
									mPoints[j+0].y = VDRoundToIntFast((float)minval * yscale) + yoffset;
									mPoints[j+1].x = x++;
									mPoints[j+1].y = VDRoundToIntFast((float)maxval * yscale) + yoffset + 1;
									j += 2;
								}

								// Do another run through the points, and extend ranges to touch whenever needed.
								for(uint32 k = basePtIdx; k + 2 < j; k += 2) {
									POINT& pt0 = mPoints[k+0];
									POINT& pt1 = mPoints[k+1];
									POINT& pt2 = mPoints[k+2];
									POINT& pt3 = mPoints[k+3];

									// Check for the two disjoint cases and connect the spans at the midpoint
									// if so.
									if (pt1.y < pt2.y)
										pt1.y = pt2.y = (pt1.y + pt2.y) >> 1;
									else if (pt0.y > pt3.y)
										pt0.y = pt3.y = (pt0.y + pt3.y) >> 1;
								}
							}
						}
					}

					int w = (int)(mPoints.size() >> 1) / mChanCount;
					if (x2 > w)
						x2 = w;

					DWORD twos[128];
					for(int i=0; i<128; ++i)
						twos[i] = 2;

					for(int ch=0; ch<mChanCount; ++ch) {
						for(int x=x1; x<x2; x += 128)
							PolyPolyline(hdc, &mPoints[x*2 + ch*w*2], twos, std::min<int>(x2-x, 128));
					}
				} else {
					if (mbPointsDirty) {
						mbPointsDirty = false;

						size_t count = mImage.size() / mChanCount;

						mPoints.resize(count);

						if (count > 1) {
							for(uint32 ch=0; ch<mChanCount; ++ch) {
								const uint8 *src = &mImage[ch];
								int yoffset = mChanHeight * ch;

								for(uint32 i=0; i<count; ++i) {
									mPoints[i].x = VDRoundToIntFast((float)(i * mPixelsPerSample));
									mPoints[i].y = VDRoundToIntFast((float)src[i * mChanCount] * yscale + yoffset);
								}

								Polyline(hdc, mPoints.data(), count - 1);
							}
						}
					}
				}

				SelectObject(hdc, hOldPen);
			}
			DeleteObject(hpen);
		}
	}
}

bool VDAudioDisplayControl::ProcessAudio(const void *src, int count, int chanStride, int sampleStride, bool bit16) {
	if (mReadPosition < 0 || mChanCount < 1)
		return false;

	if (count <= 0)
		return mReadPosition >= 0;

	mReadPosition += count;

	if (mbSpectrumMode) {
		if (mAccumulatedSamples >= mChanWidth) {
			mReadPosition = -1;
			return false;
		}

		unsigned pitch = (mChanWidth + 3) & ~3;
		int x1 = mAccumulatedSamples;

		while(count > 0) {
			int tc = 8192 - (int)mBufferedWindowSamples;
			VDASSERT(tc >= 0);

			if (!tc) {
				mBufferedWindowSamples -= mSamplesPerPixel;

				static const float kSpecScale = 24.525815695112377925118719577032f;		// 255 / ln(32768)
				static const float kSpecOffset = 289.0f + 17.0f;
				static const float kLn2 = 0.69314718055994530941723212145818f;

				float offset = kSpecOffset + (float)mSpectralBoost * (kSpecScale * kLn2);

				unsigned char *dst = &mImage[mAccumulatedSamples];

				for(int ch=0; ch<mChanCount; ++ch) {
					mTransforms[ch].Transform();
					mTransforms[ch].Advance(mSamplesPerPixel);
					for(unsigned i=0; i<256; ++i) {
						double x = mTransforms[ch].GetPower(i);

						int y = 0;
						if (x > 1e-10) {
							double yf = log(x);
							y = VDRoundToIntFast((float)(yf*kSpecScale + offset));
						}

						y += abs(y);

						if ((unsigned)y >= 256)
							y = 255;

						*dst = (uint8)y;
						dst += pitch;
					}
				}

				if (++mAccumulatedSamples >= mChanWidth)
					break;

				continue;
			}

			if (tc > count)
				tc = count;
			count -= tc;

			VDASSERT(tc > 0);

			if (bit16) {
				for(int ch=0; ch<mChanCount; ++ch)
					mTransforms[ch].CopyIn16S((const signed short *)((const char *)src + chanStride*ch), tc, sampleStride);
				src = (const signed short *)((const unsigned char *)src + sampleStride*tc);
			} else {
				for(int ch=0; ch<mChanCount; ++ch)
					mTransforms[ch].CopyIn8U((const unsigned char *)src + chanStride*ch, tc, sampleStride);
				src = (const unsigned char *)src + sampleStride*tc;
			}

			mBufferedWindowSamples += tc;
		}

		if (mAccumulatedSamples > x1)
			InvalidateRange(x1, mAccumulatedSamples);
	} else {
		if (mImage.size() >= mChanWidth*mSamplesPerPixel*mChanCount) {
			mReadPosition = -1;
			return false;
		}

		uint32 rawcount = (uint32)mImage.size();
		int x1 = (int)(rawcount / (mSamplesPerPixel * mChanCount));
		
		VDASSERT(rawcount == mAccumulatedSamples * mChanCount);
		mAccumulatedSamples += count;

		mImage.resize(rawcount + count * mChanCount, 0);

		uint8 *dst = &mImage[rawcount];

		if (bit16) {
			for(int ch = 0; ch < mChanCount; ++ch) {
				const sint16 *src16 = (const sint16 *)((const char *)src + chanStride*ch);
				uint8 *dst8 = dst + ch;

				for(int i=0; i<count; ++i) {
					*dst8 = (uint8)((*src16 + 0x8000) >> 8);
					dst8 += mChanCount;
					src16 = (const sint16 *)((const char *)src16 + sampleStride);
				}
			}
		} else {
			for(int ch = 0; ch < mChanCount; ++ch) {
				const uint8 *src8 = (const uint8 *)src + chanStride*ch;
				uint8 *dst8 = dst + ch;

				for(int i=0; i<count; ++i) {
					*dst8 = *src8;
					dst8 += mChanCount;
					src8 += sampleStride;
				}
			}
		}

		VDASSERT(mImage.size() == mAccumulatedSamples * mChanCount);

		int x2 = (int)(mImage.size() / (mSamplesPerPixel * mChanCount));

		InvalidateRange(x1, x2);
		mbPointsDirty = true;
	}

	return true;
}

void VDAudioDisplayControl::FastFill(HDC hdc, int x1, int y1, int x2, int y2, DWORD c) {
	RECT r = {x1,y1,x2,y2};

	SetBkColor(hdc, c);
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r, (LPCSTR)&r, 0, NULL);
}

namespace {
	uint32 VDCeilToNiceDivision(uint32 v) {
		static const uint32 kScaleValues[]={
			1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000,
			10000, 20000, 50000, 100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000, 20000000, 50000000,
			100000000, 200000000, 500000000, 1000000000, 2000000000
		};

		for(uint32 i=0; i<sizeof(kScaleValues)/sizeof(kScaleValues[0]); ++i) {
			if (kScaleValues[i] >= v)
				return kScaleValues[i];
		}

		return v;
	}
}

void VDAudioDisplayControl::RecomputeMarkerSteps() {
	mMarkerMinorStep = VDCeilToNiceDivision((uint32)VDCeilToInt(mSamplesPerPixel * mFontDigitWidth * mMarkerInvRate));
	mMarkerMajorStep = (uint32)VDCeilToInt(mSamplesPerPixel * mFontDigitWidth * 8 * mMarkerInvRate);

	mMarkerMajorStep = mMarkerMinorStep * VDCeilToNiceDivision((uint32)VDCeilToInt((double)mMarkerMajorStep / (double)mMarkerMinorStep));

	mMarkerMinorRate = mMarkerRate * mMarkerMinorStep;
	mMarkerMinorInvRate = 1.0 / mMarkerMinorRate;
}

void VDAudioDisplayControl::InvalidateRange(int x1, int x2) {
	if (mUpdateX1 > x1)
		mUpdateX1 = x1;
	if (mUpdateX2 < x2)
		mUpdateX2 = x2;

	if (!mUpdateTimer)
		mUpdateTimer = SetTimer(mhwnd, 1, 100, NULL);
}

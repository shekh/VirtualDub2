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

#include <list>
#include <vector>

#include <windows.h>

#include <vd2/system/text.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdstring.h>

#include "oshelper.h"
#include "FilterGraph.h"

extern HINSTANCE g_hInst;
extern const char g_szError[];

extern const char g_szFilterGraphControlName[]="phaeronFilterGraphControl";

#define vdforeach(type, cont) if(0);else for(type::iterator it((cont).begin()), itEnd((cont).end()); it!=itEnd; ++it)

namespace {
	class GDIAutoselect {
	public:
		GDIAutoselect(HDC hdc, HGDIOBJ obj) : mhdc(hdc), mOldObj(obj?SelectObject(hdc, obj):NULL) {}
		~GDIAutoselect() { if (mOldObj) SelectObject(mhdc, mOldObj); }

		const HDC mhdc;
		const HGDIOBJ mOldObj;
	};

	class GDIAutoPtr {
	public:
		GDIAutoPtr(HGDIOBJ obj) : mObject(obj) {}
		~GDIAutoPtr() { if (mObject) DeleteObject(mObject); }

		operator HGDIOBJ() const { return mObject; }

		const HGDIOBJ mObject;
	};
}


class VDFilterGraphControl : public IVDFilterGraphControl {
public:
	VDFilterGraphControl(HWND hwnd);
	~VDFilterGraphControl() throw();

	static VDFilterGraphControl *Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id);

	HWND GetHwnd() const { return mhwnd; }

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	int OnScroll(int type, int action);
	bool OnKey(int k);
	void OnPaint();
	void OnMouseMove(int x, int y);
	void OnLButtonDown(int x, int y);
	void OnLButtonUp(int x, int y);
	void OnLButtonDbl(int x, int y);

	struct Filter;

	struct PinConnection {
		Filter *pSrc;
		int srcpin;
		Filter *pDst;
		int dstpin;

		VDStringW mFormat;
	};

	struct Filter {
		int x, y, w, h;
		VDStringW name;
		int inputs, outputs;
		int rank;
		bool bProtected;
		bool bMarked;

		std::vector<PinConnection *>	inpins;
		std::vector<PinConnection *>	outpins;

		vdrefptr<IVDRefCount>	pInstance;
	};

	enum ObjectType {
		kNoHit		= 0,
		kFilter,
		kInputPin,
		kOutputPin
	};

protected:	// interface routines
	void SetCallback(IVDFilterGraphControlCallback *pCB) { mpCB = pCB; }
	void Arrange();
	void DeleteSelection();
	void ConfigureSelection();
	void EnableAutoArrange(bool aa) { mbAutoArrange = aa; }
	void EnableAutoConnect(bool ac) { mbAutoConnect = ac; }

	IVDRefCount *GetSelection() {
		return mpSelectedFilter ? &*mpSelectedFilter->pInstance : NULL;
	}

protected:	// internal functions
	void ArrangeSort(std::vector<Filter *>& v, Filter *pf);
	void Connect(Filter *pSrcFilter, int nSrcPin, Filter *pDstFilter, int nDstPin);
	void BreakConnection(PinConnection *pc);
	void RecomputeWorkspace();
	void Clear();
	void SelectFilter(Filter *);
	void SelectConnection(PinConnection *pc);
	void RedrawFilter(Filter *pFilter);
	void AddFilter(const wchar_t *name, int inpins, int outpins, bool bProtected, IVDRefCount *pInstance);
	Filter *AddFilter2(const wchar_t *name, int inpins, int outpins, bool bProtected, IVDRefCount *pInstance);
	void RenderFilter(HDC hdc, Filter& f, bool bSelected);
	void GetFilterPinRect(Filter& f, RECT& r, bool bOutputPin, int pin);
	void GetFilterPinCenter(Filter& f, int& x, int& y, bool bOutputPin, int pin);
	void RenderArrow(HDC hdc, int x1, int y1, int x2, int y2);
	ObjectType HitTestFilter(Filter *pFilter, int x, int y, int& pin);
	ObjectType HitTestAllFilters(int x, int y, Filter *&pFilter, int& pin);
	PinConnection *HitTestConnections(int x, int y);
	bool IsReachable(Filter *pFrom, Filter *pTo);
	void SerializeFilter(std::vector<VDFilterGraphNode>& filters, std::vector<VDFilterGraphConnection>& connections, Filter& f);
	void GetFilterGraph(std::vector<VDFilterGraphNode>& filters, std::vector<VDFilterGraphConnection>& connections);
	void SetFilterGraph(const std::vector<VDFilterGraphNode>& filters, const std::vector<VDFilterGraphConnection>& connections);
	void SetConnectionLabel(IVDRefCount *pInstance, int outpin, const wchar_t *pLabel);
	void RequeryFormats();

	typedef std::list<Filter> tFilterList;
	tFilterList	mFilters;

	typedef std::list<PinConnection> tConnectionList;
	tConnectionList mConnections;

	Filter *mpSelectedFilter;
	ObjectType	mSelectedObject;
	int		mSelectedPin;

	PinConnection	*mpSelectedConnection;

	HBRUSH	mhbrHighlight;

	int		mDragOffsetX, mDragOffsetY;
	bool	mbDragActive;
	Filter *mpDragDstFilter;
	int		mDragDstPin;

	int		mArrowX1, mArrowY1, mArrowX2, mArrowY2;
	bool	mbArrowHasTarget;
	bool	mbArrowHasValidTarget;
	bool	mbArrowActive;

	bool	mbAutoConnect;
	bool	mbAutoArrange;

	int		mWorkL, mWorkT, mWorkR, mWorkB;
	int		mWorkX, mWorkY;

	IVDFilterGraphControlCallback *mpCB;

	const int mPinW;
	const int mPinH;
	const int mPinSpacing;

	const HWND mhwnd;
};

/////////////////////////////////////////////////////////////////////////////

VDFilterGraphControl::VDFilterGraphControl(HWND hwnd)
	: mhwnd(hwnd)
	, mpSelectedFilter(NULL)
	, mpSelectedConnection(NULL)
	, mbDragActive(false)
	, mbArrowActive(false)
	, mbAutoConnect(true)
	, mbAutoArrange(true)
	, mWorkL(0)
	, mWorkT(0)
	, mWorkR(0)
	, mWorkB(0)
	, mWorkX(0)
	, mWorkY(0)
	, mpCB(NULL)
	, mPinW(6)
	, mPinH(10)
	, mPinSpacing(10)
{
}

VDFilterGraphControl::~VDFilterGraphControl() throw() {
}

VDFilterGraphControl *VDFilterGraphControl::Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id) {
	HWND hwnd = CreateWindowEx(WS_EX_TRANSPARENT, g_szFilterGraphControlName, "", WS_VISIBLE|WS_CHILD|WS_HSCROLL|WS_VSCROLL, x, y, cx, cy, hwndParent, (HMENU)id, g_hInst, NULL);

	if (hwnd)
		return (VDFilterGraphControl *)GetWindowLongPtr(hwnd, 0);

	return NULL;
}

ATOM RegisterFilterGraphControl() {
	WNDCLASS wc;

	wc.style		= CS_DBLCLKS;
	wc.lpfnWndProc	= VDFilterGraphControl::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDFilterGraphControl *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= g_szFilterGraphControlName;

	return RegisterClass(&wc);
}

IVDFilterGraphControl *VDGetIFilterGraphControl(HWND hwnd) {
	return static_cast<IVDFilterGraphControl *>(reinterpret_cast<VDFilterGraphControl *>(GetWindowLongPtr(hwnd, 0)));
}

/////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDFilterGraphControl::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDFilterGraphControl *pThis = (VDFilterGraphControl *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		if (!(pThis = new_nothrow VDFilterGraphControl(hwnd)))
			return FALSE;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
		break;

	case WM_NCDESTROY:
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return pThis->WndProc(msg, wParam, lParam);
}

LRESULT VDFilterGraphControl::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_CREATE:
		mhbrHighlight = CreateSolidBrush(RGB(64,160,255));
		return 0;
	case WM_DESTROY:
		if (mhbrHighlight)
			DeleteObject(mhbrHighlight);
		return 0;
	case WM_SIZE:
		RecomputeWorkspace();
		return 0;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(mWorkX + (SHORT)LOWORD(lParam), mWorkY + (SHORT)HIWORD(lParam));
		return 0;
	case WM_LBUTTONDOWN:
		OnLButtonDown(mWorkX + (SHORT)LOWORD(lParam), mWorkY + (SHORT)HIWORD(lParam));
		SetFocus(mhwnd);
		break;
	case WM_LBUTTONUP:
		OnLButtonUp(mWorkX + (SHORT)LOWORD(lParam), mWorkY + (SHORT)HIWORD(lParam));
		break;
	case WM_LBUTTONDBLCLK:
		OnLButtonDbl(mWorkX + (SHORT)LOWORD(lParam), mWorkY + (SHORT)HIWORD(lParam));
		break;
	case WM_KEYUP:
		if (OnKey(wParam))
			return 0;
		break;
	case WM_GETDLGCODE:
		if (lParam) {
			const MSG *pmsg = (const MSG *)lParam;

			if (pmsg->message == WM_KEYUP)
				OnKey(pmsg->wParam);

			return DLGC_WANTMESSAGE;
		}
		return DLGC_WANTALLKEYS|DLGC_WANTARROWS|DLGC_WANTCHARS;

	case WM_HSCROLL:
		if (LOWORD(wParam) == SB_ENDSCROLL || LOWORD(wParam) == SB_THUMBPOSITION) {
			RecomputeWorkspace();
			return 0;
		} else {
			int pos = OnScroll(SB_HORZ, LOWORD(wParam));

			RECT r;
			GetClientRect(mhwnd, &r);
			ScrollWindow(mhwnd, mWorkX - pos, 0, NULL, &r);

			mWorkX = pos;
		}
		return 0;
	case WM_VSCROLL:
		if (LOWORD(wParam) == SB_ENDSCROLL || LOWORD(wParam) == SB_THUMBPOSITION) {
			RecomputeWorkspace();
			return 0;
		} else {
			int pos = OnScroll(SB_VERT, LOWORD(wParam));

			RECT r;
			GetClientRect(mhwnd, &r);
			ScrollWindow(mhwnd, 0, mWorkY - pos, NULL, &r);

			mWorkY = pos;
		}
		return 0;
	}
	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

int VDFilterGraphControl::OnScroll(int type, int action) {
	SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL };

	GetScrollInfo(mhwnd, type, &si);

	switch(action) {
	case SB_LEFT:
		--si.nPos;
		break;
	case SB_RIGHT:
		++si.nPos;
		break;
	case SB_LINELEFT:
		--si.nPos;
		break;
	case SB_LINERIGHT:
		++si.nPos;
		break;
	case SB_PAGELEFT:
		si.nPos -= si.nPage;
		break;
	case SB_PAGERIGHT:
		si.nPos += si.nPage;
		break;
	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;
	}

	if (si.nPos < si.nMin)
		si.nPos = si.nMin;
	if (si.nPos > si.nMax - (int)si.nPage + 1)
		si.nPos = si.nMax - (int)si.nPage + 1;

	si.fMask = SIF_POS;

	SetScrollInfo(mhwnd, type, &si, TRUE);

	return si.nPos;
}

bool VDFilterGraphControl::OnKey(int k) {
	if (k == VK_DELETE) {
		DeleteSelection();
		return true;
	}

	return false;
}

void VDFilterGraphControl::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwnd, &ps);

	OffsetViewportOrgEx(hdc, -mWorkX, -mWorkY, NULL);

	{
		GDIAutoselect as_font(hdc, GetStockObject(DEFAULT_GUI_FONT));

		COLORREF color_black	= RGB(0,0,0);
		COLORREF color_red		= RGB(128,0,0);
		COLORREF color_green	= RGB(0,128,0);
		COLORREF color_blue		= RGB(0,0,255);

		GDIAutoPtr pen_normal(CreatePen(PS_SOLID, 0, color_black));
		GDIAutoPtr brush_normal(CreateSolidBrush(color_black));
		GDIAutoPtr pen_sel(CreatePen(PS_SOLID, 0, color_green));
		GDIAutoPtr brush_sel(CreateSolidBrush(color_green));
		GDIAutoPtr pen_invalid(CreatePen(PS_SOLID, 0, color_red));
		GDIAutoPtr brush_invalid(CreateSolidBrush(color_red));
		GDIAutoPtr pen_hi(CreatePen(PS_SOLID, 0, color_blue));
		GDIAutoPtr brush_hi(CreateSolidBrush(color_blue));

		{
			for(tFilterList::reverse_iterator it(mFilters.rbegin()), itEnd(mFilters.rend()); it!=itEnd; ++it) {
				Filter& f = *it;

				RenderFilter(hdc, f, &f == mpSelectedFilter && mSelectedObject == kFilter);
			}
		}

		{
			GDIAutoselect aspen(hdc, pen_normal);
			GDIAutoselect asbrush(hdc, brush_normal);

			for(tConnectionList::iterator it(mConnections.begin()), itEnd(mConnections.end()); it!=itEnd; ++it) {
				PinConnection& conn = *it;

				Filter& fsrc  = *conn.pSrc;
				Filter& fdest = *conn.pDst;
				RECT rs, rt;

				GetFilterPinRect(fsrc,  rs, true, conn.srcpin);
				GetFilterPinRect(fdest, rt, false, conn.dstpin);

				int x1 = (rs.left+rs.right)/2;
				int y1 = (rs.top+rs.bottom)/2;
				int x2 = (rt.left+rt.right)/2;
				int y2 = (rt.top+rt.bottom)/2;

				if (mpSelectedConnection == &conn) {

					GDIAutoselect aspenhi(hdc, pen_hi);
					GDIAutoselect asbrushhi(hdc, brush_hi);

					RenderArrow(hdc, x1, y1, x2, y2);
				} else
					RenderArrow(hdc, x1, y1, x2, y2);

				if (!conn.mFormat.empty()) {
					SetBkMode(hdc, TRANSPARENT);
					SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
					SetTextAlign(hdc, TA_LEFT | TA_BASELINE);

					VDStringA label(VDTextWToA(conn.mFormat));

					TextOut(hdc, rs.right, y1, label.data(), label.size());
				}
			}
		}

		if (mbArrowActive) {
			GDIAutoselect aspen(hdc, mbArrowHasTarget ? mbArrowHasValidTarget ? pen_sel : pen_invalid : pen_normal);
			GDIAutoselect asbrush(hdc, mbArrowHasTarget ? mbArrowHasValidTarget ? brush_sel : brush_invalid : brush_normal);

			RenderArrow(hdc, mArrowX1, mArrowY1, mArrowX2, mArrowY2);
		}
	}

	EndPaint(mhwnd, &ps);
}

void VDFilterGraphControl::OnMouseMove(int x, int y) {
	if (mbDragActive) {
		if (mSelectedObject == kFilter) {
			RedrawFilter(mpSelectedFilter);
			mpSelectedFilter->x = x - mDragOffsetX;
			mpSelectedFilter->y = y - mDragOffsetY;
			RedrawFilter(mpSelectedFilter);
			RecomputeWorkspace();
		} else if (mbArrowActive) {
			// look for a target
			Filter *pFilter;
			int pin;
			ObjectType r = HitTestAllFilters(x, y, pFilter, pin);

			mbArrowHasTarget = false;
			mbArrowHasValidTarget = false;

			if ((r == kInputPin && mSelectedObject == kOutputPin)
				|| (r == kOutputPin && mSelectedObject == kInputPin)) {

				mbArrowHasTarget = true;
				mbArrowHasValidTarget = true;
				mpDragDstFilter = pFilter;
				mDragDstPin = pin;
			}

			// modify rendering

			RECT r1 = { mArrowX1, mArrowY1, mArrowX2+1, mArrowY2+1 };
			if (r1.left > r1.right) std::swap(r1.left, r1.right);
			if (r1.top > r1.bottom) std::swap(r1.top, r1.bottom);
			mArrowX2 = x;
			mArrowY2 = y;
			RECT r2 = { mArrowX1, mArrowY1, mArrowX2+1, mArrowY2+1 };
			if (r2.left > r2.right) std::swap(r2.left, r2.right);
			if (r2.top > r2.bottom) std::swap(r2.top, r2.bottom);

			InflateRect(&r1, 11, 11);
			InflateRect(&r2, 11, 11);

			OffsetRect(&r1, -mWorkX, -mWorkY);
			OffsetRect(&r2, -mWorkX, -mWorkY);

			InvalidateRect(mhwnd, &r1, TRUE);
			InvalidateRect(mhwnd, &r2, TRUE);
		}
	}
}

void VDFilterGraphControl::OnLButtonDown(int x, int y) {
	Filter *pFilter;
	int pin;

	if (ObjectType r = HitTestAllFilters(x, y, pFilter, pin)) {
		SetCapture(mhwnd);
		SelectFilter(pFilter);

		mSelectedObject = r;

		if (r == kFilter) {
			mDragOffsetX = x - pFilter->x;
			mDragOffsetY = y - pFilter->y;
		} else {
			mArrowX1 = x;
			mArrowY1 = y;
			mArrowX2 = x;
			mArrowY2 = y;
			mSelectedPin = pin;
			mbArrowHasTarget = false;
			mbArrowHasValidTarget = false;

			// check if the pin already has a connection

			mbArrowActive = true;
			if ((r == kInputPin && pFilter->inpins[pin])
				|| (r == kOutputPin && pFilter->outpins[pin])) {
				mbArrowActive = false;
			}
		}

		// redraw the filter, since hitting a pin will deselect the filter itself
		RedrawFilter(mpSelectedFilter);

		mbDragActive = true;
		return;
	}

	if (PinConnection *pc = HitTestConnections(x, y)) {
		SelectConnection(pc);
		return;
	}

	SelectFilter(NULL);
	SelectConnection(NULL);
}

void VDFilterGraphControl::OnLButtonUp(int x, int y) {
	if (mbDragActive) {
		OnMouseMove(x, y);
		ReleaseCapture();

		// Establish pin connection?

		if (mbArrowActive && mbArrowHasTarget && mbArrowHasValidTarget) {
			Filter *pSrcFilter = mpSelectedFilter;
			Filter *pDstFilter = mpDragDstFilter;
			int srcPin = mSelectedPin;
			int dstPin = mDragDstPin;

			if (mSelectedObject == kInputPin) {
				std::swap(pSrcFilter, pDstFilter);
				std::swap(srcPin, dstPin);
			}

			if (IsReachable(pDstFilter, pSrcFilter)) {
				MessageBox(mhwnd, "This connection would create a cycle in the filter graph and cannot be created.", g_szError, MB_OK|MB_ICONERROR);
				InvalidateRect(mhwnd, NULL, TRUE);
			} else {
				Connect(pSrcFilter, srcPin, pDstFilter, dstPin);
				if (mbAutoArrange)
					Arrange();
				RequeryFormats();
			}
		}

		mbDragActive = false;
		mbArrowActive = false;
	}
}

void VDFilterGraphControl::OnLButtonDbl(int x, int y) {
	ConfigureSelection();
}

namespace {
	struct Column {
		int x;
		int y;
		
		Column() : x(0), y(0) {}
	};
}

void VDFilterGraphControl::ArrangeSort(std::vector<Filter *>& v, Filter *pf) {
	int i;

	pf->bMarked = true;

	for(i=0; i<pf->inputs; ++i)
		if (pf->inpins[i] && !pf->inpins[i]->pSrc->bMarked)
			ArrangeSort(v, pf->inpins[i]->pSrc);

	v.push_back(pf);

	for(i=0; i<pf->outputs; ++i)
		if (pf->outpins[i] && !pf->outpins[i]->pDst->bMarked)
			ArrangeSort(v, pf->outpins[i]->pDst);
}

void VDFilterGraphControl::Arrange() {
	// Sort filters by depth.

	vdforeach(tFilterList, mFilters) {
		(*it).bMarked = false;
	}

	std::vector<Filter *> sortedFilters;

	vdforeach(tFilterList, mFilters) {
		Filter& f = *it;

		if (f.inputs && !f.bMarked)		// we deliberately look for a filter that has inputs in the hope of better 'detwist' behavior
			ArrangeSort(sortedFilters, &f);
	}

	// catch any filters that haven't been swept yet
	vdforeach(tFilterList, mFilters) {
		Filter& f = *it;

		if (!f.bMarked)
			ArrangeSort(sortedFilters, &f);
	}

	// The 'rank' of a filter is the depth of the filter in the filter
	// graph and is computed as max(rank(inputs))+1.  Since the filters
	// are now sorted we can compute these in one pass by pushing ranks
	// forward like audio samples.

	int maxrank = 0;
	vdforeach(std::vector<Filter *>, sortedFilters) {
		Filter& f = **it;
		int rank = 0;

		for(int i=0; i<f.inputs; ++i) {
			if (f.inpins[i]) {
				int r2 = f.inpins[i]->pSrc->rank+1;
				if (r2 > rank)
					rank = r2;
			}
		}

		f.rank = rank;

		maxrank = std::max<int>(maxrank, rank);
	}

	std::vector<Column> cols(maxrank+1);

	vdforeach(std::vector<Filter *>, sortedFilters) {
		Filter& f = **it;
		Column& col = cols[f.rank];

		if (col.x < f.w)
			col.x = f.w;
	}

	int x = 0;

	vdforeach(std::vector<Column>, cols) {
		Column& col = *it;
		int x_next = x + col.x + 32;
		col.x = x;
		x = x_next;
	}

	vdforeach(std::vector<Filter *>, sortedFilters) {
		Filter& f = **it;
		Column& col = cols[f.rank];

		f.x = col.x;
		f.y = col.y;
		col.y += f.h + 32;
	}

	RecomputeWorkspace();
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDFilterGraphControl::Connect(Filter *pSrcFilter, int nSrcPin, Filter *pDstFilter, int nDstPin) {
	mConnections.push_back(PinConnection());
	PinConnection& conn = mConnections.back();

	conn.pSrc	= pSrcFilter;
	conn.srcpin	= nSrcPin;
	conn.pDst	= pDstFilter;
	conn.dstpin	= nDstPin;
	pSrcFilter->outpins[nSrcPin] = &conn;
	pDstFilter->inpins[nDstPin] = &conn;
}

void VDFilterGraphControl::DeleteSelection() {
	if (mpSelectedFilter && mSelectedObject == kFilter) {
		Filter *pf = mpSelectedFilter;
		int i;

		if (pf->bProtected) {
			MessageBox(mhwnd, "This filter cannot be removed from the filter graph.", g_szError, MB_OK|MB_ICONERROR);
			return;
		}

		SelectFilter(NULL);

		for(i=0; i<pf->inputs; ++i)
			BreakConnection(pf->inpins[i]);

		for(i=0; i<pf->outputs; ++i)
			BreakConnection(pf->outpins[i]);

		tFilterList::iterator it(mFilters.begin()), itEnd(mFilters.end());

		for(; it!=itEnd; ++it) {
			if (&*it == pf) {
				mFilters.erase(it);
				break;
			}
		}
		InvalidateRect(mhwnd, NULL, TRUE);
		return;
	}

	if (mpSelectedConnection) {
		PinConnection *ppc = mpSelectedConnection;

		SelectConnection(NULL);
		BreakConnection(ppc);
		InvalidateRect(mhwnd, NULL, TRUE);
		return;
	}
}

void VDFilterGraphControl::ConfigureSelection() {
	if (mpSelectedFilter) {
		if (!mpCB || !mpSelectedFilter->pInstance || !mpCB->Configure((VDGUIHandle)mhwnd, mpSelectedFilter->pInstance))
			MessageBox(mhwnd, "No options are available for the selected filter.", g_szError, MB_OK|MB_ICONINFORMATION);
		else
			RequeryFormats();
	}
}

void VDFilterGraphControl::BreakConnection(PinConnection *pc) {
	if (!pc)
		return;

	pc->pSrc->outpins[pc->srcpin] = NULL;
	pc->pDst->inpins[pc->dstpin] = NULL;

	for(tConnectionList::iterator it(mConnections.begin()), itEnd(mConnections.end()); it!=itEnd; ++it)
		if (&*it == pc) {
			mConnections.erase(it);
			break;
		}
}

void VDFilterGraphControl::RecomputeWorkspace() {
	int l=0, t=0, r=0, b=0;

	vdforeach(tFilterList, mFilters) {
		Filter& f = *it;

		if (l>=r || t>=b) {
			l = f.x;
			t = f.y;
			r = f.x+f.w;
			b = f.y+f.h;
		} else {
			l = std::min<int>(l, f.x);
			t = std::min<int>(t, f.y);
			r = std::max<int>(r, f.x+f.w);
			b = std::max<int>(b, f.y+f.h);
		}
	}

	RECT rView={0};
	GetClientRect(mhwnd, &rView);

	l = std::min<int>(l, mWorkX);
	t = std::min<int>(t, mWorkY);
	r = std::max<int>(r, mWorkX + rView.right);
	b = std::max<int>(b, mWorkY + rView.bottom);

	mWorkL = l;
	mWorkT = t;
	mWorkR = r;
	mWorkB = b;

	SCROLLINFO siHoriz, siVert;

	siHoriz.cbSize		= sizeof(SCROLLINFO);
	siHoriz.fMask		= SIF_PAGE|SIF_POS|SIF_RANGE;
	siHoriz.nMin		= mWorkL;
	siHoriz.nMax		= mWorkR-1;
	siHoriz.nPage		= rView.right;
	siHoriz.nPos		= mWorkX;

	siVert.cbSize		= sizeof(SCROLLINFO);
	siVert.fMask		= SIF_PAGE|SIF_POS|SIF_RANGE;
	siVert.nMin			= mWorkT;
	siVert.nMax			= mWorkB-1;
	siVert.nPage		= rView.bottom;
	siVert.nPos			= mWorkY;

	SetScrollInfo(mhwnd, SB_HORZ, &siHoriz, TRUE);
	SetScrollInfo(mhwnd, SB_VERT, &siVert, TRUE);
}

void VDFilterGraphControl::Clear() {
	mpSelectedFilter = NULL;
	mpSelectedConnection = NULL;
	mFilters.clear();
	mConnections.clear();
	RecomputeWorkspace();
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDFilterGraphControl::SelectFilter(Filter *pFilter) {
	if (mpSelectedFilter != pFilter) {
		Filter *pOldSel = mpSelectedFilter;
		mpSelectedFilter = pFilter;
		mSelectedObject = kFilter;
		if (pFilter)
			SelectConnection(NULL);

		// move filter to front
		tFilterList::iterator it(mFilters.begin()), itEnd(mFilters.end());

		for(; it!=itEnd; ++it)
			if (&*it == pFilter) {
				mFilters.splice(mFilters.begin(), mFilters, it);
				break;
			}

		// redraw filters
		if (pOldSel)
			RedrawFilter(pOldSel);
		if (pFilter)
			RedrawFilter(pFilter);

		// notify
		if (mpCB)
			mpCB->SelectionChanged(pFilter ? &*pFilter->pInstance : NULL);
	}
}

void VDFilterGraphControl::SelectConnection(PinConnection *pc) {
	if (mpSelectedConnection != pc) {
		mpSelectedConnection = pc;
		if (pc)
			SelectFilter(NULL);

		InvalidateRect(mhwnd, NULL, TRUE);
	}
}

void VDFilterGraphControl::RedrawFilter(Filter *pFilter) {
//	RECT r = { pFilter->x, pFilter->y, pFilter->x + pFilter->w, pFilter->y + pFilter->h };
//	InvalidateRect(mhwnd, &r, TRUE);

	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDFilterGraphControl::AddFilter(const wchar_t *name, int inpins, int outpins, bool bProtected, IVDRefCount *pInstance) {
	Filter *pSrcFilter = mpSelectedFilter;
	Filter *pDstFilter = AddFilter2(name, inpins, outpins, bProtected, pInstance);

	if (mbAutoConnect && pSrcFilter && pDstFilter->inputs) {
		for(int nSrcPin = 0; nSrcPin < pSrcFilter->outputs; ++nSrcPin) {
			if (!pSrcFilter->outpins[nSrcPin]) {
				Connect(pSrcFilter, nSrcPin, pDstFilter, 0);
				break;
			}
		}
		RequeryFormats();
	}

	if (mbAutoArrange)
		Arrange();

	Filter *pSelect = pDstFilter;

	if (mbAutoConnect) {
		// Select a new filter.  We cycle around the graph perimeter until we
		// have either found a filter with an unbound output pin or we make
		// a full circle.

		int pin = 0;

		do {
			const int total_pins = pSelect->inputs + pSelect->outputs;
			if (!total_pins)
				break;		// hmm... a filter with no inputs or outputs.

			for(;;) {
				if (pin >= total_pins) {
					pin = 0;
					// Detect that we circled around on the starting filter.  This can
					// occur if you drop an output filter.
					if (pSelect == pDstFilter)
						break;
				}

				if (pin < pSelect->outputs) {
					PinConnection *conn = pSelect->outpins[pin];
					if (!conn)
						goto terminate_search;

					pSelect = conn->pDst;
					pin = pSelect->inputs + pSelect->outputs - conn->dstpin;
					break;
				} else {
					// inputs are reversed so we traverse clockwise the filter
					PinConnection *conn = pSelect->inpins[pSelect->inputs + pSelect->outputs - 1 - pin];

					if (conn) {
						pSelect = conn->pSrc;
						pin = conn->srcpin + 1;
						break;
					}
				}

				++pin;
			}
		} while(pSelect != pDstFilter);
terminate_search:
		;
	}

	SelectFilter(pSelect);

	if (mbAutoConnect)
		RequeryFormats();
}

VDFilterGraphControl::Filter *VDFilterGraphControl::AddFilter2(const wchar_t *name, int inpins, int outpins, bool bProtected, IVDRefCount *pInstance) {
	mFilters.push_front(Filter());
	Filter& f = mFilters.front();

	f.x		= 0;
	f.y		= 0;
	f.w		= 160;
	f.h		= 120;
	f.name			= name;
	f.inputs		= inpins;
	f.outputs		= outpins;
	f.bProtected	= bProtected;
	f.pInstance		= pInstance;

	f.inpins.resize(inpins, NULL);
	f.outpins.resize(outpins, NULL);

	if (HDC hdc = GetDC(mhwnd)) {
		if (HGDIOBJ hgo = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT))) {
			SIZE siz;

			if (GetTextExtentPoint32W(hdc, name, wcslen(name), &siz)) {
				f.w = siz.cx + 32;
				f.h = siz.cy + 32;
			}

			SelectObject(hdc, hgo);
		}
		ReleaseDC(mhwnd, hdc);
	}

	f.w += 2*mPinW;

	int minh = mPinSpacing + (mPinSpacing + mPinH) * std::max<int>(f.inputs, f.outputs);
	if (f.h < minh)
		f.h = minh;

	return &f;
}

void VDFilterGraphControl::RenderFilter(HDC hdc, Filter& f, bool bSelected) {
	RECT r = { f.x, f.y, f.x+f.w, f.y+f.h };
	RECT rt = r;
	int dx = GetSystemMetrics(SM_CXEDGE);
	int dy = GetSystemMetrics(SM_CYEDGE);
	HBRUSH hbrBackground = (bSelected ? mhbrHighlight : (HBRUSH)(COLOR_3DFACE+1));

	// draw filter body
	InflateRect(&rt, -mPinW, 0);
	DrawEdge(hdc, &rt, EDGE_RAISED, BF_ADJUST|BF_RECT);
	FillRect(hdc, &rt, hbrBackground);
	InflateRect(&rt, -dx, -dy);
	DrawEdge(hdc, &rt, EDGE_SUNKEN, BF_ADJUST|BF_RECT);
	SetBkMode(hdc, TRANSPARENT);
	DrawText(hdc, VDTextWToA(f.name).c_str(), -1, &rt, DT_VCENTER|DT_CENTER|DT_SINGLELINE);

	// draw filter tabs
	for(int i=0; i<f.inputs; ++i) {
		GetFilterPinRect(f, rt, false, i);
		DrawEdge(hdc, &rt, EDGE_RAISED, BF_ADJUST|BF_LEFT|BF_TOP|BF_BOTTOM);
		FillRect(hdc, &rt, hbrBackground);
	}

	for(int j=0; j<f.outputs; ++j) {
		GetFilterPinRect(f, rt, true, j);
		DrawEdge(hdc, &rt, EDGE_RAISED, BF_ADJUST|BF_RIGHT|BF_TOP|BF_BOTTOM);
		FillRect(hdc, &rt, hbrBackground);
	}
}

void VDFilterGraphControl::GetFilterPinRect(Filter& f, RECT& r, bool bOutput, int pin) {
	int dx = GetSystemMetrics(SM_CXEDGE);

	if (bOutput) {
		r.left		= f.x + f.w - (mPinW+dx);
		r.right		= f.x + f.w;
	} else {
		r.left		= f.x;
		r.right		= f.x + (mPinW+dx);
	}

	r.top = f.y + mPinSpacing * (1+pin) + mPinH * pin;
	r.bottom = r.top + mPinH;
}

void VDFilterGraphControl::GetFilterPinCenter(Filter& f, int& x, int& y, bool bOutputPin, int pin) {
	RECT r;

	GetFilterPinRect(f, r, bOutputPin, pin);

	x = (r.left + r.right)/2;
	y = (r.top + r.bottom)/2;
}

void VDFilterGraphControl::RenderArrow(HDC hdc, int x1, int y1, int x2, int y2) {
	const int dx = x1 - x2;
	const int dy = y1 - y2;

	if (!dx && !dy)
		return;

	MoveToEx(hdc, x1, y1, NULL);
	LineTo(hdc, x2, y2);

	const double inv_len = 10.0 / sqrt((double)(dx*dx + dy*dy));
	const double cos_f = 0.92387953251128675612818318939679 * inv_len;
	const double sin_f = 0.3826834323650897717284599840304 * inv_len;

	POINT pts[3] = {
		x2, y2,
		x2 + (int)(dx*cos_f - dy*sin_f), y2 + (int)(dy*cos_f + dx*sin_f),
		x2 + (int)(dx*cos_f + dy*sin_f), y2 + (int)(dy*cos_f - dx*sin_f),
	};

	Polygon(hdc, pts, 3);
}

VDFilterGraphControl::ObjectType VDFilterGraphControl::HitTestFilter(Filter *pFilter, int x, int y, int& pin) {
	const unsigned xoffset = x - pFilter->x;
	const unsigned yoffset = y - pFilter->y;

	if (xoffset < pFilter->w && yoffset < pFilter->h) {
		POINT pt = { x, y };
		if (xoffset < mPinW ) {
			for(pin = 0; pin < pFilter->inputs; ++pin) {
				RECT r;
				GetFilterPinRect(*pFilter, r, false, pin);
				if (PtInRect(&r, pt))
					return kInputPin;			
			}
		} else if (xoffset >= pFilter->w - mPinW) {
			for(pin = 0; pin < pFilter->outputs; ++pin) {
				RECT r;
				GetFilterPinRect(*pFilter, r, true, pin);
				if (PtInRect(&r, pt))
					return kOutputPin;
			}
		}
		return kFilter;
	}

	return kNoHit;
}

VDFilterGraphControl::ObjectType VDFilterGraphControl::HitTestAllFilters(int x, int y, Filter *&pFilter, int& pin) {
	for(tFilterList::iterator it(mFilters.begin()), itEnd(mFilters.end()); it!=itEnd; ++it) {
		Filter& f = *it;

		if (ObjectType r = HitTestFilter(&f, x, y, pin)) {
			pFilter = &f;
			return r;
		}
	}

	return kNoHit;
}

VDFilterGraphControl::PinConnection *VDFilterGraphControl::HitTestConnections(int x, int y) {
	for(tConnectionList::iterator it(mConnections.begin()), itEnd(mConnections.end()); it!=itEnd; ++it) {
		PinConnection& pc = *it;
		int x1, y1, x2, y2;

		GetFilterPinCenter(*pc.pSrc, x1, y1, true, pc.srcpin);
		GetFilterPinCenter(*pc.pDst, x2, y2, false, pc.dstpin);

		int w = abs(x1-x2);
		int h = abs(y1-y2);
		int x0 = std::min<int>(x1, x2);
		int y0 = std::min<int>(y1, y2);

		int xo = x - x0;
		int yo = y - y0;

		enum { kThreshold = 8 };

		if ((unsigned)(xo + kThreshold) < (w + 2*kThreshold) && (unsigned)(yo + kThreshold) < (h + 2*kThreshold)) {
			// Need to check the perpendicular distance of the point
			// from the line -- do this by rotating the world so the
			// line is the x-axis and checking the y-coordinate.
			//
			//        |x|
			// |h -w| | | / ||(w,h)|| <= threshold
			//        |y|
			//
			// A zero length pin connection will never pass the
			// bounding box test and so isn't a problem here.

			int distraw = (y2-y1)*(x-x1) - (x2-x1)*(y-y1);

			if (distraw*distraw <= (kThreshold * kThreshold) * (w*w+h*h))
				return &pc;
		}
	}

	return NULL;
}

bool VDFilterGraphControl::IsReachable(Filter *pFrom, Filter *pTo) {
	if (pFrom == pTo)
		return true;

	for(int i=0; i<pFrom->outputs; ++i) {
		if (PinConnection *pc = pFrom->outpins[i])
			if (IsReachable(pc->pDst, pTo))
				return true;
	}

	return false;
}

void VDFilterGraphControl::SerializeFilter(std::vector<VDFilterGraphNode>& filters, std::vector<VDFilterGraphConnection>& connections, Filter& f) {
	f.bMarked = true;

	for(int k=0; k<f.inputs; ++k) {
		if (PinConnection *pc = f.inpins[k]) {
			if (!pc->pSrc->bMarked)
				SerializeFilter(filters, connections, *pc->pSrc);
		}
	}

	VDFilterGraphNode fi;

	fi.name = f.name.c_str();
	fi.inputs = f.inputs;
	fi.outputs = f.outputs;
	fi.pInstance = f.pInstance;

	f.rank = filters.size();

	filters.push_back(fi);

	for(int i=0; i<f.inputs; ++i) {
		VDFilterGraphConnection conn = { -1, -1 };

		if (PinConnection *pc = f.inpins[i]) {
			conn.srcfilt = pc->pSrc->rank;
			conn.srcpin = pc->srcpin;
		}
		connections.push_back(conn);
	}

	for(int j=0; j<f.outputs; ++j) {
		if (PinConnection *pc = f.outpins[j]) {
			if (!pc->pDst->bMarked)
				SerializeFilter(filters, connections, *pc->pDst);
		}
	}
}

void VDFilterGraphControl::GetFilterGraph(std::vector<VDFilterGraphNode>& filters, std::vector<VDFilterGraphConnection>& connections) {
	vdforeach(tFilterList, mFilters) {
		Filter& f = *it;
		f.bMarked = false;
		f.rank = 0;
	}

	filters.clear();
	connections.clear();

	vdforeach(tFilterList, mFilters) {
		Filter& f = *it;

		if (f.bMarked)
			continue;

		bool connected = false;

		for(int i=0; i<f.inputs; ++i) {
			if (f.inpins[i]) {
				connected = true;
				break;
			}
		}

		if (!connected)
			SerializeFilter(filters, connections, f);
	}
}

void VDFilterGraphControl::SetFilterGraph(const std::vector<VDFilterGraphNode>& filters, const std::vector<VDFilterGraphConnection>& connections) {
	mpSelectedFilter = NULL;
	mpSelectedConnection = NULL;
	mFilters.clear();
	mConnections.clear();

	int nFilters = filters.size();
	int conn = 0;

	std::vector<Filter *> filtlist;

	for(int i=0; i<nFilters; ++i) {
		const VDFilterGraphNode& node = filters[i];
		Filter *pf = AddFilter2(node.name, node.inputs, node.outputs, false, node.pInstance);

		for(int j=0; j<node.inputs; ++j) {
			const VDFilterGraphConnection& c = connections[conn++];

			if (c.srcfilt >= 0 && c.srcfilt < filtlist.size()) {
				Filter *pfsrc = filtlist[c.srcfilt];

				if (c.srcpin >= 0 && c.srcpin < pfsrc->outputs && !pfsrc->outpins[c.srcpin]) {
					mConnections.push_back(PinConnection());
					PinConnection& pc = mConnections.back();

					pfsrc->outpins[c.srcpin] = &pc;
					pf->inpins[j] = &pc;

					pc.pSrc = pfsrc;
					pc.srcpin = c.srcpin;
					pc.pDst = pf;
					pc.dstpin = j;
				}
			}
		}

		filtlist.push_back(pf);
	}

	Arrange();
	RequeryFormats();
	RecomputeWorkspace();
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDFilterGraphControl::SetConnectionLabel(IVDRefCount *pInstance, int outpin, const wchar_t *pLabel) {
	vdforeach(tFilterList, mFilters) {
		Filter& f = *it;
		
		if (f.pInstance != pInstance)
			continue;

		if (outpin<0 || outpin >= f.outputs) {
			VDASSERT(false);
			return;
		}

		PinConnection *pConn = f.outpins[outpin];
		if (!pConn) {
			VDASSERT(false);
			return;
		}

		pConn->mFormat = pLabel;
		return;
	}
}

void VDFilterGraphControl::RequeryFormats() {
	bool bChange = false;

	vdforeach(tConnectionList, mConnections) {
		PinConnection& conn = *it;

		if (!conn.mFormat.empty())
			bChange = true;

		conn.mFormat.clear();
	}

	if (mpCB)
		bChange |= mpCB->RequeryFormats();

	if (bChange)
		InvalidateRect(mhwnd, NULL, TRUE);
}

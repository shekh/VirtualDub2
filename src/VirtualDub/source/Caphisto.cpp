//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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

#include "resource.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/error.h>
#include <vd2/system/thread.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Dita/w32control.h>

#include "caphisto.h"

///////////////////////////////////////////////////////////////////////////

namespace {
	struct AccumulateData {
		uint32	mCounters[256];
		uint16	mTables[3][256];
	};

	void AccumulateSpanRGB32(const uint8 *src, uint32 count, AccumulateData& ad) {
		do {
			++ad.mCounters[(ad.mTables[0][src[0]] + ad.mTables[1][src[1]] + ad.mTables[2][src[2]]) >> 8];
			src += 4;
		} while(--count);
	}

#ifndef _M_AMD64
	void __declspec(naked) AccumulateSpanRGB32_MMX(const uint8 *src, uint32 count, AccumulateData& ad) {
		static const __declspec(align(8)) __int64 mask		= 0x00ff00ff00ff00ff;
		static const __declspec(align(8)) __int64 rb_coeff	= 0x0041001900410019;
		static const __declspec(align(8)) __int64 g_coeff	= 0x0000008100000081;
		static const __declspec(align(8)) __int64 round		= 0x0000108000001080;

		__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx
			mov		eax, [esp+8+16]
			mov		ebx, [esp+4+16]
			mov		edx, [esp+12+16]
			movq	mm7, mask
			movq	mm6, g_coeff
			movq	mm5, rb_coeff
			movq	mm4, round
			shl		eax, 2
			add		ebx, eax
			neg		eax
xloop:
			movq	mm0, [eax+ebx]
			movq	mm1, mm7
			pand	mm1, mm0
			psrlw	mm0, 8
			pmaddwd	mm1, mm5
			pmaddwd	mm0, mm6

			movq	mm2, [eax+ebx+8]
			movq	mm3, mm7
			pand	mm3, mm2
			psrlw	mm2, 8
			pmaddwd	mm3, mm5
			pmaddwd	mm2, mm6

			paddd	mm0, mm1
			paddd	mm0, mm4
			psrad	mm0, 8
			movd	ecx, mm0
			psrlq	mm0, 32
			movd	esi, mm0

			paddd	mm2, mm3
			paddd	mm2, mm4
			psrad	mm2, 8
			movd	edi, mm2
			psrlq	mm2, 32
			movd	ebp, mm2

			add		dword ptr [edx+ecx*4],1
			add		dword ptr [edx+esi*4],1
			add		dword ptr [edx+edi*4],1
			add		dword ptr [edx+ebp*4],1
			add		eax, 16
			jnz		xloop
			emms
			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			ret
		}
	}
#endif

	void AccumulateSpanRGB24(const uint8 *src, uint32 count, AccumulateData& ad) {
		do {
			++ad.mCounters[(ad.mTables[0][src[0]] + ad.mTables[1][src[1]] + ad.mTables[2][src[2]]) >> 8];
			src += 3;
		} while(--count);
	}

	void AccumulateSpanRGB16(const uint8 *src, uint32 count, AccumulateData& ad) {
		do {
			++ad.mCounters[(ad.mTables[0][src[0]] + ad.mTables[1][src[1]]) >> 8];
			src += 2;
		} while(--count);
	}

	void AccumulateSpanY8_2(const uint8 *src, uint32 count, AccumulateData& ad) {
		do {
			++ad.mCounters[src[0]];
			src += 2;
		} while(--count);
	}

	void AccumulateSpanY8(const uint8 *src, uint32 count, AccumulateData& ad) {
		do {
			++ad.mCounters[src[0]];
			++src;
		} while(--count);
	}

	void InitTableRGB24(AccumulateData& ad) {
		for(int i=0; i<256; ++i) {
			ad.mTables[0][i] = (uint16)( 65*i + 0x1080);
			ad.mTables[1][i] = (uint16)(129*i);
			ad.mTables[2][i] = (uint16)( 25*i);
		}
	}

	void InitTableRGB16(AccumulateData& ad) {
		for(int i=0; i<256; ++i) {
			// Ideal coefficients are: 7.96044R + 15.628G + 3.03508B
			ad.mTables[0][i] = (uint16)(541*(i>>3) + (i&7)*(522<<3) + 0x1080);
			ad.mTables[1][i] = (uint16)((i>>5)*522 + 206*(i&31));
		}
	}

	void InitTableRGB15(AccumulateData& ad) {
		for(int i=0; i<256; ++i) {
			// Ideal coefficients are: 7.96044R + 15.628G + 3.03508B
			ad.mTables[0][i] = (uint16)(541*((i&0x7f)>>2) + (i&3)*(1062<<3) + 0x1080);
			ad.mTables[1][i] = (uint16)((i>>5)*1062 + 206*(i&31));
		}
	}
}

class VDCaptureVideoHistogram : public IVDCaptureVideoHistogram {
public:
	VDCaptureVideoHistogram();

	bool Process(const VDPixmap& px, float out[256], double scale);

protected:
	typedef void (*tpSpanRoutine)(const uint8 *, uint32, AccumulateData&);

	int				mLastFormat;
	int				mOffset;
	tpSpanRoutine	mpSpanRoutine;
	AccumulateData	mData;
};

IVDCaptureVideoHistogram *VDCreateCaptureVideoHistogram() { return new VDCaptureVideoHistogram; }

VDCaptureVideoHistogram::VDCaptureVideoHistogram()
	: mLastFormat(0)
{
}

bool VDCaptureVideoHistogram::Process(const VDPixmap& px, float out[256], double scale) {
	if (mLastFormat != px.format) {
		mOffset = 0;
		switch(px.format) {
		case nsVDPixmap::kPixFormat_XRGB1555:
			InitTableRGB15(mData);
			mpSpanRoutine = AccumulateSpanRGB16;
			break;
		case nsVDPixmap::kPixFormat_RGB565:
			InitTableRGB16(mData);
			mpSpanRoutine = AccumulateSpanRGB16;
			break;
		case nsVDPixmap::kPixFormat_RGB888:
			InitTableRGB24(mData);
			mpSpanRoutine = AccumulateSpanRGB24;
			break;
		case nsVDPixmap::kPixFormat_XRGB8888:
			InitTableRGB24(mData);
#ifndef _M_AMD64
			mpSpanRoutine = MMX_enabled ? AccumulateSpanRGB32_MMX : AccumulateSpanRGB32;
#else
			mpSpanRoutine = AccumulateSpanRGB32;
#endif
			break;
		case nsVDPixmap::kPixFormat_YUV422_UYVY:
			mOffset = 1;
			// No table setup required
			mpSpanRoutine = AccumulateSpanY8_2;
			break;
		case nsVDPixmap::kPixFormat_YUV422_YUYV:
			// No table setup required
			mpSpanRoutine = AccumulateSpanY8_2;
			break;
		case nsVDPixmap::kPixFormat_Y8:
		case nsVDPixmap::kPixFormat_YUV444_Planar:
		case nsVDPixmap::kPixFormat_YUV422_Planar:
		case nsVDPixmap::kPixFormat_YUV420_Planar:
		case nsVDPixmap::kPixFormat_YUV411_Planar:
		case nsVDPixmap::kPixFormat_YUV410_Planar:
			// No table setup required
			mpSpanRoutine = AccumulateSpanY8;
			break;
		default:
			return false;
		}
		mLastFormat = px.format;
	}

	memset(mData.mCounters, 0, sizeof mData.mCounters);
	
	const tpSpanRoutine pSpanRoutine = mpSpanRoutine;
	const uint8 *src = (const uint8 *)px.data + mOffset;
	const ptrdiff_t pitch = px.pitch;
	const vdpixsize w = px.w;
	const vdpixsize h = px.h;

	vdpixsize h2 = h;
	do {
		pSpanRoutine(src, w, mData);
		src += pitch;
	} while(--h2);

	// compute output array

	double bias = 1.0 - scale*log((double)w*h);
	for(int i=0; i<256; ++i) {
		const uint32 count = mData.mCounters[i];

		if (!count)
			out[i] = 0.f;
		else {
			double y = bias + scale*log((double)count);

			if (y < 0.0)
				y = 0.0;

			out[i] = (float)y;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	General capture histogram
//
///////////////////////////////////////////////////////////////////////////

class VDUICaptureVideoHistogramW32 : public VDUICustomControlW32, public IVDUICaptureVideoHistogram {
public:
	VDUICaptureVideoHistogramW32();
	~VDUICaptureVideoHistogramW32();

	void *AsInterface(uint32 id);

	bool Create(IVDUIParameters *pParams) { return VDUICustomControlW32::Create(pParams); }

	void SetHistogram(const float data[256]);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnPaint();
	void RecomputeHeights();

	HBRUSH	mhbrFill;
	HBRUSH	mhbrFillEnd;
	HBRUSH	mhbrErase;

	volatile bool	mbDataPending;
	VDCriticalSection	mBufferLock;

	int		mHeights[256];
	float	mBufferedData[256];
};

extern IVDUIWindow *VDCreateUICaptureVideoHistogram() { return new VDUICaptureVideoHistogramW32; }

VDUICaptureVideoHistogramW32::VDUICaptureVideoHistogramW32()
	: mhbrFill(CreateSolidBrush(RGB(0,128,192)))
	, mhbrFillEnd(CreateSolidBrush(RGB(255,0,0)))
	, mhbrErase(CreateSolidBrush(RGB(0,0,0)))
{
	memset(mHeights, 0, sizeof mHeights);
}

VDUICaptureVideoHistogramW32::~VDUICaptureVideoHistogramW32() {
	if (mhbrFill)
		DeleteObject(mhbrFill);
	if (mhbrFillEnd)
		DeleteObject(mhbrFillEnd);
	if (mhbrErase)
		DeleteObject(mhbrErase);
}

void *VDUICaptureVideoHistogramW32::AsInterface(uint32 id) {
	switch(id) {
		case IVDUICaptureVideoHistogram::kTypeID: return static_cast<IVDUICaptureVideoHistogram *>(this); break;
	}

	return VDUICustomControlW32::AsInterface(id);
}

void VDUICaptureVideoHistogramW32::SetHistogram(const float data[256]) {
	vdsynchronized(mBufferLock) {
		mbDataPending = true;
		memcpy(mBufferedData, data, sizeof mBufferedData);
	}

	InvalidateRect(mhwnd, NULL, TRUE);
}

LRESULT VDUICaptureVideoHistogramW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_PAINT:
		OnPaint();
		break;
	case WM_SIZE:
		RecomputeHeights();
		InvalidateRect(mhwnd, NULL, TRUE);
		break;
	case WM_ERASEBKGND:
		if(false){
			HDC hdc = (HDC)wParam;
			RECT r;
			GetClientRect(mhwnd, &r);

			r.top += (unsigned)(r.bottom - r.top) >> 1;

			FillRect(hdc, &r, (HBRUSH)GetClassLongPtr(mhwnd, GCLP_HBRBACKGROUND));
		}
		return TRUE;
	}
	return VDUICustomControlW32::WndProc(msg, wParam, lParam);
}

void VDUICaptureVideoHistogramW32::OnPaint() {
	PAINTSTRUCT ps;
	RECT r;
	GetClientRect(mhwnd, &r);

	if (mbDataPending)
		RecomputeHeights();

	if (HDC hdc = BeginPaint(mhwnd, &ps)) {
		POINT pt[268];

		// The valid range for Y is [16, 235], so we split the vertex buffer as follows:
		//
		//	0-15	Data points 0-15
		//	16		Extension of 15
		//	17, 18	Corners of polygon 1
		//	19		Extension of 0
		//	20-239	Data points 16-235
		//	240		Extension of 239
		//	241		TR corner of poly 2
		//	242		TL corner of poly 2
		//	243		Extension of 20
		//	244-263	Data points 236-255
		//	264		Extension of 264
		//	265		TR corner of poly 3
		//	266		TL corner of poly 3
		//	267		Extension of 244
		//
		// An index buffer would have been nice here....

		for(int i=0; i<256; ++i) {
			int j = i;

			if (j >= 16)
				j += 4;

			if (j >= 240)
				j += 4;

			pt[j].x = VDRoundToIntFast((i+0.5f) * r.right / 256.0f);
			pt[j].y = mHeights[i];
		}

		// Initialize polygon 1
		pt[16].x = VDRoundToIntFast(r.right * (16.0f / 256.0f));
		pt[16].y = (pt[15].y + pt[20].y) >> 1;
		pt[17].x = pt[16].x;
		pt[17].y = 0;
		pt[18].x = 0;
		pt[18].y = 0;
		pt[19].x = 0;
		pt[19].y = pt[0].y;

		// Initialize polygon 2
		pt[240].x = VDRoundToIntFast(r.right * (236.0f / 256.0f));
		pt[240].y = (pt[239].y + pt[244].y) >> 1;
		pt[241].x = pt[240].x;
		pt[241].y = 0;
		pt[242].x = pt[16].x;
		pt[242].y = 0;
		pt[243].x = pt[16].x;
		pt[243].y = pt[16].y;

		// Initialize polygon 3
		pt[264].x = r.right;
		pt[264].y = pt[263].y;
		pt[265].x = r.right;
		pt[265].y = 0;
		pt[266].x = pt[240].x;
		pt[266].y = 0;
		pt[267].x = pt[240].x;
		pt[267].y = pt[240].y;

		// draw top half
		HGDIOBJ hgoOldBrush = SelectObject(hdc, mhbrErase);

		Polygon(hdc, pt, 20);
		Polygon(hdc, pt+20, 224);
		Polygon(hdc, pt+244, 24);

		// draw bottom half
		//
		// We are flipping the winding direction here, but GDI doesn't care.

		pt[17].y = r.bottom;
		pt[18].y = r.bottom;
		pt[241].y = r.bottom;
		pt[242].y = r.bottom;
		pt[265].y = r.bottom;
		pt[266].y = r.bottom;

		SelectObject(hdc, mhbrFill);
		Polygon(hdc, pt+20, 224);
		SelectObject(hdc, mhbrFillEnd);
		Polygon(hdc, pt, 20);
		Polygon(hdc, pt+244, 24);

		if (hgoOldBrush)
			SelectObject(hdc, hgoOldBrush);

#if 0
		for(int i=0; i<256; ++i) {
			int x1 = VDRoundToIntFast(i*r.right/256.0f);
			int x2 = VDRoundToIntFast((i+1)*r.right/256.0f);
			int y = VDRoundToIntFast((1.0f - mBufferedData[i])*r.bottom);

			RECT r1 = { x1, 0, x2, y };
			RECT r2 = { x1, y, x2, r.bottom };

			FillRect(hdc, &r1, mhbrErase);
			if (i < 16 || i > 235) {
				FillRect(hdc, &r1, mhbrEraseEnd);
				FillRect(hdc, &r2, mhbrFillEnd);
			} else {
				FillRect(hdc, &r2, mhbrFill);
			}
		}
#endif

		EndPaint(mhwnd, &ps);
	}
}

void VDUICaptureVideoHistogramW32::RecomputeHeights() {
	RECT r;
	GetClientRect(mhwnd, &r);

	vdsynchronized(mBufferLock) {
		for(int i=0; i<256; ++i) {
			mHeights[i] = VDRoundToIntFast((1.0f - mBufferedData[i])*r.bottom);
		}

		mbDataPending = false;
	}
}

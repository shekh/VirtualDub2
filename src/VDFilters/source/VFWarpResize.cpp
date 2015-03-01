//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2006 Avery Lee
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
#include <algorithm>
#include <math.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>

#include "resource.h"

#ifdef _M_IX86
	#pragma warning(disable: 4799)		// warning C4799: function '`anonymous namespace'::CubicFilterH_MMX' has no EMMS instruction
	#pragma warning(disable: 4505)		// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed
#endif

namespace {
	void ScaleGradientPassForVis(uint32 *dst, const sint16 *src, uint32 count) {
		do {
			sint32 x = src[0] + 128;
			sint32 y = src[1] + 128;
			src += 2;

			if (x < 0)
				x = 0;
			else if (x > 255)
				x = 255;

			if (y < 0)
				y = 0;
			else if (y > 255)
				y = 255;

			*dst++ = (uint32)((x<<16) + (y<<8) + 0x000080);
		} while(--count);
	}
}

int g_lengthTable[8192];

class WarpResizeFilter : public VDXVideoFilter, public VDXVideoFilterDialog {
public:
	WarpResizeFilter();

	uint32 GetParams();
	void Start();
	void End();
	void Run();
	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	void ScriptConfigFunc(IVDXScriptInterpreter *, const VDXScriptValue *, int);

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void ComputeGradientMap();
	void ComputeBicubicImage();
	void ComputeFinalPass();

	bool	mbUseMMX;
	bool	mbShowGradientMap;
	sint32	mTargetWidth;
	sint32	mTargetHeight;

	int *mpTempRows;
	sint16 *mpGradientBuffer;
	uint32 *mpExpandedSrc;
	const uint32 **mpRowTable;

	int mCubicTable[514];
	uint32 mCubicTableMMX[257];
	int mSplineTable[514];
	uint32 mSplineTableMMX[257];
	int mNormalizeTable[8192];
};

///////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition g_VDVFWarpResize = VDXVideoFilterDefinition<WarpResizeFilter>(
	NULL,
	"warp resize",
	"Stretches an image with edge-directed sharpening using a variant of the warpsharp algorithm."
	);

WarpResizeFilter::WarpResizeFilter()
	: mbUseMMX(false)
	, mbShowGradientMap(false)
	, mTargetWidth(640)
	, mTargetHeight(480)
{
}

uint32 WarpResizeFilter::GetParams() {
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.w = mTargetWidth;
	if (pxldst.w < pxlsrc.w)
		pxldst.w = pxlsrc.w;

	pxldst.h = mTargetHeight;
	if (pxldst.h < pxlsrc.h)
		pxldst.h = pxlsrc.h;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void WarpResizeFilter::Start() {
	const sint32 srcwidth = fa->src.w;
	const sint32 dstwidth = fa->dst.w;
	const sint32 srcheight = fa->src.h;
	const sint32 dstheight = fa->dst.h;

	mpTempRows = new sint32[4*srcwidth+14 + 8*dstwidth+8];
	mpGradientBuffer = new sint16[dstwidth*dstheight*2];
	mpExpandedSrc = new uint32[(srcwidth+4)*srcheight];
	mpRowTable = new const uint32*[dstheight + 4];

	int i;
	for(i=0; i<257; ++i) {
		int j = i*2;
		const double A = -0.75;
		const double t = i * (1.0 / 256.0);
		mCubicTable[j+0] = (int)floor(0.5 + 16384.0*(A + (-2*A + A*t)*t)*t);
		mCubicTable[j+1] = (int)floor(0.5 + 16384.0*(((A+2)*t - (A+3))*t*t + 1));
		mCubicTableMMX[i] = (mCubicTable[j+0] & 0xffff) + (mCubicTable[j+1] << 16);
		mSplineTable[j+0] = (int)floor(0.5 + 16384.0*(1.0/6.0)*(((-t + 3)*t - 3)*t +1));
		mSplineTable[j+1] = (int)floor(0.5 + 16384.0*(1.0/6.0)*(((3*t - 6)*t)*t + 4));
		mSplineTableMMX[i] = (mSplineTable[j+0] & 0xffff) + (mSplineTable[j+1] << 16);
	}

	for(i=0; i<=4; ++i)
		mNormalizeTable[i] = 32768;

	for(i=5; i<8192; ++i)
		mNormalizeTable[i] = (int)(0.5 + 32768.0 * sqrt(4.0 / (double)i));

	for(i=0; i<8192; ++i)
		g_lengthTable[i] = (int)(0.5 + sqrt((double)(i*1024 + 512)));

	for(i=0; i<dstheight; ++i) {
		mpRowTable[i+2] = mpExpandedSrc + (srcwidth+4)*i;
	}

	mpRowTable[0] = mpRowTable[1] = mpRowTable[2];
	mpRowTable[2+srcheight] = mpRowTable[3+srcheight] = mpRowTable[1+srcheight];

	mbUseMMX = 0 != (ff->getCPUFlags() & CPUF_SUPPORTS_MMX);
}

void WarpResizeFilter::End() {
	delete[] mpRowTable;
	mpRowTable = NULL;

	delete[] mpExpandedSrc;
	mpExpandedSrc = NULL;

	delete[] mpGradientBuffer;
	mpGradientBuffer = NULL;

	delete[] mpTempRows;
	mpTempRows = NULL;
}

void WarpResizeFilter::Run() {
	ComputeGradientMap();

	if (mbShowGradientMap) {
		uint32 *dst = (uint32 *)fa->dst.data;
		const ptrdiff_t dstpitch = fa->dst.pitch;
		const sint32 dstwidth = fa->dst.w;
		const sint32 dstheight = fa->dst.h;

		for(sint32 y=0; y<dstheight; ++y) {
			ScaleGradientPassForVis(dst, mpGradientBuffer + (2*dstwidth)*y, dstwidth);

			dst = (uint32 *)((char *)dst + dstpitch);
		}
	} else {
		ComputeBicubicImage();
		ComputeFinalPass();
	}
}

bool WarpResizeFilter::Configure(VDXHWND hwnd) {
	bool saveGrad = mbShowGradientMap;
	int saveW = mTargetWidth;
	int saveH = mTargetHeight;

	bool success = 0 != Show(VDGetLocalModuleHandleW32(), MAKEINTRESOURCE(IDD_FILTER_WARPRESIZE), (HWND)hwnd);
	if (!success) {
		mbShowGradientMap = saveGrad;
		mTargetWidth = saveW;
		mTargetHeight = saveH;
	}

	return success;
}

INT_PTR WarpResizeFilter::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		SetDlgItemInt(mhdlg, IDC_WIDTH, mTargetWidth, FALSE);
		SetDlgItemInt(mhdlg, IDC_HEIGHT, mTargetHeight, FALSE);
		CheckDlgButton(mhdlg, IDC_SHOW_GRADIENT, mbShowGradientMap);
		SetFocus(GetDlgItem(mhdlg, IDC_WIDTH));
		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			{
				BOOL ok;
				mTargetWidth = GetDlgItemInt(mhdlg, IDC_WIDTH, &ok, FALSE);
				if (!ok) {
					SetFocus(GetDlgItem(mhdlg, IDC_WIDTH));
					MessageBeep(MB_ICONEXCLAMATION);
					return TRUE;
				}
				mTargetHeight = GetDlgItemInt(mhdlg, IDC_HEIGHT, &ok, FALSE);
				if (!ok) {
					SetFocus(GetDlgItem(mhdlg, IDC_HEIGHT));
					MessageBeep(MB_ICONEXCLAMATION);
					return TRUE;
				}
				mbShowGradientMap = !!IsDlgButtonChecked(mhdlg, IDC_SHOW_GRADIENT);
			}
			EndDialog(mhdlg, TRUE);
			return TRUE;
		case IDCANCEL:
			EndDialog(mhdlg, FALSE);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void WarpResizeFilter::GetSettingString(char *buf, int maxlen) {
	_snprintf(buf, maxlen, " (%dx%d%s)", mTargetWidth, mTargetHeight, mbShowGradientMap ? ", show gradients" : "");
	buf[maxlen - 1] = 0;
}

void WarpResizeFilter::GetScriptString(char *buf, int maxlen) {
	_snprintf(buf, maxlen, "Config(%u, %u, %d)", mTargetWidth, mTargetHeight, mbShowGradientMap);
	buf[maxlen - 1] = 0;
}

///////////////////////////////////////////////////////////////////////////

namespace {
	void ConvertToLuma(sint32 *dst, const uint32 *src, uint32 count) {
		const uint8 *src8 = (const uint8 *)src;

		do {
			*dst++ = (src8[0] * 19 + src8[1] * 183 + src8[2] * 54 + 128) >> 8;
			src8 += 4;
		} while(--count);
	}

	void ComputeBumpGradient(sint32 *dst, const sint32 *src0, const sint32 *src1, const sint32 *src2, uint32 count) {
		do {
			sint32 p0 = src0[0];
			sint32 p1 = src0[1];
			sint32 p2 = src0[2];
			sint32 p3 = src1[0];
			sint32 p5 = src1[2];
			sint32 p6 = src2[0];
			sint32 p7 = src2[1];
			sint32 p8 = src2[2];

			sint32 g0 = p8 - p0;					// [-510, 510]
			sint32 g1 = p2 - p6;					// [-510, 510]
			sint32 gx = g0 + g1 + 2*(p5 - p3);		// [-2040, 2040]
			sint32 gy = g0 - g1 + 2*(p7 - p1);		// [-2040, 2040]

			*dst++ = (gx & 0xffff) | (gy << 16);
			++src0;
			++src1;
			++src2;
		} while(--count);
	}

	void ComputeBumpFinal(sint32 *dst, uint32 count) {
		do {
			sint32 gx = (sint16)(dst[0] & 0xffff);
			sint32 gy = dst[0] >> 16;

			const union {
				float f;
				sint32 i;
			} converter = { sqrtf((float)(gx*gx + gy*gy)) + 12582912.0f };

			*dst++ = converter.i - 0x4B400000;

//			*dst++ = (sint32)sqrt(gx*gx + gy*gy);	// [0, 2040]
		} while(--count);
	}

	void ComputeGradient(sint16 *dst, const sint32 *src0, const sint32 *src1, const sint32 *src2, uint32 count) {
		do {
			sint32 p1 = src0[1];
			sint32 p3 = src1[0];
			sint32 p5 = src1[2];
			sint32 p7 = src2[1];

			sint32 gx = p5 - p3;		// [0, 4080]
			sint32 gy = p7 - p1;		// [0, 4080]

			gx *= 10;					// [0, 40800]
			gy *= 10;					// [0, 40800]

			gx = (gx+8) >> 4;			// [0, 2550]
			gy = (gy+8) >> 4;

			dst[0] = (sint16)-gx;
			dst[1] = (sint16)-gy;

			dst += 2;
			++src0;
			++src1;
			++src2;
		} while(--count);
	}

	class IRowFilter {
	public:
		virtual void Process(int y) = 0;
		virtual void *operator[](int offset) = 0;
	};

	template<class T, int N>
		class RowFilter : public IRowFilter {
	public:
		RowFilter(T *p, uint32 width, int limit)
			: mPos(-1)
			, mLimit(limit)
			, mPhase(0)
			, mWidth(width)
		{
			for(int i=0; i<N; ++i) {
				mpBuffers[i] = p + width*i;
				mpWindow[i] = mpWindow[i+N] = p;
			}
		}

		void Process(int y) {
			if (y < 0)
				y = 0;

			while(mPos < y) {
				++mPos;

				if (mPos >= mLimit)
					mpWindow[mPhase] = mpWindow[mPhase + N] = mpWindow[mPhase + (N-1)];
				else {
					T *dst = mpBuffers[mPhase];

					ProcessRow(dst, mPos, mWidth);

					mpWindow[mPhase] = mpWindow[mPhase + N] = dst;
				}

				if (++mPhase >= N)
					mPhase = 0;
			}
		}

		void *operator[](int offset) {
			return mpWindow[mPhase + offset + (N-1)];
		}

	protected:
		virtual void ProcessRow(T *dst, int pos, uint32 width) = 0;

	private:
		int mPos;
		int mLimit;
		int mPhase;
		uint32	mWidth;
		T *mpBuffers[N];
		T *mpWindow[N*2];
	};

	template<int N>
	class SrcFilter : public IRowFilter {
	public:
		SrcFilter(void *buf, ptrdiff_t pitch, int limit)
			: mPos(0)
			, mLimit(limit)
			, mPitch(pitch)
			, mpSrc(buf)
			, mPhase(0)
		{
			for(int i=0; i<N; ++i) {
				mpWindow[i] = mpWindow[i+N] = buf;
			}
		}

		void Process(int y) {
			while(mPos < y) {
				++mPos;

				if (mPos >= mLimit)
					mpWindow[mPhase] = mpWindow[mPhase + N] = mpWindow[mPhase + (N-1)];
				else {
					mpWindow[mPhase] = mpWindow[mPhase + N] = mpSrc;
					mpSrc = (char *)mpSrc + mPitch;
				}
			}
		}

		void *operator[](int offset) {
			return mpWindow[mPhase + offset + (N-1)];
		}

	private:
		int mPos;
		int mLimit;
		ptrdiff_t mPitch;
		void *mpSrc;
		int mPhase;
		void *mpWindow[N*2];
	};

	template<int N>
	class LumaFilter : public RowFilter<sint32, N> {
	public:
		LumaFilter(IRowFilter& upstream, sint32 *buf, uint32 width, int limit)
			: RowFilter<sint32, N>(buf, width, limit)
			, mUpstream(upstream)
		{
		}

		void ProcessRow(sint32 *dst, int pos, uint32 width) {
			mUpstream.Process(pos);

			ConvertToLuma(dst+1, (const uint32 *)mUpstream[0], width - 2);
			dst[0] = dst[1];
			dst[width-1] = dst[width-2];
		}

	protected:
		IRowFilter& mUpstream;
	};

	template<int N>
	class BumpFilter : public RowFilter<sint32, N> {
	public:
		BumpFilter(IRowFilter& upstream, sint32 *buf, uint32 width, int limit)
			: RowFilter<sint32, N>(buf, width, limit)
			, mUpstream(upstream)
		{
		}

		void ProcessRow(sint32 *dst, int pos, uint32 width) {
			mUpstream.Process(pos+1);

			ComputeBumpGradient(dst+4, (const sint32 *)mUpstream[-2], (const sint32 *)mUpstream[-1], (const sint32 *)mUpstream[0], width-8);

			dst[0] = dst[1] = dst[2] = dst[3] = dst[4];
			dst[width-4] = dst[width-3] = dst[width-2] = dst[width-1] = dst[width-5];
		}

	protected:
		IRowFilter& mUpstream;
	};

	void MonoSplineFilterH(sint32 *dst, const sint32 *src, uint32 count, uint32 accum, uint32 accinc, const int *table) {
		sint16 *dst16 = (sint16 *)dst;
		do {
			src += (accum >> 16);
			accum &= 0xffff;

			int phase = accum >> 8;

			accum += accinc;

			const sint32 c0 = table[0+phase*2];
			const sint32 c1 = table[1+phase*2];
			const sint32 c2 = table[513-phase*2];
			const sint32 c3 = table[512-phase*2];

			const sint16 *src16 = (const sint16 *)src;

			dst16[0] = (sint16)((c0*src16[0] + c1*src16[2] + c2*src16[4] + c3*src16[6] + 8192) >> 14);
			dst16[1] = (sint16)((c0*src16[1] + c1*src16[3] + c2*src16[5] + c3*src16[7] + 8192) >> 14);
			dst16 += 2;
		} while(--count);
	}

	#ifdef _M_IX86
		void __declspec(naked) __cdecl MonoSplineFilterH_MMX(sint32 *dst, const sint32 *src, uint32 count, uint32 accum, uint32 accinc, const uint32 *table) {
			static const __declspec(align(8)) __int64 round = 0x0000200000002000;
			__asm {
				push	ebp
				push	edi
				push	esi
				push	ebx

				mov			ebx, [esp+24+16]
				mov			ecx, [esp+8+16]
				shr			ecx, 2
				mov			esi, [esp+16+16]
				mov			edi, [esp+20+16]
				mov			eax, esi
				shr			eax, 16
				mov			ebp, edi
				shr			ebp, 16
				add			ecx, eax
				shl			esi, 16
				mov			[esp+16+16], ebp
				shl			edi, 16
				mov			ebp, [esp+12+16]
				shl			ebp, 2
				mov			edx, [esp+4+16]
				add			edx, ebp
				pxor		mm7, mm7
				neg			ebp

xloop:
				mov			eax, esi
				shr			eax, 24
				movd		mm4, [ebx+eax*4]
				neg			eax
				movd		mm5, [ebx+eax*4+1024]
				punpckldq	mm4, mm4
				movq		mm6, round
				punpckldq	mm5, mm5

				movd		mm0, dword ptr [ecx*4]
				punpcklwd	mm0, dword ptr [ecx*4+4]
				pmaddwd		mm0, mm4

				movd		mm2, dword ptr [ecx*4+12]
				punpcklwd	mm2, dword ptr [ecx*4+8]

				add			esi, edi
				adc			ecx, [esp+16+16]

				pmaddwd		mm2, mm5
				paddd		mm0, mm6
				paddd		mm0, mm2
				psrad		mm0, 14
				packssdw	mm0, mm0
				movd		[edx+ebp], mm0
				add			ebp, 4
				jnz			xloop

				emms
				pop		ebx
				pop		esi
				pop		edi
				pop		ebp
				ret
			}
		}
#endif

		void MonoSplineFilterV(sint32 *dst, const sint32 *src0, const sint32 *src1, const sint32 *src2, const sint32 *src3, uint32 count, int phase, const int *table) {
			const sint32 c0 = table[0+phase*2];
			const sint32 c1 = table[1+phase*2];
			const sint32 c2 = table[513-phase*2];
			const sint32 c3 = table[512-phase*2];
			const sint16 *src0_16 = (const sint16 *)src0;
			const sint16 *src1_16 = (const sint16 *)src1;
			const sint16 *src2_16 = (const sint16 *)src2;
			const sint16 *src3_16 = (const sint16 *)src3;

			do {
				sint32 x = (c0*src0_16[0] + c1*src1_16[0] + c2*src2_16[0] + c3*src3_16[0] + 8192) >> 14;
				sint32 y = (c0*src0_16[1] + c1*src1_16[1] + c2*src2_16[1] + c3*src3_16[1] + 8192) >> 14;

				const union {
					float f;
					sint32 i;
				} converter = { (float)sqrt((float)(x*x + y*y)) + 12582912.0f };

				*dst++ = converter.i - 0x4B400000;

				src0_16 += 2;
				src1_16 += 2;
				src2_16 += 2;
				src3_16 += 2;
			} while(--count);
		}

#ifdef _M_IX86
		void __declspec(naked) MonoSplineFilterV_MMX(sint32 *dst, const sint32 *const *srcp, uint32 count, int phase, const uint32 *table) {
			static const __declspec(align(8)) __int64 round = 0x0000200000002000;
			static const __declspec(align(8)) __int64 k8192 = 8192;
			static const __declspec(align(8)) __int64 k10 = 10;

			__asm {
				push	ebp
				push	edi
				push	esi
				push	ebx

				mov			eax, [esp+16+16]
				mov			ecx, [esp+20+16]
				movd		mm4, [ecx+eax*4]
				neg			eax
				movd		mm5, [ecx+eax*4+1024]
				punpckldq	mm4, mm4
				movq		mm6, round
				punpckldq	mm5, mm5
				movq		mm3, k8192
				movq		mm7, k10

				mov			edx, [esp+8+16]
				mov			eax, [edx]
				mov			ebx, [edx+4]
				mov			ecx, [edx+8]
				mov			edx, [edx+12]
				mov			edi, [esp+12+16]
				shl			edi, 2
				mov			esi, [esp+4+16]
				add			eax, edi
				add			ebx, edi
				add			ecx, edi
				add			edx, edi
				add			esi, edi
				neg			edi
	xloop:
				movd		mm0, dword ptr [eax+edi]
				punpcklwd	mm0, dword ptr [ebx+edi]
				pmaddwd		mm0, mm4

				movd		mm2, dword ptr [edx+edi]
				punpcklwd	mm2, dword ptr [ecx+edi]
				pmaddwd		mm2, mm5

				paddd		mm0, mm6
				paddd		mm0, mm2
				psrad		mm0, 14
				packssdw	mm0, mm0

				pmaddwd		mm0, mm0

				movq		mm1, mm0
				pcmpgtd		mm0, mm3
				pand		mm0, mm7
				psrld		mm1, mm0
				movd		ebp, mm1
				movd		mm1, dword ptr [g_lengthTable + ebp*4]
				pandn		mm0, mm7
				psrld		mm0, 1
				psrld		mm1, mm0
				movd		dword ptr [esi+edi], mm1
				add			edi, 4
				jnz			xloop

				emms
				pop		ebx
				pop		esi
				pop		edi
				pop		ebp
				ret
			}

			// workaround for stupid optimizer
			(const sint32 *const * volatile&)srcp;
		}
#endif

	template<int N>
	class BicubicSplineFilter1 : public RowFilter<sint32, N> {
	public:
		BicubicSplineFilter1(IRowFilter& upstream, sint32 *buf, uint32 dstwidth, uint32 srcwidth, int limit, const int *filter, const uint32 *filterMMX, bool useMMX)
			: RowFilter<sint32, N>(buf, dstwidth, limit)
			, mUpstream(upstream)
			, mpFilter(filter)
			, mpFilterMMX(filterMMX)
			, mbUseMMX(useMMX)
		{
			mInc = ((srcwidth << 17) / dstwidth + 1) >> 1;
			mAccum = (mInc >> 1) + 0x28000;
		}

		void ProcessRow(sint32 *dst, int pos, uint32 width) {
			mUpstream.Process(pos);

#ifdef _M_IX86
			if (mbUseMMX)
				MonoSplineFilterH_MMX(dst, (const sint32 *)mUpstream[0], width, mAccum, mInc, mpFilterMMX);
			else
#endif
				MonoSplineFilterH(dst, (const sint32 *)mUpstream[0], width, mAccum, mInc, mpFilter);
		}

	protected:
		IRowFilter& mUpstream;
		const int *mpFilter;
		const uint32 *mpFilterMMX;
		uint32	mAccum;
		uint32	mInc;
		bool	mbUseMMX;
	};

	template<int N>
	class BicubicSplineFilter2 : public RowFilter<sint32, N> {
	public:
		BicubicSplineFilter2(IRowFilter& upstream, sint32 *buf, uint32 dstwidth, uint32 dstheight, uint32 srcheight, int limit, const int *filter, const uint32 *filterMMX, bool useMMX)
			: RowFilter<sint32, N>(buf, dstwidth, limit)
			, mUpstream(upstream)
			, mpFilter(filter)
			, mpFilterMMX(filterMMX)
			, mbUseMMX(useMMX)
		{
			mInc = ((srcheight << 17) / dstheight + 1) >> 1;
			mAccum = (mInc >> 1) - 0x8000;
		}

		void ProcessRow(sint32 *dst, int pos, uint32 width) {
			sint32 acc = mAccum + mInc * pos;
			int y = acc >> 16;
			mUpstream.Process(y + 3);

#ifdef _M_IX86
			if (mbUseMMX) {
				const sint32 *streams[4]={
					(const sint32 *)mUpstream[-3],
					(const sint32 *)mUpstream[-2],
					(const sint32 *)mUpstream[-1],
					(const sint32 *)mUpstream[0]
				};

				MonoSplineFilterV_MMX(dst+1, streams, width-2, (acc & 0xff00) >> 8, mpFilterMMX);
			} else
#endif
			{
				MonoSplineFilterV(dst+1, (const sint32 *)mUpstream[-3], (const sint32 *)mUpstream[-2], (const sint32 *)mUpstream[-1], (const sint32 *)mUpstream[0], width-2, (acc & 0xff00) >> 8, mpFilter);
			}

//			ComputeBumpFinal(dst+1, width-2);

			dst[0] = dst[1];
			dst[width-1] = dst[width-2];
		}

	protected:
		IRowFilter& mUpstream;
		const int *mpFilter;
		const uint32 *mpFilterMMX;
		sint32	mAccum;
		sint32	mInc;
		bool	mbUseMMX;
	};

	template<int N>
	class GradientFilter : public RowFilter<sint32, N> {
	public:
		GradientFilter(IRowFilter& upstream, sint32 *buf, uint32 width, int limit)
			: RowFilter<sint32, N>(buf, width, limit)
			, mUpstream(upstream)
		{
		}

		void ProcessRow(sint32 *dst, int pos, uint32 width) {
			mUpstream.Process(pos+1);

			ComputeGradient((sint16 *)dst, (const sint32 *)mUpstream[-2], (const sint32 *)mUpstream[-1], (const sint32 *)mUpstream[0], width);
		}

	protected:
		IRowFilter& mUpstream;
	};
}

void WarpResizeFilter::ComputeGradientMap() {
	const sint32 srcwidth = fa->src.w;
	const sint32 srcheight = fa->src.h;
	const uint32 *src = (const uint32 *)fa->src.data;
	const ptrdiff_t srcpitch = fa->src.pitch;
	const sint32 dstwidth = fa->dst.w;
	const sint32 dstheight = fa->dst.h;

	SrcFilter<1> srcfilt((void *)src, srcpitch, srcheight);

	sint32 *buf = mpTempRows;

	LumaFilter<3> lumafilt(srcfilt, buf, srcwidth + 2, srcheight);
	buf += 3*(srcwidth + 2);

	BumpFilter<1> bumpfilt(lumafilt, buf, srcwidth + 8, srcheight);
	buf += 1*(srcwidth + 8);
	
	BicubicSplineFilter1<4> bispline1(bumpfilt, buf, dstwidth, srcwidth, srcheight, mSplineTable, mSplineTableMMX, mbUseMMX);
	buf += 4*dstwidth;
	
	BicubicSplineFilter2<3> bispline2(bispline1, buf, dstwidth + 2, dstheight, srcheight, dstheight, mSplineTable, mSplineTableMMX, mbUseMMX);
	buf += 3*(dstwidth+2);
	
	GradientFilter<1> gradfilt(bispline2, buf, dstwidth, dstheight);

	sint16 *graddst = mpGradientBuffer;
	for(sint32 y=0; y<dstheight; ++y) {
		gradfilt.Process(y);

		memcpy(graddst, gradfilt[0], 4*dstwidth);

		graddst += 2*dstwidth;
	}
}

namespace {
	void CubicFilterH(uint32 *dst, const uint32 *src, uint32 count, uint32 accum, uint32 accinc, const int *table) {
		do {
			src += (accum >> 16);
			accum &= 0xffff;

			int phase = accum >> 8;

			accum += accinc;

			const sint32 c0 = table[0+phase*2];
			const sint32 c1 = table[1+phase*2];
			const sint32 c2 = table[513-phase*2];
			const sint32 c3 = table[512-phase*2];

			const uint8 *src8 = (const uint8 *)src;
			int rawr = (c0*(sint32)src8[2] + c1*(sint32)src8[6] + c2*(sint32)src8[10] + c3*(sint32)src8[14] + 8192) >> 14;
			int rawg = (c0*(sint32)src8[1] + c1*(sint32)src8[5] + c2*(sint32)src8[ 9] + c3*(sint32)src8[13] + 8192) >> 14;
			int rawb = (c0*(sint32)src8[0] + c1*(sint32)src8[4] + c2*(sint32)src8[ 8] + c3*(sint32)src8[12] + 8192) >> 14;

			if ((unsigned)rawr >= 256)	rawr = ((~rawr)>>31) & 0xff;
			if ((unsigned)rawg >= 256)	rawg = ((~rawg)>>31) & 0xff;
			if ((unsigned)rawb >= 256)	rawb = ((~rawb)>>31) & 0xff;

			*dst++ = (rawr << 16) + (rawg << 8) + rawb;
		} while(--count);
	}

#ifdef _M_IX86
	void __declspec(naked) __cdecl CubicFilterH_MMX(uint32 *dst, const uint32 *src, uint32 count, uint32 accum, uint32 accinc, const uint32 *table) {
		static const __declspec(align(8)) __int64 round = 0x0000200000002000;
		__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx

			mov			ebx, [esp+24+16]
			mov			ecx, [esp+8+16]
			shr			ecx, 2
			mov			esi, [esp+16+16]
			mov			edi, [esp+20+16]
			mov			eax, esi
			shr			eax, 16
			mov			ebp, edi
			shr			ebp, 16
			add			ecx, eax
			shl			esi, 16
			mov			[esp+16+16], ebp
			shl			edi, 16
			mov			ebp, [esp+12+16]
			shl			ebp, 2
			mov			edx, [esp+4+16]
			add			edx, ebp
			pxor		mm7, mm7
			neg			ebp

xloop:
			mov			eax, esi
			shr			eax, 24
			movd		mm4, [ebx+eax*4]
			neg			eax
			movd		mm5, [ebx+eax*4+1024]
			punpckldq	mm4, mm4
			movq		mm6, round
			punpckldq	mm5, mm5

			movd		mm0, dword ptr [ecx*4]
			punpcklbw	mm0, dword ptr [ecx*4+4]
			movq		mm1, mm0
			punpckhbw	mm1, mm7
			punpcklbw	mm0, mm7
			pmaddwd		mm1, mm4
			pmaddwd		mm0, mm4

			movd		mm2, dword ptr [ecx*4+12]
			punpcklbw	mm2, dword ptr [ecx*4+8]

			add			esi, edi
			adc			ecx, [esp+16+16]

			movq		mm3, mm2
			punpckhbw	mm3, mm7
			punpcklbw	mm2, mm7
			pmaddwd		mm3, mm5
			pmaddwd		mm2, mm5
			paddd		mm0, mm6
			paddd		mm1, mm6
			paddd		mm0, mm2
			paddd		mm1, mm3
			psrad		mm0, 14
			psrad		mm1, 14
			packssdw	mm0, mm1
			packuswb	mm0, mm0
			movd		[edx+ebp], mm0
			add			ebp, 4
			jnz			xloop

			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			ret
		}
	}
#endif

	void CubicFilterV(uint32 *dst, const uint32 *src0, const uint32 *src1, const uint32 *src2, const uint32 *src3, uint32 count, int phase, const int *table) {
		const sint32 c0 = table[0+phase*2];
		const sint32 c1 = table[1+phase*2];
		const sint32 c2 = table[513-phase*2];
		const sint32 c3 = table[512-phase*2];

		const uint8 *src08 = (const uint8 *)src0;
		const uint8 *src18 = (const uint8 *)src1;
		const uint8 *src28 = (const uint8 *)src2;
		const uint8 *src38 = (const uint8 *)src3;

		do {
			int rawr = (c0*(sint32)src08[2] + c1*(sint32)src18[2] + c2*(sint32)src28[2] + c3*(sint32)src38[2] + 8192) >> 14;
			int rawg = (c0*(sint32)src08[1] + c1*(sint32)src18[1] + c2*(sint32)src28[1] + c3*(sint32)src38[1] + 8192) >> 14;
			int rawb = (c0*(sint32)src08[0] + c1*(sint32)src18[0] + c2*(sint32)src28[0] + c3*(sint32)src38[0] + 8192) >> 14;
			src08 += 4;
			src18 += 4;
			src28 += 4;
			src38 += 4;

			if ((unsigned)rawr >= 256)	rawr = ((~rawr)>>31) & 0xff;
			if ((unsigned)rawg >= 256)	rawg = ((~rawg)>>31) & 0xff;
			if ((unsigned)rawb >= 256)	rawb = ((~rawb)>>31) & 0xff;

			*dst++ = (rawr << 16) + (rawg << 8) + rawb;
		} while(--count);
	}

#ifdef _M_IX86
	void __declspec(naked) CubicFilterV_MMX(uint32 *dst, const uint32 *const *srcp, uint32 count, int phase, const uint32 *table) {
		static const __declspec(align(8)) __int64 round = 0x0000200000002000;

		__asm {
			push	ebp
			mov		ebp, esp
			push	edi
			push	esi
			push	ebx

			mov			eax, [ebp+20]
			mov			ecx, [ebp+24]
			movd		mm4, [ecx+eax*4]
			neg			eax
			movd		mm5, [ecx+eax*4+1024]
			punpckldq	mm4, mm4
			movq		mm6, round
			punpckldq	mm5, mm5
			pxor		mm7, mm7

			mov			edx, [ebp+12]
			mov			eax, [edx]
			mov			ebx, [edx+4]
			mov			ecx, [edx+8]
			mov			edx, [edx+12]
			mov			edi, [ebp+16]
			shl			edi, 2
			mov			esi, [ebp+8]
			add			eax, edi
			add			ebx, edi
			add			ecx, edi
			add			edx, edi
			add			esi, edi
			neg			edi
xloop:
			movd		mm0, dword ptr [eax+edi]
			punpcklbw	mm0, dword ptr [ebx+edi]
			movq		mm1, mm0
			punpckhbw	mm1, mm7
			punpcklbw	mm0, mm7
			pmaddwd		mm1, mm4
			pmaddwd		mm0, mm4

			movd		mm2, dword ptr [edx+edi]
			punpcklbw	mm2, dword ptr [ecx+edi]

			movq		mm3, mm2
			punpckhbw	mm3, mm7
			punpcklbw	mm2, mm7
			pmaddwd		mm3, mm5
			pmaddwd		mm2, mm5
			paddd		mm0, mm6
			paddd		mm1, mm6
			paddd		mm0, mm2
			paddd		mm1, mm3
			psrad		mm0, 14
			psrad		mm1, 14
			packssdw	mm0, mm1
			packuswb	mm0, mm0
			movd		[esi+edi], mm0
			add			edi, 4
			jnz			xloop

			lea		esp, [ebp-12]
			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			ret
		}
	}
#endif
}

void WarpResizeFilter::ComputeBicubicImage() {
	const sint32 srcwidth = fa->src.w;
	const sint32 srcheight = fa->src.h;
	const uint32 *src = (const uint32 *)fa->src.data;
	const ptrdiff_t srcpitch = fa->src.pitch;

	const sint32 dstwidth = fa->dst.w;
	const sint32 dstheight = fa->dst.h;
	uint32 *dst = (uint32 *)fa->dst.data;
	const ptrdiff_t dstpitch = fa->dst.pitch;

	uint32 *cubicbuf[4] = {
		(uint32 *)mpTempRows + 0*dstwidth,
		(uint32 *)mpTempRows + 1*dstwidth,
		(uint32 *)mpTempRows + 2*dstwidth,
		(uint32 *)mpTempRows + 3*dstwidth
	};

	uint32 *cubicwin[8] = {
		cubicbuf[0],
		cubicbuf[0],
		cubicbuf[0],
		cubicbuf[0],
		cubicbuf[0],
		cubicbuf[0],
		cubicbuf[0],
		cubicbuf[0],
	};

	// compute gradients and initial accumulators
	sint32 xscale = (sint32)(0.5 + 65536.0 * (double)srcwidth  / dstwidth );
	sint32 yscale = (sint32)(0.5 + 65536.0 * (double)srcheight / dstheight);
	sint32 xaccum = (xscale >> 1) + 0x8000;
	sint32 yaccum = (yscale >> 1) + 0x8000;

	uint32 *expandedsrc = mpExpandedSrc;
	sint32 expandedsrcwidth = srcwidth + 4;

	int epos = -1;
	int ypos = -1;

	for(sint32 y=0; y<dstheight; ++y) {
		// advance window position
		int ynewpos = (yaccum >> 16) + 1;
		if (ynewpos < 0)
			ynewpos = 0;

		if (ypos < ynewpos-4)
			ypos = ynewpos-4;

		while(ypos < ynewpos) {
			++ypos;

			uint32 *cubicdst;

			if (ypos >= srcheight) {
				cubicdst = cubicwin[(ypos&3) + 3];
			} else {
				while(epos < ypos) {
					++epos;
					memcpy(expandedsrc+2, src, srcwidth*4);
					src = (const uint32 *)((char *)src + srcpitch);
					expandedsrc[0] = expandedsrc[1] = expandedsrc[2];
					expandedsrc[srcwidth+2] = expandedsrc[srcwidth+3] = expandedsrc[srcwidth+1];
					expandedsrc += expandedsrcwidth;
				}

				cubicdst = cubicbuf[ypos & 3];
#ifdef _M_IX86
				if (mbUseMMX)
					CubicFilterH_MMX(cubicdst, expandedsrc - expandedsrcwidth, dstwidth, xaccum, xscale, mCubicTableMMX);
				else
#endif
					CubicFilterH(cubicdst, expandedsrc - expandedsrcwidth, dstwidth, xaccum, xscale, mCubicTable);
			}
			cubicwin[(ypos&3)] = cubicwin[(ypos&3)+4] = cubicdst;
		}

		// scanout one destination line
		uint32 **winptr = cubicwin + (ypos & 3);
#ifdef _M_IX86
		if (mbUseMMX)
			CubicFilterV_MMX(dst, winptr+1, dstwidth, (yaccum & 0xff00) >> 8, mCubicTableMMX);
		else
#endif
			CubicFilterV(dst, winptr[1], winptr[2], winptr[3], winptr[0], dstwidth, (yaccum & 0xff00) >> 8, mCubicTable);
		yaccum += yscale;
		dst = (uint32 *)((char *)dst + dstpitch);
	}

#ifdef _M_IX86
	if (mbUseMMX)
		__asm emms
#endif
}

void SampleBicubic(uint32 *dst, const uint32 *const *srcptrs, uint32 offset, const void *xf0, const void *xf1, const void *yf0, const void *yf1) {
	const uint8 *src0 = (const uint8 *)(srcptrs[0] + offset);
	const uint8 *src1 = (const uint8 *)(srcptrs[1] + offset);
	const uint8 *src2 = (const uint8 *)(srcptrs[2] + offset);
	const uint8 *src3 = (const uint8 *)(srcptrs[3] + offset);

	const int xc0 = ((const int *)xf0)[0];
	const int xc1 = ((const int *)xf0)[1];
	const int xc2 = ((const int *)xf1)[1];
	const int xc3 = ((const int *)xf1)[0];
	const int yc0 = ((const int *)yf0)[0];
	const int yc1 = ((const int *)yf0)[1];
	const int yc2 = ((const int *)yf1)[1];
	const int yc3 = ((const int *)yf1)[0];

	sint32 r0 = (src0[2]*xc0 + src0[6]*xc1 + src0[10]*xc2 + src0[14]*xc3 + 8192) >> 14;
	sint32 g0 = (src0[1]*xc0 + src0[5]*xc1 + src0[ 9]*xc2 + src0[13]*xc3 + 8192) >> 14;
	sint32 b0 = (src0[0]*xc0 + src0[4]*xc1 + src0[ 8]*xc2 + src0[12]*xc3 + 8192) >> 14;

	sint32 r1 = (src1[2]*xc0 + src1[6]*xc1 + src1[10]*xc2 + src1[14]*xc3 + 8192) >> 14;
	sint32 g1 = (src1[1]*xc0 + src1[5]*xc1 + src1[ 9]*xc2 + src1[13]*xc3 + 8192) >> 14;
	sint32 b1 = (src1[0]*xc0 + src1[4]*xc1 + src1[ 8]*xc2 + src1[12]*xc3 + 8192) >> 14;

	sint32 r2 = (src2[2]*xc0 + src2[6]*xc1 + src2[10]*xc2 + src2[14]*xc3 + 8192) >> 14;
	sint32 g2 = (src2[1]*xc0 + src2[5]*xc1 + src2[ 9]*xc2 + src2[13]*xc3 + 8192) >> 14;
	sint32 b2 = (src2[0]*xc0 + src2[4]*xc1 + src2[ 8]*xc2 + src2[12]*xc3 + 8192) >> 14;

	sint32 r3 = (src3[2]*xc0 + src3[6]*xc1 + src3[10]*xc2 + src3[14]*xc3 + 8192) >> 14;
	sint32 g3 = (src3[1]*xc0 + src3[5]*xc1 + src3[ 9]*xc2 + src3[13]*xc3 + 8192) >> 14;
	sint32 b3 = (src3[0]*xc0 + src3[4]*xc1 + src3[ 8]*xc2 + src3[12]*xc3 + 8192) >> 14;

	sint32 rfinal = (r0*yc0 + r1*yc1 + r2*yc2 + r3*yc3 + 8192) >> 14;
	sint32 gfinal = (g0*yc0 + g1*yc1 + g2*yc2 + g3*yc3 + 8192) >> 14;
	sint32 bfinal = (b0*yc0 + b1*yc1 + b2*yc2 + b3*yc3 + 8192) >> 14;

	sint32 cprev = dst[0];
	sint32 rprev = (cprev >> 16) & 0xff;
	sint32 gprev = (cprev >>  8) & 0xff;
	sint32 bprev = (cprev      ) & 0xff;

	rfinal = rfinal+rfinal - rprev;
	gfinal = gfinal+gfinal - gprev;
	bfinal = bfinal+bfinal - bprev;

	if ((uint32)rfinal >= 256) rfinal = (~rfinal >> 31) & 0xff;
	if ((uint32)gfinal >= 256) gfinal = (~gfinal >> 31) & 0xff;
	if ((uint32)bfinal >= 256) bfinal = (~bfinal >> 31) & 0xff;

	dst[0] = (rfinal << 16) + (gfinal << 8) + bfinal;
}

#ifdef _M_IX86
	void __declspec(naked) SampleBicubic_MMX(uint32 *dst, const uint32 *const *srcptrs, uint32 offset, const void *xf0, const void *xf1, const void *yf0, const void *yf1) {
		static const __declspec(align(8)) __int64 round = 0x0000200000002000;
		__asm {
			push		ebp
			mov			ebp, esp
			push		edi
			push		esi
			push		ebx
			and			esp, -8
			sub			esp, 32

			mov			edi, [ebp+12]
			mov			esi, [ebp+16]
			shl			esi, 2
			mov			eax, [ebp+20]
			mov			ebx, [ebp+24]
			movd		mm4, dword ptr [eax]
			movd		mm5, dword ptr [ebx]
			punpckldq	mm4, mm4
			punpckldq	mm5, mm5
			movq		mm6, round
			pxor		mm7, mm7

			;process row
			mov			ecx, [edi+0]
			add			ecx, esi

			movd		mm0, dword ptr [ecx]
			punpcklbw	mm0, dword ptr [ecx+4]
			movq		mm1, mm0
			punpcklbw	mm0, mm7
			punpckhbw	mm1, mm7
			pmaddwd		mm0, mm4
			pmaddwd		mm1, mm4

			movd		mm2, dword ptr [ecx+12]
			punpcklbw	mm2, dword ptr [ecx+8]
			movq		mm3, mm2
			punpcklbw	mm2, mm7
			punpckhbw	mm3, mm7
			pmaddwd		mm2, mm5
			pmaddwd		mm3, mm5

			paddd		mm0, mm2
			paddd		mm1, mm3
			paddd		mm0, mm6
			paddd		mm1, mm6
			psrad		mm0, 14
			psrad		mm1, 14
			packssdw	mm0, mm1

			movq		[esp], mm0

			;process row
			mov			ecx, [edi+4]
			add			ecx, esi

			movd		mm0, dword ptr [ecx]
			punpcklbw	mm0, dword ptr [ecx+4]
			movq		mm1, mm0
			punpcklbw	mm0, mm7
			punpckhbw	mm1, mm7
			pmaddwd		mm0, mm4
			pmaddwd		mm1, mm4

			movd		mm2, dword ptr [ecx+12]
			punpcklbw	mm2, dword ptr [ecx+8]
			movq		mm3, mm2
			punpcklbw	mm2, mm7
			punpckhbw	mm3, mm7
			pmaddwd		mm2, mm5
			pmaddwd		mm3, mm5

			paddd		mm0, mm2
			paddd		mm1, mm3
			paddd		mm0, mm6
			paddd		mm1, mm6
			psrad		mm0, 14
			psrad		mm1, 14
			packssdw	mm0, mm1

			movq		[esp+8], mm0

			;process row
			mov			ecx, [edi+8]
			add			ecx, esi

			movd		mm0, dword ptr [ecx]
			punpcklbw	mm0, dword ptr [ecx+4]
			movq		mm1, mm0
			punpcklbw	mm0, mm7
			punpckhbw	mm1, mm7
			pmaddwd		mm0, mm4
			pmaddwd		mm1, mm4

			movd		mm2, dword ptr [ecx+12]
			punpcklbw	mm2, dword ptr [ecx+8]
			movq		mm3, mm2
			punpcklbw	mm2, mm7
			punpckhbw	mm3, mm7
			pmaddwd		mm2, mm5
			pmaddwd		mm3, mm5

			paddd		mm0, mm2
			paddd		mm1, mm3
			paddd		mm0, mm6
			paddd		mm1, mm6
			psrad		mm0, 14
			psrad		mm1, 14
			packssdw	mm0, mm1

			movq		[esp+16], mm0

			;process row
			mov			ecx, [edi+12]
			add			ecx, esi

			movd		mm0, [ecx]
			punpcklbw	mm0, [ecx+4]
			movq		mm1, mm0
			punpcklbw	mm0, mm7
			punpckhbw	mm1, mm7
			pmaddwd		mm0, mm4
			pmaddwd		mm1, mm4

			movd		mm2, dword ptr [ecx+12]
			punpcklbw	mm2, dword ptr [ecx+8]
			movq		mm3, mm2
			punpcklbw	mm2, mm7
			punpckhbw	mm3, mm7
			pmaddwd		mm2, mm5
			pmaddwd		mm3, mm5

			paddd		mm0, mm2
			paddd		mm1, mm3
			paddd		mm0, mm6
			paddd		mm1, mm6
			psrad		mm0, 14
			psrad		mm1, 14
			packssdw	mm0, mm1

			movq		[esp+24], mm0

			;process column
			mov			eax, [ebp+28]
			mov			ebx, [ebp+32]
			movd		mm4, dword ptr [eax]
			movd		mm5, dword ptr [ebx]
			punpckldq	mm4, mm4
			punpckldq	mm5, mm5

			movd		mm0, dword ptr [esp]
			punpcklwd	mm0, dword ptr [esp+8]
			movd		mm1, dword ptr [esp+4]
			punpcklwd	mm1, dword ptr [esp+12]
			movd		mm2, dword ptr [esp+24]
			punpcklwd	mm2, dword ptr [esp+16]
			movd		mm3, dword ptr [esp+28]
			punpcklwd	mm3, dword ptr [esp+20]
			pmaddwd		mm0, mm4
			pmaddwd		mm1, mm4
			pmaddwd		mm2, mm5
			pmaddwd		mm3, mm5
			paddd		mm0, mm2
			paddd		mm1, mm3
			paddd		mm0, mm6
			paddd		mm1, mm6
			psrad		mm0, 14
			psrad		mm1, 14
			packssdw	mm0, mm1
			packuswb	mm0, mm0

			mov			edx, [ebp+8]
			movd		mm1, dword ptr [edx]
			movq		mm2, mm0
			psubusb		mm0, mm1		// new - old
			psubusb		mm1, mm2		// old - new
			paddusb		mm2, mm0
			psubusb		mm2, mm1
			movd		dword ptr [edx], mm2

			lea			esp, [ebp-12]
			pop			ebx
			pop			esi
			pop			edi
			pop			ebp
			ret
		}
	}
#endif

void WarpResizeFilter::ComputeFinalPass() {
	const sint32 srcwidth = fa->src.w;
	const sint32 srcheight = fa->src.h;

	const sint32 dstwidth = fa->dst.w;
	const sint32 dstheight = fa->dst.h;
	uint32 *dst = (uint32 *)fa->dst.data;
	const ptrdiff_t dstpitch = fa->dst.pitch;

	const sint16 *gradsrc = mpGradientBuffer;

	sint32 xscale = (sint32)(0.5 + 65536.0 * (double)srcwidth  / dstwidth );
	sint32 yscale = (sint32)(0.5 + 65536.0 * (double)srcheight / dstheight);
	sint32 xaccum = (xscale >> 1) + 0x8000;
	sint32 yaccum = (yscale >> 1) + 0x8000;

	for(sint32 y=0; y<dstheight; ++y) {
		sint32 xaccum2 = xaccum;
		sint32 yoffset = yaccum >> 8;

#ifdef _M_IX86
		if (mbUseMMX) {
			for(sint32 x=0; x<dstwidth; ++x) {
				int xoffset = (xaccum2 >> 8);

				sint32 dx = *gradsrc++;
				sint32 dy = *gradsrc++;

				sint32 dnorm2 = dx*dx + dy*dy;

				int scale = mNormalizeTable[dnorm2 >> 10];

				dx = (dx*scale + 16384) >> 15;
				dy = (dy*scale + 16384) >> 15;

				dx += xoffset;
				dy += yoffset;

				int xphase = (dx & 255);
				int yphase = (dy & 255);

				SampleBicubic_MMX(
					&dst[x],
					&mpRowTable[(dy >> 8)],
					(dx >> 8),
					&mCubicTableMMX[xphase],
					&mCubicTableMMX[256-xphase],
					&mCubicTableMMX[yphase],
					&mCubicTableMMX[256-yphase]);

				xaccum2 += xscale;
			}
		} else
#endif
		{
			for(sint32 x=0; x<dstwidth; ++x) {
				int xoffset = (xaccum2 >> 8);

				sint32 dx = *gradsrc++;
				sint32 dy = *gradsrc++;

				sint32 dnorm2 = dx*dx + dy*dy;

				int scale = mNormalizeTable[dnorm2 >> 10];

				dx = (dx*scale + 16384) >> 15;
				dy = (dy*scale + 16384) >> 15;

				dx += xoffset;
				dy += yoffset;

				int xphase = (dx & 255)*2;
				int yphase = (dy & 255)*2;

				SampleBicubic(
					&dst[x],
					&mpRowTable[(dy >> 8)],
					(dx >> 8),
					&mCubicTable[xphase],
					&mCubicTable[512-xphase],
					&mCubicTable[yphase],
					&mCubicTable[512-yphase]);

				xaccum2 += xscale;
			}
		}

		dst = (uint32 *)((char *)dst + dstpitch);
		yaccum += yscale;
	}

#ifdef _M_IX86
	if (mbUseMMX)
		__asm emms
#endif
}

void WarpResizeFilter::ScriptConfigFunc(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mTargetWidth = argv[0].asInt();
	if (mTargetWidth < 1)
		mTargetWidth = 1;
	mTargetHeight = argv[1].asInt();
	if (mTargetHeight < 1)
		mTargetHeight = 1;
	mbShowGradientMap = argv[2].asInt() != 0;
}

VDXVF_BEGIN_SCRIPT_METHODS(WarpResizeFilter)
	VDXVF_DEFINE_SCRIPT_METHOD(WarpResizeFilter, ScriptConfigFunc, "iii")
VDXVF_END_SCRIPT_METHODS()

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)

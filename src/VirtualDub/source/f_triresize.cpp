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
#include <commctrl.h>

#include <vd2/system/vdtypes.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include "resource.h"
#include "gui.h"
#include "filter.h"
#include "vbitmap.h"

extern HINSTANCE g_hInst;

#ifdef _MSC_VER
	#pragma warning(disable: 4799)			// warning C4799: function has no EMMS instruction
#endif

///////////////////////

struct TriResizeFilterData {
	long new_x, new_y;

	IFilterPreview *ifp;
};

////////////////////

#ifdef _M_IX86
static void __declspec(naked) __cdecl asm_resize_tri(Pixel32 *dst, const Pixel32 *src1, const Pixel32 *src2, int w, uint32 uinc, uint32 ufinc, uint32 vfrac) {
	__asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov			edi, [esp+4+16]
		mov			ecx, [esp+8+16]
		mov			edx, [esp+12+16]
		mov			ebp, [esp+16+16]
		mov			ebx, [esp+24+16]
		xor			esi, esi
		xor			eax, eax

		movd		mm7, [esp+28+16]
		pshufw		mm7, mm7, 55h
		psrlw		mm7, 1

xloop:
		movd		mm1, [ecx+esi*4]
		movd		mm0, [ecx+esi*4+4]
		movd		mm3, [edx+esi*4]
		movd		mm2, [edx+esi*4+4]
		movq		mm4, mm0
		movq		mm5, mm1
		pavgb		mm4, mm2
		pavgb		mm5, mm3
		pavgb		mm4, mm5
		psadbw		mm0, mm4
		psadbw		mm1, mm4
		psadbw		mm2, mm4
		psadbw		mm3, mm4
		pmaxsw		mm0, mm3
		pmaxsw		mm1, mm2
		psubd		mm0, mm1
		psrad		mm0, 31

		movd		mm1, [ecx+esi*4]
		movd		mm2, [ecx+esi*4+4]
		movd		mm3, [edx+esi*4]
		movd		mm4, [edx+esi*4+4]
		movq		mm5, mm2
		movq		mm6, mm4
		pxor		mm5, mm1
		pxor		mm6, mm3
		pand		mm5, mm0
		pand		mm6, mm0
		pxor		mm1, mm5
		pxor		mm2, mm5
		pxor		mm3, mm6
		pxor		mm4, mm6
		pxor		mm5, mm5
		punpcklbw	mm1, mm5
		punpcklbw	mm2, mm5
		punpcklbw	mm3, mm5
		punpcklbw	mm4, mm5

		movd		mm6, eax
		pxor		mm0, mm6

		pshufw		mm0, mm0, 55h
		psrlw		mm0, 1
		movq		mm6, mm0
		pcmpgtw		mm6, mm7

		movq		mm5, mm4
		psubw		mm4, mm2	// dvdy (upper tri)
		psubw		mm2, mm1	// dudx (upper tri)
		psubw		mm5, mm3	// dudx (lower tri)
		psubw		mm3, mm1	// dvdy (lower tri)

		pxor		mm2, mm5
		pxor		mm4, mm3
		pand		mm2, mm6
		pand		mm4, mm6
		pxor		mm5, mm2
		pxor		mm3, mm4
		paddw		mm5, mm5
		paddw		mm3, mm3
		pmulhw		mm5, mm0
		pmulhw		mm3, mm7
		paddw		mm1, mm5
		paddw		mm1, mm3
		packuswb	mm1, mm1
		movd		[edi], mm1
		add			edi, 4

		add			eax, ebx
		adc			esi, [esp+20+16]

		dec			ebp
		jne			xloop

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	}
}
#endif

namespace {
	int sq(int x) { return x*x; }
}

static int triresize_run(const FilterActivation *fa, const FilterFunctions *ff) {
	const long dstw = fa->dst.w;
	const long dsth = fa->dst.h;
	const long srcw = fa->src.w;
	const long srch = fa->src.h;

	const sint32 dudx = ((srcw<<16)-0x10001) / (dstw-1);
	const sint32 dvdy = ((srch<<16)-0x10001) / (dsth-1);

	uint32 v = 0;

	for(int y=0; y<dsth; ++y) {
		const sint32 iv = v >> 16;
		const uint32 *row0 = (const uint32 *)((const char *)fa->src.data + fa->src.pitch * iv);
		const uint32 *row1 = (const uint32 *)((const char *)fa->src.data + fa->src.pitch * (iv+1));
		uint32 *dst = (uint32 *)((char *)fa->dst.data + fa->dst.pitch * y);

#ifndef _M_IX86
		sint32 vf = (v & 0xffff) >> 8;
		uint32 u = 0;
		for(int x=0; x<dstw; ++x) {
			const sint32 iu = u >> 16;
			sint32 uf = (u & 0xffff) >> 8;

			uint32 p0 = row0[iu];
			uint32 p1 = row0[iu+1];
			uint32 p2 = row1[iu];
			uint32 p3 = row1[iu+1];

			const int b0 = (p0&0xff);
			const int b1 = (p1&0xff);
			const int b2 = (p2&0xff);
			const int b3 = (p3&0xff);
			const int g0 = ((p0>>8)&0xff);
			const int g1 = ((p1>>8)&0xff);
			const int g2 = ((p2>>8)&0xff);
			const int g3 = ((p3>>8)&0xff);
			const int r0 = ((p0>>16)&0xff);
			const int r1 = ((p1>>16)&0xff);
			const int r2 = ((p2>>16)&0xff);
			const int r3 = ((p3>>16)&0xff);

			int y0 = 54*r0 + 183*g0 + 19*b0;
			int y1 = 54*r1 + 183*g1 + 19*b1;
			int y2 = 54*r2 + 183*g2 + 19*b2;
			int y3 = 54*r3 + 183*g3 + 19*b3;

			int yavg = (y0+y1+y2+y3+2)>>2;

			int code = -8*((y3-yavg)>>31) - 4*((y2-yavg)>>31) - 2*((y1-yavg)>>31) - 1*((y0-yavg)>>31);

			switch(code) {
			case 1:
			case 7:
			case 8:
			case 14:
				uf ^= 0xff;
				std::swap(p0, p1);
				std::swap(p2, p3);
			case 2:
			case 4:
			case 11:
			case 13:
				if (uf > vf) {
					uint32 rb = ((p0 & 0xff00ff) + (((((p1&0xff00ff) - (p0&0xff00ff))*uf) + ((p3&0xff00ff) - (p1&0xff00ff))*vf + 0x800080) >> 8)) & 0xff00ff;
					uint32 g  = ((p0 & 0x00ff00) + (((((p1&0x00ff00) - (p0&0x00ff00))*uf) + ((p3&0x00ff00) - (p1&0x00ff00))*vf + 0x008000) >> 8)) & 0x00ff00;

					dst[x] = rb + g;
				} else {
					uint32 rb = ((p0 & 0xff00ff) + (((((p3&0xff00ff) - (p2&0xff00ff))*uf) + ((p2&0xff00ff) - (p0&0xff00ff))*vf + 0x800080) >> 8)) & 0xff00ff;
					uint32 g  = ((p0 & 0x00ff00) + (((((p3&0x00ff00) - (p2&0x00ff00))*uf) + ((p2&0x00ff00) - (p0&0x00ff00))*vf + 0x008000) >> 8)) & 0x00ff00;
					dst[x] = rb + g;
				}
				break;
			case 0:
			case 3:
			case 5:
			case 6:
			case 9:
			case 10:
			case 12:
			case 15:
				{
					uint32 rb0 = ((p0 & 0xff00ff) + (((((p1&0xff00ff) - (p0&0xff00ff))*uf) + 0x800080) >> 8)) & 0xff00ff;
					uint32 g0  = ((p0 & 0x00ff00) + (((((p1&0x00ff00) - (p0&0x00ff00))*uf) + 0x008000) >> 8)) & 0x00ff00;
					uint32 rb1 = ((p2 & 0xff00ff) + (((((p3&0xff00ff) - (p2&0xff00ff))*uf) + 0x800080) >> 8)) & 0xff00ff;
					uint32 g1  = ((p2 & 0x00ff00) + (((((p3&0x00ff00) - (p2&0x00ff00))*uf) + 0x008000) >> 8)) & 0x00ff00;
					uint32 rb  = (rb0 + ((((rb1 - rb0)*vf) + 0x800080) >> 8)) & 0xff00ff;
					uint32 g   = (g0  + ((((g1  - g0 )*vf) + 0x008000) >> 8)) & 0x00ff00;

					dst[x] = rb + g;
				}
				break;
			}

			u += dudx;
		}
#elif 0
		uint32 u = 0;
		for(int x=0; x<dstw; ++x) {
			const sint32 iu = u >> 16;
			sint32 uf = (u & 0xffff) >> 8;

			uint32 p0 = row0[iu];
			uint32 p1 = row0[iu+1];
			uint32 p2 = row1[iu];
			uint32 p3 = row1[iu+1];

			const int b0 = (p0&0xff);
			const int b1 = (p1&0xff);
			const int b2 = (p2&0xff);
			const int b3 = (p3&0xff);
			const int g0 = ((p0>>8)&0xff);
			const int g1 = ((p1>>8)&0xff);
			const int g2 = ((p2>>8)&0xff);
			const int g3 = ((p3>>8)&0xff);
			const int r0 = ((p0>>16)&0xff);
			const int r1 = ((p1>>16)&0xff);
			const int r2 = ((p2>>16)&0xff);
			const int r3 = ((p3>>16)&0xff);

			const int ravg = (r0 + r1 + r2 + r3 + 2) >> 2;
			const int gavg = (g0 + g1 + g2 + g3 + 2) >> 2;
			const int bavg = (b0 + b1 + b2 + b3 + 2) >> 2;

			int diff0 = sq(ravg - r0) + sq(gavg - g0) + sq(bavg - b0);
			int diff1 = sq(ravg - r1) + sq(gavg - g1) + sq(bavg - b1);
			int diff2 = sq(ravg - r2) + sq(gavg - g2) + sq(bavg - b2);
			int diff3 = sq(ravg - r3) + sq(gavg - g3) + sq(bavg - b3);

			if (std::max(diff0, diff3) > std::max(diff1, diff2)) {
				uf ^= 0xff;
				std::swap(p0, p1);
				std::swap(p2, p3);
			}

			if (uf > vf) {
				uint32 rb = ((p0 & 0xff00ff) + (((((p1&0xff00ff) - (p0&0xff00ff))*uf) + ((p3&0xff00ff) - (p1&0xff00ff))*vf + 0x800080) >> 8)) & 0xff00ff;
				uint32 g  = ((p0 & 0x00ff00) + (((((p1&0x00ff00) - (p0&0x00ff00))*uf) + ((p3&0x00ff00) - (p1&0x00ff00))*vf + 0x008000) >> 8)) & 0x00ff00;

				dst[x] = rb + g;
			} else {
				uint32 rb = ((p0 & 0xff00ff) + (((((p3&0xff00ff) - (p2&0xff00ff))*uf) + ((p2&0xff00ff) - (p0&0xff00ff))*vf + 0x800080) >> 8)) & 0xff00ff;
				uint32 g  = ((p0 & 0x00ff00) + (((((p3&0x00ff00) - (p2&0x00ff00))*uf) + ((p2&0x00ff00) - (p0&0x00ff00))*vf + 0x008000) >> 8)) & 0x00ff00;
				dst[x] = rb + g;
			}

			u += dudx;
		}
#else
		asm_resize_tri(dst, row0, row1, dstw, dudx>>16, dudx<<16, v<<16);
#endif
		v += dvdy;
	}

#ifdef _M_IX86
	__asm emms
#endif

	return 0;
}

static long triresize_param(FilterActivation *fa, const FilterFunctions *ff) {
	TriResizeFilterData *mfd = (TriResizeFilterData *)fa->filter_data;

	fa->dst.w		= mfd->new_x;
	fa->dst.h		= mfd->new_y;

	fa->dst.AlignTo8();

	return FILTERPARAM_SWAP_BUFFERS;
}

static INT_PTR CALLBACK triresizeDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	TriResizeFilterData *mfd = (struct TriResizeFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				mfd = (TriResizeFilterData *)lParam;

				SetDlgItemInt(hDlg, IDC_WIDTH, mfd->new_x, FALSE);
				SetDlgItemInt(hDlg, IDC_HEIGHT, mfd->new_y, FALSE);

				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)mfd);

				mfd->ifp->InitButton(GetDlgItem(hDlg, IDC_PREVIEW));
			}
            return (TRUE);

        case WM_COMMAND:                      
			switch(LOWORD(wParam)) {
			case IDOK:
				mfd->ifp->Close();
				EndDialog(hDlg, 0);
				return TRUE;

			case IDCANCEL:
				mfd->ifp->Close();
                EndDialog(hDlg, 1);
                return TRUE;

			case IDC_WIDTH:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					long new_x;
					BOOL success;

					new_x = GetDlgItemInt(hDlg, IDC_WIDTH, &success, FALSE);
					if (!success || new_x < 16) {
						SetFocus((HWND)lParam);
						MessageBeep(MB_ICONQUESTION);
						return TRUE;
					}

					mfd->ifp->UndoSystem();
					mfd->new_x = new_x;
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_HEIGHT:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					long new_y;
					BOOL success;

					new_y = GetDlgItemInt(hDlg, IDC_HEIGHT, &success, FALSE);
					if (!success || new_y < 16) {
						SetFocus((HWND)lParam);
						MessageBeep(MB_ICONQUESTION);
						return TRUE;
					}

					mfd->ifp->UndoSystem();
					mfd->new_y = new_y;
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_PREVIEW:
				mfd->ifp->Toggle(hDlg);
				return TRUE;
            }
            break;
    }
    return FALSE;
}

static int triresize_config(FilterActivation *fa, const FilterFunctions *ff, HWND hWnd) {
	TriResizeFilterData *mfd = (TriResizeFilterData *)fa->filter_data;
	TriResizeFilterData mfd2 = *mfd;
	int ret;

	mfd->ifp = fa->ifp;

	if (mfd->new_x < 16)
		mfd->new_x = 320;
	if (mfd->new_y < 16)
		mfd->new_y = 240;

	ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_TRIRESIZE), hWnd, triresizeDlgProc, (LONG)mfd);

	if (ret)
		*mfd = mfd2;

	return ret;
}

static int triresize_start(FilterActivation *fa, const FilterFunctions *ff) {
	TriResizeFilterData *mfd = (TriResizeFilterData *)fa->filter_data;
	long dstw = mfd->new_x;
	long dsth = mfd->new_y;

	if (dstw<16 || dsth<16)
		return 1;

	return 0;
}

static int triresize_stop(FilterActivation *fa, const FilterFunctions *ff) {
	return 0;
}

static void triresize_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;

	TriResizeFilterData *mfd = (TriResizeFilterData *)fa->filter_data;

	mfd->new_x	= argv[0].asInt();
	mfd->new_y	= argv[1].asInt();
}

static ScriptFunctionDef triresize_func_defs[]={
	{ (ScriptFunctionPtr)triresize_script_config, "Config", "0ii" },
	{ NULL },
};

static CScriptObject triresize_obj={
	NULL, triresize_func_defs
};

static bool triresize_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	TriResizeFilterData *mfd = (TriResizeFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d,%d)", mfd->new_x, mfd->new_y);

	return true;
}

FilterDefinition filterDef_triresize={
	0,0,NULL,
	"triangular resize",
	"Resizes the image to a new size using triangulation-based interpolation."
#ifdef USE_ASM
			"\n\n[Assembly optimized] [MMX optimized]"
#endif
			,
	NULL,NULL,
	sizeof(TriResizeFilterData),
	NULL,NULL,
	triresize_run,
	triresize_param,
	triresize_config,
	NULL,
	triresize_start,
	triresize_stop,

	&triresize_obj,
	triresize_script_line,
};
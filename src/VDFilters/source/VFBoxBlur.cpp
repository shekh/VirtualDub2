//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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
//
/////////////////////////////////////////////////////////////////////////////
//
//	COMPILER BUG WARNING:
//
//	VC6 will sometimes miscompile expressions having both downcasts and
//	upcasts:
//
//		// compile with /O2axb2 /G6s
//		void foo(unsigned short *dst, unsigned v, int mul) {
//			*dst = (int)(unsigned char)(v>>8) * mul;
//
//	The emitted code is missing the truncating downcast to unsigned
//	char, and thus fails to separate channels properly.  To work around
//	the problem, this filter now explicitly does (&255).  The bug is
//	not present in Visual Studio .NET (2003).

#include "stdafx.h"

#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/memory.h>

extern HINSTANCE g_hInst;
extern "C" unsigned char YUV_clip_table[];

#if defined(VD_COMPILER_MSVC) && defined(VD_CPU_X86)
	#pragma warning(disable: 4799)		// warning C4799: function has no EMMS instruction
#endif

///////////////////////////////////////////////////////////////////////////

int boxInitProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int boxRunProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int boxStartProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int boxEndProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
long boxParamProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
int boxConfigProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hwnd);
void boxStringProc2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *str, int maxlen);
void boxScriptConfig(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc);
bool boxFssProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen);

///////////////////////////////////////////////////////////////////////////

typedef unsigned short uint16;

typedef struct BoxFilterData {
	IVDXFilterPreview *ifp;
	uint32 *rows;
	uint16 *trow;
	int 	filter_width;
	int 	filter_power;
} BoxFilterData;

VDXScriptFunctionDef box_func_defs[]={
	{ (VDXScriptFunctionPtr)boxScriptConfig, "Config", "0ii" },
	{ NULL },
};

VDXScriptObject box_obj={
	NULL, box_func_defs
};

extern const VDXFilterDefinition g_VDVFBoxBlur = {

	NULL, NULL, NULL,		// next, prev, module
	"box blur",					// name
	"Performs a fast box, triangle, or cubic blur.",
							// desc
	NULL,					// maker
	NULL,					// private_data
	sizeof(BoxFilterData),	// inst_data_size

	boxInitProc,			// initProc
	NULL,					// deinitProc
	boxRunProc, 			// runProc
	boxParamProc,			// paramProc
	boxConfigProc,			// configProc
	NULL,
	boxStartProc,			// startProc
	boxEndProc, 			// endProc

	&box_obj,				// script_obj
	boxFssProc, 			// fssProc
	boxStringProc2,			// stringProc2

};

///////////////////////////////////////////////////////////////////////////

int boxInitProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	mfd->filter_width = 1;
	mfd->filter_power = 1;

	return 0;
}

int boxStartProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	if (mfd->filter_power < 1)
		mfd->filter_power = 1;

	if (mfd->filter_width < 1)
		mfd->filter_width = 1;

	mfd->trow = NULL;

	if (!(mfd->rows = (uint32 *)malloc(((fa->dst.w+1)&-2)*4*fa->dst.h)))
		return 1;

	if (!(mfd->trow = (uint16 *)malloc(fa->dst.w*8)))
		return 1;

	return 0;
}

static void box_filter_row(uint32 *dst, uint32 *src, int filtwidth, int cnt, int mult) {
	int i;
	unsigned r, g, b;
	uint32 A, B;

	A = src[0];
	r = (int)((A>>16)&255) * filtwidth;
	g = (int)((A>> 8)&255) * filtwidth;
	b = (int)((A    )&255) * filtwidth;

	i = filtwidth + 1;
	do {
		B = *src++;

		r += (int)((B>>16) & 255);
		g += (int)((B>> 8) & 255);
		b += (int)((B    ) & 255);
	} while(--i);

	i = filtwidth;

	do {
		*dst++ = ((r*mult)&0xff0000) + (((g*mult)>>8)&0xff00) + ((b*mult)>>16);

		B = src[0];
		++src;

		r = r - (int)((A>>16) & 255) + (int)((B>>16) & 255);
		g = g - (int)((A>> 8) & 255) + (int)((B>> 8) & 255);
		b = b - (int)((A    ) & 255) + (int)((B    ) & 255);

	} while(--i);

	i = cnt - 2*filtwidth - 1;
	do {
		*dst++ = ((r*mult)&0xff0000) + (((g*mult)>>8)&0xff00) + ((b*mult)>>16);

		A = src[-(2*filtwidth+1)];
		B = src[0];
		++src;

		r = r - (int)((A>>16) & 255) + (int)((B>>16) & 255);
		g = g - (int)((A>> 8) & 255) + (int)((B>> 8) & 255);
		b = b - (int)((A    ) & 255) + (int)((B    ) & 255);
	} while(--i);

	i = filtwidth;
	do {
		*dst++ = ((r*mult)&0xff0000) + (((g*mult)>>8)&0xff00) + ((b*mult)>>16);

		A = src[-(2*filtwidth+1)];
		++src;

		r = r - (int)((A>>16) & 255) + (int)((B>>16) & 255);
		g = g - (int)((A>> 8) & 255) + (int)((B>> 8) & 255);
		b = b - (int)((A    ) & 255) + (int)((B    ) & 255);

	} while(--i);

	*dst = ((r*mult)&0xff0000) + (((g*mult)>>8)&0xff00) + ((b*mult)>>16);
}

#ifdef _M_IX86
static void __declspec(naked) box_filter_row_MMX(uint32 *dst, uint32 *src, int filtwidth, int cnt, int divisor) {
	__asm {
		push		ebx

		mov			ecx,[esp+12+4]			;ecx = filtwidth
		mov			eax,[esp+8+4]			;eax = src
		movd		mm6,ecx
		pxor		mm7,mm7
		movd		mm5,[eax]				;A = source pixel
		punpcklwd	mm6,mm6
		pcmpeqw		mm4,mm4					;mm4 = all -1's
		punpckldq	mm6,mm6
		psubw		mm6,mm4					;mm6 = filtwidth+1
		punpcklbw	mm5,mm7					;mm5 = src[0] (word)
		movq		mm0,mm5					;mm0 = A
		pmullw		mm5,mm6					;mm5 = src[0]*(filtwidth+1)
		add			eax,4					;next source pixel
xloop1:
		movd		mm1,[eax]				;B = next source pixel
		pxor		mm7,mm7
		punpcklbw	mm1,mm7
		add			eax,4
		paddw		mm5,mm1
		dec			ecx
		jne			xloop1

		mov			ecx,[esp+12+4]			;ecx = filtwidth
		movd		mm6,[esp+20+4]
		punpcklwd	mm6,mm6
		mov			edx,[esp+4+4]			;edx = dst
		punpckldq	mm6,mm6
xloop2:
		movd		mm1,[eax]				;B = next source pixel
		movq		mm2,mm5					;mm1 = accum

		pmulhw		mm2,mm6
		punpcklbw	mm1,mm7

		psubw		mm5,mm0					;accum -= A
		add			eax,4

		paddw		mm5,mm1					;accum += B
		add			edx,4

		packuswb	mm2,mm2
		dec			ecx

		movd		[edx-4],mm2
		jne			xloop2

		;main loop.
		
		mov			ebx,[esp+12+4]			;ebx = filtwidth
		mov			ecx,[esp+16+4]			;ecx = cnt
		lea			ebx,[ebx+ebx+1]			;ebx = 2*filtwidth+1
		sub			ecx,ebx					;ecx = cnt - (2*filtwidth+1)
		jz			xloop3e
		shl			ebx,2
		neg			ebx						;ebx = -4*(2*filtwidth+1)
xloop3:
		movd		mm0,[eax+ebx]			;mm0 = A = src[-(2*filtwidth+1)]
		movq		mm2,mm5

		movd		mm1,[eax]				;mm0 = B = src[0]
		pmulhw		mm2,mm6

		punpcklbw	mm0,mm7
		add			edx,4

		punpcklbw	mm1,mm7
		add			eax,4

		psubw		mm5,mm0					;accum -= A
		packuswb	mm2,mm2					;pack finished pixel

		paddw		mm5,mm1					;accum += B
		dec			ecx

		movd		[edx-4],mm2
		jne			xloop3
xloop3e:

		;finish up remaining pixels

		mov			ecx,[esp+12+4]			;ecx = filtwidth
xloop4:
		movd		mm0,[eax+ebx]			;mm0 = A = src[-(2*filtwidth+1)]
		movq		mm2,mm5

		pmulhw		mm2,mm6
		add			edx,4

		punpcklbw	mm0,mm7
		add			eax,4

		psubw		mm5,mm0					;accum -= A
		packuswb	mm2,mm2					;pack finished pixel

		paddw		mm5,mm1					;accum += B
		dec			ecx

		movd		[edx-4],mm2
		jne			xloop4

		pmulhw		mm5,mm6
		packuswb	mm5,mm5
		movd		[edx],mm5

		pop			ebx
		ret
	}
}
#endif

///////////////////////////////////////////////////////////////////////////

static void box_filter_mult_row(uint16 *dst, uint32 *src, int cnt, int mult) {
	uint32 A;

#ifdef _M_IX86
	if (MMX_enabled)
		__asm {
			mov			eax,src
			movd		mm6,mult
			mov			edx,dst
			punpcklwd	mm6,mm6
			mov			ecx,cnt
			punpckldq	mm6,mm6
xloop:
			movd		mm0,[eax+ecx*4-4]
			pxor		mm7,mm7

			punpcklbw	mm0,mm7

			pmullw		mm0,mm6

			movq		[edx+ecx*8-8],mm0

			dec			ecx
			jne			xloop
		}
	else
#endif
		do {
			A = *src++;

			dst[0] = (uint16)((int)((A>>16) & 255) * mult);
			dst[1] = (uint16)((int)((A>> 8) & 255) * mult);
			dst[2] = (uint16)((int)((A    ) & 255) * mult);

			dst += 3;
		} while(--cnt);
}

static void box_filter_add_row(uint16 *dst, uint32 *src, int cnt) {
	uint32 A;

#ifdef _M_IX86
	if (MMX_enabled)
		__asm {
			mov			eax,src
			mov			edx,dst
			mov			ecx,cnt
xloop:
			movd		mm0,[eax+ecx*4-4]
			pxor		mm7,mm7

			punpcklbw	mm0,mm7

			paddw		mm0,[edx+ecx*8-8]

			movq		[edx+ecx*8-8],mm0

			dec			ecx
			jne			xloop
		}
	else
#endif
		do {
			A = *src++;

			dst[0] = (uint16)(dst[0] + ((A>>16) & 255));
			dst[1] = (uint16)(dst[1] + ((A>> 8) & 255));
			dst[2] = (uint16)(dst[2] + ((A    ) & 255));

			dst += 3;
		} while(--cnt);
}

static void box_filter_produce_row(uint32 *dst, uint16 *tmp, uint32 *src_add, uint32 *src_sub, int cnt, int filter_width) {
	uint32 A, B;
	uint16 r, g, b;
	int mult = 0xffff / (2*filter_width+1) + 1;

#ifdef _M_IX86
	if (MMX_enabled)
		__asm {
			mov			eax,src_add
			movd		mm6,mult
			mov			edx,tmp
			punpcklwd	mm6,mm6
			mov			ecx,cnt
			punpckldq	mm6,mm6
			mov			ebx,src_sub
			mov			edi,dst
xloop:
			movq		mm2,[edx+ecx*8-8]
			pxor		mm7,mm7

			movd		mm0,[eax+ecx*4-4]
			movq		mm3,mm2

			movd		mm1,[ebx+ecx*4-4]
			pmulhw		mm2,mm6

			punpcklbw	mm0,mm7
			;

			punpcklbw	mm1,mm7
			paddw		mm0,mm3

			psubw		mm0,mm1
			packuswb	mm2,mm2

			movq		[edx+ecx*8-8],mm0

			movd		[edi+ecx*4-4],mm2

			dec			ecx
			jne			xloop
		}
	else
#endif
		do {
			A = *src_add++;
			B = *src_sub++;

			r = tmp[0];
			g = tmp[1];
			b = tmp[2];

			*dst++	= ((r*mult) & 0xff0000)
					+(((g*mult) & 0xff0000) >> 8)
					+ ((b*mult) >> 16);

			tmp[0] = (uint16)(r + ((A>>16)&255) - ((B>>16) & 255));
			tmp[1] = (uint16)(g + ((A>> 8)&255) - ((B>> 8) & 255));
			tmp[2] = (uint16)(b + ((A    )&255) - ((B    ) & 255));

			tmp += 3;
		} while(--cnt);
}

static void box_filter_produce_row2(uint32 *dst, uint16 *tmp, int cnt, int filter_width) {
	uint16 r, g, b;
	int mult = 0xffff / (2*filter_width+1) + 1;

#ifdef _M_IX86
	if (MMX_enabled)
		__asm {
			movd		mm6,mult
			mov			eax,tmp
			punpcklwd	mm6,mm6
			mov			ecx,cnt
			punpckldq	mm6,mm6
			mov			edx,dst
xloop:
			movq		mm2,[eax+ecx*8-8]
			pxor		mm7,mm7

			pmulhw		mm2,mm6

			packuswb	mm2,mm2

			movd		[edx+ecx*4-4],mm2

			dec			ecx
			jne			xloop
		}
	else
#endif
		do {
			r = tmp[0];
			g = tmp[1];
			b = tmp[2];

			*dst++	= ((r*mult) & 0xff0000)
					+(((g*mult) & 0xff0000) >> 8)
					+ ((b*mult) >> 16);

			tmp += 3;
		} while(--cnt);
}

///////////////////////////////////////////////////////////////////////

static void box_do_vertical_pass(uint32 *dst, ptrdiff_t dstpitch, uint32 *src, ptrdiff_t srcpitch, uint16 *trow, int w, int h, int filtwidth) {
	uint32 *srch = src;
	int j;

	box_filter_mult_row(trow, src, w, filtwidth + 1);

	src = (uint32 *)((char *)src + srcpitch);

	for(j=0; j<filtwidth; j++) {
		box_filter_add_row(trow, src, w);

		src = (uint32 *)((char *)src + srcpitch);
	}

	for(j=0; j<filtwidth; j++) {
		box_filter_produce_row(dst, trow, src, srch, w, filtwidth);

		src = (uint32 *)((char *)src + srcpitch);
		dst = (uint32 *)((char *)dst + dstpitch);
	}
	
	for(j=0; j<h - (2*filtwidth+1); j++) {
		box_filter_produce_row(dst, trow, src, (uint32 *)((char *)src - srcpitch*(2*filtwidth+1)), w, filtwidth);

		src = (uint32 *)((char *)src + srcpitch);
		dst = (uint32 *)((char *)dst + dstpitch);
	}

	srch = (uint32 *)((char *)src - srcpitch);

	for(j=0; j<filtwidth; j++) {
		box_filter_produce_row(dst, trow, srch, (uint32 *)((char *)src - srcpitch*(2*filtwidth+1)), w, filtwidth);

		src = (uint32 *)((char *)src + srcpitch);
		dst = (uint32 *)((char *)dst + dstpitch);
	}

	box_filter_produce_row2(dst, trow, w, filtwidth);
}

int boxRunProc(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;
	uint32 h;
	uint32 *src, *dst;
	uint32 *tmp;
	ptrdiff_t srcpitch, dstpitch;
	int i;

	src = fa->src.data;
	dst = fa->dst.data;
	tmp = mfd->rows;

	// Horizontal filtering.
	int hfiltwidth = mfd->filter_width;

	if (hfiltwidth * 2 + 1 > fa->dst.w)
		hfiltwidth = (fa->dst.w - 1) >> 1;

	if (hfiltwidth > 0) {
#ifdef _M_IX86
		void (*const pRowFilt)(uint32*, uint32*, int, int, int) = MMX_enabled ? box_filter_row_MMX : box_filter_row;
#else
		void (*const pRowFilt)(uint32*, uint32*, int, int, int) = box_filter_row;
#endif
		int mult = 0xffff / (2*hfiltwidth+1) + 1;

		h = fa->src.h;
		do {
			switch(mfd->filter_power) {
			case 1:
				pRowFilt(tmp, src, hfiltwidth, fa->dst.w, mult);
				break;
			case 2:
				pRowFilt(tmp, src, hfiltwidth, fa->dst.w, mult);
				pRowFilt(dst, tmp, hfiltwidth, fa->dst.w, mult);
				break;
			case 3:
				pRowFilt(tmp, src, hfiltwidth, fa->dst.w, mult);
				pRowFilt(dst, tmp, hfiltwidth, fa->dst.w, mult);
				pRowFilt(tmp, dst, hfiltwidth, fa->dst.w, mult);
				break;
			}

			src = (uint32 *)((char *)src + fa->src.pitch);
			dst = (uint32 *)((char *)dst + fa->dst.pitch);
			tmp += (fa->dst.w+1)&-2;
		} while(--h);
	} else if (mfd->filter_power & 1){
		VDMemcpyRect(tmp, ((fa->dst.w + 1) & -2)*4, src, fa->src.pitch, fa->src.w * 4, fa->src.h);
	}

	// Vertical filtering.

	if (mfd->filter_power & 1) {
		src = mfd->rows;
		dst = fa->dst.data;
		srcpitch = ((fa->dst.w+1)&-2)*4;
		dstpitch = fa->dst.pitch;
	} else {
		src = fa->dst.data;
		dst = mfd->rows;
		srcpitch = fa->dst.pitch;
		dstpitch = ((fa->dst.w+1)&-2)*4;
	}

	int vfiltwidth = mfd->filter_width;
	if (2*vfiltwidth+1 > fa->dst.h)
		vfiltwidth = (fa->dst.h - 1) >> 1;

	if (vfiltwidth > 0) {
		for(i=0; i<mfd->filter_power; i++) {
			if (i & 1)
				box_do_vertical_pass(src, srcpitch, dst, dstpitch, mfd->trow, fa->dst.w, fa->dst.h, vfiltwidth);
			else
				box_do_vertical_pass(dst, dstpitch, src, srcpitch, mfd->trow, fa->dst.w, fa->dst.h, vfiltwidth);
		}
	} else if (mfd->filter_power & 1) {
		VDMemcpyRect(dst, dstpitch, src, srcpitch, fa->dst.w * 4, fa->dst.h);
	}

#ifdef _M_IX86
	if (MMX_enabled)
		__asm emms
#endif

	return 0;
}


int boxEndProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	free(mfd->rows);	mfd->rows = NULL;
	free(mfd->trow);	mfd->trow = NULL;

	return 0;
}

long boxParamProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXPixmapLayout& pxsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxdst = *fa->dst.mpPixmapLayout;

	if (pxsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxdst.data = pxsrc.data;
	pxdst.pitch = pxsrc.pitch;

	return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}


INT_PTR CALLBACK boxConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	BoxFilterData *mfd = (BoxFilterData *)GetWindowLongPtr(hdlg, DWLP_USER);
	HWND hwndInit;
	char buf[64];

	switch(msg) {
		case WM_INITDIALOG:
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);
			mfd = (BoxFilterData *)lParam;

			hwndInit = GetDlgItem(hdlg, IDC_SLIDER_WIDTH);
			SendMessage(hwndInit, TBM_SETRANGE, TRUE, MAKELONG(1,48));
			SendMessage(hwndInit, TBM_SETPOS, TRUE, mfd->filter_width);

			hwndInit = GetDlgItem(hdlg, IDC_SLIDER_POWER);
			SendMessage(hwndInit, TBM_SETRANGE, TRUE, MAKELONG(1,3));
			SendMessage(hwndInit, TBM_SETPOS, TRUE, mfd->filter_power);

			mfd->ifp->InitButton((VDXHWND)GetDlgItem(hdlg, IDC_PREVIEW));

			// cheat to initialize the labels

			mfd->filter_width = -1;
			mfd->filter_power = -1;

		case WM_HSCROLL:
			{
				static const char *const szPowers[]={ "1 - box", "2 - quadratic", "3 - cubic" };
				int new_width, new_power;

				new_width = SendDlgItemMessage(hdlg, IDC_SLIDER_WIDTH, TBM_GETPOS, 0, 0);
				new_power = SendDlgItemMessage(hdlg, IDC_SLIDER_POWER, TBM_GETPOS, 0, 0);

				if (new_width != mfd->filter_width || new_power != mfd->filter_power) {
					mfd->filter_width = new_width;
					mfd->filter_power = new_power;

					sprintf(buf, "radius %d", mfd->filter_width + mfd->filter_power - 1);
					SetDlgItemText(hdlg, IDC_STATIC_WIDTH, buf);

					SetDlgItemText(hdlg, IDC_STATIC_POWER, szPowers[new_power-1]);

					mfd->ifp->RedoFrame();
				}
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				EndDialog(hdlg, 0);
				return TRUE;
			case IDCANCEL:
				EndDialog(hdlg, 1);
				return TRUE;
			case IDC_PREVIEW:
				mfd->ifp->Toggle((VDXHWND)hdlg);
				return TRUE;
			}
			break;

	}

	return FALSE;
}

int boxConfigProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hwnd) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;
	BoxFilterData mfd_old;
	int res;

	mfd_old = *mfd;
	mfd->ifp = fa->ifp;

	res = DialogBoxParam(g_hInst,
			MAKEINTRESOURCE(IDD_FILTER_BOX), (HWND)hwnd,
			boxConfigDlgProc, (LPARAM)mfd);

	if (res)
		*mfd = mfd_old;

	return res;
}

void boxStringProc2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *str, int maxlen) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	_snprintf(str, maxlen, " (radius %d, power %d)", mfd->filter_width+mfd->filter_power-1, mfd->filter_power);
}

void boxScriptConfig(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
	VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	mfd->filter_width	= argv[0].asInt();
	mfd->filter_power	= argv[1].asInt();
}

bool boxFssProc(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d, %d)",
		mfd->filter_width,
		mfd->filter_power);

	return true;
}


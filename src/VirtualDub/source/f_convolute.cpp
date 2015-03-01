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

#include "stdafx.h"

#include <windows.h>

#include <vd2/system/thunk.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>

#include "resource.h"
#include "filter.h"
#include "dub.h"

#include "disasm.h"

#include "f_convolute.h"

#ifdef VD_CPU_X86
#define USE_ASM
#define USE_ASM_DYNAMIC
#endif

extern "C" void asm_convolute_run(
		void *dst,
		void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride,
		const long *matrix);

extern "C" void asm_dynamic_convolute_run(
		void *dst,
		void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride,
		const long *matrix,
		const void *dyna_func);

extern HINSTANCE g_hInst;

///////////////////////////////////

#define C_TOPOK		(1)
#define C_BOTTOMOK	(2)
#define C_LEFTOK	(4)
#define C_RIGHTOK	(8)

static void inline conv_add(long& rt, long& gt, long& bt, unsigned long dv, long m) {
	bt += m*(0xFFUL & (dv));
	gt += m*(0xFFUL & (dv>>8));
	rt += m*(0xFFUL & (dv>>16));
}

static unsigned long __fastcall do_conv(unsigned long *data, const ConvoluteFilterData *cfd, long sflags, long pit) {
	long rt0=cfd->m[9], gt0=cfd->m[9], bt0=cfd->m[9];
	long rt,gt,bt;

	if (sflags & C_TOPOK) {
		if (sflags & (C_LEFTOK))		conv_add(rt0, gt0, bt0, data[        -1], cfd->m[6]);
		else							conv_add(rt0, gt0, bt0, data[         0], cfd->m[6]);

										conv_add(rt0, gt0, bt0, data[         0], cfd->m[7]);

		if (sflags & (C_RIGHTOK))		conv_add(rt0, gt0, bt0, data[        +1], cfd->m[8]);
		else							conv_add(rt0, gt0, bt0, data[         0], cfd->m[8]);
	} else {
		// top is clipped... push down.

		if (sflags & (C_LEFTOK))		conv_add(rt0, gt0, bt0, data[(pit>>2)-1], cfd->m[6]);
		else							conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[6]);

										conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[7]);

		if (sflags & (C_RIGHTOK))		conv_add(rt0, gt0, bt0, data[(pit>>2)+1], cfd->m[8]);
		else							conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[8]);

	}

	if (sflags & (C_LEFTOK))			conv_add(rt0, gt0, bt0, data[(pit>>2)-1], cfd->m[3]);
	else								conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[3]);

										conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[4]);

	if (sflags & (C_RIGHTOK))			conv_add(rt0, gt0, bt0, data[(pit>>2)+1], cfd->m[5]);
	else								conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[5]);

	if (sflags & C_BOTTOMOK) {
		if (sflags & (C_LEFTOK))		conv_add(rt0, gt0, bt0, data[(pit>>1)-1], cfd->m[0]);
		else							conv_add(rt0, gt0, bt0, data[(pit>>1)  ], cfd->m[0]);

										conv_add(rt0, gt0, bt0, data[(pit>>1)  ], cfd->m[1]);

		if (sflags & (C_RIGHTOK))		conv_add(rt0, gt0, bt0, data[(pit>>1)+1], cfd->m[2]);
		else							conv_add(rt0, gt0, bt0, data[(pit>>1)  ], cfd->m[2]);
	} else {
		// bottom is clipped... push up.

		if (sflags & (C_LEFTOK))		conv_add(rt0, gt0, bt0, data[(pit>>2)-1], cfd->m[0]);
		else							conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[0]);

										conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[1]);

		if (sflags & (C_RIGHTOK))		conv_add(rt0, gt0, bt0, data[(pit>>2)+1], cfd->m[2]);
		else							conv_add(rt0, gt0, bt0, data[(pit>>2)  ], cfd->m[2]);
	}

	rt = rt0>>8;	if (rt<0) rt=0; else if (rt>255) rt=255;
	gt = gt0>>8;	if (gt<0) gt=0; else if (gt>255) gt=255;
	bt = bt0>>8;	if (bt<0) bt=0; else if (bt>255) bt=255;

	return (unsigned long)((rt<<16) | (gt<<8) | (bt));
}

static inline unsigned long do_conv2(unsigned long *data, const long *m, long pit) {
	long rt0=m[9], gt0=m[9], bt0=m[9];
	long rt,gt,bt;

	conv_add(rt0, gt0, bt0, data[        -1], m[6]);
	conv_add(rt0, gt0, bt0, data[         0], m[7]);
	conv_add(rt0, gt0, bt0, data[        +1], m[8]);
	conv_add(rt0, gt0, bt0, data[(pit>>2)-1], m[3]);
	conv_add(rt0, gt0, bt0, data[(pit>>2)  ], m[4]);
	conv_add(rt0, gt0, bt0, data[(pit>>2)+1], m[5]);
	conv_add(rt0, gt0, bt0, data[(pit>>1)-1], m[0]);
	conv_add(rt0, gt0, bt0, data[(pit>>1)  ], m[1]);
	conv_add(rt0, gt0, bt0, data[(pit>>1)+1], m[2]);

	rt = rt0>>8;	if (rt<0) rt=0; else if (rt>255) rt=255;
	gt = gt0>>8;	if (gt<0) gt=0; else if (gt>255) gt=255;
	bt = bt0>>8;	if (bt<0) bt=0; else if (bt>255) bt=255;

	return (unsigned long)((rt<<16) | (gt<<8) | (bt));
}

int filter_convolute_run(const FilterActivation *fa, const FilterFunctions *ff) {
	unsigned long w,h;
	unsigned long *src = (unsigned long *)fa->src.data, *dst = (unsigned long *)fa->dst.data;
	ptrdiff_t pitch = fa->src.pitch;
	const ConvoluteFilterData *cfd = (ConvoluteFilterData *)fa->filter_data;
	const long *const m = cfd->m;

#ifdef USE_ASM
#ifdef USE_ASM_DYNAMIC
	if (((ConvoluteFilterData *)fa->filter_data)->dyna_func)
		asm_dynamic_convolute_run(dst+(fa->dst.pitch>>2)+1, src, fa->src.w-2, fa->src.h-2, fa->dst.pitch, fa->src.pitch, m, ((ConvoluteFilterData *)fa->filter_data)->dyna_func);
	else
#endif
		asm_convolute_run(dst+(fa->dst.pitch>>2)+1, src, fa->src.w-2, fa->src.h-2, fa->dst.pitch, fa->src.pitch, m);
#endif

	src -= pitch>>2;

	*dst++ = do_conv(src++, cfd, C_BOTTOMOK | C_RIGHTOK, pitch);
	w = fa->src.w-2;
	do { *dst++ = do_conv(src++, cfd, C_BOTTOMOK | C_LEFTOK | C_RIGHTOK, pitch); } while(--w);
	*dst++ = do_conv(src++, cfd, C_BOTTOMOK | C_LEFTOK, pitch);

	src += fa->src.modulo>>2;
	dst += fa->dst.modulo>>2;

#ifndef USE_ASM
	h = fa->src.h-2;
	do {
		*dst++ = do_conv(src++, cfd, C_TOPOK | C_BOTTOMOK | C_RIGHTOK, pitch);
		w = fa->src.w-2;
		do { *dst++ = do_conv2(src++, m, pitch); } while(--w);
		*dst++ = do_conv(src++, cfd, C_TOPOK | C_BOTTOMOK | C_LEFTOK, pitch);

		src += fa->src.modulo>>2;
		dst += fa->dst.modulo>>2;
	} while(--h);
#else
	h = fa->src.h-2;
	w = fa->src.w;
	do {
		dst[0] = do_conv(src+0, cfd, C_TOPOK | C_BOTTOMOK | C_RIGHTOK, pitch);
		dst[w-1] = do_conv(src+w-1, cfd, C_TOPOK | C_BOTTOMOK | C_LEFTOK, pitch);

		src += fa->src.pitch>>2;
		dst += fa->dst.pitch>>2;
	} while(--h);
#endif

	*dst++ = do_conv(src++, cfd, C_TOPOK | C_RIGHTOK, pitch);
	w = fa->src.w-2;
	do { *dst++ = do_conv(src++, cfd, C_TOPOK | C_LEFTOK | C_RIGHTOK, pitch); } while(--w);
	*dst++ = do_conv(src++, cfd, C_TOPOK | C_LEFTOK, pitch);

	return 0;
}

long filter_convolute_param(FilterActivation *fa, const FilterFunctions *ff) {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

//////////////////

static int convolute_init(FilterActivation *fa, const FilterFunctions *ff) {
	((ConvoluteFilterData *)fa->filter_data)->m[4] = 256;
	((ConvoluteFilterData *)fa->filter_data)->bias = 128;
	((ConvoluteFilterData *)fa->filter_data)->dyna_func = NULL;

	return 0;
}

static INT_PTR CALLBACK convoluteDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	int i;

    switch (message)
    {
        case WM_INITDIALOG:
			{
				struct ConvoluteFilterData *cfd = (ConvoluteFilterData *)lParam;

				CheckDlgButton(hDlg, IDC_ENABLE_CLIPPING, cfd->fClip);
				SetDlgItemInt(hDlg, IDC_BIAS, (cfd->bias-128)>>8, TRUE);

				for(i=0; i<9; i++)
					SetDlgItemInt(hDlg, IDC_MATRIX_1+i, cfd->m[i], TRUE);

				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)cfd);
			}
            return (TRUE);

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK) {
				struct ConvoluteFilterData *cfd = (struct ConvoluteFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

				cfd->fClip = !!IsDlgButtonChecked(hDlg, IDC_ENABLE_CLIPPING);
				cfd->bias = GetDlgItemInt(hDlg, IDC_BIAS, NULL, TRUE)*256 + 128;

				for(i=0; i<9; i++)
					cfd->m[i] = GetDlgItemInt(hDlg, IDC_MATRIX_1+i, NULL, TRUE);

				EndDialog(hDlg, 0);
				return TRUE;
			} else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, 1);  
                return TRUE;
            }
            break;
    }
    return FALSE;
}

static int convolute_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	return DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_CONVOLUTE), (HWND)hWnd, convoluteDlgProc, (LPARAM)fa->filter_data);
}

static void convolute_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;

	ConvoluteFilterData *cfd = (ConvoluteFilterData *)fa->filter_data;

	for(int i=0; i<9; i++) {
		cfd->m[i] = argv[i].asInt();
	}

	cfd->bias = argv[9].asInt();
	cfd->fClip = !!argv[10].asInt();
}

static ScriptFunctionDef convolute_func_defs[]={
	{ (ScriptFunctionPtr)convolute_script_config, "Config", "0iiiiiiiiiii" },
	{ NULL },
};

static CScriptObject convolute_obj={
	NULL, convolute_func_defs
};

static bool convolute_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	ConvoluteFilterData *cfd = (ConvoluteFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)"
			,cfd->m[0]
			,cfd->m[1]
			,cfd->m[2]
			,cfd->m[3]
			,cfd->m[4]
			,cfd->m[5]
			,cfd->m[6]
			,cfd->m[7]
			,cfd->m[8]
			,cfd->bias
			,cfd->fClip?1:0
			);

	return true;
}

///////////////////////////////////

extern DubOptions g_dubOpts;

#define REG_EAX		(0)
#define REG_ECX		(1)
#define REG_EDX		(2)
#define REG_EBX		(3)
#define REG_ESP		(4)
#define REG_EBP		(5)
#define REG_ESI		(6)
#define REG_EDI		(7)

char *DCG_emit_MOV(char *ptr, int dest, int src) {
	ptr[0] = (char)0x8b;
	ptr[1] = (char)(0xc0 + (dest<<3) + src);

	return ptr+2;
}

char *DCG_emit_MOV_from_indexed(char *ptr, int dest, int src, long disp32) {
	ptr[0] = (char)0x8b;
	if (src != REG_ESP) {
		if (src != REG_EBP && !disp32) {
			ptr[1] = (char)((dest<<3) + src);
			return ptr+2;
		}
		if (disp32>=-128 && disp32<128) {
			ptr[1] = (char)(0x40 + (dest<<3) + src);
			ptr[2] = (char)(disp32 & 0xff);
			return ptr+3;
		}
		ptr[1] = (char)(0x80 + (dest<<3) + src);
		*(long *)(ptr+2) = disp32;
		return ptr+6;
	}

	ptr[2] = 0x24;
	if (!disp32) {
		ptr[1] = (char)(0x00 + (dest<<3));
		return ptr+3;
	}
	if (disp32>=-128 && disp32<128) {
		ptr[1] = (char)(0x40 + (dest<<3));
		ptr[3] = 0;
		return ptr+4;
	}
	ptr[1] = (char)(0x80 + (dest<<3));
	*(long *)(ptr+3) = 0;
	return ptr+7;
}

char *DCG_emit_ADD(char *ptr, int dest, int src) {
	ptr[0] = 0x03;
	ptr[1] = (char)(0xc0 + (dest<<3) + src);

	return ptr+2;
}

char *DCG_emit_SUB(char *ptr, int dest, int src) {
	ptr[0] = 0x2B;
	ptr[1] = (char)(0xc0 + (dest<<3) + src);

	return ptr+2;
}

char *DCG_emit_SHL(char *ptr, int dest, int bits) {
	if (bits==1) {
		ptr[0] = (char)0xd1;
		ptr[1] = (char)(0xe0 + dest);

		return ptr+2;
	}
	ptr[0] = (char)0xc1;
	ptr[1] = (char)(0xe0 + dest);
	ptr[2] = (char)bits;

	return ptr+3;
}

char *DCG_emit_SHR(char *ptr, int dest, int bits) {
	if (bits==1) {
		ptr[0] = (char)0xd1;
		ptr[1] = (char)(0xe8 + dest);

		return ptr+2;
	}
	ptr[0] = (char)0xc1;
	ptr[1] = (char)(0xe8 + dest);
	ptr[2] = (char)bits;

	return ptr+3;
}

char *DCG_emit_AND_immediate(char *ptr, int dest, long imm32) {
	ptr[0] = (char)0x81;
	ptr[1] = (char)(0xe0 + dest);
	*(long *)(ptr+2) = imm32;

	return ptr+6;
}

// LEA dest,[src1*scale] (scale = 0 to 3)

char *DCG_emit_LEA(char *ptr, int dest, int src1, int scale) {
	ptr[0] = (char)0x8d;
	if (!scale && src1!=REG_EBP && src1!=REG_ESP) {
		ptr[1] = (char)((dest<<3) | src1);
		return ptr+2;
	}
	ptr[1] = (char)(0x04 | (dest<<3));
	ptr[2] = (char)((scale<<6) | (src1<<3) | 0x05);
	*(long *)(ptr+3)=0;

	return ptr+7;
}

// LEA dest,[src2 + src1*scale]

char *DCG_emit_LEA(char *ptr, int dest, int src1, int scale, int src2) {
	ptr[0] = (char)0x8d;
	if (src2 == REG_EBP) {
		ptr[1] = (char)(0x44 | (dest<<3));
		ptr[2] = (char)((scale<<6) | (src1<<3) | src2);
		ptr[3] = 0;
		return ptr+4;
	}
	ptr[1] = (char)(0x04 | (dest<<3));
	ptr[2] = (char)((scale<<6) | (src1<<3) | src2);

	return ptr+3;
}

enum {
	CODE_LEA1	= 0,	// 2 dest * 2 src1 * 2 src2 * 4 multipliers = 32 ops
	CODE_LEA2	= 32,	// 2 dest * 2 src1 * 4 multipliers = 16 ops
	CODE_SHL	= 48,	// 2 dest * 16 shifts = 32 ops
	CODE_SUB	= 80,	// 2 dest * 2 src = 4 ops
	CODE_MAX	= 84
};

// src2(16), multiplier(4), src1(2), dest(1)

char *DCG_emit_mult_op(char *ptr, int reg1, int reg2, char op) {
	if (op < CODE_LEA2) {
		// special cases:
		//		add - scale=1, src1=dest or src2=dest

		if (!(op&12) && !((op ^ (op>>1))&1))
			return DCG_emit_ADD(ptr, op&1 ? reg2 : reg1, op&16 ? reg2 : reg1);
		else if (!(op&12) && !((op ^ (op>>4))&1))
			return DCG_emit_ADD(ptr, op&1 ? reg2 : reg1, op&2 ? reg2 : reg1);
		else
			return DCG_emit_LEA(
					ptr,
					op&1 ? reg2 : reg1,
					op&2 ? reg2 : reg1,
					(op>>2) & 3,
					op&16 ? reg2 : reg1);
	} else if (op < CODE_SHL) {
		// special cases:
		//		mov - scale=1

		if (!((op>>2)&3))
			return DCG_emit_MOV(ptr, op&1 ? reg2 : reg1, op&2 ? reg2 : reg1);
		else
			return DCG_emit_LEA(
					ptr,
					op&1 ? reg2 : reg1,
					op&2 ? reg2 : reg1,
					(op>>2) & 3);
	} else if (op < CODE_SUB) {
		// special cases:
		//		add - shift=1

		if (((op-CODE_SHL)>>1)==1)
			return DCG_emit_ADD(ptr, op&1?reg2:reg1, op&1?reg2:reg1);
		else
			return DCG_emit_SHL(ptr, op&1?reg2:reg1, (op - CODE_SHL)>>1);
	} else {
		return DCG_emit_SUB(ptr, op&1?reg2:reg1, op&2?reg2:reg1);
	}
}

extern "C" char *asm_dynamic_convolute_codecopy(char *, int);
extern "C" const char *const asm_const_multiply_tab[2049];

int filter_convolute_start(FilterActivation *fa, const FilterFunctions *ff) {
#ifdef USE_ASM_DYNAMIC
	ConvoluteFilterData *cfd = (ConvoluteFilterData *)fa->filter_data;
	char *pptr,c;
	const char *pcode;
	int x, y;
	long m;

	///////////////

	if (!g_dubOpts.perf.dynamicEnable) return 0;

	if (!(cfd->dyna_func = VDAllocateThunkMemory(4096))) return 1;

	pptr = (char *)cfd->dyna_func;

	pptr = asm_dynamic_convolute_codecopy(pptr, 0);

	for(y=0; y<3; y++)
		for(x=0; x<3; x++) {
			m = cfd->m[(2-y)*3+x];
			if (!m) continue;

			// MOV eax,[esi+disp32]
			// SHR eax,(0/8/16)
			// AND eax,000000ffh
			// (generate multiplication series)

			// red=EBP, green=EDI, blue=EDX

			// If the coefficient is 2, 4, or 8, generate a compact LEA sequence

			if (m==2 || m==4 || m==8) {
				int scale = (m>>2)+1;

				// v MOV eax,[esi+disp32]
				// u MOV ebx,eax
				// v AND eax,000000ffh			eax: ready
				// u MOV ecx,ebx
				// v AND ebx,0000ff00h
				// u SHR ebx,8					ebx: ready
				// v AND ecx,00ff0000h
				// u SHR ecx,16
				// v LEA edx,[edx+eax*mult]
				// u LEA edi,[edi+ebx*mult]
				// v LEA ebp,[ebp+ecx*mult]		;AGI (arrgh!!!)

				pptr = DCG_emit_MOV_from_indexed(pptr, REG_EAX, REG_ESI, fa->src.pitch*y + 4*x - 4);
				pptr = DCG_emit_MOV(pptr, REG_EBX, REG_EAX);
				pptr = DCG_emit_AND_immediate(pptr, REG_EAX, 0x000000ff);
				pptr = DCG_emit_MOV(pptr, REG_ECX, REG_EBX);
				pptr = DCG_emit_AND_immediate(pptr, REG_EBX, 0x0000ff00);
				pptr = DCG_emit_SHR(pptr, REG_EBX, 8);
				pptr = DCG_emit_AND_immediate(pptr, REG_ECX, 0x00ff0000);
				pptr = DCG_emit_SHR(pptr, REG_ECX, 16);
				pptr = DCG_emit_LEA(pptr, REG_EDX, REG_EAX, scale, REG_EDX);
				pptr = DCG_emit_LEA(pptr, REG_EDI, REG_EBX, scale, REG_EDI);
				pptr = DCG_emit_LEA(pptr, REG_EBP, REG_ECX, scale, REG_EBP);

			// If the coefficient is <=256, then generate red and blue simultaneously

			} else if (abs(m)<=256) {

				pptr = DCG_emit_MOV_from_indexed(pptr, REG_EAX, REG_ESI, fa->src.pitch*y + 4*x - 4);
				pptr = DCG_emit_AND_immediate(pptr, REG_EAX, 0x00ff00ff);

				pcode = asm_const_multiply_tab[abs(m)];
				while((c=*pcode++) != (char)0xff) {
					pptr = DCG_emit_mult_op(pptr, REG_EAX, REG_EBX, c);
				}

				pptr = DCG_emit_MOV(pptr, REG_EBX, REG_EAX);

				pptr = DCG_emit_SHR(pptr, REG_EAX, 16);
				pptr = DCG_emit_AND_immediate(pptr, REG_EBX, 0x0000ffff);

				if (m<0)	pptr = DCG_emit_SUB(pptr, REG_EBP, REG_EAX);
				else		pptr = DCG_emit_ADD(pptr, REG_EBP, REG_EAX);

				if (m<0)	pptr = DCG_emit_SUB(pptr, REG_EDX, REG_EBX);
				else		pptr = DCG_emit_ADD(pptr, REG_EDX, REG_EBX);

			} else {

				// red

				pptr = DCG_emit_MOV_from_indexed(pptr, REG_EAX, REG_ESI, fa->src.pitch*y + 4*x - 4);
				pptr = DCG_emit_SHR(pptr, REG_EAX, 16);
				pptr = DCG_emit_AND_immediate(pptr, REG_EAX, 0x000000ff);

				pcode = asm_const_multiply_tab[abs(m)];
				while((c=*pcode++) != (char)0xff) {
					pptr = DCG_emit_mult_op(pptr, REG_EAX, REG_EBX, c);
				}

				if (m<0)	pptr = DCG_emit_SUB(pptr, REG_EBP, REG_EAX);
				else		pptr = DCG_emit_ADD(pptr, REG_EBP, REG_EAX);

				// blue

				pptr = DCG_emit_MOV_from_indexed(pptr, REG_EAX, REG_ESI, fa->src.pitch*y + 4*x - 4);
				pptr = DCG_emit_AND_immediate(pptr, REG_EAX, 0x000000ff);

				pcode = asm_const_multiply_tab[abs(m)];
				while((c=*pcode++) != (char)0xff)
					pptr = DCG_emit_mult_op(pptr, REG_EAX, REG_EBX, c);

				if (m<0)	pptr = DCG_emit_SUB(pptr, REG_EDX, REG_EAX);
				else		pptr = DCG_emit_ADD(pptr, REG_EDX, REG_EAX);
			}

			// green

			pptr = DCG_emit_MOV_from_indexed(pptr, REG_EAX, REG_ESI, fa->src.pitch*y + 4*x - 4);
			pptr = DCG_emit_AND_immediate(pptr, REG_EAX, 0x0000ff00);

			pcode = asm_const_multiply_tab[abs(m)];
			while((c=*pcode++) != (char)0xff)
				pptr = DCG_emit_mult_op(pptr, REG_EAX, REG_EBX, c);

			if (m<0)	pptr = DCG_emit_SUB(pptr, REG_EDI, REG_EAX);
			else		pptr = DCG_emit_ADD(pptr, REG_EDI, REG_EAX);
		}

	if (cfd->bias)
		pptr = asm_dynamic_convolute_codecopy(pptr, 1);

	pptr = asm_dynamic_convolute_codecopy(pptr, 2);

	pptr[0] = 0x0f;
	pptr[1] = (char)0x85;
	*(long *)(pptr+2) = (char *)cfd->dyna_func - (pptr+6);
	pptr[6] = (char)0xc3;	// RET

	cfd->dyna_size = (pptr+7) - (char *)cfd->dyna_func;

	//////

	if (g_dubOpts.perf.dynamicShowDisassembly) {
		CodeDisassemblyWindow cdw(cfd->dyna_func, cfd->dyna_size, cfd->dyna_func, cfd->dyna_func);

		cdw.parse();
		cdw.post(NULL);
	}
#endif

	return 0;
}

///////////////////////////////////

int filter_convolute_end(FilterActivation *fa, const FilterFunctions *ff) {
	ConvoluteFilterData *cfd = (ConvoluteFilterData *)fa->filter_data;

	if (cfd->dyna_func) {
		VDFreeThunkMemory(cfd->dyna_func, cfd->dyna_size);
		cfd->dyna_func = NULL;
	}

	return 0;
}

FilterDefinition filterDef_convolute={
	0,0,NULL,
	"general convolution",
	"Applies a general 3x3 convolution matrix to a pixel that depends on the pixel's value and the eight neighboring pixels around it.\n\n"
		"[Assembly optimized] [Dynamic compilation]",
	NULL,NULL,
	sizeof(ConvoluteFilterData),
	convolute_init,
	NULL,
	filter_convolute_run,
	filter_convolute_param,
	convolute_config,
	NULL,
	filter_convolute_start,
	filter_convolute_end,
	&convolute_obj,
	convolute_script_line,
};
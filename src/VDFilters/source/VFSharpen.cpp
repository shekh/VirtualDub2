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

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include "SingleValueDialog.h"

#ifdef VD_CPU_X86
#define USE_ASM
#endif

#ifdef USE_ASM
extern "C" void asm_sharpen_run(
		void *dst,
		void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride,
		long a_mult,
		long b_mult);
#endif

#define C_TOPOK		(1)
#define C_BOTTOMOK	(2)
#define C_LEFTOK	(4)
#define C_RIGHTOK	(8)

void inline conv_add(long& rt, long& gt, long& bt, unsigned long dv, long m) {
	bt += m*(0xFF & (dv));
	gt += m*(0xFF & (dv>>8));
	rt += m*(0xFF & (dv>>16));
}

void inline conv_add2(long& rt, long& gt, long& bt, unsigned long dv) {
	bt += 0xFF & (dv);
	gt += 0xFF & (dv>>8);
	rt += 0xFF & (dv>>16);
}

static unsigned long __fastcall do_conv(unsigned long *data, sint32 *m, long sflags, long pit) {
	long rt=0, gt=0, bt=0;

	if (sflags & C_TOPOK) {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[        -1]);
		else							conv_add2(rt, gt, bt, data[         0]);
										conv_add2(rt, gt, bt, data[         0]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[        +1]);
		else							conv_add2(rt, gt, bt, data[         0]);
	} else {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>2)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
										conv_add2(rt, gt, bt, data[(pit>>2)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>2)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	}
	if (sflags & (C_LEFTOK))			conv_add2(rt, gt, bt, data[(pit>>2)-1]);
	else								conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	if (sflags & (C_RIGHTOK))			conv_add2(rt, gt, bt, data[(pit>>2)+1]);
	else								conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	if (sflags & C_BOTTOMOK) {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>1)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>1)  ]);
										conv_add2(rt, gt, bt, data[(pit>>1)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>1)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>1)  ]);
	} else {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>2)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
										conv_add2(rt, gt, bt, data[(pit>>2)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>2)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	}

	rt = rt*m[0]+m[9];
	gt = gt*m[0]+m[9];
	bt = bt*m[0]+m[9];

	conv_add(rt, gt, bt, data[(pit>>2)  ],m[4]);

	rt>>=8;	if (rt<0) rt=0; else if (rt>255) rt=255;
	gt>>=8;	if (gt<0) gt=0; else if (gt>255) gt=255;
	bt>>=8;	if (bt<0) bt=0; else if (bt>255) bt=255;

	return (unsigned long)((rt<<16) | (gt<<8) | (bt));
}

#ifndef USE_ASM
static inline unsigned long do_conv2(unsigned long *data, sint32 *m, long pit) {
	long rt=0, gt=0, bt=0;

	conv_add2(rt, gt, bt, data[        -1]);
	conv_add2(rt, gt, bt, data[         0]);
	conv_add2(rt, gt, bt, data[        +1]);
	conv_add2(rt, gt, bt, data[(pit>>2)-1]);
	conv_add2(rt, gt, bt, data[(pit>>2)+1]);
	conv_add2(rt, gt, bt, data[(pit>>1)-1]);
	conv_add2(rt, gt, bt, data[(pit>>1)  ]);
	conv_add2(rt, gt, bt, data[(pit>>1)+1]);
	rt = rt*m[0]+m[9];
	gt = gt*m[0]+m[9];
	bt = bt*m[0]+m[9];

	conv_add(rt, gt, bt, data[(pit>>2)  ], m[4]);

	rt>>=8;	if (rt<0) rt=0; else if (rt>255) rt=255;
	gt>>=8;	if (gt<0) gt=0; else if (gt>255) gt=255;
	bt>>=8;	if (bt<0) bt=0; else if (bt>255) bt=255;

	return (unsigned long)((rt<<16) | (gt<<8) | (bt));
}
#endif

class VDVFSharpen : public VDXVideoFilter {
public:
	VDVFSharpen();

	virtual uint32 GetParams();
	virtual void Run();
	virtual bool Configure(VDXHWND hwnd);
	virtual void GetSettingString(char *buf, int maxlen);
	virtual void GetScriptString(char *buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();

	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

protected:
	static void Update(long value, void *pThis);

	sint32 mMatrix[9];
	sint32 mBias;
};

VDVFSharpen::VDVFSharpen() {
	Update(16, this);
}

uint32 VDVFSharpen::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.pitch = 0;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVFSharpen::Run() {
	unsigned long w,h;
	unsigned long *src = (unsigned long *)fa->src.data, *dst = (unsigned long *)fa->dst.data;
	ptrdiff_t pitch = fa->src.pitch;
	sint32 *m = mMatrix;

	src -= pitch>>2;

	*dst++ = do_conv(src++, m, C_BOTTOMOK | C_RIGHTOK, pitch);
	w = fa->src.w-2;
	do { *dst++ = do_conv(src++, m, C_BOTTOMOK | C_LEFTOK | C_RIGHTOK, pitch); } while(--w);
	*dst++ = do_conv(src++, m, C_BOTTOMOK | C_LEFTOK, pitch);

	src += fa->src.modulo>>2;
	dst += fa->dst.modulo>>2;

#ifdef USE_ASM
	asm_sharpen_run(
			dst+1,
			src+1,
			fa->src.w-2,
			fa->src.h-2,
			fa->src.pitch,
			fa->dst.pitch,
			m[0],
			m[4]);
#endif

	h = fa->src.h-2;
	do {
		*dst++ = do_conv(src++, m, C_TOPOK | C_BOTTOMOK | C_RIGHTOK, pitch);
#ifdef USE_ASM
		src += fa->src.w-2;
		dst += fa->src.w-2;
#else
		w = fa->src.w-2;
		do { *dst++ = do_conv2(src++, m, pitch); } while(--w);
#endif
		*dst++ = do_conv(src++, m, C_TOPOK | C_BOTTOMOK | C_LEFTOK, pitch);

		src += fa->src.modulo>>2;
		dst += fa->dst.modulo>>2;
	} while(--h);

	*dst++ = do_conv(src++, m, C_TOPOK | C_RIGHTOK, pitch);
	w = fa->src.w-2;
	do { *dst++ = do_conv(src++, m, C_TOPOK | C_LEFTOK | C_RIGHTOK, pitch); } while(--w);
	*dst++ = do_conv(src++, m, C_TOPOK | C_LEFTOK, pitch);
}

void VDVFSharpen::Update(long value, void *pvThis) {
	VDVFSharpen *pThis = (VDVFSharpen *)pvThis;

	for(int i=0; i<9; i++) {
		if (i == 4)
			pThis->mMatrix[4] = 256+8*value;
		else
			pThis->mMatrix[i] = -value;
	}

	pThis->mBias = -value*4;
}

bool VDVFSharpen::Configure(VDXHWND hwnd) {
	if (!hwnd)
		return true;

	sint32 lv;

	if (!VDFilterGetSingleValue(hwnd, -mMatrix[0], &lv, 0, 64, "sharpen", fa->ifp2, Update, this)) {
		Update(lv, this);
		return false;
	}

	Update(lv, this);
	return true;
}

void VDVFSharpen::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (by %ld)", -mBias/4);
}

void VDVFSharpen::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d)", -mBias/4);
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFSharpen)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVFSharpen, ScriptConfig, "i")
VDXVF_END_SCRIPT_METHODS()

void VDVFSharpen::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	int lv = argv[0].asInt();

	for(int i=0; i<9; i++) {
		if (i==4)
			mMatrix[4] = 256+8*lv;
		else
			mMatrix[i]=-lv;
	}

	mBias = -lv*4;
}

extern const VDXFilterDefinition g_VDVFSharpen = VDXVideoFilterDefinition<VDVFSharpen>(
	NULL,
	"sharpen",
	"Enhances contrast between adjacent elements in an image.\n\n[Assembly optimized] [MMX optimized]");

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

#include <vd2/system/vdtypes.h>
#include "ScriptInterpreter.h"
#include "ScriptError.h"

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/Kasumi/pixel.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/resample.h>
#include <vd2/Kasumi/resample_kernels.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/plugin/vdvideoaccel.h>
#include "resource.h"
#include "filter.h"
#include "vbitmap.h"
#include "f_resize_config.h"

#include "f_resize.inl"

///////////////////////////////////////////////////////////////////////////////

namespace {
	void VDPixmapRectFillPlane8(void *dst, ptrdiff_t dstpitch, int x, int y, int w, int h, uint8 c) {
		VDMemset8Rect(vdptroffset(dst, dstpitch*y + x), dstpitch, c, w, h);
	}

	void VDPixmapRectFillPlane16(void *dst, ptrdiff_t dstpitch, int x, int y, int w, int h, uint16 c) {
		VDMemset16Rect(vdptroffset(dst, dstpitch*y + x*2), dstpitch, c, w, h);
	}

	void VDPixmapRectFillPlane24(void *dst, ptrdiff_t dstpitch, int x, int y, int w, int h, uint32 c) {
		VDMemset24Rect(vdptroffset(dst, dstpitch*y + x*4), dstpitch, c, w, h);
	}

	void VDPixmapRectFillPlane32(void *dst, ptrdiff_t dstpitch, int x, int y, int w, int h, uint32 c) {
		VDMemset32Rect(vdptroffset(dst, dstpitch*y + x*4), dstpitch, c, w, h);
	}

	void VDPixmapRectFillPlane64(void *dst, ptrdiff_t dstpitch, int x, int y, int w, int h, uint64 c) {
		VDMemset64Rect(vdptroffset(dst, dstpitch*y + x*8), dstpitch, c, w, h);
	}

	void VDPixmapRectFillRaw(const VDPixmap& px, const vdrect32f& r0, uint32 c) {
		using namespace nsVDPixmap;

		vdrect32f r(r0);

		if (r.left < 0.0f)
			r.left = 0.0f;
		if (r.top < 0.0f)
			r.top = 0.0f;
		if (r.right > (float)px.w)
			r.right = (float)px.w;
		if (r.bottom > (float)px.h)
			r.bottom = (float)px.h;

		if (r.right < r.left || r.bottom < r.top)
			return;

		int ix = VDCeilToInt(r.left		- 0.5f);
		int iy = VDCeilToInt(r.top		- 0.5f);
		int iw = VDCeilToInt(r.right	- 0.5f) - ix;
		int ih = VDCeilToInt(r.bottom	- 0.5f) - iy;

		int c0 = (c >> 8) & 0xFF;
		int c1 = c & 0xFF;
		int c2 = (c >> 16) & 0xFF;

		switch(px.format) {
		case kPixFormat_YUV420_Planar16:
		case kPixFormat_YUV422_Planar16:
		case kPixFormat_YUV444_Planar16:
		case kPixFormat_YUV420_Alpha_Planar16:
		case kPixFormat_YUV422_Alpha_Planar16:
		case kPixFormat_YUV444_Alpha_Planar16:
			VDRoundYCbCr(c0/255.0f,c1/255.0f,c2/255.0f,px.info.ref_r,c0,c1,c2);
			break;
		}

		if(px.info.alpha_type) switch(px.format) {
		case kPixFormat_YUV444_Alpha_Planar16:
		case kPixFormat_YUV422_Alpha_Planar16:
		case kPixFormat_YUV420_Alpha_Planar16:
			VDPixmapRectFillPlane16(px.data, px.pitch, ix, iy, iw, ih, 0);
			break;
		case kPixFormat_YUV444_Alpha_Planar:
		case kPixFormat_YUV422_Alpha_Planar:
		case kPixFormat_YUV420_Alpha_Planar:
			VDPixmapRectFillPlane8(px.data, px.pitch, ix, iy, iw, ih, 0);
			break;
		}

		switch(px.format) {
		case kPixFormat_Pal1:
		case kPixFormat_Pal2:
		case kPixFormat_Pal4:
		case kPixFormat_Pal8:
		case kPixFormat_Y8:
		case kPixFormat_Y8_FR:
			VDPixmapRectFillPlane8(px.data, px.pitch, ix, iy, iw, ih, (uint8)c);
			break;
		case kPixFormat_XRGB1555:
		case kPixFormat_RGB565:
			VDPixmapRectFillPlane16(px.data, px.pitch, ix, iy, iw, ih, (uint16)c);
			break;
		case kPixFormat_RGB888:
			VDPixmapRectFillPlane24(px.data, px.pitch, ix, iy, iw, ih, c);
			break;
		case kPixFormat_XRGB8888:
			VDPixmapRectFillPlane32(px.data, px.pitch, ix, iy, iw, ih, c);
			break;
		case kPixFormat_XRGB64:
			{
				int b = c & 0xFF;
				int g = (c>>8) & 0xFF;
				int r = (c>>16) & 0xFF;
				int a = (c>>24) & 0xFF;
				uint64 c64 = b*0x101ul | g*0x1010000ul | r*0x10100000000ul | a*0x101000000000000ul;
				VDPixmapRectFillPlane64(px.data, px.pitch, ix, iy, iw, ih, c64);
			}
			break;
		case kPixFormat_YUV444_Planar:
		case kPixFormat_YUV444_Planar_FR:
		case kPixFormat_YUV444_Planar_709:
		case kPixFormat_YUV444_Planar_709_FR:
		case kPixFormat_YUV444_Alpha_Planar:
			VDPixmapRectFillPlane8(px.data, px.pitch, ix, iy, iw, ih, (uint8)(c >> 8));
			VDPixmapRectFillPlane8(px.data2, px.pitch2, ix, iy, iw, ih, (uint8)c);
			VDPixmapRectFillPlane8(px.data3, px.pitch3, ix, iy, iw, ih, (uint8)(c >> 16));
			break;
		case kPixFormat_YUV422_Planar:
		case kPixFormat_YUV422_Planar_FR:
		case kPixFormat_YUV422_Planar_709:
		case kPixFormat_YUV422_Planar_709_FR:
		case kPixFormat_YUV422_Alpha_Planar:
			{
				int isubx = VDCeilToInt(r.left		* 0.5f - 0.25f);
				int isuby = VDCeilToInt(r.top		* 1.0f - 0.5f );
				int isubw = VDCeilToInt(r.right		* 0.5f - 0.25f) - isubx;
				int isubh = VDCeilToInt(r.bottom	* 1.0f - 0.5f ) - isuby;

				VDPixmapRectFillPlane8(px.data, px.pitch, ix, iy, iw, ih, (uint8)(c >> 8));
				VDPixmapRectFillPlane8(px.data2, px.pitch2, isubx, isuby, isubw, isubh, (uint8)c);
				VDPixmapRectFillPlane8(px.data3, px.pitch3, isubx, isuby, isubw, isubh, (uint8)(c >> 16));
			}
			break;
		case kPixFormat_YUV420_Planar:
		case kPixFormat_YUV420_Planar_FR:
		case kPixFormat_YUV420_Planar_709:
		case kPixFormat_YUV420_Planar_709_FR:
		case kPixFormat_YUV420_Alpha_Planar:
			{
				int isubx = VDCeilToInt(r.left		* 0.5f - 0.25f);
				int isuby = VDCeilToInt(r.top		* 0.5f - 0.5f );
				int isubw = VDCeilToInt(r.right		* 0.5f - 0.25f) - isubx;
				int isubh = VDCeilToInt(r.bottom	* 0.5f - 0.5f ) - isuby;

				VDPixmapRectFillPlane8(px.data, px.pitch, ix, iy, iw, ih, (uint8)(c >> 8));
				VDPixmapRectFillPlane8(px.data2, px.pitch2, isubx, isuby, isubw, isubh, (uint8)c);
				VDPixmapRectFillPlane8(px.data3, px.pitch3, isubx, isuby, isubw, isubh, (uint8)(c >> 16));
			}
			break;
		case kPixFormat_YUV411_Planar:
		case kPixFormat_YUV411_Planar_FR:
		case kPixFormat_YUV411_Planar_709:
		case kPixFormat_YUV411_Planar_709_FR:
			{
				int isubx = VDCeilToInt(r.left		* 0.25f - 0.125f);
				int isuby = VDCeilToInt(r.top		* 1.00f - 0.500f);
				int isubw = VDCeilToInt(r.right		* 0.25f - 0.125f) - isubx;
				int isubh = VDCeilToInt(r.bottom	* 1.00f - 0.500f) - isuby;

				VDPixmapRectFillPlane8(px.data, px.pitch, ix, iy, iw, ih, (uint8)(c >> 8));
				VDPixmapRectFillPlane8(px.data2, px.pitch2, isubx, isuby, isubw, isubh, (uint8)c);
				VDPixmapRectFillPlane8(px.data3, px.pitch3, isubx, isuby, isubw, isubh, (uint8)(c >> 16));
			}
			break;
		case kPixFormat_YUV410_Planar:
		case kPixFormat_YUV410_Planar_FR:
		case kPixFormat_YUV410_Planar_709:
		case kPixFormat_YUV410_Planar_709_FR:
			{
				int isubx = VDCeilToInt(r.left		* 0.25f - 0.5f);
				int isuby = VDCeilToInt(r.top		* 0.25f - 0.5f);
				int isubw = VDCeilToInt(r.right		* 0.25f - 0.5f) - isubx;
				int isubh = VDCeilToInt(r.bottom	* 0.25f - 0.5f) - isuby;

				VDPixmapRectFillPlane8(px.data, px.pitch, ix, iy, iw, ih, (uint8)(c >> 8));
				VDPixmapRectFillPlane8(px.data2, px.pitch2, isubx, isuby, isubw, isubh, (uint8)c);
				VDPixmapRectFillPlane8(px.data3, px.pitch3, isubx, isuby, isubw, isubh, (uint8)(c >> 16));
			}
			break;
		case kPixFormat_YUV444_Planar16:
		case kPixFormat_YUV444_Alpha_Planar16:
			VDPixmapRectFillPlane16(px.data, px.pitch, ix, iy, iw, ih, (uint16)c0);
			VDPixmapRectFillPlane16(px.data2, px.pitch2, ix, iy, iw, ih, (uint16)c1);
			VDPixmapRectFillPlane16(px.data3, px.pitch3, ix, iy, iw, ih, (uint16)c2);
			break;
		case kPixFormat_YUV422_Planar16:
		case kPixFormat_YUV422_Alpha_Planar16:
			{
				int isubx = VDCeilToInt(r.left		* 0.5f - 0.25f);
				int isuby = VDCeilToInt(r.top		* 1.0f - 0.5f );
				int isubw = VDCeilToInt(r.right		* 0.5f - 0.25f) - isubx;
				int isubh = VDCeilToInt(r.bottom	* 1.0f - 0.5f ) - isuby;

				VDPixmapRectFillPlane16(px.data, px.pitch, ix, iy, iw, ih, (uint16)c0);
				VDPixmapRectFillPlane16(px.data2, px.pitch2, isubx, isuby, isubw, isubh, (uint16)c1);
				VDPixmapRectFillPlane16(px.data3, px.pitch3, isubx, isuby, isubw, isubh, (uint16)c2);
			}
			break;
		case kPixFormat_YUV420_Planar16:
		case kPixFormat_YUV420_Alpha_Planar16:
			{
				int isubx = VDCeilToInt(r.left		* 0.5f - 0.25f);
				int isuby = VDCeilToInt(r.top		* 0.5f - 0.5f );
				int isubw = VDCeilToInt(r.right		* 0.5f - 0.25f) - isubx;
				int isubh = VDCeilToInt(r.bottom	* 0.5f - 0.5f ) - isuby;

				VDPixmapRectFillPlane16(px.data, px.pitch, ix, iy, iw, ih, (uint16)c0);
				VDPixmapRectFillPlane16(px.data2, px.pitch2, isubx, isuby, isubw, isubh, (uint16)c1);
				VDPixmapRectFillPlane16(px.data3, px.pitch3, isubx, isuby, isubw, isubh, (uint16)c2);
			}
			break;
		}
	}

	void VDPixmapRectFillRGB32(const VDPixmap& px, const vdrect32f& rDst, uint32 c) {
		using namespace nsVDPixmap;

		switch(px.format) {
		case kPixFormat_Pal1:
		case kPixFormat_Pal2:
		case kPixFormat_Pal4:
		case kPixFormat_Pal8:
			VDASSERT(false);
			break;

		case kPixFormat_XRGB1555:
			VDPixmapRectFillRaw(px, rDst, ((c & 0xf80000) >> 9) + ((c & 0xf800) >> 6) + ((c & 0xf8) >> 3));
			break;

		case kPixFormat_RGB565:
			VDPixmapRectFillRaw(px, rDst, ((c & 0xf80000) >> 8) + ((c & 0xfc00) >> 5) + ((c & 0xf8) >> 3));
			break;

		case kPixFormat_RGB888:
		case kPixFormat_XRGB8888:
		case kPixFormat_XRGB64:
			VDPixmapRectFillRaw(px, rDst, c);
			break;

		case kPixFormat_Y8:
			VDPixmapRectFillRaw(px, rDst, (VDConvertRGBToYCbCr(c) >> 8) & 0xff);
			break;

		case kPixFormat_Y8_FR:
			VDPixmapRectFillRaw(px, rDst, (VDConvertRGBToYCbCr((uint8)((c >> 16) & 0xff), (uint8)((c >> 8) & 0xff), (uint8)(c & 0xff), false, true) >> 8) & 0xff);
			break;

		case kPixFormat_YUV444_Planar:
		case kPixFormat_YUV422_Planar:
		case kPixFormat_YUV420_Planar:
		case kPixFormat_YUV411_Planar:
		case kPixFormat_YUV410_Planar:
		case kPixFormat_YUV420_Planar16:
		case kPixFormat_YUV422_Planar16:
		case kPixFormat_YUV444_Planar16:
		case kPixFormat_YUV420_Alpha_Planar16:
		case kPixFormat_YUV422_Alpha_Planar16:
		case kPixFormat_YUV444_Alpha_Planar16:
		case kPixFormat_YUV420_Alpha_Planar:
		case kPixFormat_YUV422_Alpha_Planar:
		case kPixFormat_YUV444_Alpha_Planar:
			VDPixmapRectFillRaw(px, rDst, VDConvertRGBToYCbCr(c));
			break;

		case kPixFormat_YUV444_Planar_FR:
		case kPixFormat_YUV422_Planar_FR:
		case kPixFormat_YUV420_Planar_FR:
		case kPixFormat_YUV411_Planar_FR:
		case kPixFormat_YUV410_Planar_FR:
			VDPixmapRectFillRaw(px, rDst, VDConvertRGBToYCbCr((uint8)((c >> 16) & 0xff), (uint8)((c >> 8) & 0xff), (uint8)(c & 0xff), false, true));
			break;

		case kPixFormat_YUV444_Planar_709:
		case kPixFormat_YUV422_Planar_709:
		case kPixFormat_YUV420_Planar_709:
		case kPixFormat_YUV411_Planar_709:
		case kPixFormat_YUV410_Planar_709:
			VDPixmapRectFillRaw(px, rDst, VDConvertRGBToYCbCr((uint8)((c >> 16) & 0xff), (uint8)((c >> 8) & 0xff), (uint8)(c & 0xff), true, false));
			break;

		case kPixFormat_YUV444_Planar_709_FR:
		case kPixFormat_YUV422_Planar_709_FR:
		case kPixFormat_YUV420_Planar_709_FR:
		case kPixFormat_YUV411_Planar_709_FR:
		case kPixFormat_YUV410_Planar_709_FR:
			VDPixmapRectFillRaw(px, rDst, VDConvertRGBToYCbCr((uint8)((c >> 16) & 0xff), (uint8)((c >> 8) & 0xff), (uint8)(c & 0xff), true, true));
			break;
		}
	}
}

class VDVideoFilterResize : public VDXVideoFilter {
public:
	VDVideoFilterResize();
	VDVideoFilterResize(const VDVideoFilterResize&);

	virtual bool Init();
	virtual void Run();
	virtual uint32 GetParams();
	virtual bool Configure(VDXHWND hwnd);
	virtual void GetSettingString(char *buf, int maxlen);
	virtual void GetScriptString(char *buf, int maxlen);
	virtual void Start();
	virtual void End();

	virtual void StartAccel(IVDXAContext *vdxa);
	virtual void StopAccel(IVDXAContext *vdxa);
	virtual void RunAccel(IVDXAContext *vdxa);

	void ScriptConfig(IVDXScriptInterpreter *env, const VDXScriptValue *argv, int argc);
	void ScriptConfig2(IVDXScriptInterpreter *env, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	VDResizeFilterData mConfig;

	IVDPixmapResampler *mpResampler;
	IVDPixmapResampler *mpResampler2;

	double	mVDXAImageW;
	double	mVDXAImageH;
	VDXRect	mVDXADestRect;
	int		mVDXADestW;
	int		mVDXADestH;
	int		mVDXADestCroppedW;
	int		mVDXADestCroppedH;
	double	mVDXAHorizFactor;
	double	mVDXAVertFactor;
	int		mVDXAHorizTaps;
	int		mVDXAVertTaps;

	uint32 mVDXAFP_2Tap;
	uint32 mVDXAFP_4Tap;
	uint32 mVDXAFP_6Tap;
	uint32 mVDXAFP_8Tap;
	uint32 mVDXAFP_10Tap;
	uint32 mVDXART_Temp;
	uint32 mVDXATex_HorizFilt;
	uint32 mVDXATex_VertFilt;
};

VDVideoFilterResize::VDVideoFilterResize()
	: mpResampler(NULL)
	, mpResampler2(NULL)
	, mVDXAHorizFactor(1)
	, mVDXAVertFactor(1)
	, mVDXAHorizTaps(0)
	, mVDXAVertTaps(0)
	, mVDXAFP_2Tap(0)
	, mVDXAFP_4Tap(0)
	, mVDXAFP_6Tap(0)
	, mVDXAFP_8Tap(0)
	, mVDXART_Temp(0)
	, mVDXATex_HorizFilt(0)
	, mVDXATex_VertFilt(0)
{
}

VDVideoFilterResize::VDVideoFilterResize(const VDVideoFilterResize& src)
	: mConfig(src.mConfig)
	, mpResampler(NULL)
	, mpResampler2(NULL)
	, mVDXAHorizFactor(1)
	, mVDXAVertFactor(1)
	, mVDXAHorizTaps(0)
	, mVDXAVertTaps(0)
	, mVDXAFP_2Tap(0)
	, mVDXAFP_4Tap(0)
	, mVDXAFP_6Tap(0)
	, mVDXAFP_8Tap(0)
	, mVDXAFP_10Tap(0)
	, mVDXART_Temp(0)
	, mVDXATex_HorizFilt(0)
	, mVDXATex_VertFilt(0)
{
}

bool VDVideoFilterResize::Init() {
	mConfig.ExchangeWithRegistry(false);
	return true;
}

void VDVideoFilterResize::Run() {
	VDPixmap pxdst = VDPixmap::copy(*fa->dst.mpPixmap);
	VDPixmap pxsrc = VDPixmap::copy(*fa->src.mpPixmap);

	if (fa->fma && fa->fma->fmpixmap) {
		pxdst.info = *fa->fma->fmpixmap->GetPixmapInfo(fa->dst.mpPixmap);
		pxsrc.info = *fa->fma->fmpixmap->GetPixmapInfo(fa->src.mpPixmap);
	}

	const float fx1 = 0.0f;
	const float fy1 = 0.0f;
	const float fx2 = mConfig.mDstRect.left;
	const float fy2 = mConfig.mDstRect.top;
	const float fx3 = mConfig.mDstRect.right;
	const float fy3 = mConfig.mDstRect.bottom;
	const float fx4 = (float)pxdst.w;
	const float fy4 = (float)pxdst.h;
	
	uint32 fill = mConfig.mFillColor;
	VDPixmapRectFillRGB32(pxdst, vdrect32f(fx1, fy1, fx4, fy2), fill);
	VDPixmapRectFillRGB32(pxdst, vdrect32f(fx1, fy2, fx2, fy3), fill);
	VDPixmapRectFillRGB32(pxdst, vdrect32f(fx3, fy2, fx4, fy3), fill);
	VDPixmapRectFillRGB32(pxdst, vdrect32f(fx1, fy3, fx4, fy4), fill);

	if (mConfig.mbInterlaced) {
		VDPixmap pxdst1(pxdst);
		VDPixmap pxsrc1(pxsrc);
		pxdst1.pitch += pxdst1.pitch;
		pxdst1.pitch2 += pxdst1.pitch2;
		pxdst1.pitch3 += pxdst1.pitch3;
		pxdst1.h = (pxdst1.h + 1) >> 1;
		pxsrc1.pitch += pxsrc1.pitch;
		pxsrc1.pitch2 += pxsrc1.pitch2;
		pxsrc1.pitch3 += pxsrc1.pitch3;
		pxsrc1.h = (pxsrc1.h + 1) >> 1;

		VDPixmap pxdst2(pxdst);
		VDPixmap pxsrc2(pxsrc);
		pxdst2.data = (char *)pxdst2.data + pxdst2.pitch;
		pxdst2.data2 = (char *)pxdst2.data2 + pxdst2.pitch2;
		pxdst2.data3 = (char *)pxdst2.data3 + pxdst2.pitch3;
		pxdst2.pitch += pxdst2.pitch;
		pxdst2.pitch2 += pxdst2.pitch2;
		pxdst2.pitch3 += pxdst2.pitch3;
		pxdst2.h = (pxdst2.h + 0) >> 1;
		pxsrc2.data = (char *)pxsrc2.data + pxsrc2.pitch;
		pxsrc2.data2 = (char *)pxsrc2.data2 + pxsrc2.pitch2;
		pxsrc2.data3 = (char *)pxsrc2.data3 + pxsrc2.pitch3;
		pxsrc2.pitch += pxsrc2.pitch;
		pxsrc2.pitch2 += pxsrc2.pitch2;
		pxsrc2.pitch3 += pxsrc2.pitch3;
		pxsrc2.h = (pxsrc2.h + 0) >> 1;

		mpResampler ->Process(pxdst1, pxsrc1);
		mpResampler2->Process(pxdst2, pxsrc2);
	} else {
		mpResampler->Process(pxdst, pxsrc);
	}
}

uint32 VDVideoFilterResize::GetParams() {
	switch(fa->src.mpPixmapLayout->format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_XRGB64:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
			break;

		case nsVDXPixmap::kPixFormat_VDXA_RGB:
		case nsVDXPixmap::kPixFormat_VDXA_YUV:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
			if (mConfig.mbInterlaced)
				return FILTERPARAM_NOT_SUPPORTED;

			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}
	
	if (mConfig.Validate()) {
		// uh oh.
		return 0;
	}

	double imgw, imgh;
	uint32 framew, frameh;
	mConfig.ComputeSizes(fa->src.w, fa->src.h, imgw, imgh, framew, frameh, true, true, fa->src.mpPixmapLayout->format);
	mConfig.ComputeDestRect(framew, frameh, imgw, imgh);

	switch(fa->src.mpPixmapLayout->format) {
		case nsVDXPixmap::kPixFormat_VDXA_RGB:
		case nsVDXPixmap::kPixFormat_VDXA_YUV:
			{
				if (mConfig.mFilterMode == VDResizeFilterData::FILTER_NONE)
					return FILTERPARAM_NOT_SUPPORTED;

				// compute horizontal and vertical taps
				mVDXAHorizFactor = std::max<float>(1.0f, (float)fa->src.w / (float)imgw);
				mVDXAVertFactor = std::max<float>(1.0f, (float)fa->src.h / (float)imgh);

				static const float kBaseSizePerMode[]={
					1,
					2,
					4,
					2,
					4,
					4,
					4,
					6,
				};

				VDASSERTCT(sizeof(kBaseSizePerMode)/sizeof(kBaseSizePerMode[0]) == VDResizeFilterData::FILTER_COUNT);

				if (mConfig.mFilterMode < VDResizeFilterData::FILTER_TABLEBILINEAR) {
					mVDXAHorizFactor = 1.0f;
					mVDXAVertFactor = 1.0f;
				}

				float basetaps = kBaseSizePerMode[mConfig.mFilterMode];
				mVDXAHorizTaps = (VDCeilToInt(basetaps * mVDXAHorizFactor) + 1) & ~1;
				mVDXAVertTaps = (VDCeilToInt(basetaps * mVDXAVertFactor) + 1) & ~1;

				if (mVDXAHorizTaps > 10 || mVDXAVertTaps > 10)
					return FILTERPARAM_NOT_SUPPORTED;

				fa->src.mBorderWidth = mVDXAHorizTaps >> 1;
				fa->src.mBorderHeight = 0;
			}
			break;
	}

	fa->dst.mpPixmapLayout->format = fa->src.mpPixmapLayout->format;
	fa->dst.mpPixmapLayout->w = framew;
	fa->dst.mpPixmapLayout->h = frameh;
	fa->dst.mpPixmapLayout->pitch = 0;
	fa->dst.depth = 0;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_NORMALIZE16;
}

bool VDVideoFilterResize::Configure(VDXHWND hwnd) {
	VDResizeFilterData *mfd = &mConfig;
	VDResizeFilterData mfd2 = *mfd;

	if (!VDFilterResizeActivateConfigDialog(*mfd, fa->ifp2, fa->src.w, fa->src.h, (VDGUIHandle)hwnd)) {
		*mfd = mfd2;
		return false;
	}

	return true;
}

void VDVideoFilterResize::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (%s)", VDResizeFilterData::kFilterNames[mConfig.mFilterMode]);
}

void VDVideoFilterResize::Start() {
	const VDXPixmapLayout& pxdst = *fa->dst.mpPixmapLayout;
	const VDXPixmapLayout& pxsrc = *fa->src.mpPixmapLayout;

	if (const char *error = mConfig.Validate())
		ff->Except("%s", error);

	double dstw, dsth;
	uint32 framew, frameh;
	mConfig.ComputeSizes(pxsrc.w, pxsrc.h, dstw, dsth, framew, frameh, true, true, fa->src.mpPixmapLayout->format);

	mpResampler = VDCreatePixmapResampler();
	if (!mpResampler)
		ff->ExceptOutOfMemory();

	IVDPixmapResampler::FilterMode fmode;
	bool bInterpolationOnly = true;
	float splineFactor = -0.60f;

	switch(mConfig.mFilterMode) {
		case VDResizeFilterData::FILTER_NONE:
			fmode = IVDPixmapResampler::kFilterPoint;
			break;

		case VDResizeFilterData::FILTER_TABLEBILINEAR:
			bInterpolationOnly = false;
		case VDResizeFilterData::FILTER_BILINEAR:
			fmode = IVDPixmapResampler::kFilterLinear;
			break;

		case VDResizeFilterData::FILTER_TABLEBICUBIC060:
			splineFactor = -0.60;
			fmode = IVDPixmapResampler::kFilterCubic;
			bInterpolationOnly = false;
			break;

		case VDResizeFilterData::FILTER_TABLEBICUBIC075:
			bInterpolationOnly = false;
		case VDResizeFilterData::FILTER_BICUBIC:
			splineFactor = -0.75;
			fmode = IVDPixmapResampler::kFilterCubic;
			break;

		case VDResizeFilterData::FILTER_TABLEBICUBIC100:
			bInterpolationOnly = false;
			splineFactor = -1.0;
			fmode = IVDPixmapResampler::kFilterCubic;
			break;

		case VDResizeFilterData::FILTER_LANCZOS3:
			bInterpolationOnly = false;
			fmode = IVDPixmapResampler::kFilterLanczos3;
			break;
	}

	mpResampler->SetSplineFactor(splineFactor);
	mpResampler->SetFilters(fmode, fmode, bInterpolationOnly);

	vdrect32f srcrect(0.0f, 0.0f, (float)pxsrc.w, (float)pxsrc.h);

	if (mConfig.mbInterlaced) {
		mpResampler2 = VDCreatePixmapResampler();
		if (!mpResampler2)
			ff->ExceptOutOfMemory();

		mpResampler2->SetSplineFactor(splineFactor);
		mpResampler2->SetFilters(fmode, fmode, bInterpolationOnly);

		vdrect32f srcrect1(srcrect);
		vdrect32f srcrect2(srcrect);
		vdrect32f dstrect1(mConfig.mDstRect);
		vdrect32f dstrect2(mConfig.mDstRect);

		srcrect1.transform(1.0f, 0.5f, 0.0f, 0.25f);
		dstrect1.transform(1.0f, 0.5f, 0.0f, 0.25f);
		srcrect2.transform(1.0f, 0.5f, 0.0f, -0.25f);
		dstrect2.transform(1.0f, 0.5f, 0.0f, -0.25f);

		VDVERIFY(mpResampler ->Init(dstrect1, pxdst.w, (pxdst.h+1) >> 1, pxdst.format, srcrect1, pxsrc.w, (pxsrc.h+1) >> 1, pxsrc.format));
		VDVERIFY(mpResampler2->Init(dstrect2, pxdst.w, (pxdst.h+0) >> 1, pxdst.format, srcrect2, pxsrc.w, (pxsrc.h+0) >> 1, pxsrc.format));
	} else {
		VDVERIFY(mpResampler->Init(mConfig.mDstRect, pxdst.w, pxdst.h, pxdst.format, srcrect, pxsrc.w, pxsrc.h, pxsrc.format));
	}
}

void VDVideoFilterResize::End() {
	delete mpResampler;		mpResampler = NULL;
	delete mpResampler2;	mpResampler2 = NULL;
}

namespace {
	void CreateFilterTexture(uint32 *dst0, ptrdiff_t dstpitch, float x0, float xr0, sint32 idx, double dx, double sx, int tapcount, IVDResamplerFilter& filter) {
		VDASSERT(tapcount <= 10);

		double step = sx / dx;

		// prestep from left/top of uncropped destination to first pixel center
		double accum = step * ((ceil(xr0 - 0.5) + 0.5) - x0);

		uint32 *dst1 = (uint32 *)((char *)dst0 + dstpitch);
		uint32 *dst2 = (uint32 *)((char *)dst1 + dstpitch);
		double kernoffset = (double)-((tapcount >> 1) - 1);
		double taps[12];
		int itaps[12] = {0};

		for(sint32 i=0; i<idx; ++i) {
			// compute coordinate of pixel center used by texture sampler
			double snapcoord = floor(accum - 0.5) + 0.5;

			// compute error from pixel center
			double fracstep = accum - snapcoord;

			double sum = 0;
			for(int j=0; j<tapcount; ++j) {
				double w = filter.EvaluateFilter(kernoffset - fracstep + (double)j);
				taps[j] = w;
				sum += w;
			}

			double scale = 128.0 / sum;
			double error = 0;
			int isum = 0;
			for(int j=0; j<tapcount - 1; ++j) {
				int perm_index = j >> 1;
				if (j & 1)
					perm_index = (tapcount - 1) - perm_index;

				double w = taps[perm_index] * scale + error;
				int iw = VDRoundToInt(w);
				isum += iw;
				error += w - iw;

				itaps[perm_index] = (iw + 64) & 0xff;
			}


			itaps[(tapcount - 1) - ((tapcount - 1) >> 1)] = 192 - isum;

			int istep = VDClampedRoundFixedToUint8Fast((float)(snapcoord - (accum - 0.5) + 0.5));
			if (tapcount > 8) {
				dst0[i] = (itaps[0] << 16) + (itaps[1] << 8) + itaps[2] + (itaps[3] << 24);
				dst1[i] = (itaps[4] << 16) + (itaps[5] << 8) + itaps[6] + (itaps[7] << 24);
				dst2[i] = (itaps[8] << 16) + (itaps[9] << 8) + itaps[10] + (istep << 24);			
			} else if (tapcount > 4) {
				dst0[i] = (itaps[0] << 16) + (itaps[1] << 8) + itaps[2] + (itaps[3] << 24);
				dst1[i] = dst2[i] = (itaps[4] << 16) + (itaps[5] << 8) + itaps[6] + (istep << 24);			
			} else {
				dst0[i] = dst1[i] = dst2[i] = (itaps[0] << 16) + (itaps[1] << 8) + itaps[2] + (istep << 24);
			}

			accum += step;
		}
	}
}

void VDVideoFilterResize::StartAccel(IVDXAContext *vdxa) {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;

	if (const char *error = mConfig.Validate())
		ff->Except("%s", error);

	double dstw, dsth;
	uint32 framew, frameh;
	mConfig.ComputeSizes(pxsrc.w, pxsrc.h, dstw, dsth, framew, frameh, true, true, fa->src.mpPixmapLayout->format);
	mVDXAImageW = dstw;
	mVDXAImageH = dsth;

	const int x1 = VDCeilToInt(mConfig.mDstRect.left   - 0.5f);
	const int y1 = VDCeilToInt(mConfig.mDstRect.top    - 0.5f);
	const int x2 = VDCeilToInt(mConfig.mDstRect.right  - 0.5f);
	const int y2 = VDCeilToInt(mConfig.mDstRect.bottom - 0.5f);

	mVDXADestRect.left = std::max<sint32>(x1, 0);
	mVDXADestRect.top = std::max<sint32>(y1, 0);
	mVDXADestRect.right = std::min<sint32>(x2, fa->dst.w);
	mVDXADestRect.bottom = std::min<sint32>(y2, fa->dst.h);
	mVDXADestW = x2 - x1;
	mVDXADestH = y2 - y1;
	mVDXADestCroppedW = std::min<int>(mVDXADestW, pxdst.w);
	mVDXADestCroppedH = std::min<int>(mVDXADestH, pxdst.h);

	vdfastvector<uint32> filt(std::max<uint32>(mVDXADestCroppedW, mVDXADestCroppedH) * 3, 0);

	vdautoptr<IVDResamplerFilter> horizFilter;
	vdautoptr<IVDResamplerFilter> vertFilter;

	const double horiz_twofc = 1.0f / mVDXAHorizFactor;
	const double vert_twofc = 1.0f / mVDXAVertFactor;

	switch(mConfig.mFilterMode) {
		case VDResizeFilterData::FILTER_BILINEAR:
			horizFilter = new VDResamplerLinearFilter(horiz_twofc);
			vertFilter = new VDResamplerLinearFilter(vert_twofc);
			break;
		case VDResizeFilterData::FILTER_BICUBIC:
			horizFilter = new VDResamplerCubicFilter(horiz_twofc, -0.75f);
			vertFilter = new VDResamplerCubicFilter(vert_twofc, -0.75f);
			break;
		case VDResizeFilterData::FILTER_TABLEBILINEAR:
			horizFilter = new VDResamplerLinearFilter(horiz_twofc);
			vertFilter = new VDResamplerLinearFilter(vert_twofc);
			break;
		case VDResizeFilterData::FILTER_TABLEBICUBIC060:
			horizFilter = new VDResamplerCubicFilter(horiz_twofc, -0.60f);
			vertFilter = new VDResamplerCubicFilter(vert_twofc, -0.60f);
			break;
		case VDResizeFilterData::FILTER_TABLEBICUBIC075:
			horizFilter = new VDResamplerCubicFilter(horiz_twofc, -0.75f);
			vertFilter = new VDResamplerCubicFilter(vert_twofc, -0.75f);
			break;
		case VDResizeFilterData::FILTER_TABLEBICUBIC100:
			horizFilter = new VDResamplerCubicFilter(horiz_twofc, -1.0f);
			vertFilter = new VDResamplerCubicFilter(vert_twofc, -1.0f);
			break;
		case VDResizeFilterData::FILTER_LANCZOS3:
			horizFilter = new VDResamplerLanczos3Filter(horiz_twofc);
			vertFilter = new VDResamplerLanczos3Filter(vert_twofc);
			break;
	}

	VDXAInitData2D initData;
	initData.mpData = filt.data();
	initData.mPitch = sizeof(uint32) * mVDXADestCroppedW;

	CreateFilterTexture(filt.data(), initData.mPitch, mConfig.mDstRect.left, (float)mVDXADestRect.left, mVDXADestCroppedW, dstw, fa->src.w, mVDXAHorizTaps, *horizFilter);
	mVDXATex_HorizFilt = vdxa->CreateTexture2D(mVDXADestCroppedW, 3, 1, kVDXAF_A8R8G8B8, false, &initData);

	initData.mPitch = sizeof(uint32) * mVDXADestCroppedH;
	CreateFilterTexture(filt.data(), initData.mPitch, mConfig.mDstRect.top, (float)mVDXADestRect.top, mVDXADestCroppedH, dsth, fa->src.h, mVDXAVertTaps, *vertFilter);
	mVDXATex_VertFilt = vdxa->CreateTexture2D(mVDXADestCroppedH, 3, 1, kVDXAF_A8R8G8B8, false, &initData);

	mVDXAFP_2Tap = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterResizeFP_2tap, sizeof kVDFilterResizeFP_2tap);
	mVDXAFP_4Tap = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterResizeFP_4tap, sizeof kVDFilterResizeFP_4tap);
	mVDXAFP_6Tap = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterResizeFP_6tap, sizeof kVDFilterResizeFP_6tap);
	mVDXAFP_8Tap = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterResizeFP_8tap, sizeof kVDFilterResizeFP_8tap);
	mVDXAFP_10Tap = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterResizeFP_10tap, sizeof kVDFilterResizeFP_10tap);

	mVDXART_Temp = vdxa->CreateRenderTexture(mVDXADestCroppedW, pxsrc.h, 0, mVDXAVertTaps >> 1, kVDXAF_Unknown, false);
	VDASSERT(mVDXART_Temp);
}

void VDVideoFilterResize::StopAccel(IVDXAContext *vdxa) {
	if (mVDXATex_HorizFilt) {
		vdxa->DestroyObject(mVDXATex_HorizFilt);
		mVDXATex_HorizFilt = 0;
	}

	if (mVDXATex_VertFilt) {
		vdxa->DestroyObject(mVDXATex_VertFilt);
		mVDXATex_VertFilt = 0;
	}

	if (mVDXART_Temp) {
		vdxa->DestroyObject(mVDXART_Temp);
		mVDXART_Temp = 0;
	}

	if (mVDXAFP_10Tap) {
		vdxa->DestroyObject(mVDXAFP_10Tap);
		mVDXAFP_10Tap = 0;
	}

	if (mVDXAFP_8Tap) {
		vdxa->DestroyObject(mVDXAFP_8Tap);
		mVDXAFP_8Tap = 0;
	}

	if (mVDXAFP_6Tap) {
		vdxa->DestroyObject(mVDXAFP_6Tap);
		mVDXAFP_6Tap = 0;
	}

	if (mVDXAFP_4Tap) {
		vdxa->DestroyObject(mVDXAFP_4Tap);
		mVDXAFP_4Tap = 0;
	}

	if (mVDXAFP_2Tap) {
		vdxa->DestroyObject(mVDXAFP_2Tap);
		mVDXAFP_2Tap = 0;
	}
}

void VDVideoFilterResize::RunAccel(IVDXAContext *vdxa) {
	const int x1 = VDCeilToInt(mConfig.mDstRect.left   - 0.5f);
	const int y1 = VDCeilToInt(mConfig.mDstRect.top    - 0.5f);
	const int x2 = VDCeilToInt(mConfig.mDstRect.right  - 0.5f);
	const int y2 = VDCeilToInt(mConfig.mDstRect.bottom - 0.5f);
	const int framew = fa->dst.w;
	const int frameh = fa->dst.h;
	const int dstw = x2 - x1;
	const int dsth = y2 - y1;

	// HORIZONTAL PASS
	vdxa->SetSampler(0, fa->src.mVDXAHandle, kVDXAFilt_Point);
	vdxa->SetSampler(1, mVDXATex_HorizFilt, kVDXAFilt_Point);

	VDXATextureDesc srcdesc;
	vdxa->GetTextureDesc(fa->src.mVDXAHandle, srcdesc);

	float srcc[4];
	srcc[0] = srcdesc.mInvTexWidth;
	srcc[1] = 0;
	srcc[2] = 0;
	srcc[3] = 0;
	vdxa->SetFragmentProgramConstF(0, 1, srcc);

	static const int kSrcTexCoordCount[5]={ 2, 4, 6, 6, 5 };
	static const int kSrcTexCoordOffset[5]={ 0, 0, 0, 6, 5 };
	int texCoords = kSrcTexCoordCount[(mVDXAHorizTaps >> 1) - 1];
	int coordOffset = kSrcTexCoordOffset[(mVDXAHorizTaps >> 1) - 1];

	float dudx = ((float)fa->src.w / (float)mVDXAImageW) * srcdesc.mInvTexWidth;

	float srcm[12];
	srcm[0] = dudx * mVDXADestCroppedW;
	srcm[1] = 0.0f;
	srcm[2] = 0.0f;
	srcm[3] = srcm[0];
	srcm[4] = 0.0f;
	srcm[5] = srcdesc.mInvTexHeight * fa->src.h;
	srcm[6] = srcm[5];
	srcm[7] = 0.0f;

	// Terms included here:
	//
	//	- Prestep from uncropped left/top edge to cropped left/top edge. This handles
	//	  both the cases where the dest rect has been cropped to the frame and when
	//	  a fractional dest size has introduced a fractional crop.
	//
	//	- Compensation for tap count (-(floor(taps/2)-1)). This only kicks in if we have
	//	  taps to the left/top of the center pair, so a 2-tap filter has no compensation
	//	  and a 4-tap filter has a one-texel offset.
	//
	//	- A -0.5 texel offset so that the point sampler steps exactly when the filter
	//	  window steps.
	//
	//	- Another -0.5 texel offset to compensate for us putting a signed correction
	//	  into the filter texture, which is an unsigned texture.
	//
	// The two -0.5 offsets cancel the -(-1) offset in the tap count shift.
	//
	srcm[8] = ((float)mVDXADestRect.left - mConfig.mDstRect.left) * dudx - (float)(mVDXAHorizTaps >> 1) * srcdesc.mInvTexWidth;

	srcm[9] = 0.0f;
	srcm[10] = 0.0f;
	srcm[11] = srcm[8] + coordOffset * srcdesc.mInvTexWidth;

	for(int i=0; i<texCoords; ++i) {
		vdxa->SetTextureMatrix(i, 0, 0, 0, srcm);
		srcm[8] += srcdesc.mInvTexWidth;
		srcm[11] += srcdesc.mInvTexWidth;
	}

	VDXATextureDesc filtdesc;
	vdxa->GetTextureDesc(mVDXATex_HorizFilt, filtdesc);
	float filtm[12];
	filtm[0] = filtdesc.mInvTexWidth * mVDXADestCroppedW;
	filtm[1] = 0.0f;
	filtm[2] = 0.0f;
	filtm[3] = 0.0f;
	filtm[4] = 0.0f;
	filtm[5] = 0.0f;
	filtm[6] = 0.0f;
	filtm[7] = 0.0f;
	filtm[8] = 0.0f;
	filtm[9] = filtdesc.mInvTexHeight * 0.5f;
	filtm[10] = 0.0f;
	filtm[11] = 0.0f;

	switch(mVDXAHorizTaps) {
		case 2:
			vdxa->SetTextureMatrix(2, 0, 0, 0, filtm);
			vdxa->DrawRect(mVDXART_Temp, mVDXAFP_2Tap, NULL);
			break;

		case 4:
			vdxa->SetTextureMatrix(4, 0, 0, 0, filtm);
			vdxa->DrawRect(mVDXART_Temp, mVDXAFP_4Tap, NULL);
			break;

		case 6:
			vdxa->SetTextureMatrix(6, 0, 0, 0, filtm);
			filtm[9] += filtdesc.mInvTexHeight;
			vdxa->SetTextureMatrix(7, 0, 0, 0, filtm);
			vdxa->DrawRect(mVDXART_Temp, mVDXAFP_6Tap, NULL);
			break;

		case 8:
			vdxa->SetTextureMatrix(6, 0, 0, 0, filtm);
			filtm[9] += filtdesc.mInvTexHeight;
			vdxa->SetTextureMatrix(7, 0, 0, 0, filtm);
			vdxa->DrawRect(mVDXART_Temp, mVDXAFP_8Tap, NULL);
			break;

		case 10:
			vdxa->SetTextureMatrix(5, 0, 0, 0, filtm);
			filtm[9] += filtdesc.mInvTexHeight;
			vdxa->SetTextureMatrix(6, 0, 0, 0, filtm);
			filtm[9] += filtdesc.mInvTexHeight;
			vdxa->SetTextureMatrix(7, 0, 0, 0, filtm);
			vdxa->DrawRect(mVDXART_Temp, mVDXAFP_10Tap, NULL);
			break;
	}

	// VERTICAL PASS
	vdxa->GetTextureDesc(mVDXART_Temp, srcdesc);

	srcc[0] = 0;
	srcc[1] = srcdesc.mInvTexHeight;
	srcc[2] = 0;
	srcc[3] = 0;
	vdxa->SetFragmentProgramConstF(0, 1, srcc);

	texCoords = kSrcTexCoordCount[(mVDXAVertTaps >> 1) - 1];
	coordOffset = kSrcTexCoordOffset[(mVDXAVertTaps >> 1) - 1];

	const float dvdy = ((float)fa->src.h / (float)mVDXAImageH) * srcdesc.mInvTexHeight;

	srcm[0] = srcdesc.mInvTexWidth * mVDXADestCroppedW;
	srcm[1] = 0.0f;
	srcm[2] = 0.0f;
	srcm[3] = srcm[0];
	srcm[4] = 0.0f;
	srcm[5] = dvdy * mVDXADestCroppedH;
	srcm[6] = srcm[5];
	srcm[7] = 0.0f;
	srcm[8] = 0.0f;
	srcm[9] = ((float)mVDXADestRect.top - mConfig.mDstRect.top) * dvdy - (float)(mVDXAVertTaps >> 1) * srcdesc.mInvTexHeight;

	srcm[10] = srcm[9] + coordOffset * srcdesc.mInvTexHeight;
	srcm[11] = 0.0f;

	for(int i=0; i<texCoords; ++i) {
		vdxa->SetTextureMatrix(i, 0, 0, 0, srcm);
		srcm[9] += srcdesc.mInvTexHeight;
		srcm[10] += srcdesc.mInvTexHeight;
	}

	vdxa->GetTextureDesc(mVDXATex_VertFilt, filtdesc);
	filtm[0] = 0.0f;
	filtm[1] = 0.0f;
	filtm[2] = 0.0f;
	filtm[3] = 0.0f;
	filtm[4] = filtdesc.mInvTexWidth * mVDXADestCroppedH;
	filtm[5] = 0.0f;
	filtm[6] = 0.0f;
	filtm[7] = 0.0f;
	filtm[8] = 0.0f;
	filtm[9] = filtdesc.mInvTexHeight * 0.5f;
	filtm[10] = 0.0f;
	filtm[11] = 0.0f;

	vdxa->SetSampler(0, mVDXART_Temp, kVDXAFilt_Point);
	vdxa->SetSampler(1, mVDXATex_VertFilt, kVDXAFilt_Point);

	VDXRect dstRect(mVDXADestRect);

	switch(mVDXAVertTaps) {
		case 2:
			vdxa->SetTextureMatrix(2, 0, 0, 0, filtm);
			vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAFP_2Tap, &dstRect);
			break;

		case 4:
			vdxa->SetTextureMatrix(4, 0, 0, 0, filtm);
			vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAFP_4Tap, &dstRect);
			break;

		case 6:
			vdxa->SetTextureMatrix(6, 0, 0, 0, filtm);
			filtm[9] += filtdesc.mInvTexHeight;
			vdxa->SetTextureMatrix(7, 0, 0, 0, filtm);
			vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAFP_6Tap, &dstRect);
			break;

		case 8:
			vdxa->SetTextureMatrix(6, 0, 0, 0, filtm);
			filtm[9] += filtdesc.mInvTexHeight;
			vdxa->SetTextureMatrix(7, 0, 0, 0, filtm);
			vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAFP_8Tap, &dstRect);
			break;

		case 10:
			vdxa->SetTextureMatrix(5, 0, 0, 0, filtm);
			filtm[9] += filtdesc.mInvTexHeight;
			vdxa->SetTextureMatrix(6, 0, 0, 0, filtm);
			filtm[9] += filtdesc.mInvTexHeight;
			vdxa->SetTextureMatrix(7, 0, 0, 0, filtm);
			vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAFP_10Tap, &dstRect);
			break;
	}

	if (dstw < framew || dsth < frameh) {
		VDXRect r[4];
		VDXRect *rdst = r;

		if (y1 > 0) {
			rdst->left = 0;
			rdst->top = 0;
			rdst->right = framew;
			rdst->bottom = y1;
			++rdst;
		}

		if (y2 > y1) {
			if (x1 > 0) {
				rdst->left = 0;
				rdst->top = y1;
				rdst->right = x1;
				rdst->bottom = y2;
				++rdst;
			}

			if (x2 < framew) {
				rdst->left = x2;
				rdst->top = y1;
				rdst->right = framew;
				rdst->bottom = y2;
				++rdst;
			}
		}

		if (y2 < frameh) {
			rdst->left = 0;
			rdst->top = y2;
			rdst->right = framew;
			rdst->bottom = frameh;
			++rdst;
		}

		if (rdst > r) {
			uint32 c = mConfig.mFillColor;

			if (fa->dst.mpPixmap->format == nsVDXPixmap::kPixFormat_VDXA_YUV)
				c = VDConvertRGBToYCbCr(c);

			vdxa->FillRects(fa->dst.mVDXAHandle, rdst - r, r, c);
		}
	}
}

void VDVideoFilterResize::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	mConfig.mImageW	= argv[0].asDouble();
	mConfig.mImageH	= argv[1].asDouble();
	mConfig.mbUseRelative = false;
	mConfig.mImageAspectMode = VDResizeFilterData::kImageAspectNone;

	if (argv[2].isInt())
		mConfig.mFilterMode = argv[2].asInt();
	else {
		char *s = *argv[2].asString();

		if (!_stricmp(s, "point") || !_stricmp(s, "nearest"))
			mConfig.mFilterMode = VDResizeFilterData::FILTER_NONE;
		else if (!_stricmp(s, "bilinear"))
			mConfig.mFilterMode = VDResizeFilterData::FILTER_BILINEAR;
		else if (!_stricmp(s, "bicubic"))
			mConfig.mFilterMode = VDResizeFilterData::FILTER_BICUBIC;
		else
			VDSCRIPT_EXT_ERROR(FCALL_UNKNOWN_STR);
	}

	mConfig.mbInterlaced = false;

	if (mConfig.mFilterMode & 128) {
		mConfig.mbInterlaced = true;
		mConfig.mFilterMode &= 127;
	}

	mConfig.mFrameMode = VDResizeFilterData::kFrameModeNone;
	if (argc > 3) {
		mConfig.mFrameMode = VDResizeFilterData::kFrameModeToSize;
		mConfig.mFrameW = argv[3].asInt();
		mConfig.mFrameH = argv[4].asInt();
		mConfig.mFillColor = argv[5].asInt();
	}

	// make the sizes somewhat sane
	if (mConfig.mImageW < 1.0f)
		mConfig.mImageW = 1.0f;
	if (mConfig.mImageH < 1.0f)
		mConfig.mImageH = 1.0f;

	if (mConfig.mFrameMode) {
		if (mConfig.mFrameW < mConfig.mImageW)
			mConfig.mFrameW = (int)ceil(mConfig.mImageW);

		if (mConfig.mFrameH < mConfig.mImageH)
			mConfig.mFrameH = (int)ceil(mConfig.mImageH);
	}
}

void VDVideoFilterResize::ScriptConfig2(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	mConfig.mbUseRelative	= (argv[2].asInt() & 1) != 0;

	if (mConfig.mbUseRelative) {
		mConfig.mImageRelW	= argv[0].asDouble();
		mConfig.mImageRelH	= argv[1].asDouble();
	} else {
		mConfig.mImageW	= argv[0].asDouble();
		mConfig.mImageH	= argv[1].asDouble();
	}

	mConfig.mImageAspectNumerator		= argv[3].asDouble();
	mConfig.mImageAspectDenominator	= argv[4].asDouble();
	mConfig.mImageAspectMode			= argv[5].asInt();
	if (mConfig.mImageAspectMode >= VDResizeFilterData::kImageAspectModeCount)
		mConfig.mImageAspectMode = 0;

	mConfig.mFrameW = argv[6].asInt();
	mConfig.mFrameH = argv[7].asInt();

	mConfig.mFrameAspectNumerator		= argv[8].asDouble();
	mConfig.mFrameAspectDenominator	= argv[9].asDouble();
	mConfig.mFrameMode			= argv[10].asInt();
	if (mConfig.mFrameMode >= VDResizeFilterData::kFrameModeCount)
		mConfig.mFrameMode = 0;

	mConfig.mFilterMode = argv[11].asInt();
	mConfig.mbInterlaced = false;

	if (mConfig.mFilterMode & 128) {
		mConfig.mbInterlaced = true;
		mConfig.mFilterMode &= 127;
	}

	mConfig.mAlignment = argv[12].asInt();
	switch(mConfig.mAlignment) {
		case 1:
		case 2:
		case 4:
		case 8:
		case 16:
			break;
		default:
			mConfig.mAlignment = 1;
			break;
	}

	mConfig.mFillColor = argv[13].asInt();
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterResize)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterResize, ScriptConfig, "ddi")
	VDXVF_DEFINE_SCRIPT_METHOD2(VDVideoFilterResize, ScriptConfig, "dds")
	VDXVF_DEFINE_SCRIPT_METHOD2(VDVideoFilterResize, ScriptConfig, "ddiiii")
	VDXVF_DEFINE_SCRIPT_METHOD2(VDVideoFilterResize, ScriptConfig, "ddsiii")
	VDXVF_DEFINE_SCRIPT_METHOD2(VDVideoFilterResize, ScriptConfig2, "ddiddiiiddiiii")
VDXVF_END_SCRIPT_METHODS()

void VDVideoFilterResize::GetScriptString(char *buf, int maxlen) {
	int filtmode = mConfig.mFilterMode + (mConfig.mbInterlaced ? 128 : 0);

	SafePrintf(buf, maxlen, "Config(%g,%g,%d,%g,%g,%d,%d,%d,%g,%g,%d,%d,%d,0x%06x)"
		, mConfig.mbUseRelative ? mConfig.mImageRelW : mConfig.mImageW
		, mConfig.mbUseRelative ? mConfig.mImageRelH : mConfig.mImageH
		, mConfig.mbUseRelative
		, mConfig.mImageAspectNumerator
		, mConfig.mImageAspectDenominator
		, mConfig.mImageAspectMode
		, mConfig.mFrameW
		, mConfig.mFrameH
		, mConfig.mFrameAspectNumerator
		, mConfig.mFrameAspectDenominator
		, mConfig.mFrameMode
		, filtmode
		, mConfig.mAlignment
		, mConfig.mFillColor);
}

extern const VDXFilterDefinition filterDef_resize = VDXVideoFilterDefinition<VDVideoFilterResize>(
	NULL,
	"resize",
	"Resizes the image to a new size."
#ifdef USE_ASM
			"\n\n[Assembly/MMX optimized] [YCbCr processing]"
#endif
			);

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)

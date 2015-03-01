//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"

#include <vd2/system/vdstl.h>
#include <vd2/system/fraction.h>
#include <vd2/system/memory.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>

#include "resource.h"

////////////////////////////////////////////////////////////

struct VDVideoFilterInterlaceConfig {
	bool mbLaceFields;
	bool mbOddFieldFirst;

	VDVideoFilterInterlaceConfig()
		: mbLaceFields(false)
		, mbOddFieldFirst(true)
	{
	}
};

////////////////////////////////////////////////////////////

class VDVideoFilterInterlaceDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterInterlaceDialog(VDVideoFilterInterlaceConfig& config);

protected:
	void OnDataExchange(bool write);

	VDVideoFilterInterlaceConfig& mConfig;
};

VDVideoFilterInterlaceDialog::VDVideoFilterInterlaceDialog(VDVideoFilterInterlaceConfig& config)
	: VDDialogFrameW32(IDD_FILTER_INTERLACE)
	, mConfig(config)
{
}

void VDVideoFilterInterlaceDialog::OnDataExchange(bool write) {
	if (write) {
		mConfig.mbLaceFields = IsButtonChecked(IDC_SOURCE_ALTERNATING);
		mConfig.mbOddFieldFirst = IsButtonChecked(IDC_FIELDORDER_ODDFIRST);
	} else {
		CheckButton(mConfig.mbLaceFields ? IDC_SOURCE_ALTERNATING : IDC_SOURCE_PROGRESSIVE, true);
		CheckButton(mConfig.mbOddFieldFirst ? IDC_FIELDORDER_ODDFIRST : IDC_FIELDORDER_EVENFIRST, true);
	}
}

////////////////////////////////////////////////////////////

class VDVideoFilterInterlace : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Run();

	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);

	bool Configure(VDXHWND hwnd);

	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	uint32	mLumaWidth;
	uint32	mChromaWidth;

	VDVideoFilterInterlaceConfig mConfig;
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterInterlace)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterInterlace, ScriptConfig, "ii")
VDXVF_END_SCRIPT_METHODS()

uint32 VDVideoFilterInterlace::GetParams() {
	VDFraction fr(fa->src.mFrameRateHi, fa->src.mFrameRateLo);

	fr /= 2;
	fa->dst.mFrameRateHi = fr.getHi();
	fa->dst.mFrameRateLo = fr.getLo();

	if (fa->dst.mFrameCount >= 0)
		fa->dst.mFrameCount /= 2;

	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (mConfig.mbLaceFields)
		pxldst.h *= 2;
	else
		pxldst.h &= ~1;

	mChromaWidth = 0;
	mLumaWidth = pxldst.w;

	switch(pxldst.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			mLumaWidth *= 4;
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
			mChromaWidth = pxldst.w;
			break;

		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
			mChromaWidth = (pxldst.w + 1) >> 1;
			break;

		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
			mChromaWidth = (pxldst.w + 3) >> 2;
			break;

		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
			mLumaWidth = ((mLumaWidth + 1) & ~1) * 2;
			break;

		case nsVDXPixmap::kPixFormat_Y8:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	fa->dst.depth = 0;
	fa->dst.mpPixmapLayout->pitch = 0;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
}

void VDVideoFilterInterlace::Run() {
	const VDXPixmap& pxsrc1 = *fa->mpSourceFrames[0]->mpPixmap;
	const VDXPixmap& pxsrc2 = *fa->mpSourceFrames[1]->mpPixmap;
	const VDXPixmap& pxdst = *fa->mpOutputFrames[0]->mpPixmap;

	char *dst = (char *)pxdst.data;
	char *dst2 = (char *)pxdst.data2;
	char *dst3 = (char *)pxdst.data3;
	const ptrdiff_t dstpitch = pxdst.pitch;
	const ptrdiff_t dstpitch2 = pxdst.pitch2;
	const ptrdiff_t dstpitch3 = pxdst.pitch3;

	const char *src1 = (const char *)pxsrc1.data;
	const char *src12 = (const char *)pxsrc1.data2;
	const char *src13 = (const char *)pxsrc1.data3;
	const ptrdiff_t src1pitch = pxsrc1.pitch;
	const ptrdiff_t src1pitch2 = pxsrc1.pitch2;
	const ptrdiff_t src1pitch3 = pxsrc1.pitch3;

	const char *src2 = (const char *)pxsrc2.data;
	const char *src22 = (const char *)pxsrc2.data2;
	const char *src23 = (const char *)pxsrc2.data3;
	const ptrdiff_t src2pitch = pxsrc2.pitch;
	const ptrdiff_t src2pitch2 = pxsrc2.pitch2;
	const ptrdiff_t src2pitch3 = pxsrc2.pitch3;

	const uint32 fieldHeight = pxdst.h >> 1;

	if (mConfig.mbLaceFields) {
		if (mConfig.mbOddFieldFirst) {
			VDMemcpyRect(dst + dstpitch,	dstpitch * 2, src1, src1pitch, mLumaWidth, fieldHeight);
			VDMemcpyRect(dst,				dstpitch * 2, src2, src2pitch, mLumaWidth, fieldHeight);

			if (mChromaWidth) {
				VDMemcpyRect(dst2 + dstpitch2,	dstpitch2 * 2, src12, src1pitch2, mChromaWidth, fieldHeight);
				VDMemcpyRect(dst2,				dstpitch2 * 2, src22, src2pitch2, mChromaWidth, fieldHeight);

				VDMemcpyRect(dst3 + dstpitch3,	dstpitch3 * 2, src13, src1pitch3, mChromaWidth, fieldHeight);
				VDMemcpyRect(dst3,				dstpitch3 * 2, src23, src2pitch3, mChromaWidth, fieldHeight);
			}
		} else {
			VDMemcpyRect(dst,				dstpitch * 2, src1, src1pitch, mLumaWidth, fieldHeight);
			VDMemcpyRect(dst + dstpitch,	dstpitch * 2, src2, src2pitch, mLumaWidth, fieldHeight);

			if (mChromaWidth) {
				VDMemcpyRect(dst2,				dstpitch2 * 2, src12, src1pitch2, mChromaWidth, fieldHeight);
				VDMemcpyRect(dst2 + dstpitch2,	dstpitch2 * 2, src22, src2pitch2, mChromaWidth, fieldHeight);

				VDMemcpyRect(dst3,				dstpitch3 * 2, src13, src1pitch3, mChromaWidth, fieldHeight);
				VDMemcpyRect(dst3 + dstpitch3,	dstpitch3 * 2, src23, src2pitch3, mChromaWidth, fieldHeight);
			}
		}
	} else {
		// extract a field from each frame
		if (mConfig.mbOddFieldFirst) {
			VDMemcpyRect(dst + dstpitch,	dstpitch * 2, src1 + src1pitch,	src1pitch * 2, mLumaWidth, fieldHeight);
			VDMemcpyRect(dst,				dstpitch * 2, src2,				src2pitch * 2, mLumaWidth, fieldHeight);

			if (mChromaWidth) {
				VDMemcpyRect(dst2 + dstpitch2,	dstpitch2 * 2, src12 + src1pitch2,	src1pitch2 * 2, mChromaWidth, fieldHeight);
				VDMemcpyRect(dst2,				dstpitch2 * 2, src22,				src2pitch2 * 2, mChromaWidth, fieldHeight);

				VDMemcpyRect(dst3 + dstpitch3,	dstpitch3 * 2, src13 + src1pitch3,	src1pitch3 * 2, mChromaWidth, fieldHeight);
				VDMemcpyRect(dst3,				dstpitch3 * 2, src23,				src2pitch3 * 2, mChromaWidth, fieldHeight);
			}
		} else {
			VDMemcpyRect(dst,				dstpitch * 2, src1,				src1pitch * 2, mLumaWidth, fieldHeight);
			VDMemcpyRect(dst + dstpitch,	dstpitch * 2, src2 + src2pitch,	src2pitch * 2, mLumaWidth, fieldHeight);

			if (mChromaWidth) {
				VDMemcpyRect(dst2,				dstpitch2 * 2, src12,				src1pitch2 * 2, mChromaWidth, fieldHeight);
				VDMemcpyRect(dst2 + dstpitch2,	dstpitch2 * 2, src22 + src1pitch2,	src2pitch2 * 2, mChromaWidth, fieldHeight);

				VDMemcpyRect(dst3,				dstpitch3 * 2, src13,				src1pitch3 * 2, mChromaWidth, fieldHeight);
				VDMemcpyRect(dst3 + dstpitch3,	dstpitch3 * 2, src23 + src1pitch3,	src2pitch3 * 2, mChromaWidth, fieldHeight);
			}
		}
	}
}

bool VDVideoFilterInterlace::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	prefetcher->PrefetchFrame(0, frame * 2, 0);
	prefetcher->PrefetchFrame(0, frame * 2 + 1, 0);
	return true;
}

bool VDVideoFilterInterlace::Configure(VDXHWND hwnd) {
	VDVideoFilterInterlaceConfig old(mConfig);
	VDVideoFilterInterlaceDialog dlg(mConfig);

	if (dlg.ShowDialog((VDGUIHandle)hwnd))
		return true;

	mConfig = old;
	return false;
}

void VDVideoFilterInterlace::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (%s, %s)", mConfig.mbLaceFields ? "fields" : "frames", mConfig.mbOddFieldFirst ? "BFF" : "TFF");
}

void VDVideoFilterInterlace::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d, %d)", mConfig.mbLaceFields, mConfig.mbOddFieldFirst);
}

void VDVideoFilterInterlace::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mConfig.mbLaceFields = (0 != argv[0].asInt());
	mConfig.mbOddFieldFirst = (0 != argv[1].asInt());
}

///////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition g_VDVFInterlace =
	VDXVideoFilterDefinition<VDVideoFilterInterlace>(
		NULL,
		"interlace", 
		"Converts progressive-scan video to interlace at half the frame rate.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)

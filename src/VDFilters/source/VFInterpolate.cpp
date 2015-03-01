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
#include <emmintrin.h>
#include <vd2/system/fraction.h>
#include <vd2/system/math.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDLib/Dialog.h>
#include "resource.h"

namespace {
	void CopyPlane(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, uint32 w16, uint32 h) {
		VDMemcpyRect(dst0, dstpitch, src0, srcpitch, w16*16, h);
	}

	void LerpVec16_scalar(void *dst0, ptrdiff_t dstpitch, const void *src10, const void *src20, ptrdiff_t srcpitch, uint32 w16, uint32 h, uint32 alpha) {
		uint8 *dst = (uint8 *)dst0;
		const uint8 *src1 = (const uint8 *)src10;
		const uint8 *src2 = (const uint8 *)src20;

		int ialpha = (int)alpha;
		uint32 w = w16 << 4;
		do {
			uint8 *d = dst;
			const uint8 *s1 = src1;
			const uint8 *s2 = src2;
			for(uint32 x=0; x<w; ++x) {
				int a = *s1++;
				int b = *s2++;

				*d++ = (uint8)(a + (((b-a)*ialpha + 128) >> 8));
			}

			dst += dstpitch;
			src1 += srcpitch;
			src2 += srcpitch;
		} while(--h);
	}

	void LerpVec16_SSE2(void *dst0, ptrdiff_t dstpitch, const void *src10, const void *src20, ptrdiff_t srcpitch, uint32 w16, uint32 h, uint32 alpha) {
		uint8 *dst = (uint8 *)dst0;
		const uint8 *src1 = (const uint8 *)src10;
		const uint8 *src2 = (const uint8 *)src20;

		int coeffa = (256 - (int)alpha) * 0x00010001;
		int coeffb = (int)alpha * 0x00010001;

		const __m128i coeffa1 = _mm_shuffle_epi32(_mm_cvtsi32_si128(coeffa), 0);
		const __m128i coeffb1 = _mm_shuffle_epi32(_mm_cvtsi32_si128(coeffb), 0);
		const __m128i coeffa0 = _mm_slli_epi16(coeffa1, 8);
		const __m128i coeffb0 = _mm_slli_epi16(coeffb1, 8);
		const __m128i round = { (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0, (char)0x80, 0 };
		const __m128i mask = { 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF, 0, (char)0xFF };

		do {
			__m128i *d = (__m128i *)dst;
			const __m128i *s1 = (const __m128i *)src1;
			const __m128i *s2 = (const __m128i *)src2;
			for(uint32 x=0; x<w16; ++x) {
				__m128i a = *s1++;
				__m128i b = *s2++;
				__m128i a0 = _mm_slli_epi16(a, 8);
				__m128i a1 = _mm_srli_epi16(a, 8);
				__m128i b0 = _mm_slli_epi16(b, 8);
				__m128i b1 = _mm_srli_epi16(b, 8);
				__m128i r0 = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(_mm_mulhi_epu16(a0, coeffa0), _mm_mulhi_epu16(b0, coeffb0)), round), 8);
				__m128i r1 = _mm_add_epi16(_mm_add_epi16(_mm_mullo_epi16(a1, coeffa1), _mm_mullo_epi16(b1, coeffb1)), round);
				__m128i r = _mm_or_si128(r0, _mm_and_si128(r1, mask));

				*d++ = r;
			}

			dst += dstpitch;
			src1 += srcpitch;
			src2 += srcpitch;
		} while(--h);
	}
}

struct VDVideoFilterInterpolateConfig {
	bool mbRateScale;
	bool mbLerp;
	double mRateFactor;

	VDVideoFilterInterpolateConfig()
		: mbRateScale(true)
		, mRateFactor(2.0)
		, mbLerp(true)
	{
	}
};

class VDVideoFilterInterpolateDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterInterpolateDialog(VDVideoFilterInterpolateConfig& config, IVDXFilterPreview2 *ifp2);

protected:
	bool OnLoaded();
	bool OnCommand(uint32 id, uint32 extcode);
	void OnDataExchange(bool write);

	void UpdateEnables();

	VDVideoFilterInterpolateConfig& mConfig;
	IVDXFilterPreview2 *mifp2;
};

VDVideoFilterInterpolateDialog::VDVideoFilterInterpolateDialog(VDVideoFilterInterpolateConfig& config, IVDXFilterPreview2 *ifp2)
	: VDDialogFrameW32(IDD_FILTER_INTERPOLATE)
	, mConfig(config)
	, mifp2(ifp2)
{
}

bool VDVideoFilterInterpolateDialog::OnLoaded() {
	mifp2->InitButton((VDXHWND)GetControl(IDC_PREVIEW));

	return VDDialogFrameW32::OnLoaded();
}

bool VDVideoFilterInterpolateDialog::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_PREVIEW:
			mifp2->Toggle((VDXHWND)mhdlg);
			return true;
		case IDC_FRAMERATE_SCALE:
		case IDC_FRAMERATE_TARGET:
			if (mConfig.mbRateScale != IsButtonChecked(IDC_FRAMERATE_SCALE)) {
				mConfig.mbRateScale = !mConfig.mbRateScale;

				mifp2->RedoSystem();
				UpdateEnables();
			}
			return true;
		case IDC_INTERP_NONE:
		case IDC_INTERP_LINEAR:
			if (mConfig.mbLerp != IsButtonChecked(IDC_INTERP_LINEAR)) {
				mConfig.mbLerp = !mConfig.mbRateScale;

				mifp2->RedoSystem();
			}
			return true;
	}

	return false;
}

void VDVideoFilterInterpolateDialog::OnDataExchange(bool write) {
	if (write) {
		mConfig.mbRateScale = IsButtonChecked(IDC_FRAMERATE_SCALE);
		mConfig.mbLerp = IsButtonChecked(IDC_INTERP_LINEAR);
	} else {
		if (mConfig.mbRateScale)
			CheckButton(IDC_FRAMERATE_SCALE, true);
		else
			CheckButton(IDC_FRAMERATE_TARGET, true);

		if (mConfig.mbLerp)
			CheckButton(IDC_INTERP_LINEAR, true);
		else
			CheckButton(IDC_INTERP_NONE, true);

		UpdateEnables();
	}

	if (mConfig.mbRateScale)
		ExchangeControlValueDouble(write, IDC_EDIT_SCALE, L"%g", mConfig.mRateFactor, 0.0001, 1000.0);
	else
		ExchangeControlValueDouble(write, IDC_EDIT_TARGET, L"%g", mConfig.mRateFactor, 0.0001, 1000.0);
}

void VDVideoFilterInterpolateDialog::UpdateEnables() {
	EnableControl(IDC_EDIT_SCALE, mConfig.mbRateScale);
	EnableControl(IDC_EDIT_TARGET, !mConfig.mbRateScale);
}

class VDVideoFilterInterpolate : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Run();
	bool Configure(VDXHWND hwnd);

	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);

	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	uint32	mVec16PerRow;
	uint32	mVec16PerRow2;
	uint32	mChromaHeight;
	double	mRateFactor;

	typedef void (*LerpFn)(void *dst0, ptrdiff_t dstpitch, const void *src10, const void *src20, ptrdiff_t srcpitch, uint32 w16, uint32 h, uint32 alpha);
	LerpFn	mpLerpFn;

	VDVideoFilterInterpolateConfig mConfig;
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterInterpolate)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterInterpolate, ScriptConfig, "idi")
VDXVF_END_SCRIPT_METHODS()


uint32 VDVideoFilterInterpolate::GetParams() {
	const VDXPixmapLayout& pxsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxdst = *fa->dst.mpPixmapLayout;

	double oldRate = (double)fa->dst.mFrameRateHi / (double)fa->dst.mFrameRateLo;
	double newRate = mConfig.mRateFactor;
	
	if (mConfig.mbRateScale) {
		mRateFactor = mConfig.mRateFactor;
		newRate *= oldRate;
	} else {
		mRateFactor = newRate / oldRate;
	}

	VDFraction fr(newRate);

	fa->dst.mFrameRateHi = fr.getHi();
	fa->dst.mFrameRateLo = fr.getLo();
	if (fa->dst.mFrameCount >= 0)
		fa->dst.mFrameCount = VDRoundToInt64(fa->dst.mFrameCount * mRateFactor);

	mVec16PerRow2 = 0;
	mChromaHeight = 0;

	switch(pxsrc.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			mVec16PerRow = (pxsrc.w + 3) >> 2;
			break;
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709_FR:
			mVec16PerRow = (pxsrc.w + 15) >> 4;
			mVec16PerRow2 = (pxsrc.w + 63) >> 6;
			mChromaHeight = (pxsrc.h + 3) >> 2;
			break;
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
			mVec16PerRow = (pxsrc.w + 15) >> 4;
			mVec16PerRow2 = (pxsrc.w + 63) >> 6;
			mChromaHeight = pxsrc.h;
			break;
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_709_FR:
			mVec16PerRow = (pxsrc.w + 15) >> 4;
			mVec16PerRow2 = (pxsrc.w + 31) >> 5;
			mChromaHeight = (pxsrc.h + 1) >> 1;
			break;
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
			mVec16PerRow = (pxsrc.w + 15) >> 4;
			mVec16PerRow2 = (pxsrc.w + 31) >> 5;
			mChromaHeight = pxsrc.h;
			break;
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
			mVec16PerRow = (pxsrc.w + 15) >> 4;
			mVec16PerRow2 = (pxsrc.w + 15) >> 4;
			mChromaHeight = pxsrc.h;
			break;
		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
			mVec16PerRow = (pxsrc.w + 7) >> 3;
			break;
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_Y8_FR:
			mVec16PerRow = (pxsrc.w + 15) >> 4;
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	if (!mConfig.mbLerp) {
		pxdst = pxsrc;
		return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
	}

	if (ff->getCPUFlags() & CPUF_SUPPORTS_SSE2)
		mpLerpFn = LerpVec16_SSE2;
	else
		mpLerpFn = LerpVec16_scalar;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
}

void VDVideoFilterInterpolate::Run() {
	if (!mConfig.mbLerp)
		return;

	const VDXPixmap& pxsrc1 = *fa->mpSourceFrames[0]->mpPixmap;
	const VDXPixmap& pxdst = *fa->mpOutputFrames[0]->mpPixmap;
	uint32 alpha = (uint32)fa->mpSourceFrames[0]->mCookie;

	if (alpha) {
		const VDXPixmap& pxsrc2 = *fa->mpSourceFrames[1]->mpPixmap;

		mpLerpFn(pxdst.data, pxdst.pitch, pxsrc1.data, pxsrc2.data, pxsrc1.pitch, mVec16PerRow, pxdst.h, alpha);

		if (mChromaHeight) {
			mpLerpFn(pxdst.data2, pxdst.pitch2, pxsrc1.data2, pxsrc2.data2, pxsrc1.pitch2, mVec16PerRow2, mChromaHeight, alpha);
			mpLerpFn(pxdst.data3, pxdst.pitch3, pxsrc1.data3, pxsrc2.data3, pxsrc1.pitch3, mVec16PerRow2, mChromaHeight, alpha);
		}
	} else {
		CopyPlane(pxdst.data, pxdst.pitch, pxsrc1.data, pxsrc1.pitch, mVec16PerRow, pxdst.h);

		if (mChromaHeight) {
			CopyPlane(pxdst.data2, pxdst.pitch2, pxsrc1.data2, pxsrc1.pitch2, mVec16PerRow2, mChromaHeight);
			CopyPlane(pxdst.data3, pxdst.pitch3, pxsrc1.data3, pxsrc1.pitch3, mVec16PerRow2, mChromaHeight);
		}
	}
}

bool VDVideoFilterInterpolate::Configure(VDXHWND hwnd) {
	VDVideoFilterInterpolateConfig oldcfg(mConfig);
	VDVideoFilterInterpolateDialog dlg(mConfig, fa->ifp2);

	if (dlg.ShowDialog((VDGUIHandle)hwnd))
		return true;

	mConfig = oldcfg;
	return false;
}

bool VDVideoFilterInterpolate::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	sint64 subsrcframe = VDRoundToInt64(((double)frame + 0.5) / mRateFactor * 256.0);

	if (mConfig.mbLerp)
		subsrcframe -= 128;

	sint64 srcframe = subsrcframe >> 8;

	uint32 alpha = (uint32)subsrcframe & 0xff;

	if (alpha && mConfig.mbLerp) {
		prefetcher->PrefetchFrame(0, srcframe, alpha);
		prefetcher->PrefetchFrame(0, srcframe + 1, 0);
	} else {
		prefetcher->PrefetchFrameDirect(0, srcframe);
	}

	return true;
}

void VDVideoFilterInterpolate::GetSettingString(char *buf, int maxlen) {
	const char *mode = mConfig.mbLerp ? "linear" : "nearest";

	if (mConfig.mbRateScale)
		SafePrintf(buf, maxlen, " (x%.2f, %s)", mConfig.mRateFactor, mode);
	else
		SafePrintf(buf, maxlen, " (%.1f fps, %s)", mConfig.mRateFactor, mode);
}

void VDVideoFilterInterpolate::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d, %g, %d)", mConfig.mbRateScale, mConfig.mRateFactor, mConfig.mbLerp);
}

void VDVideoFilterInterpolate::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mConfig.mbRateScale = (argv[0].asInt() != 0);
	mConfig.mRateFactor = argv[1].asDouble();
	if (!(mConfig.mRateFactor >= 0.001))
		mConfig.mRateFactor = 0.001;
	if (!(mConfig.mRateFactor <= 1000.0))
		mConfig.mRateFactor = 1000.0;

	mConfig.mbLerp = (argv[2].asInt() != 0);
}

extern const VDXFilterDefinition g_VDVFInterpolate = VDXVideoFilterDefinition<VDVideoFilterInterpolate>(
	NULL,
	"interpolate",
	"Interpolates video to a different frame rate.");

#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{44,{flat}}' }'' : unreferenced local function has been removed

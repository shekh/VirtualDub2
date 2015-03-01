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
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>

#include "resource.h"

////////////////////////////////////////////////////////////

namespace {
	class VDPixmapDifferencer {
	public:
		VDPixmapDifferencer();

		void DifferencePixmaps(const VDPixmap& dst, const VDPixmap& src1, const VDPixmap& src2);

	protected:
		int mLookupTable[1024];
	};

	VDPixmapDifferencer::VDPixmapDifferencer() {
		for(int i=0; i<1024; ++i)
			mLookupTable[i] = (int)floor(sqrt((double)i)*8.0 + 0.5);
	}

	void VDPixmapDifferencer::DifferencePixmaps(const VDPixmap& dst, const VDPixmap& src1, const VDPixmap& src2) {
		VDASSERT(src1.format == nsVDPixmap::kPixFormat_XRGB8888);
		VDASSERT(src2.format == nsVDPixmap::kPixFormat_XRGB8888);
		VDASSERT(dst.format == nsVDPixmap::kPixFormat_Y8);
		VDASSERT(dst.w == src1.w && dst.h == src1.h);
		VDASSERT(dst.w == src2.w && dst.h == src2.h);

		uint8 *dstp = (uint8 *)dst.data;
		const uint32 *src1p = (const uint32 *)src1.data;
		const uint32 *src2p = (const uint32 *)src2.data;
		const uint32 h = dst.h;
		const uint32 w = dst.w;
		for(uint32 y=0; y<h; ++y) {
			for(uint32 x=0; x<w; ++x) {
				uint32 a = src1p[x];
				uint32 b = src2p[x];

				int dr = ((int)(a & 0xff0000) - (int)(b & 0xff0000)) >> 16;
				int dg = ((int)(a & 0x00ff00) - (int)(b & 0x00ff00)) >>  8;
				int db = ((int)(a & 0x0000ff) - (int)(b & 0x0000ff));

				int d = dr*dr + dg*dg + db*db;
				if (d > 65025)
					d = 65025;

				dstp[x] = (uint8)(d >= 0x400 ? mLookupTable[d >> 6] : mLookupTable[d] >> 3);
			}

			vdptrstep(dstp, dst.pitch);
			vdptrstep(src1p, src1.pitch);
			vdptrstep(src2p, src2.pitch);
		}
	}

	struct UnpackedPixel {
		int r, g, b;
	};

	void Unpack(UnpackedPixel& upx, uint32 px) {
		upx.r = (px >> 16) & 0xff;
		upx.g = (px >>  8) & 0xff;
		upx.b = (px >>  0) & 0xff;
	}

	uint32 Pack(const UnpackedPixel& upx) {
		return (upx.r << 16) + (upx.g << 8) + upx.b;
	}

	void Lerp(UnpackedPixel& dst, const UnpackedPixel& src1, const UnpackedPixel& src2, uint32 factor256) {
		dst.r = src1.r + (((src2.r - src1.r)*factor256 + 128) >> 8);
		dst.g = src1.g + (((src2.g - src1.g)*factor256 + 128) >> 8);
		dst.b = src1.b + (((src2.b - src1.b)*factor256 + 128) >> 8);
	}

	int Difference(const UnpackedPixel& p1, const UnpackedPixel& p2) {
		int dr = p1.r - p2.r;
		int dg = p1.g - p2.g;
		int db = p1.b - p2.b;

		return dr*dr + dg*dg + db*db;
	}

	uint32 Average(uint32 a, uint32 b) {
		return (a|b) - (((a^b) & 0xfefefefe) >> 1);
	}

	void avgscan(uint32 *dst, const uint32 *src1, const uint32 *src2, uint32 w) {
		do {
			uint32 a = *src1++;
			uint32 b = *src2++;

			*dst++	= (a|b) - (((a^b) & 0xfefefefe) >> 1);
		} while(--w);
	}

	void elascan(uint32 *dst, const uint32 *src1, const uint32 *src2, uint32 w, int *tmp) {
		uint32 a = *src1;
		uint32 b = *src2;

		*dst++ = Average(a, b);

		if (w < 2)
			return;

		w -= 2;

		int *tmp2 = tmp;
		tmp2[0] = 0;
		tmp2[1] = 0;
		tmp2[2] = 0;
		tmp2 += 3;

		UnpackedPixel x0;
		UnpackedPixel x1;
		UnpackedPixel x2;
		UnpackedPixel y0;
		UnpackedPixel y1;
		UnpackedPixel y2;

		const uint32 *src1a = src1;
		const uint32 *src2a = src2;
		Unpack(x0, *src1a++);
		Unpack(x1, *src1a++);
		Unpack(y0, *src2a++);
		Unpack(y1, *src2a++);

		int wt = w;
		for(;;) {
			Unpack(x2, *src1a++);
			Unpack(y2, *src2a++);

			tmp2[0] = Difference(x0, y2);
			tmp2[1] = Difference(x1, y1);
			tmp2[2] = Difference(x2, y0);
			tmp2 += 3;

			if (!--wt)
				break;

			Unpack(x0, *src1a++);
			Unpack(y0, *src2a++);

			tmp2[0] = Difference(x1, y0);
			tmp2[1] = Difference(x2, y2);
			tmp2[2] = Difference(x0, y1);
			tmp2 += 3;

			if (!--wt)
				break;

			Unpack(x1, *src1a++);
			Unpack(y1, *src2a++);

			tmp2[0] = Difference(x2, y1);
			tmp2[1] = Difference(x0, y0);
			tmp2[2] = Difference(x1, y2);
			tmp2 += 3;

			if (!--wt)
				break;
		}

		tmp2[0] = 0;
		tmp2[1] = 0;
		tmp2[2] = 0;

		for(uint32 x=0; x<w; ++x) {
			int sum0 = tmp[0] + tmp[3] + tmp[6];
			int sum1 = tmp[1] + tmp[4] + tmp[7];
			int sum2 = tmp[2] + tmp[5] + tmp[8];

			if (sum0 < sum2) {
				if (sum0 < sum1) {
					a = src1[0];
					b = src2[2];
				} else {
					a = src1[1];
					b = src2[1];
				}
			} else {
				if (sum2 < sum1) {
					a = src1[2];
					b = src2[0];
				} else {
					a = src1[1];
					b = src2[1];
				}
			}

			*dst++ = Average(a, b);
			tmp += 3;
			++src1;
			++src2;
		}

		a = src1[1];
		b = src2[1];

		*dst++ = Average(a, b);
	}

	void elascan2(uint32 *dst, const uint32 *src1, const uint32 *srcWeave, const uint32 *src2, const uint8 *diff1, const uint8 *diff2, const uint8 *diff3, uint32 w, int *tmp) {
		uint32 a = *src1;
		uint32 b = *src2;

		*dst++ = Average(a, b);

		if (w < 2)
			return;

		++diff1;
		++diff2;
		++diff3;
		++srcWeave;
		w -= 2;

		int *tmp2 = tmp;
		tmp2[0] = 0;
		tmp2[1] = 0;
		tmp2[2] = 0;
		tmp2 += 3;

		UnpackedPixel x0;
		UnpackedPixel x1;
		UnpackedPixel x2;
		UnpackedPixel y0;
		UnpackedPixel y1;
		UnpackedPixel y2;

		const uint32 *src1a = src1;
		const uint32 *src2a = src2;
		Unpack(x0, *src1a++);
		Unpack(x1, *src1a++);
		Unpack(y0, *src2a++);
		Unpack(y1, *src2a++);

		int wt = w;
		for(;;) {
			Unpack(x2, *src1a++);
			Unpack(y2, *src2a++);

			tmp2[0] = Difference(x0, y2);
			tmp2[1] = Difference(x1, y1);
			tmp2[2] = Difference(x2, y0);
			tmp2 += 3;

			if (!--wt)
				break;

			Unpack(x0, *src1a++);
			Unpack(y0, *src2a++);

			tmp2[0] = Difference(x1, y0);
			tmp2[1] = Difference(x2, y2);
			tmp2[2] = Difference(x0, y1);
			tmp2 += 3;

			if (!--wt)
				break;

			Unpack(x1, *src1a++);
			Unpack(y1, *src2a++);

			tmp2[0] = Difference(x2, y1);
			tmp2[1] = Difference(x0, y0);
			tmp2[2] = Difference(x1, y2);
			tmp2 += 3;

			if (!--wt)
				break;
		}

		tmp2[0] = 0;
		tmp2[1] = 0;
		tmp2[2] = 0;

		UnpackedPixel ua, ub, uc;
		for(uint32 x=0; x<w; ++x) {
			sint32 motionSum = (uint32)*diff1++ + 2*(uint32)*diff2++ + (uint32)*diff3++;

			motionSum -= 40;
			motionSum <<= 3;

			if (motionSum <= 0) {
				*dst = *srcWeave;
//				*dst = 0;
			} else {
				int sum0 = tmp[0] + tmp[3] + tmp[6];
				int sum1 = tmp[1] + tmp[4] + tmp[7];
				int sum2 = tmp[2] + tmp[5] + tmp[8];

				if (sum0 < sum2) {
					if (sum0 < sum1) {
						a = src1[0];
						b = src2[2];
					} else {
						a = src1[1];
						b = src2[1];
					}
				} else {
					if (sum2 < sum1) {
						a = src1[2];
						b = src2[0];
					} else {
						a = src1[1];
						b = src2[1];
					}
				}

				uint32 ela = Average(a, b);

				if (motionSum < 256) {
					Unpack(ua, *srcWeave);
					Unpack(ub, ela);
					Lerp(uc, ua, ub, motionSum);
					ela = Pack(uc);
//					ela = 0x010101*motionSum;
				} else {
//					ela = 0xFFFFFF;
				}

				*dst = ela;
			}
			++dst;
			tmp += 3;
			++src1;
			++srcWeave;
			++src2;
		}

		a = src1[1];
		b = src2[1];

		*dst++ = Average(a, b);
	}

	class VFBitmapRowIterator {
	public:
		VFBitmapRowIterator(const VDXBitmap& vb)
			: mpData((char *)vb.data + vb.pitch * (vb.h - 1))
			, mPitch(-vb.pitch)
		{
		}

		uint32 *operator*() { return (uint32 *)mpData; }
		const uint32 *operator*() const { return (const uint32 *)mpData; }

		uint32 *operator[](uint32 y) { return (uint32 *)(mpData + mPitch * y); }
		const uint32 *operator[](uint32 y) const { return (const uint32 *)(mpData + mPitch * y); }

		VFBitmapRowIterator& operator--() { mpData -= mPitch; return *this; }
		VFBitmapRowIterator operator--(int) {
			VFBitmapRowIterator tmp(*this);
			mpData -= mPitch;
			return tmp;
		}
		VFBitmapRowIterator& operator++() { mpData += mPitch; return *this; }
		VFBitmapRowIterator operator++(int) {
			VFBitmapRowIterator tmp(*this);
			mpData += mPitch;
			return tmp;
		}

	protected:
		char			*mpData;
		const ptrdiff_t	mPitch;
	};

	template<class T>
	class VDPixmapRowIterator {
	public:
		VDPixmapRowIterator(const VDXPixmap& px)
			: mpData((char *)px.data)
			, mPitch(px.pitch)
		{
		}

		VDPixmapRowIterator(const VDPixmap& px)
			: mpData((char *)px.data)
			, mPitch(px.pitch)
		{
		}

		T *operator*() { return (T *)mpData; }
		const T *operator*() const { return (const T *)mpData; }

		T *operator[](uint32 y) { return (T *)(mpData + mPitch * y); }
		const T *operator[](uint32 y) const { return (const T *)(mpData + mPitch * y); }

		VDPixmapRowIterator& operator--() { mpData -= mPitch; return *this; }
		VDPixmapRowIterator operator--(int) {
			VFBitmapRowIterator tmp(*this);
			mpData -= mPitch;
			return tmp;
		}
		VDPixmapRowIterator& operator++() { mpData += mPitch; return *this; }
		VDPixmapRowIterator operator++(int) {
			VDPixmapRowIterator tmp(*this);
			mpData += mPitch;
			return tmp;
		}

	protected:
		char			*mpData;
		const ptrdiff_t	mPitch;
	};
}

////////////////////////////////////////////////////////////

struct VDVideoFilterFieldBob2Config {
	enum Mode {
		kModeBob,
		kModeELA,
		kModeAdaptiveELA,
		kModeNoneHalf,
		kModeNoneFull,
		kModeCount
	};

	Mode	mMode;
	bool	mbOddFieldFirst;

	VDVideoFilterFieldBob2Config()
		: mMode(kModeBob)
		, mbOddFieldFirst(true)
	{
	}
};

class VDVideoFilterFieldBob2ConfigDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterFieldBob2ConfigDialog(VDVideoFilterFieldBob2Config& config, IVDXFilterPreview2 *ifp2);

protected:
	void OnDataExchange(bool write);
	bool OnLoaded();
	bool OnCommand(uint32 id, uint32 extcode);
	bool OnCancel();

	IVDXFilterPreview2 *mifp2;

	VDVideoFilterFieldBob2Config& mConfig;
	VDVideoFilterFieldBob2Config mConfigBackup;
};

VDVideoFilterFieldBob2ConfigDialog::VDVideoFilterFieldBob2ConfigDialog(VDVideoFilterFieldBob2Config& config, IVDXFilterPreview2 *ifp2)
	: VDDialogFrameW32(IDD_FILTER_BOBDOUBLER)
	, mifp2(ifp2)
	, mConfig(config)
	, mConfigBackup(config)
{
}

void VDVideoFilterFieldBob2ConfigDialog::OnDataExchange(bool write) {
	if (!write) {
		CheckButton(IDC_FIELDORDER_TFF, !mConfig.mbOddFieldFirst);
		CheckButton(IDC_FIELDORDER_BFF, mConfig.mbOddFieldFirst);
		CheckButton(IDC_DEINTERLACE_BOB, mConfig.mMode == VDVideoFilterFieldBob2Config::kModeBob);
		CheckButton(IDC_DEINTERLACE_ELA, mConfig.mMode == VDVideoFilterFieldBob2Config::kModeELA);
		CheckButton(IDC_DEINTERLACE_ADAPTIVEELA, mConfig.mMode == VDVideoFilterFieldBob2Config::kModeAdaptiveELA);
		CheckButton(IDC_DEINTERLACE_NONEHALF, mConfig.mMode == VDVideoFilterFieldBob2Config::kModeNoneHalf);
		CheckButton(IDC_DEINTERLACE_NONEFULL, mConfig.mMode == VDVideoFilterFieldBob2Config::kModeNoneFull);
	}
}

bool VDVideoFilterFieldBob2ConfigDialog::OnLoaded() {
	mifp2->InitButton((VDXHWND)GetControl(IDC_PREVIEW));

	return VDDialogFrameW32::OnLoaded();
}

bool VDVideoFilterFieldBob2ConfigDialog::OnCommand(uint32 id, uint32 /*extcode*/) {
	switch(id) {
		case IDC_PREVIEW:
			mifp2->Toggle((VDXHWND)mhdlg);
			return true;

		case IDC_FIELDORDER_TFF:
			if (mConfig.mbOddFieldFirst) {
				mConfig.mbOddFieldFirst = false;
				mifp2->RedoFrame();
			}
			return true;

		case IDC_FIELDORDER_BFF:
			if (!mConfig.mbOddFieldFirst) {
				mConfig.mbOddFieldFirst = true;
				mifp2->RedoFrame();
			}
			return true;

		case IDC_DEINTERLACE_BOB:
			if (mConfig.mMode != VDVideoFilterFieldBob2Config::kModeBob) {
				mConfig.mMode = VDVideoFilterFieldBob2Config::kModeBob;
				mifp2->RedoSystem();
			}
			return true;

		case IDC_DEINTERLACE_ELA:
			if (mConfig.mMode != VDVideoFilterFieldBob2Config::kModeELA) {
				mConfig.mMode = VDVideoFilterFieldBob2Config::kModeELA;
				mifp2->RedoSystem();
			}
			return true;

		case IDC_DEINTERLACE_ADAPTIVEELA:
			if (mConfig.mMode != VDVideoFilterFieldBob2Config::kModeAdaptiveELA) {
				mConfig.mMode = VDVideoFilterFieldBob2Config::kModeAdaptiveELA;
				mifp2->RedoSystem();
			}
			return true;

		case IDC_DEINTERLACE_NONEHALF:
			if (mConfig.mMode != VDVideoFilterFieldBob2Config::kModeNoneHalf) {
				mConfig.mMode = VDVideoFilterFieldBob2Config::kModeNoneHalf;
				mifp2->RedoSystem();
			}
			return true;

		case IDC_DEINTERLACE_NONEFULL:
			if (mConfig.mMode != VDVideoFilterFieldBob2Config::kModeNoneFull) {
				mConfig.mMode = VDVideoFilterFieldBob2Config::kModeNoneFull;
				mifp2->RedoSystem();
			}
			return true;
	}

	return false;
}

bool VDVideoFilterFieldBob2ConfigDialog::OnCancel() {
	mConfig = mConfigBackup;	

	return VDDialogFrameW32::OnCancel();
}

////////////////////////////////////////////////////////////

class VDVideoFilterFieldBob2 : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Start();
	void End();
	void Run();
	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	VDVideoFilterFieldBob2Config mConfig;

	vdfastvector<int> mTempRow;
	VDPixmapBuffer	mPrevBuffers[2];
	VDPixmapBuffer	mDifferenceBuffer;

	VDPixmapDifferencer	mDifferencer;
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterFieldBob2)
VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterFieldBob2, ScriptConfig, "ii")
VDXVF_END_SCRIPT_METHODS()

uint32 VDVideoFilterFieldBob2::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	VDFraction fr(fa->src.mFrameRateHi, fa->src.mFrameRateLo);

	pxldst.h &= ~1;

	if (mConfig.mMode == VDVideoFilterFieldBob2Config::kModeNoneHalf)
		pxldst.h >>= 1;

	fr *= 2;
	fa->dst.mFrameRateHi = fr.getHi();
	fa->dst.mFrameRateLo = fr.getLo();
	if (fa->dst.mFrameCount >= 0)
		fa->dst.mFrameCount *= 2;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
}

void VDVideoFilterFieldBob2::Start() {
	const VDXPixmapLayout& pxsrc = *fa->src.mpPixmapLayout;

	switch(mConfig.mMode) {
	case VDVideoFilterFieldBob2Config::kModeAdaptiveELA:
		mPrevBuffers[0].init(pxsrc.w, pxsrc.h, nsVDPixmap::kPixFormat_XRGB8888);
		mPrevBuffers[1].init(pxsrc.w, pxsrc.h, nsVDPixmap::kPixFormat_XRGB8888);
		mDifferenceBuffer.init(pxsrc.w, pxsrc.h, nsVDPixmap::kPixFormat_Y8);
		// fall through
	case VDVideoFilterFieldBob2Config::kModeELA:
		mTempRow.resize(pxsrc.w * 3);
		break;
	}
}

void VDVideoFilterFieldBob2::End() {
	mPrevBuffers[0].clear();
	mPrevBuffers[1].clear();
	mDifferenceBuffer.clear();
	mTempRow.clear();
}

void VDVideoFilterFieldBob2::Run() {
	bool second = (fa->pfsi->mOutputFrame & 1) != 0;
	bool odd = second;

	if (mConfig.mbOddFieldFirst)
		odd = !odd;

	if (mConfig.mMode == VDVideoFilterFieldBob2Config::kModeNoneHalf) {
		VDMemcpyRect(
			vdptroffset(fa->dst.data, fa->dst.pitch * (fa->dst.h - 1)),
			-fa->dst.pitch,
			vdptroffset(fa->src.data, fa->src.pitch * (fa->src.h - 1 - odd)),
			-fa->src.pitch * 2,
			fa->dst.w * 4,
			fa->dst.h);
	} else if (mConfig.mMode == VDVideoFilterFieldBob2Config::kModeNoneFull) {
		VDMemcpyRect(
			vdptroffset(fa->dst.data, fa->dst.pitch * (fa->dst.h - 1)),
			-fa->dst.pitch,
			vdptroffset(fa->src.data, fa->src.pitch * (fa->src.h - 1)),
			-fa->src.pitch,
			fa->dst.w * 4,
			fa->dst.h);
	} else if (mConfig.mMode == VDVideoFilterFieldBob2Config::kModeBob) {
		uint32 w = fa->dst.w;
		uint32 h = fa->dst.h;

		VFBitmapRowIterator srcIt(fa->src);
		VFBitmapRowIterator dstIt(fa->dst);

		memcpy(*dstIt++, srcIt[odd ? 1 : 0], 4*w);

		for(uint32 y=1; y<h-1; ++y) {
			bool scanOdd = (bool)(y & 1);

			if (scanOdd == odd)
				memcpy(*dstIt++, srcIt[y], 4*w);
			else
				avgscan(*dstIt++, srcIt[y-1], srcIt[y+1], w);
		}

		memcpy(*dstIt++, srcIt[odd ? h-1 : h-2], 4*w);
	} else if (mConfig.mMode == VDVideoFilterFieldBob2Config::kModeELA) {
		uint32 w = fa->dst.w;
		uint32 h = fa->dst.h;

		VFBitmapRowIterator srcIt(fa->src);
		VFBitmapRowIterator dstIt(fa->dst);

		memcpy(*dstIt++, srcIt[odd ? 1 : 0], 4*w);

		for(uint32 y=1; y<h-1; ++y) {
			bool scanOdd = (bool)(y & 1);

			if (scanOdd == odd)
				memcpy(*dstIt++, srcIt[y], 4*w);
			else
				elascan(*dstIt++, srcIt[y-1], srcIt[y+1], w, mTempRow.data());
		}

		memcpy(*dstIt++, srcIt[odd ? h-1 : h-2], 4*w);
	} else if (mConfig.mMode == VDVideoFilterFieldBob2Config::kModeAdaptiveELA) {

		// compute difference for new field
		const VDXPixmap& pxsrc = *fa->src.mpPixmap;
		const VDXPixmap& pxdst = *fa->dst.mpPixmap;
		uint32 w = pxdst.w;
		uint32 h = pxdst.h;
		mDifferencer.DifferencePixmaps(VDPixmapExtractField(mDifferenceBuffer, odd), VDPixmapExtractField(mPrevBuffers[0], odd), VDPixmapExtractField((const VDPixmap&)pxsrc, odd));

		VDPixmapRowIterator<uint32> srcIt(pxsrc);
		VDPixmapRowIterator<uint32> src2It(!second ? (const VDXPixmap&)static_cast<const VDPixmap&>(mPrevBuffers[0]) : static_cast<const VDXPixmap&>(pxsrc));
		VDPixmapRowIterator<uint32> dstIt(pxdst);
		VDPixmapRowIterator<uint8> diffIt(mDifferenceBuffer);

		memcpy(*dstIt++, srcIt[odd ? 1 : 0], 4*w);

		for(uint32 y=1; y<h-1; ++y) {
			bool scanOdd = (bool)(y & 1);

			if (scanOdd == odd)
				memcpy(*dstIt++, srcIt[y], 4*w);
			else
				elascan2(*dstIt++, srcIt[y-1], src2It[y], srcIt[y+1], diffIt[y-1], diffIt[y], diffIt[y+1], w, mTempRow.data());
		}

		memcpy(*dstIt++, srcIt[odd ? h-1 : h-2], 4*w);

		// rotate backing store
		if (second) {
			mPrevBuffers[0].swap(mPrevBuffers[1]);
			VDPixmapBlt(mPrevBuffers[0], (const VDPixmap&)pxsrc);
		}
	}
}

bool VDVideoFilterFieldBob2::Configure(VDXHWND hwnd) {
	VDVideoFilterFieldBob2ConfigDialog dlg(mConfig, fa->ifp2);

	return 0 != dlg.ShowDialog((VDGUIHandle)hwnd);
}

void VDVideoFilterFieldBob2::GetSettingString(char *buf, int maxlen) {
	static const char *const kDeinterlaceModes[]={
		"bob",
		"ELA",
		"adaptive ELA",
		"none-fields",
		"none-frames",
	};

	SafePrintf(buf, maxlen, " (%s, %s)", mConfig.mbOddFieldFirst ? "BFF" : "TFF", kDeinterlaceModes[mConfig.mMode]);
}

void VDVideoFilterFieldBob2::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d,%d)", mConfig.mbOddFieldFirst, mConfig.mMode);
}

void VDVideoFilterFieldBob2::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	mConfig.mbOddFieldFirst = argv[0].asInt() != 0;

	int mode = argv[1].asInt();
	if (mode < 0)
		mode = 0;
	if (mode >= VDVideoFilterFieldBob2Config::kModeCount)
		mode = VDVideoFilterFieldBob2Config::kModeCount - 1;

	mConfig.mMode = (VDVideoFilterFieldBob2Config::Mode)mode;
}

///////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition g_VDVFFieldBob2 =
	VDXVideoFilterDefinition<VDVideoFilterFieldBob2>(
		NULL,
		"bob doubler", 
		"Upsamples an interlaced video to double frame rate.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)

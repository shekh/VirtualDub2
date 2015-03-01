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
#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/vdstl.h>
#include <vd2/VDLib/fft.h>
#include "af_base.h"
#include "gui.h"
#include "resource.h"

#ifdef _MSC_VER
	#pragma warning(disable: 4324)		// warning C4324: 'VDAudioFilterPitchScale' : structure was padded due to __declspec(align())
#endif

///////////////////////////////////////////////////////////////////////////

VDAFBASE_BEGIN_CONFIG(PitchScale);
VDAFBASE_CONFIG_ENTRY(PitchScale, 0, Double, ratio, L"Pitch ratio", L"Factor by which to multiply pitch of sound.");
VDAFBASE_END_CONFIG(PitchScale, 0);

typedef VDAudioFilterData_PitchScale VDAudioFilterPitchScaleConfig;

class VDDialogAudioFilterPitchScaleConfig : public VDDialogBaseW32 {
public:
	VDDialogAudioFilterPitchScaleConfig(VDAudioFilterPitchScaleConfig& config) : VDDialogBaseW32(IDD_AF_PITCHSHIFT), mConfig(config) {}

	bool Activate(VDGUIHandle hParent) {
		return 0 != ActivateDialog(hParent);
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
		char buf[256];

		switch(msg) {
		case WM_INITDIALOG:
			sprintf(buf, "%.4f", mConfig.ratio);
			SetDlgItemText(mhdlg, IDC_RATIO, buf);
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					double v;

					if (!GetDlgItemText(mhdlg, IDC_RATIO, buf, sizeof buf) || (v=atof(buf))<0.5 || (v>2.0)) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(mhdlg, IDC_RATIO));
						return TRUE;
					}
					mConfig.ratio = v;
					End(TRUE);
				}
				return TRUE;
			case IDCANCEL:
				End(FALSE);
				return TRUE;
			}
		}

		return FALSE;
	}

	VDAudioFilterPitchScaleConfig& mConfig;
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterPitchScale : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	VDAudioFilterPitchScale();
	~VDAudioFilterPitchScale();

	uint32 Prepare();
	void Start();
	uint32 Run();
	sint64 Seek(sint64 microsecs);

	void *GetConfigPtr() { return &mConfig; }

	bool Config(HWND hwnd) {
		VDAudioFilterPitchScaleConfig	config(mConfig);

		if (!hwnd)
			return true;

		if (VDDialogAudioFilterPitchScaleConfig(config).Activate((VDGUIHandle)hwnd)) {
			mConfig = config;
			return true;
		}
		return false;
	}

protected:
	enum { kDelayLineSize = 16384 };
	enum { kDelayLineMask = kDelayLineSize - 1 };
	enum { kWindowSize = 2048 };
	enum { kHalfWindowSize = kWindowSize >> 1 };
	enum { kHalfWindowMask = kHalfWindowSize - 1 };
	enum { kOverlapTestSize = kWindowSize >> 1 };
	enum { kFFTSize = kWindowSize * 2 };
	enum { kFFTSizeBits = 12 };
	
	VDAudioFilterPitchScaleConfig	mConfig;
	vdfastvector<sint16>			mDelayLineBuffers;
	vdfastvector<float>				mFFTBuffers;
	uint32 mRate;
	uint32 mSrcOffset1;
	uint32 mSrcOffset2;
	uint32 mDstOffset;
	uint32 mInputDelay;
	uint32 mActiveInputDelay;

	struct ChannelInfo {
		sint16	*mpDelayLine;
	};

	vdfastvector<ChannelInfo> mChannels;

	sint16	mWindow[kWindowSize];
	float	mOverlapWindow[kOverlapTestSize];

	uint32	mFFTRevBits[kFFTSize];
	__declspec(align(16)) float	mFFTBuffer1[kFFTSize];
	__declspec(align(16)) float	mFFTBuffer2[kFFTSize];
	__declspec(align(16)) float	mFFTBuffer3[kFFTSize];
};

void __cdecl VDAudioFilterPitchScale::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterPitchScale;
}

VDAudioFilterPitchScale::VDAudioFilterPitchScale() {
	mConfig.ratio = 1.0;
}

VDAudioFilterPitchScale::~VDAudioFilterPitchScale() {
}

uint32 VDAudioFilterPitchScale::Prepare() {
	const VDXWaveFormat& inFormat = *mpContext->mpInputs[0]->mpFormat;

	if (   inFormat.mTag != VDXWaveFormat::kTagPCM
		|| (inFormat.mSampleBits != 8 && inFormat.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	VDXWaveFormat *pwf = mpContext->mpAudioCallbacks->CopyWaveFormat(&inFormat);

	if (!pwf) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	mpContext->mpOutputs[0]->mpFormat = pwf;

	pwf->mSampleBits	= 16;
	pwf->mBlockSize		= (uint16)(2 * pwf->mChannels);
	pwf->mDataRate		= pwf->mSamplingRate * pwf->mBlockSize;

	return 0;
}

void VDAudioFilterPitchScale::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;
	const uint32 nch = format.mChannels;

	// initialize FFT window and overlap window
	for(int i=0; i<kWindowSize; ++i) {
		mWindow[i] = (sint16)VDRoundToInt(16384.0 * (0.5 - 0.5*cos(i * (3.1415926535*2.0/(double)kWindowSize))));
	}

	for(int i=0; i<kOverlapTestSize; ++i) {
		mOverlapWindow[i] = (float)sin(i * (3.1415926535/(double)kOverlapTestSize));
	}

	mRate = (uint32)((uint64)0x1000 * mConfig.ratio);
	mInputDelay = (kWindowSize*5)>>2;

	mSrcOffset1 = 0;
	mSrcOffset2 = 0;
	mDstOffset = mInputDelay;
	mActiveInputDelay = mInputDelay;

	mChannels.resize(nch);
	mDelayLineBuffers.resize(kDelayLineSize * nch, 0);

	sint16 *pDelaySpace = mDelayLineBuffers.data();

	for(uint32 i=0; i<nch; ++i) {
		ChannelInfo& chanInfo = mChannels[i];

		chanInfo.mpDelayLine = pDelaySpace;
		pDelaySpace += kDelayLineSize;
	}

	VDMakePermuteTable(mFFTRevBits, kFFTSizeBits - 1);
}

uint32 VDAudioFilterPitchScale::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	// foo
	sint16 buf[4096];

	// compute output samples
	int samples = std::min<int>(mpContext->mCommonSamples, 4096 / format.mChannels);

	if (!samples) {
		if (pin.mbEnded && !mpContext->mInputSamples)
			return kVFARun_Finished;

		return 0;
	}

	// read buffer

	int actual_samples = mpContext->mpInputs[0]->Read(buf, samples, false, kVFARead_PCM16);
	VDASSERT(actual_samples == samples);

	const uint32 nch = format.mChannels;
	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;
	const sint16 *src = buf;
	uint32 output = 0;

	while(samples > 0) {
		if (mActiveInputDelay) {
			uint32 discard = mActiveInputDelay;

			if (discard > samples)
				discard = samples;

			mActiveInputDelay -= discard;
			samples -= discard;

			for(unsigned ch=0; ch<nch; ++ch) {
				ChannelInfo& chanInfo = mChannels[ch];
				const sint16 *src2 = src + ch;
				sint16 *delay = chanInfo.mpDelayLine;

				// write new samples
				for(int i=0; i<discard; ++i) {
					delay[(mDstOffset + i) & kDelayLineMask] = *src2;
					src2 += nch;
				}
			}

			mDstOffset += discard;
			src += discard*nch;
			continue;
		}

		int tc = kHalfWindowSize - (mDstOffset & (kHalfWindowSize - 1));
		if (tc > samples)
			tc = samples;

		for(unsigned ch=0; ch<nch; ++ch) {
			ChannelInfo& chanInfo = mChannels[ch];
			const sint16 *src2 = src + ch;
			sint16 *delay = chanInfo.mpDelayLine;
			sint16 *dst2 = dst + ch;
			const sint16 *win = mWindow + (mDstOffset & (kWindowSize - 1));

			// write new samples
			for(int i=0; i<tc; ++i) {
				delay[(mDstOffset + i) & kDelayLineMask] = *src2;
				src2 += nch;
			}

			// scan out old samples
			const uint32 srcStep = mRate;
			uint32 srcOffset1 = mSrcOffset1;
			uint32 srcOffset2 = mSrcOffset2;
			for(int i=0; i<tc; ++i) {
				sint16 src1 = delay[(srcOffset1 >> 12) & kDelayLineMask];
				sint16 src2 = delay[(srcOffset2 >> 12) & kDelayLineMask];
				*dst2 = (sint16)(src2 + ((((sint32)src1 - (sint32)src2)*(*win++) + 8192) >> 14));
				dst2 += nch;

				srcOffset1 += srcStep;
				srcOffset2 += srcStep;
			}

			if (ch == (nch - 1)) {
				mSrcOffset1 = srcOffset1;
				mSrcOffset2 = srcOffset2;
			}
		}

		mDstOffset += tc;
		dst += tc*nch;
		src += tc*nch;
		output += tc;

		if (!(mDstOffset & kHalfWindowMask)) {
			int i;
			uint32 currentBase = (mDstOffset & (kWindowSize >> 1)) ? mSrcOffset1 >> 12 : mSrcOffset2 >> 12;
			uint32 base = mDstOffset - ((kWindowSize*3)>>1);

			// compute correlations
			for(unsigned ch=0; ch<nch; ++ch) {
				ChannelInfo& chanInfo = mChannels[ch];

				// compute FFT of search pattern
				for(i=0; i<kOverlapTestSize; ++i)
					mFFTBuffer1[i] = chanInfo.mpDelayLine[(currentBase + i) & (kDelayLineSize - 1)] * mOverlapWindow[i];

				memset(mFFTBuffer1 + kOverlapTestSize, 0, sizeof(float) * (kFFTSize - kOverlapTestSize));
				VDPermuteRevBitsComplex(mFFTBuffer1, kFFTSizeBits - 1, mFFTRevBits);
				VDComputeRealFFT(mFFTBuffer1, kFFTSizeBits);

				// compute FFT of search area
				for(i=0; i<((kWindowSize*3)>>1); ++i)
					mFFTBuffer2[i] = chanInfo.mpDelayLine[(base + i) & (kDelayLineSize - 1)];

				memset(mFFTBuffer2 + ((kWindowSize*3)>>1), 0, sizeof(float) * (kFFTSize - ((3*kWindowSize)>>1)));

				VDPermuteRevBitsComplex(mFFTBuffer2, kFFTSizeBits - 1, mFFTRevBits);
				VDComputeRealFFT(mFFTBuffer2, kFFTSizeBits);

				if (!ch) {
					mFFTBuffer3[0] = mFFTBuffer1[0]*mFFTBuffer2[0];
					mFFTBuffer3[1] = mFFTBuffer1[1]*mFFTBuffer2[1];
					for(i=2; i<kFFTSize; i+=2) {
						float r1 = mFFTBuffer1[i];
						float i1 = mFFTBuffer1[i+1];
						float r2 = mFFTBuffer2[i];
						float i2 = mFFTBuffer2[i+1];

						// (A-Bi)*(C+Di) = (AC+BD)+(AD-BC)i
						mFFTBuffer3[i  ] = r1*r2 + i1*i2;
						mFFTBuffer3[i+1] = r1*i2 - i1*r2;
					}
				} else {
					mFFTBuffer3[0] += mFFTBuffer1[0]*mFFTBuffer2[0];
					mFFTBuffer3[1] += mFFTBuffer1[1]*mFFTBuffer2[1];
					for(i=2; i<kFFTSize; i+=2) {
						float r1 = mFFTBuffer1[i];
						float i1 = mFFTBuffer1[i+1];
						float r2 = mFFTBuffer2[i];
						float i2 = mFFTBuffer2[i+1];

						// (A-Bi)*(C+Di) = (AC+BD)+(AD-BC)i
						mFFTBuffer3[i  ] += r1*r2 + i1*i2;
						mFFTBuffer3[i+1] += r1*i2 - i1*r2;
					}
				}
			}

			VDComputeRealIFFT(mFFTBuffer3, kFFTSizeBits);
			VDPermuteRevBitsComplex(mFFTBuffer3, kFFTSizeBits - 1, mFFTRevBits);

			int maxpos = kWindowSize >> 2;
			for(i=0; i<=(kWindowSize>>1); ++i) {
				if (mFFTBuffer3[i] > mFFTBuffer3[maxpos])
					maxpos = i;
			}

			if (mDstOffset & (kWindowSize >> 1))
				mSrcOffset2 = (base + maxpos) << 12;
			else
				mSrcOffset1 = (base + maxpos) << 12;
		}

		samples -= tc;
	}

	mpContext->mpOutputs[0]->mSamplesWritten = output;

	return 0;
}

sint64 VDAudioFilterPitchScale::Seek(sint64 microsecs) {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;
	const uint32 nch = format.mChannels;

	for(unsigned ch=0; ch<nch; ++ch) {
		ChannelInfo& chanInfo = mChannels[ch];

		memset(chanInfo.mpDelayLine, 0, sizeof(sint16)*kDelayLineSize);
	}

	mSrcOffset1 = 0;
	mSrcOffset2 = 0;
	mDstOffset = mInputDelay;
	mActiveInputDelay = mInputDelay;

	return microsecs;
}

extern const struct VDAudioFilterDefinition afilterDef_pitchscale = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_HasConfig,

	sizeof(VDAudioFilterPitchScale),	1,	1,

	&VDAudioFilterData_PitchScale::members.info,

	VDAudioFilterPitchScale::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_pitchscale = {
	sizeof(VDPluginInfo),
	L"pitch shift",
	NULL,
	L"Scales the pitch of audio by a fixed ratio.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_pitchscale
};

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
#include <vfw.h>
#include <vd2/system/file.h>
#include <vd2/system/Error.h>
#include <vd2/Dita/resources.h>

#include "gui.h"
#include "AudioSourceAVI.h"
#include "AVIReadHandler.h"

//////////////////////////////////////////////////////////////////////////////

extern bool VDPreferencesIsAVIVBRWarningEnabled();

//////////////////////////////////////////////////////////////////////////////

namespace {
	enum { kVDST_AudioSource = 6 };

	enum {
		kVDM_TruncatedMP3FormatFixed,
		kVDM_VBRAudioDetected,
		kVDM_MP3BitDepthFixed,
		kVDM_TruncatedCompressedFormatFixed
	};
}

///////////////////////////

AudioSourceAVI::AudioSourceAVI(InputFileAVI *pParent, IAVIReadHandler *pAVI, int streamIndex, bool bAutomated)
	: VDAudioSourceAVISourced(pParent)
	, mStreamIndex(streamIndex)
{
	pAVIFile	= pAVI;
	pAVIStream	= NULL;
	bQuiet = bAutomated;	// ugh, this needs to go... V1.5.0.
}

AudioSourceAVI::~AudioSourceAVI() {
	if (pAVIStream)
		delete pAVIStream;
}

bool AudioSourceAVI::init() {
	LONG format_len;

	pAVIStream = pAVIFile->GetStream(streamtypeAUDIO, mStreamIndex);
	if (!pAVIStream) return FALSE;

	if (pAVIStream->Info(&streamInfo))
		return FALSE;

	pAVIStream->FormatSize(0, &format_len);

	if (!allocFormat(format_len)) return FALSE;

	if (pAVIStream->ReadFormat(0, getFormat(), &format_len))
		return FALSE;

	mSampleFirst = pAVIStream->Start();
	mSampleLast = pAVIStream->End();

	// Check for invalid (truncated) MP3 format.
	VDWaveFormat *pwfex = (VDWaveFormat *)getWaveFormat();

	if (pwfex->mTag == WAVE_FORMAT_MPEGLAYER3) {
		if (format_len < sizeof(MPEGLAYER3WAVEFORMAT)) {
			MPEGLAYER3WAVEFORMAT wf;

			wf.wfx				= *(const WAVEFORMATEX *)pwfex;
			wf.wfx.cbSize		= MPEGLAYER3_WFX_EXTRA_BYTES;

			wf.wID				= MPEGLAYER3_ID_MPEG;

			// Attempt to detect the padding mode and block size for the stream.

			double byterate = wf.wfx.nAvgBytesPerSec;
			double fAverageFrameSize = 1152.0 * byterate / wf.wfx.nSamplesPerSec;

			int estimated_bitrate = (int)floor(0.5 + byterate * (1.0/1000.0)) * 8;
			double fEstimatedFrameSizeISO = 144000.0 * estimated_bitrate / wf.wfx.nSamplesPerSec;

			if (wf.wfx.nSamplesPerSec < 32000) {	// MPEG-2?
				fAverageFrameSize *= 0.5;
				fEstimatedFrameSizeISO *= 0.5;
			}

			double fEstimatedFrameSizePaddingOff = floor(fEstimatedFrameSizeISO);
			double fEstimatedFrameSizePaddingOn = fEstimatedFrameSizePaddingOff + 1.0;
			double fErrorISO = fabs(fEstimatedFrameSizeISO - fAverageFrameSize);
			double fErrorPaddingOn = fabs(fEstimatedFrameSizePaddingOn - fAverageFrameSize);
			double fErrorPaddingOff = fabs(fEstimatedFrameSizePaddingOff - fAverageFrameSize);

			if (fErrorISO <= fErrorPaddingOn)				// iso < on
				if (fErrorISO <= fErrorPaddingOff)			// iso < on, off
					wf.fdwFlags			= MPEGLAYER3_FLAG_PADDING_ISO;
				else										// off < iso < on
					wf.fdwFlags			= MPEGLAYER3_FLAG_PADDING_OFF;
			else											// on < iso
				if (fErrorPaddingOn <= fErrorPaddingOff)	// on < iso, off
					wf.fdwFlags			= MPEGLAYER3_FLAG_PADDING_ON;
				else										// off < on < iso
					wf.fdwFlags			= MPEGLAYER3_FLAG_PADDING_OFF;

			wf.nBlockSize		= (uint16)floor(0.5 + fAverageFrameSize);	// This is just a guess.  The MP3 codec always turns padding off, so I don't know whether this should be rounded up or not.
			wf.nFramesPerBlock	= 1;
			wf.nCodecDelay		= 1393;									// This is the number of samples padded by the compressor.  1393 is the value typically written by the codec.

			if (!allocFormat(sizeof wf))
				return FALSE;

			pwfex = (VDWaveFormat *)getWaveFormat();
			memcpy(pwfex, &wf, sizeof wf);

			const int bad_len = format_len;
			const int good_len = sizeof(MPEGLAYER3WAVEFORMAT);
			VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_TruncatedMP3FormatFixed, 2, &bad_len, &good_len);
		}

		// Check if the wBitsPerSample tag is something other than zero, and reset it
		// if so.
		if (pwfex->mSampleBits != 0) {
			pwfex->mSampleBits = 0;

			VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_MP3BitDepthFixed, 0);
		}
	} else {
		uint32 cbSize = 0;

		if (format_len >= sizeof(WAVEFORMATEX))
			cbSize = pwfex->mExtraSize;

		uint32 requiredFormatSize = sizeof(WAVEFORMATEX) + cbSize;
		if ((uint32)format_len < requiredFormatSize && pwfex->mTag != WAVE_FORMAT_PCM) {
			vdstructex<WAVEFORMATEX> newFormat(requiredFormatSize);
			memset(newFormat.data(), 0, requiredFormatSize);
			memcpy(newFormat.data(), pwfex, format_len);

			if (!allocFormat(requiredFormatSize))
				return FALSE;

			pwfex = (VDWaveFormat *)getWaveFormat();
			memcpy(pwfex, &*newFormat, requiredFormatSize);

			const int bad_len = format_len;
			const int good_len = sizeof(WAVEFORMATEX) + cbSize;
			VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_TruncatedCompressedFormatFixed, 2, &bad_len, &good_len);
		}
	}

	// Check for VBR format.
	if (!bQuiet && VDPreferencesIsAVIVBRWarningEnabled()) {
		double mean, stddev, maxdev;

		if (pAVIStream->getVBRInfo(mean, stddev, maxdev)) {
			double meanOut = mean*0.001;
			double stddevOut = stddev*0.001;
			double maxdevOut = maxdev*1000.0;

			VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_VBRAudioDetected, 3, &maxdevOut, &meanOut, &stddevOut);
		}
	}

	return TRUE;
}

void AudioSourceAVI::Reinit() {
	pAVIStream->Info(&streamInfo);

	mSampleFirst = pAVIStream->Start();
	mSampleLast = pAVIStream->End();
}

int AudioSourceAVI::GetPreloadSamples() {
	long bytes, samples;
	if (pAVIStream->Read(mSampleFirst, AVISTREAMREAD_CONVENIENT, NULL, 0, &bytes, &samples))
		return 0;

	sint64 astart = pAVIStream->getSampleBytePosition(mSampleFirst);

	if (astart < 0)
		return (int)samples;

	IAVIReadStream *pVS = pAVIFile->GetStream(streamtypeVIDEO, 0);
	if (!pVS)
		return (int)samples;

	sint64 vstart = pVS->getSampleBytePosition(pVS->Start());
	delete pVS;

	if (vstart >= 0 && vstart < astart)
		return 0;

	return (int)samples;
}

bool AudioSourceAVI::isStreaming() {
	return pAVIStream->isStreaming();
}

void AudioSourceAVI::streamBegin(bool fRealTime, bool bForceReset) {
	pAVIStream->BeginStreaming(mSampleFirst, mSampleLast, fRealTime ? 1000 : 2000);
}

void AudioSourceAVI::streamEnd() {
	pAVIStream->EndStreaming();

}

bool AudioSourceAVI::_isKey(VDPosition lSample) {
	return pAVIStream->IsKeyFrame(lSample);
}

int AudioSourceAVI::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lpBytesRead, uint32 *lpSamplesRead) {
	int err;
	long lBytes, lSamples;

	// There are some video clips roaming around with truncated audio streams
	// (audio streams that state their length as being longer than they
	// really are).  We use a kludge here to get around the problem.

	err = pAVIStream->Read(lStart, lCount, lpBuffer, cbBuffer, &lBytes, &lSamples);

	if (lpBytesRead)
		*lpBytesRead = lBytes;
	if (lpSamplesRead)
		*lpSamplesRead = lSamples;

	if (err != AVIERR_FILEREAD)
		return err;

	// Suspect a truncated stream.
	//
	// AVISTREAMREAD_CONVENIENT will tell us if we're actually encountering a
	// true read error or not.  At least for the AVI handler, it returns
	// AVIERR_ERROR if we've broached the end.  

	*lpBytesRead = *lpSamplesRead = 0;

	while(lCount > 0) {
		err = pAVIStream->Read(lStart, AVISTREAMREAD_CONVENIENT, NULL, 0, &lBytes, &lSamples);

		if (err)
			return 0;

		if (!lSamples) return IVDStreamSource::kOK;

		if (lSamples > lCount) lSamples = lCount;

		err = pAVIStream->Read(lStart, lSamples, lpBuffer, cbBuffer, &lBytes, &lSamples);

		if (err)
			return err;

		lpBuffer = (LPVOID)((char *)lpBuffer + lBytes);
		cbBuffer -= lBytes;
		lCount -= lSamples;

		*lpBytesRead += lBytes;
		*lpSamplesRead += lSamples;
	}

	return IVDStreamSource::kOK;
}

VDPosition AudioSourceAVI::TimeToPositionVBR(VDTime us) const {
	return pAVIStream->TimeToPosition(us);
}

VDTime AudioSourceAVI::PositionToTimeVBR(VDPosition samples) const {
	return pAVIStream->PositionToTime(samples);
}

IVDStreamSource::VBRMode AudioSourceAVI::GetVBRMode() const {
	double bitrate_mean, bitrate_stddev, maxdev;
	return pAVIStream->getVBRInfo(bitrate_mean, bitrate_stddev, maxdev) ? kVBRModeVariableFrames : kVBRModeNone;
}

///////////////////////////////////////////////////////////////////////////

AudioSourceDV::AudioSourceDV(InputFileAVI *pParent, IAVIReadStream *pStream, bool bAutomated)
	: VDAudioSourceAVISourced(pParent)
	, mpStream(pStream)
	, mLastFrame(-1)
	, mErrorMode(kErrorModeReportAll)
	, mbGatherTabInited(false)
	, mbGatherTabInitedAsPAL(false)
	, mbGatherTabInitedAs16Bit(false)
{
	bQuiet = bAutomated;	// ugh, this needs to go... V1.5.0.
}

AudioSourceDV::~AudioSourceDV() {
	if (mpStream)
		delete mpStream;
}

bool AudioSourceDV::init() {
	LONG format_len;

	mpStream->FormatSize(0, &format_len);

	if (!allocFormat(sizeof(WAVEFORMATEX)))
		return false;

	WAVEFORMATEX *pwfex = (WAVEFORMATEX *)getFormat();

	// fetch AAUX packet from stream format and determine sampling rate
	long size;
	if (FAILED(mpStream->FormatSize(0, &size)))
		return false;

	if (size < 24)
		return false;

	vdblock<uint8> format(size);
	if (FAILED(mpStream->ReadFormat(0, format.data(), &size)))
		return false;

	uint8 aaux_as_pc4 = format[3];
	uint8 vaux_vs_pc3 = format[18];

	// Sometimes the values in the DVINFO block are wrong, so attempt
	// to extract from the first frame instead.
	const VDPosition streamStart = mpStream->Start();
	const VDPosition streamEnd = mpStream->End();
	VDPosition streamTestPos = streamStart;
	sint32 samplingRate;
	
	for(int i=0; i<5; ++i) {
		if (streamTestPos < streamEnd) {
			long bytes, samples;

			if (!mpStream->Read(streamTestPos, 1, NULL, 0, &bytes, &samples) && bytes >= 120000) {
				vdblock<uint8> tmp(bytes);

				if (!mpStream->Read(streamTestPos, 1, tmp.data(), tmp.size(), &bytes, &samples)) {
					aaux_as_pc4 = tmp[80*(3*150 + 6) + 7];		// DIF sequence 3, block 6, AAUX pack 0
					vaux_vs_pc3 = tmp[80*(1*150 + 3) + 6];		// DIF sequence 1, block 3, VAUX pack 0
				}
			}
		}

		bool isPAL = 0 != (vaux_vs_pc3 & 0x20);

		switch(aaux_as_pc4 & 0x38) {
		case 0x00:
			samplingRate = 48000;
			mSamplesPerSet		= isPAL ? 19200 : 16016;
			goto open_ok;
		case 0x08:
			samplingRate = 44100;
			mSamplesPerSet		= isPAL ? 17640 : 14715;
			goto open_ok;
		case 0x10:
			samplingRate = 32000;
			mSamplesPerSet		= isPAL ? 12800 : 10677;
			goto open_ok;
		default:
			break;
		}

		streamTestPos += (uint64)(1 << i);
	}

open_ok:

	// check for 12-bit quantization
	mTempBuffer.resize(144000);		// always use PAL worst case

	pwfex->wFormatTag		= WAVE_FORMAT_PCM;
	pwfex->nChannels		= 2;
	pwfex->nSamplesPerSec	= samplingRate;
	pwfex->nAvgBytesPerSec	= samplingRate*4;
	pwfex->nBlockAlign		= 4;
	pwfex->wBitsPerSample	= 16;
	pwfex->cbSize			= 0;

	mGatherTab.resize(1960);
	memset(mGatherTab.data(), 0, mGatherTab.size() * sizeof mGatherTab[0]);
	mbGatherTabInited = false;

	if (mpStream->Info(&streamInfo))
		return false;

	// wonk most of the stream values since they're not appropriate for audio
	streamInfo.fccType			= streamtypeAUDIO;
	streamInfo.fccHandler		= 0;
	streamInfo.dwStart			= VDRoundToInt((double)streamInfo.dwScale / streamInfo.dwRate * samplingRate);
	streamInfo.dwRate			= pwfex->nAvgBytesPerSec;
	streamInfo.dwScale			= pwfex->nBlockAlign;
	streamInfo.dwInitialFrames	= 0;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality		= (DWORD)-1;
	streamInfo.dwSampleSize		= pwfex->nBlockAlign;
	streamInfo.rcFrameLeft		= 0;
	streamInfo.rcFrameTop		= 0;
	streamInfo.rcFrameRight		= 0;
	streamInfo.rcFrameBottom	= 0;

	Reinit();

	return true;
}

void AudioSourceDV::Reinit() {
	const VDPosition start = mpStream->Start();
	const VDPosition end = mpStream->End();

	mRawStart		= start;
	mRawEnd			= end;
	mRawFrames		= end - start;
	mSampleFirst	= (start * mSamplesPerSet) / 10;
	mSampleLast		= (start+end * mSamplesPerSet) / 10;

	VDPosition len(mSampleLast - mSampleFirst);
	streamInfo.dwLength	= (uint32)len == len ? (uint32)len : 0xFFFFFFFF;

	FlushCache();
}

bool AudioSourceDV::isStreaming() {
	return mpStream->isStreaming();
}

void AudioSourceDV::streamBegin(bool fRealTime, bool bForceReset) {
	mpStream->BeginStreaming(mSampleFirst, mSampleLast, fRealTime ? 1000 : 2000);
}

void AudioSourceDV::streamEnd() {
	mpStream->EndStreaming();

}

bool AudioSourceDV::_isKey(VDPosition lSample) {
	return true;
}

int AudioSourceDV::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lpBytesRead, uint32 *lpSamplesRead) {
	if (lpBuffer && cbBuffer < 4)
		return IVDStreamSource::kBufferTooSmall;

	if (lCount == IVDStreamSource::kConvenient)
		lCount = mSamplesPerSet;

	VDPosition baseSet = lStart / mSamplesPerSet;
	uint32 offset = (uint32)(lStart % mSamplesPerSet);

	if (lCount > mSamplesPerSet - offset)
		lCount = mSamplesPerSet - offset;

	if (lpBuffer && lCount > (cbBuffer>>2))
		lCount = cbBuffer>>2;

	if (lpBuffer) {
		const CacheLine *pLine = LoadSet(baseSet);
		if (!pLine)
			throw MyError("Unable to read audio samples starting at %u from the DV stream.", (unsigned)lStart);

		memcpy(lpBuffer, (char *)pLine->mResampledData + offset*4, 4*lCount);
	}

	if (lpBytesRead)
		*lpBytesRead = lCount * 4;
	if (lpSamplesRead)
		*lpSamplesRead = lCount;

	return IVDStreamSource::kOK;
}

const AudioSourceDV::CacheLine *AudioSourceDV::LoadSet(VDPosition setpos) {
	// For now we will be lazy and just direct map the cache.
	unsigned line = (unsigned)setpos & 3;

	CacheLine& cline = mCache[line];

	if (mCacheLinePositions[line] != setpos) {
		mCacheLinePositions[line] = -1;

		// load up to 10 frames and linearize the raw data
		uint32 setSize = mSamplesPerSet;
		VDPosition pos = mRawStart + 10 * setpos;
		VDPosition limit = pos + 10;
		if (limit > mRawEnd) {
			limit = mRawEnd;

			setSize = mSamplesPerSet * ((sint32)limit - (sint32)pos) / 10;
		}
		cline.mRawSamples = 0;

		uint8 *dst = (uint8 *)cline.mRawData;

		while(pos < limit) {
			long bytes, samples;

			int err = mpStream->Read(pos++, 1, mTempBuffer.data(), mTempBuffer.size(), &bytes, &samples);
			if (err)
				return NULL;

			if (!bytes) {
zero_fill:
				uint32 n = mSamplesPerSet / 10;

				if (cline.mRawSamples+n >= sizeof cline.mRawData / sizeof cline.mRawData[0]) {
					VDDEBUG("AudioSourceDV: Sample count overflow!\n");
					VDASSERT(false);
					break;
				}

				cline.mRawSamples += n;

				memset(dst, 0, n*4);
				dst += n*4;
			} else {
				if (bytes != 120000 && bytes != 144000)
					return NULL;

				const uint8 *pAAUX = &mTempBuffer[80*(1*150 + 6) + 3];

				uint8 vaux_vs_pc3 = mTempBuffer[80*(1*150 + 3) + 6];		// DIF sequence 1, block 3, VAUX pack 0

				bool isPAL = 0 != (vaux_vs_pc3 & 0x20);

				if (bytes != (isPAL ? 144000 : 120000)) {
					if (mErrorMode != kErrorModeReportAll)
						goto zero_fill;

					return NULL;
				}

				uint32 minimumFrameSize;

				switch(pAAUX[4] & 0x38) {
				case 0x00:
					minimumFrameSize	= isPAL ? 1896 : 1580;
					break;
				case 0x08:
					minimumFrameSize	= isPAL ? 1742 : 1452;
					break;
				case 0x10:
					minimumFrameSize	= isPAL ? 1264 : 1053;
					break;
				default:
					if (mErrorMode != kErrorModeReportAll)
						goto zero_fill;

					return NULL;
				}

				const uint32 n = minimumFrameSize + (pAAUX[1] & 0x3f);

				if (cline.mRawSamples+n >= sizeof cline.mRawData / sizeof cline.mRawData[0]) {
					VDDEBUG("AudioSourceDV: Sample count overflow!\n");
					VDASSERT(false);
					break;
				}

				cline.mRawSamples += n;

				if ((pAAUX[4] & 7) == 1) {	// 1: 12-bit nonlinear
					const uint8 *src0 = (const uint8 *)mTempBuffer.data();
					const sint32 *pOffsets = GetGatherTab(isPAL, false);

					sint16 *dst16 = (sint16 *)dst;
					dst += 4*n;

					for(int i=0; i<n; ++i) {
						const ptrdiff_t pos = *pOffsets++;
						const uint8 *srcF = src0 + pos;

						// Convert 12-bit nonlinear (one's complement floating-point) sample to 16-bit linear.
						// This value is a 1.3.8 floating-point value similar to IEEE style, except that the
						// sign is represented via 1's-c rather than sign-magnitude.
						//
						// Thus, 0000..00FF are positive denormals, 8000 is the largest negative value, etc.

						sint32 vL = ((sint32)srcF[0]<<4) + (srcF[2]>>4);		// reconstitute left 12-bit value
						sint32 vR = ((sint32)srcF[1]<<4) + (srcF[2] & 15);		// reconstitute right 12-bit value

						static const sint32 addend[16]={
							-0x0000 << 0,
							-0x0000 << 0,
							-0x0100 << 1,
							-0x0200 << 2,
							-0x0300 << 3,
							-0x0400 << 4,
							-0x0500 << 5,
							-0x0600 << 6,

							-0x09ff << 6,
							-0x0aff << 5,
							-0x0bff << 4,
							-0x0cff << 3,
							-0x0dff << 2,
							-0x0eff << 1,
							-0x0fff << 0,
							-0x0fff << 0,
						};
						static const int	shift [16]={0,0,1,2,3,4,5,6,6,5,4,3,2,1,0,0};

						int expL = vL >> 8;
						int expR = vR >> 8;

						dst16[0] = (sint16)((vL << shift[expL]) + addend[expL]);
						dst16[1] = (sint16)((vR << shift[expR]) + addend[expR]);
						dst16 += 2;
					}
				} else {					// 0: 16-bit linear
					const uint8 *src0 = (const uint8 *)mTempBuffer.data();
					const ptrdiff_t rightOffset = isPAL ? 12000*6 : 12000*5;	// left channel is first 5/6 DIF sequences
					const sint32 *pOffsets = GetGatherTab(isPAL, true);

					for(int i=0; i<n; ++i) {
						const ptrdiff_t pos = *pOffsets++;
						const uint8 *srcL = src0 + pos;
						const uint8 *srcR = srcL + rightOffset;

						// convert big-endian sample to little-endian
						dst[0] = srcL[1];
						dst[1] = srcL[0];
						dst[2] = srcR[1];
						dst[3] = srcR[0];
						dst += 4;
					}
				}
			}
		}

		// resample if required
		if (cline.mRawSamples == setSize) {
			// no resampling required -- straight copy
			memcpy(cline.mResampledData, cline.mRawData, 4*setSize);
		} else {
			const sint16 *src = &cline.mRawData[0][0];
			      sint16 *dst = &cline.mResampledData[0][0];

			// copy first sample
			dst[0] = src[0];
			dst[1] = src[1];
			dst += 2;

			// linearly interpolate middle samples
			uint32 dudx = ((cline.mRawSamples-1) << 16) / (setSize-1);
			uint32 u = dudx + (dudx >> 1) - 0x8000;

			VDASSERT((sint32)u >= 0);

			unsigned n = setSize-2;
			do {
				const sint16 *src2 = src + (u>>16)*2;
				sint32 f = (u>>4)&0xfff;

				u += dudx;

				sint32 a0 = src2[0];
				sint32 b0 = src2[1];
				sint32 da = (sint32)src2[2] - a0;
				sint32 db = (sint32)src2[3] - b0;

				dst[0] = (sint16)(a0 + ((da*f + 0x800) >> 12));
				dst[1] = (sint16)(b0 + ((db*f + 0x800) >> 12));
				dst += 2;
			} while(--n);

			VDASSERT(((u - dudx) >> 16) < cline.mRawSamples);

			// copy last sample
			dst[0] = src[cline.mRawSamples*2-2];
			dst[1] = src[cline.mRawSamples*2-1];
		}

		VDDEBUG("AudioSourceDV: Loaded cache line %u for %u raw samples (%u samples expected)\n", (unsigned)setpos*10, cline.mRawSamples, setSize);

		mCacheLinePositions[line] = setpos;
	}

	return &cline;
}

void AudioSourceDV::FlushCache() {
	for(int i=0; i<kCacheLines; ++i)
		mCacheLinePositions[i] = -1;
}

const sint32 *AudioSourceDV::GetGatherTab(bool pal, bool sixteenBit) {
	if (!mbGatherTabInited || mbGatherTabInitedAsPAL != pal || mbGatherTabInitedAs16Bit != sixteenBit) {
		mbGatherTabInited = true;
		mbGatherTabInitedAsPAL = pal;
		mbGatherTabInitedAs16Bit = sixteenBit;

		unsigned bytesPerSample = sixteenBit ? 2 : 3;
		if (pal) {
			for(int i=0; i<1944; ++i) {
				int dif_sequence	= ((i/3)+2*(i%3))%6;
				int dif_block		= 6 + 16*(3*(i%3) + ((i%54)/18));
				int byte_offset		= 8 + bytesPerSample*(i/54);

				mGatherTab[i] = 12000*dif_sequence + 80*dif_block + byte_offset;
			}
		} else {
			for(int i=0; i<1620; ++i) {
				int dif_sequence	= ((i/3)+2*(i%3))%5;
				int dif_block		= 6 + 16*(3*(i%3) + ((i%45)/15));
				int byte_offset		= 8 + bytesPerSample*(i/45);

				mGatherTab[i] = 12000*dif_sequence + 80*dif_block + byte_offset;
			}
		}
	}

	return mGatherTab.data();
}

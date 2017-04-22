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

#include "VideoSequenceCompressor.h"
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/log.h>
#include <vd2/system/protscope.h>
#include <vd2/system/w32assist.h>
#include <vd2/Dita/resources.h>
#include <vd2/Riza/videocodec.h>
#include "crash.h"
#include "misc.h"

extern IVDVideoCodecBugTrap *g_pVDVideoCodecBugTrap;

//////////////////////////////////////////////////////////////////////////////
//
//	IMITATING WIN2K AVISAVEV() BEHAVIOR IN 0x7FFFFFFF EASY STEPS
//
//	It seems some codecs are rather touchy about how exactly you call
//	them, and do a variety of odd things if you don't imitiate the
//	standard libraries... compressing at top quality seems to be the most
//	common symptom.
//
//	ICM_COMPRESS_FRAMES_INFO:
//
//		dwFlags			Trashed with address of lKeyRate in tests. Something
//						might be looking for a non-zero value here, so better
//						set it.
//		lpbiOutput		NULL.
//		lOutput			0.
//		lpbiInput		NULL.
//		lInput			0.
//		lStartFrame		0.
//		lFrameCount		Number of frames.
//		lQuality		Set to quality factor, or zero if not supported.
//		lDataRate		Set to data rate in 1024*kilobytes, or zero if not
//						supported.
//		lKeyRate		Set to the desired maximum keyframe interval.  For
//						all keyframes, set to 1.		
//
//	ICM_COMPRESS:
//
//		lpbiOutput->biSizeImage	Indeterminate (same as last time).
//
//		dwFlags			Equal to ICCOMPRESS_KEYFRAME if a keyframe is
//						required, and zero otherwise.
//		lpckid			Always points to zero.
//		lpdwFlags		Points to AVIIF_KEYFRAME if a keyframe is required,
//						and zero otherwise.
//		lFrameNum		Ascending from zero.
//		dwFrameSize		Always set to 7FFFFFFF (Win9x) or 00FFFFFF (WinNT)
//						for first frame.  Set to zero for subsequent frames
//						if data rate control is not active or not supported,
//						and to the desired frame size in bytes if it is.
//		dwQuality		Set to quality factor from 0-10000 if quality is
//						supported.  Otherwise, it is zero.
//		lpbiPrev		Set to NULL if not required.
//		lpPrev			Set to NULL if not required.
//
//////////////////////////////////////////////////////////////////////////////

VideoSequenceCompressor::VideoSequenceCompressor() {
	pPrevBuffer		= NULL;
	pConfigData		= NULL;
	fCompressionStarted = false;
	mbCompressionRestarted = false;
}

VideoSequenceCompressor::~VideoSequenceCompressor() {
	Stop();

	if (mbOwnHandle)
		delete driver;

	delete pConfigData;
	delete pPrevBuffer;
}

void VideoSequenceCompressor::SetDriver(EncoderHIC* driver, uint32 kilobytesPerSecond, long quality, long keyrate, bool ownHandle) {
	mbOwnHandle = ownHandle;
	this->driver = driver;
	lDataRate = kilobytesPerSecond;
	lKeyRate = keyrate;
	lQuality = quality;

	ICINFO info = {sizeof(ICINFO)};
	DWORD rv;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		rv = driver->getInfo(info);
	}

	mbKeyframeOnly = true;
	if (keyrate != 1 && rv >= sizeof info) {
		if (info.dwFlags & (VIDCF_TEMPORAL | VIDCF_FASTTEMPORALC))
			mbKeyframeOnly = false;
	}
}

void VideoSequenceCompressor::GetOutputFormat(const void *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat) {
	vdprotected("querying video compressor for output format") {
		DWORD icErr;
		LONG formatSize = driver->compressGetFormatSize((LPBITMAPINFO)inputFormat);
		if (formatSize < ICERR_OK)
			throw MyICError("Output compressor", formatSize);

		outputFormat.resize(formatSize);

		// Huffyuv doesn't initialize a few padding bytes at the end of its format
		// struct, so we clear them here.
		memset(&*outputFormat, 0, outputFormat.size());

		if (ICERR_OK != (icErr = driver->compressGetFormat((LPBITMAPINFO)inputFormat, (LPBITMAPINFO)&*outputFormat)))
			throw MyICError("Output compressor", icErr);
	}
}

void VideoSequenceCompressor::GetOutputFormat(const VDPixmapLayout *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat) {
	vdprotected("querying video compressor for output format") {
		DWORD icErr;
		LONG formatSize = driver->compressGetFormatSize(0,inputFormat);
		if (formatSize < ICERR_OK)
			throw MyICError("Output compressor", formatSize);

		outputFormat.resize(formatSize);

		// Huffyuv doesn't initialize a few padding bytes at the end of its format
		// struct, so we clear them here.
		memset(&*outputFormat, 0, outputFormat.size());

		if (ICERR_OK != (icErr = driver->compressGetFormat(0, (LPBITMAPINFO)&*outputFormat, inputFormat)))
			throw MyICError("Output compressor", icErr);
	}
}

const void *VideoSequenceCompressor::GetOutputFormat() {
	return mOutputFormat.data();
}

uint32 VideoSequenceCompressor::GetOutputFormatSize() {
	return mOutputFormat.size();
}

void VideoSequenceCompressor::Start(const void *inputFormat, uint32 inputFormatSize, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount) {
	const BITMAPINFOHEADER *pbihInput = (const BITMAPINFOHEADER *)inputFormat;
	mInputFormat.assign(pbihInput, inputFormatSize);
	mInputLayout.format = 0;
	internalStart(outputFormat, outputFormatSize, frameRate, frameCount);
}

void VideoSequenceCompressor::Start(const VDPixmapLayout& layout, FilterModPixmapInfo& info, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount) {
	mInputLayout = layout;
	mInputInfo = info;
	internalStart(outputFormat, outputFormatSize, frameRate, frameCount);
	GetOutputFormat(&layout, mOutputFormat);
}

void VideoSequenceCompressor::internalStart(const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount) {
	const BITMAPINFOHEADER *pbihOutput = (const BITMAPINFOHEADER *)outputFormat;
	mOutputFormat.assign(pbihOutput, outputFormatSize);
	mFrameRate = frameRate;
	mFrameCount = frameCount;

	lKeyRateCounter = 1;

	// Retrieve compressor information.
	ICINFO	info;
	LRESULT	res;
	res = driver->getInfo(info);

	if (!res)
		throw MyError("Unable to retrieve video compressor information.");

	const wchar_t *pName = info.szDescription;
	mCodecName = pName;
	mDriverName = VDswprintf(L"The video codec \"%s\"", 1, &pName);

	// Analyze compressor.

	this->dwFlags = info.dwFlags;

	if (info.dwFlags & (VIDCF_TEMPORAL | VIDCF_FASTTEMPORALC)) {
		if (!(info.dwFlags & VIDCF_FASTTEMPORALC)) {
			// Allocate backbuffer

			if (!(pPrevBuffer = new char[mInputFormat->biSizeImage]))
				throw MyMemoryError();
		}
	}

	if (!(info.dwFlags & VIDCF_QUALITY))
		lQuality = 0;

	// Allocate destination buffer

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		lMaxPackedSize = driver->compressGetSize((LPBITMAPINFO)&*mInputFormat, (LPBITMAPINFO)&*mOutputFormat, &mInputLayout);
	}

	// Work around a bug in Huffyuv.  Ben tried to save some memory
	// and specified a "near-worst-case" bound in the codec instead
	// of the actual worst case bound.  Unfortunately, it's actually
	// not that hard to exceed the codec's estimate with noisy
	// captures -- the most common way is accidentally capturing
	// static from a non-existent channel.
	//
	// According to the 2.1.1 comments, Huffyuv uses worst-case
	// values of 24-bpp for YUY2/UYVY and 40-bpp for RGB, while the
	// actual worst case values are 43 and 51.  We'll compute the
	// 43/51 value, and use the higher of the two.

	//if (isEqualFOURCC(info.fccHandler, 'UYFH')) {
	if ((info.fccHandler & 0xdfdfdfdf) == 'UYFH') {
		long lRealMaxPackedSize = mInputFormat->biWidth * abs(mInputFormat->biHeight);

		if (mInputFormat->biCompression == BI_RGB)
			lRealMaxPackedSize = (lRealMaxPackedSize * 51) >> 3;
		else
			lRealMaxPackedSize = (lRealMaxPackedSize * 43) >> 3;

		if (lRealMaxPackedSize > lMaxPackedSize)
			lMaxPackedSize = lRealMaxPackedSize;
	}

	// Save configuration state.
	//
	// Ordinarily, we wouldn't do this, but there seems to be a bug in
	// the Microsoft MPEG-4 compressor that causes it to reset its
	// configuration data after a compression session.  This occurs
	// in all versions from V1 through V3.
	//
	// Stupid fscking Matrox driver returns -1!!!

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		cbConfigData = driver->getStateSize();
	}

	if (cbConfigData > 0) {
		if (!(pConfigData = new char[cbConfigData]))
			throw MyMemoryError();

		{
			VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
			cbConfigData = driver->getState(pConfigData, cbConfigData);
		}

		// As odd as this may seem, if this isn't done, then the Indeo5
		// compressor won't allow data rate control until the next
		// compression operation!

		if (cbConfigData) {
			VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
			driver->setState(pConfigData, cbConfigData);
		}
	}

	lMaxFrameSize = 0;
	lSlopSpace = 0;
	lKeySlopSpace = 0;

	if (lDataRate && (dwFlags & (VIDCF_CRUNCH|VIDCF_QUALITY)))
		lMaxFrameSize = VDRoundToInt(lDataRate / frameRate.asDouble());
	else
		lMaxFrameSize = 0;

	// Indeo 5 needs this message for data rate clamping.

	// The Morgan codec requires the message otherwise it assumes 100%
	// quality :(

	// The original version (2700) MPEG-4 V1 requires this message, period.
	// V3 (DivX) gives crap if we don't send it.  So special case it.

	ICINFO ici;

	driver->getInfo(ici);

	vdprotected("passing operation parameters to the video codec") {
		ICCOMPRESSFRAMES icf;

		memset(&icf, 0, sizeof icf);

		icf.dwFlags		= (DWORD)&icf.lKeyRate;
		icf.lStartFrame = 0;
		icf.lFrameCount = frameCount > 0xFFFFFFFF ? 0xFFFFFFFF : (uint32)frameCount;
		icf.lQuality	= lQuality;
		icf.lDataRate	= lDataRate;
		icf.lKeyRate	= lKeyRate;
		icf.dwRate		= frameRate.getHi();
		icf.dwScale		= frameRate.getLo();

		{
			VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
			driver->sendMessage(ICM_COMPRESS_FRAMES_INFO, (LPARAM)&icf, sizeof(ICCOMPRESSFRAMES));
			driver->compressMatrixInfo(&mInputLayout);
		}
	}

	vdprotected("passing start message to video compressor") {
		// Start compression process

		{
			VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
			res = driver->compressBegin((LPBITMAPINFO)&*mInputFormat, (LPBITMAPINFO)&*mOutputFormat, &mInputLayout);
		}

		if (res != ICERR_OK)
			throw MyICError(res, "Cannot start video compression:\n\n%%s\n(error code %d)", (int)res);

		// Start decompression process if necessary

		if (pPrevBuffer) {
			{
				VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
				res = driver->decompressBegin((LPBITMAPINFO)&*mOutputFormat, (LPBITMAPINFO)&*mInputFormat);
			}

			if (res != ICERR_OK) {
				{
					VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
					driver->compressEnd();
				}

				throw MyICError(res, "Cannot start video compression:\n\n%%s\n(error code %d)", (int)res);
			}
		}
	}

	fCompressionStarted = true;
	mbCompressionRestarted = true;
	lFrameSent = 0;
	lFrameDone = 0;


	mQualityLo = 0;
	mQualityLast = 10000;
	mQualityHi = 10000;
}

void VideoSequenceCompressor::Stop() {
	if (!fCompressionStarted)
		return;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);

		if (pPrevBuffer)
			driver->decompressEnd();

		driver->compressEnd();
	}

	fCompressionStarted = false;

	// Reset MPEG-4 compressor

	if (cbConfigData && pConfigData) {
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		driver->setState(pConfigData, cbConfigData);
	}
}

void VideoSequenceCompressor::dropFrame() {
	if (lKeyRate && lKeyRateCounter>1)
		--lKeyRateCounter;

	// Hmm, this seems to make Cinepak restart on a key frame.
	++lFrameDone;
}

bool VideoSequenceCompressor::packFrame(void *dst, const void *src, bool& keyframe, uint32& size) {
	DWORD dwFlagsOut=0, dwFlagsIn = ICCOMPRESS_KEYFRAME;
	long lAllowableFrameSize=0;//xFFFFFF;	// yes, this is illegal according
											// to the docs (see below)

	long lKeyRateCounterSave = lKeyRateCounter;
	mbCompressionRestarted = false;

	// Figure out if we should force a keyframe.  If we don't have any
	// keyframe interval, force only the first frame.  Otherwise, make
	// sure that the key interval is lKeyRate or less.  We count from
	// the last emitted keyframe, since the compressor can opt to
	// make keyframes on its own.

	if (!mbKeyframeOnly) {
		if (!lKeyRate) {
			if (lFrameSent)
				dwFlagsIn = 0;
		} else {
			if (--lKeyRateCounter)
				dwFlagsIn = 0;
			else
				lKeyRateCounter = lKeyRate;
		}
	}

	// Figure out how much space to give the compressor, if we are using
	// data rate stricting.  If the compressor takes up less than quota
	// on a frame, save the space for later frames.  If the compressor
	// uses too much, reduce the quota for successive frames, but do not
	// reduce below half datarate.

	if (lMaxFrameSize) {
		lAllowableFrameSize = lMaxFrameSize + (lSlopSpace>>2);

		if (lAllowableFrameSize < (lMaxFrameSize>>1))
			lAllowableFrameSize = lMaxFrameSize>>1;
	}

	// Save the first byte of the framebuffer, to detect when a codec is
	// incorrectly modifying its input buffer. VFW itself relies on this,
	// such as when it emulates crunch using quality. The MSU lossless
	// codec 0.5.2 is known to do this.
	
	const uint8 firstInputByte = *(const uint8 *)src;

	// A couple of notes:
	//
	//	o  ICSeqCompressFrame() passes 0x7FFFFFFF when data rate control
	//	   is inactive.  Docs say 0.  We pass 0x7FFFFFFF here to avoid
	//	   a bug in the Indeo 5 QC driver, which page faults if
	//	   keyframe interval=0 and max frame size = 0.

	// Compress!

	sint32 bytes;

	if (lMaxFrameSize && !(dwFlags & VIDCF_CRUNCH)) {
		sint32 maxDelta = lMaxFrameSize/20 + 1;
		int packs = 0;

		PackFrameInternal(dst, 0, mQualityLast, src, dwFlagsIn, dwFlagsOut, bytes);
		++packs;

		// Don't do crunching for key frames to keep consistent quality.
		if (abs(bytes - lAllowableFrameSize) <= maxDelta || dwFlagsIn)
			goto crunch_complete;

		if (bytes < lAllowableFrameSize) {		// too low -- squeeze [mid, hi]
			PackFrameInternal(dst, 0, mQualityHi, src, dwFlagsIn, dwFlagsOut, bytes);
			++packs;

			if (abs(bytes - lAllowableFrameSize) <= maxDelta) {
				mQualityLast = mQualityHi;
				mQualityHi = (mQualityHi + 10001) >> 1;
				goto crunch_complete;
			}

			if (bytes < lAllowableFrameSize) {
				mQualityLast = mQualityHi;
				mQualityHi = 10000;
			}

			if (mQualityHi > mQualityLast + 1000)
				mQualityHi = mQualityLast + 1000;

			sint32 lo = mQualityLast, hi = mQualityHi, q;

			while(lo <= hi) {
				q = (lo+hi)>>1;

				PackFrameInternal(dst, 0, q, src, dwFlagsIn, dwFlagsOut, bytes);
				++packs;

				sint32 delta = (bytes - lAllowableFrameSize);

				if (delta < -maxDelta)
					lo = q+1;
				else if (delta > +maxDelta)
					hi = q-1;
				else
					break;
			}

			if (q + q > mQualityHi + mQualityLast)
				mQualityHi += 100;
			else
				mQualityHi -= 100;

			if (mQualityHi <= q + 100)
				mQualityHi = q + 100;

			if (mQualityHi > 10000)
				mQualityHi = 10000;

			mQualityLast = q;

		} else {							// too low -- squeeze [lo, mid]
			PackFrameInternal(dst, 0, mQualityLo, src, dwFlagsIn, dwFlagsOut, bytes);
			++packs;

			if (abs(bytes - lAllowableFrameSize)*20 <= lAllowableFrameSize) {
				mQualityLast = mQualityLo;
				mQualityLo = mQualityLo >> 1;
				goto crunch_complete;
			}

			if (bytes > lAllowableFrameSize) {
				mQualityLast = mQualityLo;
				mQualityLo = 1;
			}

			if (mQualityLo < mQualityLast - 1000)
				mQualityLo = mQualityLast - 1000;

			sint32 lo = mQualityLo, hi = mQualityLast, q = lo;

			while(lo <= hi) {
				q = (lo+hi)>>1;

				PackFrameInternal(dst, 0, q, src, dwFlagsIn, dwFlagsOut, bytes);
				++packs;

				sint32 delta = (bytes - lAllowableFrameSize);

				if (delta < -maxDelta)
					lo = q+1;
				else if (delta > +maxDelta)
					hi = q-1;
				else
					break;
			}

			if (q + q < mQualityLo + mQualityLast)
				mQualityLo -= 100;
			else
				mQualityLo += 100;

			if (mQualityLo >= q - 100)
				mQualityLo = q - 100;

			if (mQualityLo < 1)
				mQualityLo = 1;

			mQualityLast = q;
		}

crunch_complete:
		;

//		VDDEBUG("VideoSequenceCompressor: Packed frame %5d to %6u bytes; target=%d bytes / %d bytes, iterations = %d, range = [%5d, %5d, %5d]\n", lFrameNum, bytes, lAllowableFrameSize, lMaxFrameSize, packs, mQualityLo, mQualityLast, mQualityHi);
	} else {	// No crunching or crunch directly supported
		PackFrameInternal(dst, lAllowableFrameSize, lQuality, src, dwFlagsIn, dwFlagsOut, bytes);

//		VDDEBUG("VideoSequenceCompressor: Packed frame %5d to %6u bytes; target=%d bytes / %d bytes\n", lFrameNum, bytes, lAllowableFrameSize, lMaxFrameSize);
	}

	// Flag a warning if the codec is improperly modifying its input buffer.
	if (!lFrameSent && *(const uint8 *)src != firstInputByte) {
		if (g_pVDVideoCodecBugTrap)
			g_pVDVideoCodecBugTrap->OnCodecModifiedInput(mCodecName.c_str());
	}

	// Special handling for DivX 5 and XviD codecs:
	//
	// A one-byte frame starting with 0x7f should be discarded
	// (lag for B-frame).

	++lFrameSent;
	bool bNoOutputProduced = false;

	if (mOutputFormat->biCompression == '05xd' ||
		mOutputFormat->biCompression == '05XD' ||
		mOutputFormat->biCompression == 'divx' ||
		mOutputFormat->biCompression == 'DIVX'
		) {
		if (bytes == 1 && *(char *)dst == 0x7f) {
			bNoOutputProduced = true;
		}
	}

	if (mInputLayout.format && (dwFlagsOut & VDCOMPRESS_WAIT)!=0) {
		bNoOutputProduced = true;
	}

	if (bNoOutputProduced) {
		lKeyRateCounter = lKeyRateCounterSave;
		return false;
	}

	// If we're using a compressor with a stupid algorithm (Microsoft Video 1),
	// we have to decompress the frame again to compress the next one....

	if (pPrevBuffer && (!lKeyRate || lKeyRateCounter>1)) {
		DWORD res;

		{
			VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
			vdprotected4("decompressing frame %u from %08x to %08x using codec \"%ls\"", unsigned, lFrameDone, unsigned, (unsigned)dst, unsigned, (unsigned)pPrevBuffer, const wchar_t *, mCodecName.c_str()) {
				res = driver->decompress(dwFlagsOut & AVIIF_KEYFRAME ? 0 : ICDECOMPRESS_NOTKEYFRAME,
						&*mOutputFormat,
						dst,
						&*mInputFormat,
						pPrevBuffer);
			}
		}
		if (res != ICERR_OK)
			throw MyICError("Video compression", res);
	}

	++lFrameDone;
	size = bytes;

	// Update quota.

	if (lMaxFrameSize) {
		if (lKeyRate && dwFlagsIn)
			lKeySlopSpace += lMaxFrameSize - bytes;
		else
			lSlopSpace += lMaxFrameSize - bytes;

		if (lKeyRate) {
			long delta = lKeySlopSpace / lKeyRateCounter;
			lSlopSpace += delta;
			lKeySlopSpace -= delta;
		}
	}

	// Was it a keyframe?

	if (dwFlagsOut & AVIIF_KEYFRAME) {
		keyframe = true;
		lKeyRateCounter = lKeyRate;
	} else {
		keyframe = false;
	}

	return true;
}

void VideoSequenceCompressor::PackFrameInternal(void* dst, DWORD frameSize, DWORD q, const void *src, DWORD dwFlagsIn, DWORD& dwFlagsOut, sint32& bytes) {
	DWORD dwChunkId = 0;
	DWORD res;

	dwFlagsOut = 0;
	if (dwFlagsIn)
		dwFlagsOut = AVIIF_KEYFRAME;

	DWORD sizeImage = mOutputFormat->biSizeImage;

	VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
	vdprotected4("compressing frame %u from %08x to %08x using codec \"%ls\"", unsigned, lFrameSent, unsigned, (unsigned)src, unsigned, (unsigned)dst, const wchar_t *, mCodecName.c_str()) {
		res = driver->compress(dwFlagsIn,
				mOutputFormat.data(), dst,
				mInputFormat.data(), (LPVOID)src,
				&dwChunkId,
				&dwFlagsOut,
				lFrameSent,
				lFrameSent ? frameSize : 0xFFFFFF,
				q,
				dwFlagsIn & ICCOMPRESS_KEYFRAME ? NULL : mInputFormat.data(),
				dwFlagsIn & ICCOMPRESS_KEYFRAME ? NULL : pPrevBuffer,
				&mInputLayout);
	}

	bytes = mOutputFormat->biSizeImage;
	mOutputFormat->biSizeImage = sizeImage;

	if (res != ICERR_OK)
		throw MyICError("Video compression", res);
}

void* VideoSequenceCompressor::createResultBuffer() {
	void* r = new char[lMaxPackedSize];
	if (!r)	throw MyMemoryError();
	return r;
}

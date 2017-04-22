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

#include <windows.h>
#include <vfw.h>

#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/log.h>
#include <vd2/system/math.h>
#include <vd2/system/protscope.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/kasumi/pixmap.h>
#include <vd2/Riza/videocodec.h>
#include <vd2/plugin/vdplugin.h>

extern IVDVideoCodecBugTrap *g_pVDVideoCodecBugTrap;

class VDVideoCompressorDescVCM : public vdrefcounted<IVDVideoCompressorDesc> {
public:
	void *AsInterface(uint32 id) { return NULL; }
	void CreateInstance(IVDVideoCompressor **comp);

	uint32	mfccType;
	uint32	mfccHandler;
	uint32	mKilobytesPerSecond;
	int		mQuality;
	int		mKeyFrameInterval;
	vdfastvector<uint8> mState;
};

void VDVideoCompressorDescVCM::CreateInstance(IVDVideoCompressor **comp) {
	EncoderHIC* hic = EncoderHIC::open(mfccType, mfccHandler, ICMODE_COMPRESS);
	if (!hic)
		throw MyError("Unable to create video compressor.");

	try {
		*comp = VDCreateVideoCompressorVCM(hic, mKilobytesPerSecond, mQuality, mKeyFrameInterval, true);
	} catch(...) {
		delete hic;
		throw;
	}
}

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

class VDVideoCompressorVCM : public IVDVideoCompressor {
public:
	VDVideoCompressorVCM();
	~VDVideoCompressorVCM();
	void SetDriver(EncoderHIC* driver, uint32 kilobytesPerSecond, long quality, long keyrate, bool ownHandle);

	bool IsKeyFrameOnly();
	virtual int QueryInputFormat(FilterModPixmapInfo* info) {
		return driver->queryInputFormat(info);
	}
	virtual int GetInputFormat(FilterModPixmapInfo* info) {
		if (info) {
			info->copy_ref(mInputInfo);
			info->copy_alpha(mInputInfo);
		}
		return mInputLayout.format; 
	}
	virtual void GetInputBitmapFormat(vdstructex<tagBITMAPINFOHEADER>& bm) {
		bm = mInputFormat;
	}
	bool Query(const void *inputFormat, const void *outputFormat);
	bool Query(const VDPixmapLayout *inputFormat, const void *outputFormat = NULL);
	void GetOutputFormat(const void *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat);
	void GetOutputFormat(const VDPixmapLayout *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat);
	const void *GetOutputFormat();
	uint32 GetOutputFormatSize();
	void Start(const void *inputFormat, uint32 inputFormatSize, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount);
	void Start(const VDPixmapLayout& layout, FilterModPixmapInfo& info, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount);
	void internalStart(const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount);
	void Restart();
	void SkipFrame();
	void DropFrame();
	bool CompressFrame(void *dst, const void *src, uint32& size, VDPacketInfo& packetInfo);
	void Stop();

	void Clone(IVDVideoCompressor **vcRet);

	void GetDesc(IVDVideoCompressorDesc **desc);

	uint32 GetMaxOutputSize() {
		return lMaxPackedSize;
	}

private:
	void PackFrameInternal(void *dst, DWORD frameSize, DWORD q, const void *pBits, DWORD dwFlagsIn, DWORD& dwFlagsOut, VDPacketInfo& packetInfo, sint32& bytes);
	void GetState(vdfastvector<uint8>& data);

	EncoderHIC*	driver;
	bool		mbOwnHandle;
	DWORD		dwFlags;
	DWORD		mVFWExtensionMessageID;
	vdstructex<BITMAPINFOHEADER>	mInputFormat;
	vdstructex<BITMAPINFOHEADER>	mOutputFormat;
	VDPixmapLayout  	mInputLayout;
	FilterModPixmapInfo mInputInfo;
	VDFraction	mFrameRate;
	VDPosition	mFrameCount;
	char		*pPrevBuffer;
	long		lFrameSent;
	long		lFrameDone;
	long		lKeyRate;
	long		lQuality;
	long		lDataRate;
	long		lKeyRateCounter;
	long		lMaxFrameSize;
	long		lMaxPackedSize;
	bool		fCompressionStarted;
	long		lSlopSpace;
	long		lKeySlopSpace;

	bool		mbKeyframeOnly;
	bool		mbCompressionRestarted;

	// crunch emulation
	sint32		mQualityLo;
	sint32		mQualityLast;
	sint32		mQualityHi;

	void		*pConfigData;
	int			cbConfigData;

	VDStringW	mCodecName;
	VDStringW	mDriverName;
};

IVDVideoCompressor *VDCreateVideoCompressorVCM(EncoderHIC *pHIC, uint32 kilobytesPerSecond, long quality, long keyrate, bool ownHandle) {
	VDVideoCompressorVCM* r = new VDVideoCompressorVCM();
	r->SetDriver(pHIC, kilobytesPerSecond, quality, keyrate, ownHandle);
	return r;
}

VDVideoCompressorVCM::VDVideoCompressorVCM() {
	pPrevBuffer		= NULL;
	pConfigData		= NULL;
	fCompressionStarted = false;
	mbCompressionRestarted = false;
}

VDVideoCompressorVCM::~VDVideoCompressorVCM() {
	Stop();

	if (mbOwnHandle)
		delete driver;

	delete pConfigData;
	delete pPrevBuffer;
}

void VDVideoCompressorVCM::SetDriver(EncoderHIC* driver, uint32 kilobytesPerSecond, long quality, long keyrate, bool ownHandle) {
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

bool VDVideoCompressorVCM::IsKeyFrameOnly() {
	return mbKeyframeOnly;
}

bool VDVideoCompressorVCM::Query(const void *inputFormat, const void *outputFormat) {
	DWORD res;
	vdprotected("asking video compressor if conversion is possible") {
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		res = driver->compressQuery((LPBITMAPINFO)inputFormat, (LPBITMAPINFO)outputFormat);
	}
	return res == ICERR_OK;
}

bool VDVideoCompressorVCM::Query(const VDPixmapLayout *inputFormat, const void *outputFormat) { 
	DWORD res;
	vdprotected("asking video compressor if conversion is possible") {
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		res = driver->compressQuery(0, (LPBITMAPINFO)outputFormat, inputFormat);
	}
	return res == ICERR_OK;
}

void VDVideoCompressorVCM::GetOutputFormat(const void *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat) {
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

void VDVideoCompressorVCM::GetOutputFormat(const VDPixmapLayout *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat) {
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

const void *VDVideoCompressorVCM::GetOutputFormat() {
	return mOutputFormat.data();
}

uint32 VDVideoCompressorVCM::GetOutputFormatSize() {
	return mOutputFormat.size();
}

void VDVideoCompressorVCM::Start(const void *inputFormat, uint32 inputFormatSize, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount) {
	const BITMAPINFOHEADER *pbihInput = (const BITMAPINFOHEADER *)inputFormat;
	mInputFormat.assign(pbihInput, inputFormatSize);
	mInputLayout.format = 0;
	internalStart(outputFormat, outputFormatSize, frameRate, frameCount);
}

void VDVideoCompressorVCM::Start(const VDPixmapLayout& layout, FilterModPixmapInfo& info, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount) {
	mInputLayout = layout;
	mInputInfo = info;
	internalStart(outputFormat, outputFormatSize, frameRate, frameCount);
	GetOutputFormat(&layout, mOutputFormat);
}

void VDVideoCompressorVCM::internalStart(const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount) {
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

void VDVideoCompressorVCM::Stop() {
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

void VDVideoCompressorVCM::GetDesc(IVDVideoCompressorDesc **descRet) {
	vdrefptr<VDVideoCompressorDescVCM> desc(new VDVideoCompressorDescVCM);

	ICINFO info = {0};
	if (!driver->getInfo(info))
		throw MyError("Unable to retrieve base video codec information.");

	desc->mfccType = info.fccType;
	desc->mfccHandler = info.fccHandler;
	desc->mKilobytesPerSecond = lDataRate;
	desc->mQuality = lQuality;
	desc->mKeyFrameInterval = lKeyRate;

	GetState(desc->mState);

	*descRet = desc.release();
}

void VDVideoCompressorVCM::Clone(IVDVideoCompressor **vcRet) {
	vdrefptr<IVDVideoCompressorDesc> desc;
	GetDesc(~desc);

	vdautoptr<IVDVideoCompressor> vc;
	desc->CreateInstance(~vc);

	if (fCompressionStarted) {
		if (mInputLayout.format)
			vc->Start(mInputLayout, mInputInfo, mOutputFormat.data(), mOutputFormat.size(), mFrameRate, mFrameCount);
		else
			vc->Start(mInputFormat.data(), mInputFormat.size(), mOutputFormat.data(), mOutputFormat.size(), mFrameRate, mFrameCount);
	}

	*vcRet = vc.release();
}

void VDVideoCompressorVCM::SkipFrame() {
	if (lKeyRate && lKeyRateCounter>1)
		--lKeyRateCounter;
}

void VDVideoCompressorVCM::DropFrame() {
	if (lKeyRate && lKeyRateCounter>1)
		--lKeyRateCounter;

	// Hmm, this seems to make Cinepak restart on a key frame.
	++lFrameDone;
}

void VDVideoCompressorVCM::Restart() {
	if (mbCompressionRestarted)
		return;

	mbCompressionRestarted = true;
	lFrameSent = 0;
	lFrameDone = 0;
	lKeyRateCounter = 1;

	DWORD res;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);

		driver->compressEnd();
		res = driver->compressBegin((LPBITMAPINFO)&*mInputFormat, (LPBITMAPINFO)&*mOutputFormat, &mInputLayout);
	}

	if (res != ICERR_OK)
		throw MyICError(res, "Cannot restart video compression:\n\n%%s\n(error code %d)", (int)res);
}

bool VDVideoCompressorVCM::CompressFrame(void *dst, const void *src, uint32& size, VDPacketInfo& packetInfo) {
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

		PackFrameInternal(dst, 0, mQualityLast, src, dwFlagsIn, dwFlagsOut, packetInfo, bytes);
		++packs;

		// Don't do crunching for key frames to keep consistent quality.
		if (abs(bytes - lAllowableFrameSize) <= maxDelta || dwFlagsIn)
			goto crunch_complete;

		if (bytes < lAllowableFrameSize) {		// too low -- squeeze [mid, hi]
			PackFrameInternal(dst, 0, mQualityHi, src, dwFlagsIn, dwFlagsOut, packetInfo, bytes);
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

				PackFrameInternal(dst, 0, q, src, dwFlagsIn, dwFlagsOut, packetInfo, bytes);
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
			PackFrameInternal(dst, 0, mQualityLo, src, dwFlagsIn, dwFlagsOut, packetInfo, bytes);
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

				PackFrameInternal(dst, 0, q, src, dwFlagsIn, dwFlagsOut, packetInfo, bytes);
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
		PackFrameInternal(dst, lAllowableFrameSize, lQuality, src, dwFlagsIn, dwFlagsOut, packetInfo, bytes);

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
		packetInfo.keyframe = true;
		lKeyRateCounter = lKeyRate;
	} else {
		packetInfo.keyframe = false;
	}

	return true;
}

void VDVideoCompressorVCM::PackFrameInternal(void *dst, DWORD frameSize, DWORD q, const void *src, DWORD dwFlagsIn, DWORD& dwFlagsOut, VDPacketInfo& packetInfo, sint32& bytes) {
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
				packetInfo,
				&mInputLayout);
	}

	bytes = mOutputFormat->biSizeImage;
	mOutputFormat->biSizeImage = sizeImage;

	if (res != ICERR_OK)
		throw MyICError("Video compression", res);
}

void VDVideoCompressorVCM::GetState(vdfastvector<uint8>& data) {
	DWORD res;
	sint32 size;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		size = driver->getStateSize();
	}

	if (size <= 0) {
		data.clear();
		return;
	}

	data.resize(size);

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		res = driver->getState(data.data(), size);
	}

	if (res < 0)
		throw MyICError("Video compression", res);
}

//------------------------------------------------------------------------------------------------------

EncoderHIC* EncoderHIC::load(const VDStringW& path, DWORD type, DWORD handler, DWORD flags) {
	HMODULE module = LoadLibraryW(path.c_str());
	if(!module) return 0;

	DriverProc* proc = (DriverProc*)GetProcAddress(module,"DriverProc");
	if(!proc){
		FreeLibrary(module);
		return 0;
	}
	proc(0,0,DRV_LOAD,0,0);

	ICOPEN open = {sizeof(ICOPEN)};
	open.fccType = type;
	open.fccHandler = handler;
	open.dwFlags = flags;
	DWORD_PTR obj = (DWORD_PTR)proc(0,0,DRV_OPEN,0,(LPARAM)&open);
	if(!obj){
		FreeLibrary(module);
		return 0;
	}

	EncoderHIC* plugin = new EncoderHIC;
	plugin->path = path;
	plugin->module = module;
	plugin->proc = proc;
	plugin->obj = obj;
	plugin->vdproc = (DriverProc*)GetProcAddress(module,"VDDriverProc");

	return plugin;
}

EncoderHIC* EncoderHIC::open(DWORD type, DWORD handler, DWORD flags) {
	HIC hic = ICOpen(type,handler,flags);
	if(!hic) return 0;
	EncoderHIC* plugin = new EncoderHIC;
	plugin->hic = hic;
	return plugin;
}

DWORD EncoderHIC::getNext(DWORD handler) {
	if(vdproc) return vdproc(0,0,VDICM_ENUMFORMATS,handler,0);
	return -1;
}

int EncoderHIC::getInfo(ICINFO& info) {
	if(obj) return proc(obj,0,ICM_GETINFO,(LPARAM)&info,sizeof(info));
	if(hic) return ICGetInfo(hic,&info,sizeof(info));
	return 0;
}

int EncoderHIC::compressQuery(void* src, void* dst, const VDPixmapLayout* pxsrc) {
	if(vdproc && pxsrc && pxsrc->format) {
		return vdproc(obj,0,VDICM_COMPRESS_QUERY,(LPARAM)pxsrc,(LPARAM)dst);
	}
	if(src){
		if(obj) return proc(obj,0,ICM_COMPRESS_QUERY,(LPARAM)src,(LPARAM)dst);
		if(hic) return ICCompressQuery(hic,src,dst);
	}
	return -1;
}

int EncoderHIC::compressGetFormat(BITMAPINFO* b1, BITMAPINFO* b2, const VDPixmapLayout* pxsrc) { 
	if(vdproc && pxsrc && pxsrc->format) {
		return vdproc(obj,0,VDICM_COMPRESS_GET_FORMAT,(LPARAM)pxsrc,(LPARAM)b2);
	}
	return sendMessage(ICM_COMPRESS_GET_FORMAT,(LPARAM)b1,(LPARAM)b2); 
}

int EncoderHIC::compressGetSize(BITMAPINFO* b1, BITMAPINFO* b2, const VDPixmapLayout* pxsrc) {
	if(vdproc && pxsrc && pxsrc->format) {
		return vdproc(obj,0,VDICM_COMPRESS_GET_SIZE,(LPARAM)pxsrc,(LPARAM)b2);
	}
	return sendMessage(ICM_COMPRESS_GET_SIZE,(LPARAM)b1,(LPARAM)b2); 
}

int EncoderHIC::compressMatrixInfo(const VDPixmapLayout* pxsrc) {
	if(vdproc && pxsrc && pxsrc->format) {
		return vdproc(obj,0,VDICM_COMPRESS_MATRIX_INFO,pxsrc->formatEx.colorSpaceMode,pxsrc->formatEx.colorRangeMode);
	}
	return -1;
}

void VDLog_s(int severity, const wchar_t *s) { VDLog(severity,s); }

int EncoderHIC::compressBegin(BITMAPINFO* b1, BITMAPINFO* b2, const VDPixmapLayout* pxsrc) {
	if(vdproc) vdproc(obj,0,VDICM_LOGPROC,(LPARAM)VDLog_s,0);
	if(vdproc && pxsrc && pxsrc->format) {
		return vdproc(obj,0,VDICM_COMPRESS_BEGIN,(LPARAM)pxsrc,(LPARAM)b2);
	}
	return sendMessage(ICM_COMPRESS_BEGIN,(LPARAM)b1,(LPARAM)b2); 
}

int EncoderHIC::sendMessage(int msg, LPARAM p1, LPARAM p2) {
	if(obj) return proc(obj,0,msg,p1,p2);
	if(hic) return ICSendMessage(hic,msg,p1,p2);
	return -1;
}

void EncoderHIC::close() {
	if(obj){ proc(obj,0,DRV_CLOSE,0,0); obj=0; }
	if(hic){ ICClose(hic); hic=0; }
}

void EncoderHIC::configure(HWND hwnd) {
	if(obj) proc(obj,0,ICM_CONFIGURE,(LPARAM)hwnd,0);
	if(hic) ICConfigure(hic,hwnd);
}

void EncoderHIC::about(HWND hwnd) {
	if(obj) proc(obj,0,ICM_ABOUT,(LPARAM)hwnd,0);
	if(hic) ICAbout(hic,hwnd);
}

bool EncoderHIC::queryConfigure() {
	if(obj) return sendMessage(ICM_CONFIGURE,-1,ICMF_CONFIGURE_QUERY)==ICERR_OK;
	if(hic) return ICQueryConfigure(hic);
	return false;
}

bool EncoderHIC::queryAbout() {
	if(obj) return sendMessage(ICM_ABOUT,-1,ICMF_ABOUT_QUERY)==ICERR_OK;
	if(hic) return ICQueryAbout(hic);
	return false;
}

void EncoderHIC::setState(void* data, int size) {
	if(obj) proc(obj,0,ICM_SETSTATE,(LPARAM)data,size);
	if(hic) ICSetState(hic,data,size);
}

int EncoderHIC::getState(void* data, int size) {
	if(obj) return proc(obj,0,ICM_GETSTATE,(LPARAM)data,size);
	if(hic) return ICGetState(hic,data,size);
	return 0;
}

DWORD EncoderHIC::compress(
	DWORD               dwFlags,
	LPBITMAPINFOHEADER  lpbiOutput,
	LPVOID              lpData,
	LPBITMAPINFOHEADER  lpbiInput,
	LPVOID              lpBits,
	LPDWORD             lpckid,
	LPDWORD             lpdwFlags,
	LONG                lFrameNum,
	DWORD               dwFrameSize,
	DWORD               dwQuality,
	LPBITMAPINFOHEADER  lpbiPrev,
	LPVOID              lpPrev,
	VDPacketInfo&       packetInfo,
	const VDPixmapLayout* pxsrc
)
{
	if(vdproc && pxsrc && pxsrc->format) {
		ICCOMPRESS c;
		c.dwFlags = dwFlags;
		c.lpbiOutput = lpbiOutput;
		c.lpOutput = lpData;
		c.lpbiInput = 0;
		c.lpInput = lpBits;
		c.lpckid = lpckid;
		c.lpdwFlags = lpdwFlags;
		c.lFrameNum = lFrameNum;
		c.dwFrameSize = dwFrameSize;
		c.dwQuality = dwQuality;
		c.lpbiPrev = 0;
		c.lpPrev = lpPrev;
		VDXPictureCompress pc;
		pc.px = (const VDXPixmapLayout*)pxsrc;
		DWORD r = vdproc(obj,0,VDICM_COMPRESS2,(LPARAM)&c,(LPARAM)&pc);
		if (r!=ICERR_UNSUPPORTED) {
			packetInfo.pts = pc.pts;
			packetInfo.dts = pc.dts;
			packetInfo.duration = pc.duration;
			return r;
		}
		return vdproc(obj,0,VDICM_COMPRESS,(LPARAM)&c,(LPARAM)pxsrc);
	}
	if(obj){
		ICCOMPRESS c;
		c.dwFlags = dwFlags;
		c.lpbiOutput = lpbiOutput;
		c.lpOutput = lpData;
		c.lpbiInput = lpbiInput;
		c.lpInput = lpBits;
		c.lpckid = lpckid;
		c.lpdwFlags = lpdwFlags;
		c.lFrameNum = lFrameNum;
		c.dwFrameSize = dwFrameSize;
		c.dwQuality = dwQuality;
		c.lpbiPrev = lpbiPrev;
		c.lpPrev = lpPrev;
		return proc(obj,0,ICM_COMPRESS,(LPARAM)&c,0);
	}
	if(hic) return ICCompress(hic, dwFlags,
		lpbiOutput, lpData,
		lpbiInput, lpBits,
		lpckid,
		lpdwFlags,
		lFrameNum,
		dwFrameSize,
		dwQuality,
		lpbiPrev,
		lpPrev);
	return -1;
}

DWORD EncoderHIC::decompress(
	DWORD               dwFlags, 
	LPBITMAPINFOHEADER  lpbiFormat, 
	LPVOID              lpData, 
	LPBITMAPINFOHEADER  lpbi, 
	LPVOID              lpBits
)
{
	if(obj){
		ICDECOMPRESS c;
		c.dwFlags = dwFlags;
		c.lpbiInput = lpbiFormat;
		c.lpInput = lpData;
		c.lpbiOutput = lpbi;
		c.lpOutput = lpBits;
		c.ckid = 0;
		return proc(obj,0,ICM_DECOMPRESS,(LPARAM)&c,0);
	}
	if(hic) return ICDecompress(hic,dwFlags,lpbiFormat,lpData,lpbi,lpBits);
	return -1;
}

int EncoderHIC::queryInputFormat(FilterModPixmapInfo* info) {
	if(info) info->clear();
	if (vdproc) {
		FilterModPixmapInfo info2;
		int format = vdproc(obj,0,VDICM_COMPRESS_INPUT_FORMAT,(LPARAM)&info2,0);
		if (info) {
			info->copy_ref(info2);
			info->copy_alpha(info2);
		}
		return format;
	}
	return 0;
}

void FreeCompressor(COMPVARS2 *pCompVars) {
	if (!(pCompVars->dwFlags & ICMF_COMPVARS_VALID))
		return;

	delete pCompVars->driver;
	pCompVars->driver = 0;
	pCompVars->hic = NULL;
	pCompVars->dwFlags &= ~ICMF_COMPVARS_VALID;
}


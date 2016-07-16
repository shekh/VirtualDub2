//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
#include "DubOutput.h"
#include <vd2/system/error.h>
#include <list>
#include "AVIOutput.h"
#include "AVIOutputFile.h"
#include "AVIOutputFLM.h"
#include "AVIOutputGIF.h"
#include "AVIOutputAPNG.h"
#include "AVIOutputWAV.h"
#include "AVIOutputRawAudio.h"
#include "AVIOutputRawVideo.h"
#include "AVIOutputImages.h"
#include "AVIOutputStriped.h"
#include "AVIOutputPreview.h"
#include "AVIOutputSegmented.h"
#include "AVIOutputCLI.h"
#include "ExternalEncoderProfile.h"

///////////////////////////////////////////

extern "C" unsigned long version_num;
extern uint32 VDPreferencesGetAVIAlignmentThreshold();
extern void VDPreferencesGetAVIIndexingLimits(uint32& superindex, uint32& subindex);

///////////////////////////////////////////////////////////////////////////
//
//	VDDubberOutputSystem
//
///////////////////////////////////////////////////////////////////////////

VDDubberOutputSystem::VDDubberOutputSystem()
	: mVideoStreamInfo()
	, mVideoFormat()
	, mAudioStreamInfo()
	, mAudioFormat()
	, mbAudioVBR(false)
	, mbAudioInterleaved(false)
{
	memset(&mVideoImageLayout, 0, sizeof mVideoImageLayout);
}

VDDubberOutputSystem::~VDDubberOutputSystem() {
}

void VDDubberOutputSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDDubberOutputSystem::SetVideoImageLayout(const AVIStreamHeader_fixed& asi, const VDPixmapLayout& layout) {
	mVideoStreamInfo = asi;
	mVideoImageLayout = layout;
}

void VDDubberOutputSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	mbAudioVBR = vbr;
	mbAudioInterleaved = bInterleaved;
	memcpy(mAudioFormat.data(), pFormat, cbFormat);
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputFileSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputFileSystem::VDAVIOutputFileSystem()
	: mbAllowCaching(false)
	, mbAllowIndexing(true)
	, mbUse1GBLimit(false)
	, mCurrentSegment(0)
	, mBufferSize(0)
	, mAlignment(0)
{
}

VDAVIOutputFileSystem::~VDAVIOutputFileSystem() {
}

void VDAVIOutputFileSystem::SetCaching(bool bAllowOSCaching) {
	mbAllowCaching = bAllowOSCaching;
}

void VDAVIOutputFileSystem::SetIndexing(bool bAllowHierarchicalExtensions) {
	mbAllowIndexing = bAllowHierarchicalExtensions;
}

void VDAVIOutputFileSystem::Set1GBLimit(bool bUse1GBLimit) {
	mbUse1GBLimit = bUse1GBLimit;
}

void VDAVIOutputFileSystem::SetBuffer(int bufferSize) {
	mBufferSize = bufferSize;
}

void VDAVIOutputFileSystem::SetTextInfo(const std::list<std::pair<uint32, VDStringA> >& info) {
	mTextInfo = info;
}

IVDMediaOutput *VDAVIOutputFileSystem::CreateSegment() {
	VDASSERT(mBufferSize > 0);

	vdautoptr<IVDMediaOutputAVIFile> pOutput(VDCreateMediaOutputAVIFile());

	if (!mbAllowCaching)
		pOutput->disable_os_caching();

	if (!mbAllowIndexing)
		pOutput->disable_extended_avi();

	if (mbUse1GBLimit)
		pOutput->set_1Gb_limit();

	VDStringW s(mSegmentBaseName);

	if (mSegmentDigits) {
		VDStringW temp;
		temp.sprintf(L".%0*d", mSegmentDigits, mCurrentSegment++);
		s += temp;
		s += mSegmentExt;

		pOutput->setSegmentHintBlock(true, NULL, 1);
	}

	if (!mVideoFormat.empty()) {
		IVDMediaOutputStream *pVideoOut = pOutput->createVideoStream();
		pVideoOut->setFormat(&mVideoFormat[0], mVideoFormat.size());
		pVideoOut->setStreamInfo(mVideoStreamInfo);
		pOutput->setAlignment(0, mAlignment);
	}

	if (!mAudioFormat.empty()) {
		IVDMediaOutputStream *pAudioOut = pOutput->createAudioStream();
		pAudioOut->setFormat(&mAudioFormat[0], mAudioFormat.size());
		pAudioOut->setStreamInfo(mAudioStreamInfo);
	}

	if (!mTextInfo.empty()) {
		tTextInfo::const_iterator it(mTextInfo.begin()), itEnd(mTextInfo.end());

		for(; it!=itEnd; ++it)
			pOutput->setTextInfo((*it).first, (*it).second.c_str());
	}

	pOutput->setBuffering(mBufferSize, mBufferSize >> 2);
	pOutput->setInterleaved(mbAudioInterleaved);

	char buf[80];

	sprintf(buf, "VirtualDub build %d/%s", version_num,
#ifdef _DEBUG
		"debug"
#else
		"release"
#endif
				);

	pOutput->setHiddenTag(buf);

	uint32 superIndexLimit, subIndexLimit;
	VDPreferencesGetAVIIndexingLimits(superIndexLimit, subIndexLimit);

	pOutput->setIndexingLimits(superIndexLimit, subIndexLimit);

	pOutput->init(s.c_str());

	return pOutput.release();
}

void VDAVIOutputFileSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<IVDMediaOutputAVIFile> pFileSegment(static_cast<IVDMediaOutputAVIFile *>(pSegment));
	if (mSegmentDigits)
		pFileSegment->setSegmentHintBlock(bLast, NULL, 1);
	pFileSegment->finalize();
}

void VDAVIOutputFileSystem::SetFilename(const wchar_t *pszFilename) {
	mSegmentBaseName	= pszFilename;
	mSegmentDigits		= 0;
}

void VDAVIOutputFileSystem::SetFilenamePattern(const wchar_t *pszFilename, const wchar_t *pszExt, int nMinimumDigits) {
	mSegmentBaseName	= pszFilename;
	mSegmentExt			= pszExt;
	mSegmentDigits		= nMinimumDigits;
}

void VDAVIOutputFileSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	VDDubberOutputSystem::SetVideo(asi, pFormat, cbFormat);

	if (uint32 alignmentThreshold = VDPreferencesGetAVIAlignmentThreshold()) {
		const BITMAPINFOHEADER& bih = *(const BITMAPINFOHEADER *)pFormat;
		switch(bih.biCompression) {
		case BI_RGB:
		case BI_BITFIELDS:
		case 'YVYU':	// UYVY
		case '2YUY':	// YUY2
		case 'VYUY':	// YUYV
		case 'UYVY':	// YVYU
		case '21VY':	// YV12
		case '024I':	// I420
		case 'VUYI':	// IYUV
		case '9UVY':	// YVU9
		case '61VY':	// YV16
		case '  8Y':	// Y8
		case '008Y':	// Y800
		case '112Y':	// Y211
		case 'P14Y':	// Y41P
		case 'VUYA':	// AYUV
			uint32 imageSize = bih.biSizeImage;

			if (!imageSize)		// This should only be true for BI_RGB, really.
				imageSize = (((bih.biWidth * bih.biBitCount + 31) >> 5) << 2) * abs(bih.biHeight);

			if (imageSize >= alignmentThreshold)
				mAlignment = 512;
			break;
		}
	}
}

bool VDAVIOutputFileSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputFileSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputStripedSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputStripedSystem::VDAVIOutputStripedSystem(const wchar_t *filename)
	: mbUse1GBLimit(false)
	, mpStripeSystem(new AVIStripeSystem(VDTextWToA(filename, -1).c_str()))
{
}

VDAVIOutputStripedSystem::~VDAVIOutputStripedSystem() {
}

void VDAVIOutputStripedSystem::Set1GBLimit(bool bUse1GBLimit) {
	mbUse1GBLimit = bUse1GBLimit;
}

IVDMediaOutput *VDAVIOutputStripedSystem::CreateSegment() {
	vdautoptr<AVIOutputStriped> pFile(new AVIOutputStriped(mpStripeSystem));

	if (!pFile)
		throw MyMemoryError();

	pFile->set_1Gb_limit();
	return pFile.release();
}

void VDAVIOutputStripedSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputStriped> pFile(static_cast<AVIOutputStriped *>(pSegment));

	pFile->finalize();
}

bool VDAVIOutputStripedSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputStripedSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputWAVSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputWAVSystem::VDAVIOutputWAVSystem(const wchar_t *pszFilename)
	: mFilename(pszFilename)
	, mBufferSize(1048576)
{
}

VDAVIOutputWAVSystem::~VDAVIOutputWAVSystem() {
}

void VDAVIOutputWAVSystem::SetBuffer(int bufferSize) {
	mBufferSize = bufferSize;
}

IVDMediaOutput *VDAVIOutputWAVSystem::CreateSegment() {
	vdautoptr<AVIOutputWAV> pOutput(new AVIOutputWAV);

	pOutput->createAudioStream()->setFormat(&mAudioFormat[0], mAudioFormat.size());

	pOutput->setBufferSize(mBufferSize);

	pOutput->init(mFilename.c_str());

	return pOutput.release();
}

void VDAVIOutputWAVSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputWAV> pFile(static_cast<AVIOutputWAV *>(pSegment));

	pFile->finalize();
}

bool VDAVIOutputWAVSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputRawSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputRawSystem::VDAVIOutputRawSystem(const wchar_t *pszFilename)
	: mFilename(pszFilename)
	, mBufferSize(1048576)
{
}

VDAVIOutputRawSystem::~VDAVIOutputRawSystem() {
}

void VDAVIOutputRawSystem::SetBuffer(int bufferSize) {
	mBufferSize = bufferSize;
}

IVDMediaOutput *VDAVIOutputRawSystem::CreateSegment() {
	vdautoptr<AVIOutputRawAudio> pOutput(new AVIOutputRawAudio);

	pOutput->createAudioStream()->setFormat(&mAudioFormat[0], mAudioFormat.size());

	pOutput->setBufferSize(mBufferSize);

	pOutput->init(mFilename.c_str());

	return pOutput.release();
}

void VDAVIOutputRawSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	AVIOutputRawAudio *pFile = static_cast<AVIOutputRawAudio *>(pSegment);

	pFile->finalize();
}

bool VDAVIOutputRawSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputRawVideoSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputRawVideoSystem::VDAVIOutputRawVideoSystem(const wchar_t *pszFilename, const VDAVIOutputRawVideoFormat& format)
	: mFilename(pszFilename)
	, mBufferSize(1048576)
	, mFormat(format)
{
}

VDAVIOutputRawVideoSystem::~VDAVIOutputRawVideoSystem() {
}

void VDAVIOutputRawVideoSystem::SetBuffer(int bufferSize) {
	mBufferSize = bufferSize;
}

IVDMediaOutput *VDAVIOutputRawVideoSystem::CreateSegment() {
	vdautoptr<AVIOutputRawVideo> pOutput(new AVIOutputRawVideo(mFormat));

	pOutput->SetInputLayout(mVideoImageLayout);
	pOutput->createVideoStream();

	pOutput->setBufferSize(mBufferSize);

	pOutput->init(mFilename.c_str());

	return pOutput.release();
}

void VDAVIOutputRawVideoSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputRawVideo> pFile(static_cast<AVIOutputRawVideo *>(pSegment));

	pFile->finalize();
}

bool VDAVIOutputRawVideoSystem::AcceptsVideo() {
	return true;
}

int VDAVIOutputRawVideoSystem::GetVideoOutputFormatOverride() {
	return mFormat.mOutputFormat;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputCLISystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputCLISystem::VDAVIOutputCLISystem(const wchar_t *pszFilename, const wchar_t *setName)
	: mFilename(pszFilename)
	, mBufferSize(1048576)
	, mEncSetName(setName)
	, mpVidEncProfile(NULL)
	, mpAudEncProfile(NULL)
	, mpMuxProfile(NULL)
{
	vdrefptr<VDExtEncSet> eset;
	if (!VDGetExternalEncoderSetByName(mEncSetName.c_str(), ~eset))
		throw MyError("There is no external encoder set named \"%ls.\"", mEncSetName.c_str());

	vdrefptr<VDExtEncProfile> vep;
	if (!eset->mVideoEncoder.empty() && !VDGetExternalEncoderProfileByName(eset->mVideoEncoder.c_str(), ~vep))
		throw MyError("Unable to find video encoder profile \"%ls\" referenced in encoder set \"%ls.\"", eset->mVideoEncoder.c_str(), mEncSetName.c_str());

	vdrefptr<VDExtEncProfile> aep;
	if (!eset->mAudioEncoder.empty() && !VDGetExternalEncoderProfileByName(eset->mAudioEncoder.c_str(), ~aep))
		throw MyError("Unable to find video encoder profile \"%ls\" referenced in encoder set \"%ls.\"", eset->mAudioEncoder.c_str(), mEncSetName.c_str());

	vdrefptr<VDExtEncProfile> mxp;
	if (!eset->mMultiplexer.empty() && !VDGetExternalEncoderProfileByName(eset->mMultiplexer.c_str(), ~mxp))
		throw MyError("Unable to find multiplexer profile \"%ls\" referenced in encoder set \"%ls.\"", eset->mMultiplexer.c_str(), mEncSetName.c_str());

	// validate set
	if (vep && vep->mType != kVDExtEncType_Video)
		throw MyError("The external encoder set \"%ls.\" does not have a valid video encoder entry.", mEncSetName.c_str());

	if (aep && aep->mType != kVDExtEncType_Audio)
		throw MyError("The external encoder set \"%ls.\" does not have a valid audio encoder entry.", mEncSetName.c_str());

	if (mxp && mxp->mType != kVDExtEncType_Mux)
		throw MyError("The external encoder set \"%ls.\" does not have a valid multiplexer entry.", mEncSetName.c_str());

	mpVidEncProfile = vep.release();
	mpAudEncProfile = aep.release();
	mpMuxProfile = mxp.release();
	mbUseOutputPathAsTemp = eset->mbUseOutputAsTemp;

	mbFinalizeOnAbort = eset->mbProcessPartialOutput;
}

VDAVIOutputCLISystem::~VDAVIOutputCLISystem() {
	if (mpVidEncProfile) {
		mpVidEncProfile->Release();
		mpVidEncProfile = NULL;
	}

	if (mpAudEncProfile) {
		mpAudEncProfile->Release();
		mpAudEncProfile = NULL;
	}

	if (mpMuxProfile) {
		mpMuxProfile->Release();
		mpMuxProfile = NULL;
	}
}

void VDAVIOutputCLISystem::SetBuffer(int bufferSize) {
	mBufferSize = bufferSize;
}

IVDMediaOutput *VDAVIOutputCLISystem::CreateSegment() {
	VDAVIOutputCLITemplate tmpl;
	tmpl.mpVideoEncoderProfile = mpVidEncProfile;
	tmpl.mpAudioEncoderProfile = mpAudEncProfile;
	tmpl.mpMultiplexerProfile = mpMuxProfile;
	tmpl.mbUseOutputPathAsTemp = mbUseOutputPathAsTemp;

	vdautoptr<IAVIOutputCLI> pOutput(VDCreateAVIOutputCLI(tmpl));
	IVDMediaOutput *out = vdpoly_cast<IVDMediaOutput *>(&*pOutput);

	if (mVideoImageLayout.format) {
		pOutput->SetInputLayout(mVideoImageLayout);
		IVDMediaOutputStream *videoOut = out->createVideoStream();
		videoOut->setStreamInfo(mVideoStreamInfo);
	}

	if (!mAudioFormat.empty())
		out->createAudioStream()->setFormat(&mAudioFormat[0], mAudioFormat.size());

	pOutput->SetBufferSize(mBufferSize);

	out->init(mFilename.c_str());

	pOutput.release();
	return out;
}

void VDAVIOutputCLISystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<IAVIOutputCLI> pFile(vdpoly_cast<IAVIOutputCLI *>(pSegment));

	if (!finalize && !mbFinalizeOnAbort)
		pFile->CloseWithoutFinalize();
	else
		pSegment->finalize();
}

bool VDAVIOutputCLISystem::AcceptsVideo() {
	return mpVidEncProfile != NULL;
}

bool VDAVIOutputCLISystem::AcceptsAudio() {
	return mpAudEncProfile != NULL;
}

int VDAVIOutputCLISystem::GetVideoOutputFormatOverride() {
	return nsVDPixmap::kPixFormat_YUV420_Planar;
}

bool VDAVIOutputCLISystem::IsCompressedAudioAllowed() {
	return mpAudEncProfile && !mpAudEncProfile->mbBypassCompression;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputImagesSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputImagesSystem::VDAVIOutputImagesSystem()
{
}

VDAVIOutputImagesSystem::~VDAVIOutputImagesSystem() {
}

IVDMediaOutput *VDAVIOutputImagesSystem::CreateSegment() {
	vdautoptr<AVIOutputImages> pOutput(new AVIOutputImages(mSegmentPrefix.c_str(), mSegmentSuffix.c_str(), mSegmentDigits, mFormat, mQuality));

	if (!mVideoFormat.empty())
		pOutput->createVideoStream()->setFormat(&mVideoFormat[0], mVideoFormat.size());

	pOutput->init(NULL);

	return pOutput.release();
}

void VDAVIOutputImagesSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputImages> pFile(static_cast<AVIOutputImages *>(pSegment));
	pFile->finalize();
}

void VDAVIOutputImagesSystem::SetFilenamePattern(const wchar_t *pszPrefix, const wchar_t *pszSuffix, int nMinimumDigits) {
	mSegmentPrefix		= pszPrefix;
	mSegmentSuffix		= pszSuffix;
	mSegmentDigits		= nMinimumDigits;
}

void VDAVIOutputImagesSystem::SetFormat(int format, int quality) {
	mFormat = format;
	mQuality = quality;
}

bool VDAVIOutputImagesSystem::AcceptsVideo() {
	return true;
}


///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputFilmstripSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputFilmstripSystem::VDAVIOutputFilmstripSystem(const wchar_t *filename)
	: mFilename(filename)
{
}

VDAVIOutputFilmstripSystem::~VDAVIOutputFilmstripSystem() {
}

IVDMediaOutput *VDAVIOutputFilmstripSystem::CreateSegment() {
	vdautoptr<AVIOutputFLM> pOutput(new AVIOutputFLM());

	if (!mVideoFormat.empty()) {
		IVDMediaOutputStream *pOutputStream = pOutput->createVideoStream();
		
		pOutputStream->setFormat(&mVideoFormat[0], mVideoFormat.size());
		pOutputStream->setStreamInfo(mVideoStreamInfo);
	}

	pOutput->init(mFilename.c_str());

	return pOutput.release();
}

void VDAVIOutputFilmstripSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputFLM> pFile(static_cast<AVIOutputFLM *>(pSegment));
	pFile->finalize();
}

bool VDAVIOutputFilmstripSystem::AcceptsVideo() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputGIFSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputGIFSystem::VDAVIOutputGIFSystem(const wchar_t *filename)
	: mFilename(filename)
	, mLoopCount(0)
{
}

VDAVIOutputGIFSystem::~VDAVIOutputGIFSystem() {
}

IVDMediaOutput *VDAVIOutputGIFSystem::CreateSegment() {
	vdautoptr<IVDAVIOutputGIF> pOutput(VDCreateAVIOutputGIF());

	pOutput->SetLoopCount(mLoopCount);

	AVIOutput *pAVIOut = pOutput->AsAVIOutput();
	if (!mVideoFormat.empty()) {
		IVDMediaOutputStream *pOutputStream = pAVIOut->createVideoStream();
		
		pOutputStream->setFormat(&mVideoFormat[0], mVideoFormat.size());
		pOutputStream->setStreamInfo(mVideoStreamInfo);
	}

	pAVIOut->init(mFilename.c_str());

	pOutput.release();

	return pAVIOut;
}

void VDAVIOutputGIFSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputFLM> pFile(static_cast<AVIOutputFLM *>(pSegment));
	pFile->finalize();
}

bool VDAVIOutputGIFSystem::AcceptsVideo() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputAPNGSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputAPNGSystem::VDAVIOutputAPNGSystem(const wchar_t *filename)
	: mFilename(filename)
	, mLoopCount(0)
{
}

VDAVIOutputAPNGSystem::~VDAVIOutputAPNGSystem() {
}

IVDMediaOutput *VDAVIOutputAPNGSystem::CreateSegment() {
	vdautoptr<IVDAVIOutputAPNG> pOutput(VDCreateAVIOutputAPNG());

    pOutput->SetFramesCount(mVideoStreamInfo.dwLength);
	pOutput->SetLoopCount(mLoopCount);
	pOutput->SetAlpha(mAlpha);
	pOutput->SetGrayscale(mGrayscale);

    uint32 dwRate  = mVideoStreamInfo.dwRate;
    uint32 dwScale = mVideoStreamInfo.dwScale;

    while ((dwRate>30000) || (dwScale>30000))
    {
        dwRate = dwRate/10;
        dwScale = dwScale/10;
    }

    pOutput->SetRate(dwRate);
	pOutput->SetScale(dwScale);

	AVIOutput *pAVIOut = pOutput->AsAVIOutput();
	if (!mVideoFormat.empty()) {
		IVDMediaOutputStream *pOutputStream = pAVIOut->createVideoStream();

		pOutputStream->setFormat(&mVideoFormat[0], mVideoFormat.size());
		pOutputStream->setStreamInfo(mVideoStreamInfo);
	}

	pAVIOut->init(mFilename.c_str());

	pOutput.release();

	return pAVIOut;
}

void VDAVIOutputAPNGSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputFLM> pFile(static_cast<AVIOutputFLM *>(pSegment));
	pFile->finalize();
}

bool VDAVIOutputAPNGSystem::AcceptsVideo() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputPreviewSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputPreviewSystem::VDAVIOutputPreviewSystem()
{
}

VDAVIOutputPreviewSystem::~VDAVIOutputPreviewSystem() {
}

IVDMediaOutput *VDAVIOutputPreviewSystem::CreateSegment() {
	vdautoptr<AVIOutputPreview> pOutput(new AVIOutputPreview);

	if (mVideoImageLayout.format)
		pOutput->createVideoStream();
	else if (!mVideoFormat.empty())
		pOutput->createVideoStream()->setFormat(&mVideoFormat[0], mVideoFormat.size());

	if (!mAudioFormat.empty()) {
		AVIAudioPreviewOutputStream *aout = static_cast<AVIAudioPreviewOutputStream *>(pOutput->createAudioStream());

		aout->setFormat(&mAudioFormat[0], mAudioFormat.size());
		aout->setStreamInfo(mAudioStreamInfo);
		aout->SetVBRMode(mbAudioVBR);
	}

	pOutput->init(NULL);

	return pOutput.release();
}

void VDAVIOutputPreviewSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputPreview> pFile(static_cast<AVIOutputPreview *>(pSegment));
	pFile->finalize();
}

bool VDAVIOutputPreviewSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputPreviewSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputNullVideoAnalysisSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputNullVideoSystem::VDAVIOutputNullVideoSystem()
{
}

VDAVIOutputNullVideoSystem::~VDAVIOutputNullVideoSystem() {
}

IVDMediaOutput *VDAVIOutputNullVideoSystem::CreateSegment() {
	vdautoptr<AVIOutputNull> pOutput(new AVIOutputNull);

	if (!mVideoFormat.empty())
		pOutput->createVideoStream()->setFormat(&mVideoFormat[0], mVideoFormat.size());

	pOutput->init(NULL);

	return pOutput.release();
}

void VDAVIOutputNullVideoSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<AVIOutputNull> pFile(static_cast<AVIOutputNull *>(pSegment));
	pFile->finalize();
}

bool VDAVIOutputNullVideoSystem::AcceptsVideo() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputSegmentedSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputSegmentedSystem::VDAVIOutputSegmentedSystem(IVDDubberOutputSystem *pSystem, bool intervalIsSeconds, double interval, double preloadInSeconds, sint64 max_bytes, sint64 max_frames)
	: mpChildSystem(pSystem)
	, mbIntervalIsSeconds(intervalIsSeconds)
	, mInterval(interval)
	, mPreload(preloadInSeconds)
	, mMaxBytes(max_bytes)
	, mMaxFrames(max_frames)
{
}

VDAVIOutputSegmentedSystem::~VDAVIOutputSegmentedSystem() {
}

IVDMediaOutput *VDAVIOutputSegmentedSystem::CreateSegment() {
	double interval = mInterval;

	if (mbIntervalIsSeconds) {
		interval *= (double)mVideoStreamInfo.dwRate / (double)mVideoStreamInfo.dwScale;
		if (interval < 1.0)
			interval = 1.0;
	}

	vdautoptr<IVDMediaOutput> pOutput(VDCreateMediaOutputSegmented(mpChildSystem, interval, mPreload, mMaxBytes, mMaxFrames));

	if (!mVideoFormat.empty()) {
		IVDMediaOutputStream *pVideoOut = pOutput->createVideoStream();
		pVideoOut->setFormat(&mVideoFormat[0], mVideoFormat.size());
		pVideoOut->setStreamInfo(mVideoStreamInfo);
	}

	if (!mAudioFormat.empty()) {
		IVDMediaOutputStream *pAudioOut = pOutput->createAudioStream();
		pAudioOut->setFormat(&mAudioFormat[0], mAudioFormat.size());
		pAudioOut->setStreamInfo(mAudioStreamInfo);
	}

	pOutput->init(NULL);

	return pOutput.release();
}

void VDAVIOutputSegmentedSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast, bool finalize) {
	vdautoptr<IVDMediaOutput> pSegment2(pSegment);
	pSegment2->finalize();
}

void VDAVIOutputSegmentedSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	VDDubberOutputSystem::SetVideo(asi, pFormat, cbFormat);
	mpChildSystem->SetVideo(asi, pFormat, cbFormat);
}

void VDAVIOutputSegmentedSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr) {
	VDDubberOutputSystem::SetAudio(asi, pFormat, cbFormat, bInterleaved, vbr);
	mpChildSystem->SetAudio(asi, pFormat, cbFormat, bInterleaved, vbr);
}

bool VDAVIOutputSegmentedSystem::AcceptsVideo() {
	return mpChildSystem->AcceptsVideo();
}

bool VDAVIOutputSegmentedSystem::AcceptsAudio() {
	return mpChildSystem->AcceptsAudio();
}

bool VDAVIOutputSegmentedSystem::AreNullFramesAllowed() {
	return mpChildSystem->AreNullFramesAllowed();
}

bool VDAVIOutputSegmentedSystem::IsVideoCompressionEnabled() {
	return mpChildSystem->IsVideoCompressionEnabled();
}

//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2007 Avery Lee
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
#include <vd2/plugin/vdinputdriver.h>

///////////////////////////////////////////////////////////////////////////////

#include <vd2/system/refcount.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/debug.h>
#include <vd2/system/w32assist.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "InputFile.h"
#include "VideoSource.h"
#include "AudioSource.h"
#include "plugins.h"
#include "project.h"

///////////////////////////

extern const char *LookupVideoCodec(uint32);
extern VDProject *g_project;

///////////////////////////////////////////////////////////////////////////////

#define VDASSERT_PREEXT_RT(condition, args) VDASSERT(condition)
#define VDASSERT_POSTEXT_RT(condition, args) VDASSERT(condition)

bool VDPreferencesIsPreferInternalVideoDecodersEnabled();

IVDVideoDecompressor *VDFindVideoDecompressorEx(uint32 fccHandler, const VDAVIBitmapInfoHeader *hdr, uint32 hdrlen, bool preferInternal);

///////////////////////////////////////////////////////////////////////////////

class VDInputDriverContextImpl : public VDXInputDriverContext, public vdrefcounted<IVDPluginCallbacks> {
public:
	VDInputDriverContextImpl(const VDPluginDescription *);
	~VDInputDriverContextImpl();

	void BeginExternalCall();
	void EndExternalCall();

public:
	virtual void * VDXAPIENTRY GetExtendedAPI(const char *pExtendedAPIName);
	virtual void VDXAPIENTRYV SetError(const char *format, ...);
	virtual void VDXAPIENTRY SetErrorOutOfMemory();
	virtual uint32 VDXAPIENTRY GetCPUFeatureFlags();

	VDStringW	mName;
	int max_api_version;
	MyError		mError;
};

VDInputDriverContextImpl::VDInputDriverContextImpl(const VDPluginDescription *pInfo) {
	mName.sprintf(L"Input driver plugin \"%s\"", pInfo->mName.c_str());
	max_api_version = pInfo->mpShadowedInfo->mTypeAPIVersionUsed;
	mpCallbacks = this;
}

VDInputDriverContextImpl::~VDInputDriverContextImpl() {
}

void VDInputDriverContextImpl::BeginExternalCall() {
	mError.clear();
}

void VDInputDriverContextImpl::EndExternalCall() {
	if (mError.gets()) {
		MyError tmp;
		tmp.TransferFrom(mError);
		throw tmp;
	}
}

void * VDXAPIENTRY VDInputDriverContextImpl::GetExtendedAPI(const char *pExtendedAPIName) {
	if (strcmp(pExtendedAPIName,"IFilterModPixmap")==0)
		return &g_project->filterModPixmap;
	return NULL;
}

void VDXAPIENTRYV VDInputDriverContextImpl::SetError(const char *format, ...) {
	va_list val;
	va_start(val, format);
	mError.vsetf(format, val);
	va_end(val);
}

void VDXAPIENTRY VDInputDriverContextImpl::SetErrorOutOfMemory() {
	MyMemoryError e;
	mError.TransferFrom(e);
}

uint32 VDXAPIENTRY VDInputDriverContextImpl::GetCPUFeatureFlags() {
	return CPUGetEnabledExtensions();
}

/*
struct VDInputDriverCallAutoScope {
	VDInputDriverCallAutoScope(VDInputDriverContextImpl *context) : mpContext(context) {
		mpContext->BeginExternalCall();
	}

	~VDInputDriverCallAutoScope() {
		mpContext->EndExternalCall();
	}

	operator bool() const { return false; }

	VDInputDriverContextImpl *mpContext;
};
*/

// We have to be careful NOT to call the error cleanup code when an exception
// occurs, because it'll throw another exception!
#define vdwithinputplugin(context) switch(VDExternalCodeBracket _exbracket = ((context)->BeginExternalCall(), VDExternalCodeBracketLocation((context)->mName.c_str(), __FILE__, __LINE__))) while((context)->EndExternalCall(), false) case false: default:

///////////////////////////////////////////////////////////////////////////////

namespace {
	typedef VDAtomicInt VDXAtomicInt;

	template<class T> class vdxunknown : public T {
	public:
		vdxunknown() : mRefCount(0) {}
		vdxunknown(const vdxunknown<T>& src) : mRefCount(0) {}		// do not copy the refcount
		virtual ~vdxunknown() {}

		vdxunknown<T>& operator=(const vdxunknown<T>&) {}			// do not copy the refcount

		inline virtual int VDXAPIENTRY AddRef() {
			return mRefCount.inc();
		}

		inline virtual int VDXAPIENTRY Release() {
			if (mRefCount == 1) {		// We are the only reference, so there is no threading issue.  Don't decrement to zero as this can cause double destruction with a temporary addref/release in destruction.
				delete this;
				return 0;
			}

			VDASSERT(mRefCount > 1);

			return mRefCount.dec();
		}

		virtual void *VDXAPIENTRY AsInterface(uint32 iid) {
			if (iid == IVDXUnknown::kIID)
				return static_cast<IVDXUnknown *>(this);

			if (iid == T::kIID)
				return static_cast<T *>(this);

			return NULL;
		}

	protected:
		VDXAtomicInt		mRefCount;
	};

	template<class T>
	inline uint32 vdxpoly_id_from_ptr(T *p) {
		return T::kIID;
	}

	template<class T>
	T vdxpoly_cast(IVDXUnknown *pUnk) {
		return pUnk ? (T)pUnk->AsInterface(vdxpoly_id_from_ptr(T(NULL))) : NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDVideoDecoderModelDefaultIP : public vdxunknown<IVDXVideoDecoderModel> {
public:
	VDVideoDecoderModelDefaultIP(IVDVideoSource *pVS);
	~VDVideoDecoderModelDefaultIP();

	void	VDXAPIENTRY Reset();
	void	VDXAPIENTRY SetDesiredFrame(sint64 frame_num);
	sint64	VDXAPIENTRY GetNextRequiredSample(bool& is_preroll);
	int		VDXAPIENTRY GetRequiredCount();
	bool	VDXAPIENTRY IsDecodable(sint64 sample_num);

protected:
	sint64	mLastFrame;
	sint64	mNextFrame;
	sint64	mDesiredFrame;

	IVDVideoSource *mpVS;
};

VDVideoDecoderModelDefaultIP::VDVideoDecoderModelDefaultIP(IVDVideoSource *pVS)
	: mpVS(pVS)
	, mLastFrame(-1)
	, mNextFrame(-1)
	, mDesiredFrame(-1)
{
}

VDVideoDecoderModelDefaultIP::~VDVideoDecoderModelDefaultIP() {
}

void VDXAPIENTRY VDVideoDecoderModelDefaultIP::Reset() {
	mLastFrame = -1;
}

void VDXAPIENTRY VDVideoDecoderModelDefaultIP::SetDesiredFrame(sint64 frame_num) {
	mDesiredFrame = frame_num;

	// Fast path for previous frame or current frame.
	if (frame_num == mLastFrame) {
		mNextFrame = -1;
		mDesiredFrame = -1;
		return;
	}

	if (frame_num == mLastFrame + 1) {
		mNextFrame = frame_num;
		return;
	}

	// Back off to last key frame.
	mNextFrame = mpVS->nearestKey(frame_num);

	// Check if we have already decoded a frame that is nearer; if so we can resume
	// decoding from that point.
	if (mLastFrame >= mNextFrame && mLastFrame < frame_num)
		mNextFrame = mLastFrame + 1;
}

sint64 VDXAPIENTRY VDVideoDecoderModelDefaultIP::GetNextRequiredSample(bool& is_preroll) {
	if (mDesiredFrame < 0) {
		is_preroll = false;
		return -1;
	}

	sint64 frame = mNextFrame++;
	is_preroll = true;

	mLastFrame = frame;

	if (frame == mDesiredFrame) {
		mDesiredFrame = -1;
		is_preroll = false;
	}

	return frame;
}

int VDXAPIENTRY VDVideoDecoderModelDefaultIP::GetRequiredCount() {
	return VDClampToSint32(mDesiredFrame - mLastFrame);
}

bool VDXAPIENTRY VDVideoDecoderModelDefaultIP::IsDecodable(sint64 sample_num) {
	if (mLastFrame == mDesiredFrame)
		return true;
	
	return mpVS->isKey(sample_num);
}

///////////////////////////////////////////////////////////////////////////////

class VDVideoDecoderDefault : public vdxunknown<IVDXVideoDecoder> {
public:
	VDVideoDecoderDefault(IVDVideoDecompressor *decomp, int w, int h);
	~VDVideoDecoderDefault();

	const void *		VDXAPIENTRY DecodeFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, sint64 sampleNumber, sint64 targetFrame);
	uint32				VDXAPIENTRY GetDecodePadding();
	void				VDXAPIENTRY Reset();
	bool				VDXAPIENTRY IsFrameBufferValid();
	const VDXPixmap&	VDXAPIENTRY GetFrameBuffer();
	bool				VDXAPIENTRY SetTargetFormat(int format, bool useDIBAlignment);
	bool				VDXAPIENTRY SetDecompressedFormat(const VDXBITMAPINFOHEADER *pbih);

	bool				VDXAPIENTRY IsDecodable(sint64 sample_num);
	const void *		VDXAPIENTRY GetFrameBufferBase();

protected:
	VDPosition	mCurrentSample;
	VDXPixmap	mFrameBuffer;
	int			mWidth;
	int			mHeight;
	void		*mpFrameBufferBase;

	vdautoptr<IVDVideoDecompressor> mpDecompressor;
};

VDVideoDecoderDefault::VDVideoDecoderDefault(IVDVideoDecompressor *decomp, int w, int h)
	: mCurrentSample(-1)
	, mpDecompressor(decomp)
	, mWidth(w)
	, mHeight(h)
	, mpFrameBufferBase(malloc(((w + 3) & ~3) * h * 4))
{
	if (!mpFrameBufferBase)
		throw MyMemoryError();
}

VDVideoDecoderDefault::~VDVideoDecoderDefault() {
	free(mpFrameBufferBase);
}

const void * VDXAPIENTRY VDVideoDecoderDefault::DecodeFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, sint64 sampleNumber, sint64 targetFrame) {
	if (inputBuffer) {
		mCurrentSample = -1;

		mpDecompressor->DecompressFrame(mpFrameBufferBase, inputBuffer, data_len, !is_preroll, is_preroll);

		mCurrentSample = sampleNumber;
	}

	return mpFrameBufferBase;
}

uint32 VDXAPIENTRY VDVideoDecoderDefault::GetDecodePadding() {
	return 16;
}

void VDXAPIENTRY VDVideoDecoderDefault::Reset() {
	mCurrentSample = -1;
}

bool VDXAPIENTRY VDVideoDecoderDefault::IsFrameBufferValid() {
	return mCurrentSample >= 0;
}

const VDXPixmap& VDXAPIENTRY VDVideoDecoderDefault::GetFrameBuffer() {
	return mFrameBuffer;
}

bool VDXAPIENTRY VDVideoDecoderDefault::SetTargetFormat(int format, bool useDIBAlignment) {
	if (!mpDecompressor->SetTargetFormat(format))
		return false;

	VDPixmapLayout layout;
	VDMakeBitmapCompatiblePixmapLayout(layout, mWidth, mHeight, mpDecompressor->GetTargetFormat(), mpDecompressor->GetTargetFormatVariant());
	const VDPixmap px = VDPixmapFromLayout(layout, mpFrameBufferBase);

	mFrameBuffer = (const VDXPixmap&)px;

	return true;
}

bool VDXAPIENTRY VDVideoDecoderDefault::SetDecompressedFormat(const VDXBITMAPINFOHEADER *pbih) {
	if (!mpDecompressor->SetTargetFormat(pbih))
		return false;

	mFrameBuffer.data		= mpFrameBufferBase;
	mFrameBuffer.palette	= NULL;
	mFrameBuffer.w			= mWidth;
	mFrameBuffer.h			= mHeight;
	mFrameBuffer.pitch		= 0;
	mFrameBuffer.format		= 0;
	mFrameBuffer.data2		= NULL;
	mFrameBuffer.pitch2		= 0;
	mFrameBuffer.data3		= NULL;
	mFrameBuffer.pitch3		= 0;

	return true;
}

bool VDXAPIENTRY VDVideoDecoderDefault::IsDecodable(sint64 sample_num) {
	return sample_num == mCurrentSample + 1;
}

const void * VDXAPIENTRY VDVideoDecoderDefault::GetFrameBufferBase() {
	return mpFrameBufferBase;
}

///////////////////////////////////////////////////////////////////////////////
class VDVideoSourcePlugin : public VideoSource {
public:
	VDVideoSourcePlugin(IVDXVideoSource *pVS, VDInputDriverContextImpl *pContext, InputFile *pParent);
	~VDVideoSourcePlugin();

	// DubSource
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);

	// VideoSource
	const void *getFrameBuffer();
	const VDFraction getPixelAspectRatio() const;

	const VDPixmap& getTargetFormat();
	bool setTargetFormat(VDPixmapFormatEx format);
	bool setDecompressedFormat(int depth);
	bool setDecompressedFormat(const VDAVIBitmapInfoHeader *pbih);

	void streamSetDesiredFrame(VDPosition frame_num);
	VDPosition streamGetNextRequiredFrame(bool& is_preroll);
	int	streamGetRequiredCount(uint32 *totalsize);
	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_sample);
	uint32 streamGetDecodePadding();

	void streamBegin(bool fRealTime, bool bForceReset);
	void streamRestart();
	void streamAppendReinit();
	void applyStreamMode(sint32 flags);
	void LoadFormat();

	void invalidateFrameBuffer();
	bool isFrameBufferValid();

	const void *getFrame(VDPosition frameNum);
	char		getFrameTypeChar(VDPosition lFrameNum);

	eDropType	getDropType(VDPosition lFrameNum);

	bool isKey(VDPosition lSample);
	VDPosition nearestKey(VDPosition lSample);
	VDPosition prevKey(VDPosition lSample);
	VDPosition nextKey(VDPosition lSample);

	bool isKeyframeOnly();
	bool isSyncDecode();

	VDPosition	streamToDisplayOrder(VDPosition sample_num);
	VDPosition	displayToStreamOrder(VDPosition display_num);
	VDPosition	getRealDisplayFrame(VDPosition display_num);

	bool		isDecodable(VDPosition sample_num);

	sint64		getSampleBytePosition(VDPosition sample_num);

protected:
	vdrefptr<InputFile>					mpParent;		// must be before IVDX objects

	vdrefptr<IVDXVideoSource> mpXVS;
	vdrefptr<IVDXStreamSource> mpXS;
	vdrefptr<IVDXStreamSourceV5> mpXS3;
	vdrefptr<IVDXVideoDecoder> mpXVDec;
	vdrefptr<IFilterModVideoDecoder> mpFMVDec;
	vdrefptr<IVDXVideoDecoderModel> mpXVDecModel;

	vdrefptr<VDInputDriverContextImpl>	mpContext;

	VDXStreamSourceInfoV3	mSSInfo;
	VDXVideoSourceInfo	mVSInfo;
	VDPixmap mTargetFormat;
	VDPixmapFormatEx formatOpt;
};

void VDVideoSourcePlugin::LoadFormat()
{
	const void *format;
	uint32 formatLen;
	vdwithinputplugin(mpContext) {
		format = mpXS->GetDirectFormat();
		formatLen = mpXS->GetDirectFormatLen();
	}

	if (format) {
		memcpy(allocFormat(formatLen), format, formatLen);
	} else {
		BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)allocFormat(sizeof(BITMAPINFOHEADER));

		bih->biSize				= sizeof(BITMAPINFOHEADER);
		bih->biWidth			= mVSInfo.mWidth;
		bih->biHeight			= mVSInfo.mHeight;
		bih->biPlanes			= 1;
		bih->biCompression		= 0xFFFFFFFF;
		bih->biBitCount			= 32;
		bih->biSizeImage		= 0;
		bih->biXPelsPerMeter	= 0;
		bih->biYPelsPerMeter	= 0;
		bih->biClrUsed			= 0;
		bih->biClrImportant		= 0;
	}
}

VDVideoSourcePlugin::VDVideoSourcePlugin(IVDXVideoSource *pVS, VDInputDriverContextImpl *pContext, InputFile *pParent)
	: mpParent(pParent)
	, mpXVS(pVS)
	, mpXS(vdxpoly_cast<IVDXStreamSource *>(mpXVS))
	, mpContext(pContext)
{
	memset(&mSSInfo, 0, sizeof mSSInfo);
	memset(&mVSInfo, 0, sizeof mVSInfo);
	formatOpt = 0;
	profile_comment = VDTextWToA(pContext->mName);

	vdwithinputplugin(mpContext) {
		IVDXStreamSourceV3 *xssv3 = (IVDXStreamSourceV3 *)mpXS->AsInterface(IVDXStreamSourceV3::kIID);
		if (xssv3)
			xssv3->GetStreamSourceInfoV3(mSSInfo);
		else
			mpXS->GetStreamSourceInfo(mSSInfo.mInfo);

		pVS->GetVideoSourceInfo(mVSInfo);

		mpXS3 = (IVDXStreamSourceV5 *)mpXS->AsInterface(IVDXStreamSourceV5::kIID);

		// create a video decoder.
		pVS->CreateVideoDecoder(~mpXVDec);
	}

	const void *format;
	uint32 formatLen;
	vdwithinputplugin(mpContext) {
		format = mpXS->GetDirectFormat();
		formatLen = mpXS->GetDirectFormatLen();
	}

	if (!mpXVDec) {
		uint32 fcc = 0;

		if (format) {
			const VDAVIBitmapInfoHeader *hdr = (const VDAVIBitmapInfoHeader *)format;
			fcc = hdr->biCompression;

			IVDVideoDecompressor *dec = VDFindVideoDecompressorEx(mSSInfo.mfccHandler, hdr, formatLen, VDPreferencesIsPreferInternalVideoDecodersEnabled());

			if (dec)
				mpXVDec = new VDVideoDecoderDefault(dec, hdr->biWidth, hdr->biHeight);
		}

		if (!mpXVDec) {
			char buf[5] = "    ";

			for(int i=0; i<4; ++i) {
				uint8 c = (uint8)((fcc >> (i * 8)) & 0xff);

				if ((uint8)(c - 0x20) < 0x7f)
					buf[i] = c;
			}

			const char *s = LookupVideoCodec(mSSInfo.mfccHandler);

			throw MyError("Unable to locate a video codec to decompress the video format '%s' (%s)."
						, buf
						,s ? s : "unknown");
		}
	}

	mpFMVDec = (IFilterModVideoDecoder*)mpXS->AsInterface(IFilterModVideoDecoder::kIID);
	if(!mpFMVDec) mpFMVDec = (IFilterModVideoDecoder*)mpXVDec->AsInterface(IFilterModVideoDecoder::kIID);

	// create a video decoder model.
	vdwithinputplugin(mpContext) {
		pVS->CreateVideoDecoderModel(~mpXVDecModel);
	}

	switch(mVSInfo.mDecoderModel) {
	case VDXVideoSourceInfo::kDecoderModelCustom:
		break;
	case VDXVideoSourceInfo::kDecoderModelDefaultIP:
		mpXVDecModel = new VDVideoDecoderModelDefaultIP(this);
		break;
	default:
		throw MyError("Error detected in input driver plugin: Unsupported video decoder model (%d).", mVSInfo.mDecoderModel);
	}

	if (!mpXVDecModel)
		throw MyMemoryError();

	mSampleFirst = 0;
	mSampleLast = mSSInfo.mInfo.mSampleCount;

	vdwithinputplugin(mpContext) {
		if (mpXVDec->SetTargetFormat(0, false)) {
			const VDXPixmap &px = mpXVDec->GetFrameBuffer();
			mDefaultFormat = px.format;
		}
	}

	LoadFormat();

	streamInfo.fccType			= VDAVIStreamInfo::kTypeVideo;
	streamInfo.fccHandler		= mSSInfo.mfccHandler;
	streamInfo.dwFlags			= 0;
	streamInfo.dwCaps			= 0;
	streamInfo.wPriority		= 0;
	streamInfo.wLanguage		= 0;
	streamInfo.dwScale			= mSSInfo.mInfo.mSampleRate.mDenominator;
	streamInfo.dwRate			= mSSInfo.mInfo.mSampleRate.mNumerator;
	streamInfo.dwStart			= 0;
	streamInfo.dwLength			= VDClampToUint32(mSSInfo.mInfo.mSampleCount);
	streamInfo.dwInitialFrames	= 0;
	streamInfo.dwSuggestedBufferSize = 0;
	streamInfo.dwQuality		= (DWORD)-1;
	streamInfo.dwSampleSize		= 0;
	streamInfo.rcFrameLeft		= 0;
	streamInfo.rcFrameTop		= 0;
	streamInfo.rcFrameRight		= (uint16)mVSInfo.mWidth;
	streamInfo.rcFrameBottom	= (uint16)mVSInfo.mHeight;
}

VDVideoSourcePlugin::~VDVideoSourcePlugin() {
}

int VDVideoSourcePlugin::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *pBytesRead, uint32 *pSamplesRead) {
	uint32 actualBytes = 0xBAADF00D;
	uint32 actualSamples = 0xBAADF00D;

	bool result;
	
	vdwithinputplugin(mpContext) {
		//if(lpBuffer) VDPROFILEBEGINEX("V-Read", (uint32)lStart);
		result = mpXS->Read(lStart, lCount, lpBuffer, cbBuffer, &actualBytes, &actualSamples);
		//if(lpBuffer) VDPROFILEEND();
	}

	if (actualBytes == 0xBAADF00D || actualSamples == 0xBAADF00D)
		throw MyError("Error detected in plugin \"%ls\": A size query call to IVDXStreamSource::Read() returned uninitialized values for sample %u.", mpContext->mName.c_str(), (unsigned)lStart);

	if (pBytesRead)
		*pBytesRead = actualBytes;
	if (pSamplesRead)
		*pSamplesRead = actualSamples;

	return result || !lpBuffer ? IVDStreamSource::kOK : IVDStreamSource::kBufferTooSmall;
}

const void *VDVideoSourcePlugin::getFrameBuffer() {
	const void *p;
	vdwithinputplugin(mpContext) {
		p = mpXVDec->GetFrameBufferBase();
	}

	return p;
}

const VDFraction VDVideoSourcePlugin::getPixelAspectRatio() const {
	return VDFraction(mSSInfo.mInfo.mPixelAspectRatio.mNumerator, mSSInfo.mInfo.mPixelAspectRatio.mDenominator);
}

const VDPixmap& VDVideoSourcePlugin::getTargetFormat() {
	vdwithinputplugin(mpContext) {
		const VDXPixmap &px = mpXVDec->GetFrameBuffer();
		mTargetFormat = VDPixmap::copy(px);
		VDPixmapFormatEx format = px.format;

		mTargetFormat.info.clear();
		if (mpFMVDec) {
			const FilterModPixmapInfo& info = mpFMVDec->GetFrameBufferInfo();
			mTargetFormat.info.copy_frame(info);
			mTargetFormat.info.copy_ref(info);
			mTargetFormat.info.copy_alpha(info);
			format.colorRangeMode = info.colorRangeMode;
			format.colorSpaceMode = info.colorSpaceMode;
			//! todo: return struct size to grab further fields
		}
		mSourceFormat = format;
		format = VDPixmapFormatCombineOpt(format,formatOpt);
		mTargetFormat.format = format;
		mTargetFormat.info.colorRangeMode = format.colorRangeMode;
		mTargetFormat.info.colorSpaceMode = format.colorSpaceMode;
		VDAdjustPixmapInfoForRange(mTargetFormat.info, mTargetFormat.format);
	}

	return mTargetFormat;
}

bool VDVideoSourcePlugin::setTargetFormat(VDPixmapFormatEx format) {
	vdwithinputplugin(mpContext) {
		if (!mpXVDec->SetTargetFormat(format, true))
			return false;

		mpXVDecModel->Reset();
	}

	const VDXPixmap *px;

	vdwithinputplugin(mpContext) {
		px = &mpXVDec->GetFrameBuffer();
	}

	if (!VDMakeBitmapFormatFromPixmapFormat(mpTargetFormatHeader, px->format, 0, px->w, px->h)) {
		if (!VDMakeBitmapFormatFromPixmapFormat(mpTargetFormatHeader, VDPixmapFormatNormalize(px->format), 0, px->w, px->h))
			mpTargetFormatHeader.clear();
	}

	formatOpt = format;

	return true;
}

bool VDVideoSourcePlugin::setDecompressedFormat(int depth) {
	switch(depth) {
		case 8:
			return setTargetFormat(nsVDPixmap::kPixFormat_Pal8);
		case 16:
			return setTargetFormat(nsVDPixmap::kPixFormat_XRGB1555);
		case 24:
			return setTargetFormat(nsVDPixmap::kPixFormat_RGB888);
		case 32:
			return setTargetFormat(nsVDPixmap::kPixFormat_XRGB8888);
		default:
			return false;
	}
}

bool VDVideoSourcePlugin::setDecompressedFormat(const VDAVIBitmapInfoHeader *pbih) {
	// Note that we are deliberately paying attention to sign here; if we have a flipped DIB
	// then we want to punt to the SetDecompressedFormat() function.
	if ((uint32)pbih->biWidth == mVSInfo.mWidth && (uint32)pbih->biHeight == mVSInfo.mHeight) {
		int format = VDBitmapFormatToPixmapFormat(*pbih);
		if (format)
			return setTargetFormat(format);
	}

	vdwithinputplugin(mpContext) {
		if (!mpXVDec->SetDecompressedFormat((const VDXBITMAPINFOHEADER *)pbih))
			return false;
	}

	mpTargetFormatHeader.assign(pbih, VDGetSizeOfBitmapHeaderW32((const BITMAPINFOHEADER *)pbih));
	return true;
}

void VDVideoSourcePlugin::streamSetDesiredFrame(VDPosition frame_num) {
	VDASSERT_PREEXT_RT((uint64)frame_num < (uint64)mSampleLast, ("streamSetDesiredFrame(): frame %I64d not in 0-%I64d", frame_num, mSampleLast - 1));

	if (frame_num >= mSampleLast)
		frame_num = mSampleLast - 1;
	if (frame_num < mSampleFirst)
		frame_num = mSampleFirst;

	sint64 stream_num;
	vdwithinputplugin(mpContext) {
		stream_num = mpXVS->GetSampleNumberForFrame(frame_num);
	}

	VDASSERT_POSTEXT_RT((uint64)stream_num < (uint64)mSampleLast, ("displayToStreamOrder(%I64d) returned out of range %I64d (should be in 0-%I64d)", frame_num, stream_num, mSampleLast - 1));

	vdwithinputplugin(mpContext) {
		mpXVDecModel->SetDesiredFrame(stream_num);
	}
}

VDPosition VDVideoSourcePlugin::streamGetNextRequiredFrame(bool& is_preroll) {
	sint64 pos;
	
	vdwithinputplugin(mpContext) {
		pos = mpXVDecModel->GetNextRequiredSample(is_preroll);
	}

	return pos;
}

int	VDVideoSourcePlugin::streamGetRequiredCount(uint32 *totalsize) {
	VDASSERT(!totalsize);
	int count;
	
	vdwithinputplugin(mpContext) {
		count = mpXVDecModel->GetRequiredCount();
	}

	return count;
}

const void *VDVideoSourcePlugin::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_num) {
	const void *fb;
	
	vdwithinputplugin(mpContext) {
		fb = mpXVDec->DecodeFrame(data_len ? inputBuffer : NULL, data_len, is_preroll, sample_num, target_num);
	}

	return fb;
}

uint32 VDVideoSourcePlugin::streamGetDecodePadding() {
	uint32 padding;
	
	vdwithinputplugin(mpContext) {
		padding = mpXVDec->GetDecodePadding();
	}

	return padding;
}

void VDVideoSourcePlugin::applyStreamMode(sint32 flags) {
	if (!mpXS3) return;
		vdwithinputplugin(mpContext) {
		mpXS3->ApplyStreamMode(flags);
	}
	LoadFormat();
}

void VDVideoSourcePlugin::streamBegin(bool fRealTime, bool bForceReset) {
	if (bForceReset) {
		vdwithinputplugin(mpContext) {
			mpXVDecModel->Reset();
		}
	}
}

void VDVideoSourcePlugin::streamRestart() {
	vdwithinputplugin(mpContext) {
		mpXVDecModel->Reset();
	}
}

void VDVideoSourcePlugin::streamAppendReinit() {
	vdwithinputplugin(mpContext) {
		IVDXStreamSourceV3 *xssv3 = (IVDXStreamSourceV3 *)mpXS->AsInterface(IVDXStreamSourceV3::kIID);
		if (xssv3)
			xssv3->GetStreamSourceInfoV3(mSSInfo);
		else
			mpXS->GetStreamSourceInfo(mSSInfo.mInfo);
	}

	mSampleLast = mSSInfo.mInfo.mSampleCount;
	streamInfo.dwLength	= VDClampToUint32(mSSInfo.mInfo.mSampleCount);
}

void VDVideoSourcePlugin::invalidateFrameBuffer() {
	vdwithinputplugin(mpContext) {
		mpXVDec->Reset();
	}
}

bool VDVideoSourcePlugin::isFrameBufferValid() {
	bool fbvalid;
	
	vdwithinputplugin(mpContext) {
		fbvalid = mpXVDec->IsFrameBufferValid();
	}

	return fbvalid;
}

const void *VDVideoSourcePlugin::getFrame(VDPosition frameNum) {
	// range check
	if (frameNum < 0 || frameNum >= mSSInfo.mInfo.mSampleCount)
		return NULL;

	sint64 sampleNum;
	vdwithinputplugin(mpContext) {
		sampleNum = mpXVS->GetSampleNumberForFrame(frameNum);
		mpXVDecModel->SetDesiredFrame(sampleNum);
	}

	// decode frames until we get to the desired point
	vdblock<char> buffer;

	bool is_preroll;
	do {
		VDPosition pos = mpXVDecModel->GetNextRequiredSample(is_preroll);
		uint32 padding = mpXVDec->GetDecodePadding();

		if (pos < 0) {
			vdwithinputplugin(mpContext) {
				mpXVDec->DecodeFrame(NULL, 0, is_preroll, -1, sampleNum);
			}
		} else {
			uint32 actualSamples;
			uint32 actualBytes;

			for(;;) {
				bool result = false;
				
				if (buffer.size() > padding) {
					vdwithinputplugin(mpContext) {
						//VDPROFILEBEGINEX("V-Read", (uint32)pos);
						result = mpXS->Read(pos, 1, buffer.data(), buffer.size() - padding, &actualBytes, &actualSamples);
						//VDPROFILEEND();
					}

					if (result && !buffer.empty())
						break;
				}

				actualSamples = 0xBAADF00D;
				actualBytes = 0xBAADF00D;

				vdwithinputplugin(mpContext) {
					result = mpXS->Read(pos, 1, NULL, 0, &actualBytes, &actualSamples);
				}

				if (!result)
					throw MyError("Error detected in plugin \"%ls\": A size query call to IVDXStreamSource::Read() returned false for sample %u.", mpContext->mName.c_str(), (unsigned)pos);

				if (actualBytes == 0xBAADF00D || actualSamples == 0xBAADF00D)
					throw MyError("Error detected in plugin \"%ls\": A size query call to IVDXStreamSource::Read() returned uninitialized values for sample %u.", mpContext->mName.c_str(), (unsigned)pos);

				if (actualBytes==0) actualBytes = 1;
				buffer.resize(actualBytes + padding);
			}

			vdwithinputplugin(mpContext) {
				mpXVDec->DecodeFrame(buffer.data(), actualBytes, is_preroll, pos, sampleNum);
			}
		}
	} while(is_preroll);

	const void *fb;
	
	vdwithinputplugin(mpContext) {
		fb = mpXVDec->GetFrameBufferBase();
	}

	return fb;
}

char VDVideoSourcePlugin::getFrameTypeChar(VDPosition lFrameNum) {
	if (lFrameNum < mSampleFirst || lFrameNum >= mSampleLast)
		return ' ';

	vdwithinputplugin(mpContext) {
		lFrameNum = mpXVS->GetSampleNumberForFrame(lFrameNum);
	}

	VDXVideoFrameInfo frameInfo;

	vdwithinputplugin(mpContext) {
		mpXVS->GetSampleInfo(lFrameNum, frameInfo);
	}

	return frameInfo.mTypeChar;
}

IVDVideoSource::eDropType VDVideoSourcePlugin::getDropType(VDPosition frame) {
	VDXVideoFrameInfo frameInfo;

	if (frame < mSampleFirst || frame >= mSampleLast)
		return IVDVideoSource::kDroppable;

	vdwithinputplugin(mpContext) {
		mpXVS->GetSampleInfo(frame, frameInfo);
	}

	switch(frameInfo.mFrameType) {
	case kVDXVFT_Independent:
		return IVDVideoSource::kIndependent;
	case kVDXVFT_Predicted:
		return IVDVideoSource::kDependant;
	case kVDXVFT_Bidirectional:
	case kVDXVFT_Null:
		return IVDVideoSource::kDroppable;
	default:
		return IVDVideoSource::kDroppable;
	}
}

bool VDVideoSourcePlugin::isKey(VDPosition frame) {
	bool iskey;

	if (frame < mSampleFirst || frame >= mSampleLast)
		return false;
	
	vdwithinputplugin(mpContext) {
		VDPosition stream_num = mpXVS->GetSampleNumberForFrame(frame);
		iskey = mpXVS->IsKey(stream_num);
	}

	return iskey;
}

VDPosition VDVideoSourcePlugin::nearestKey(VDPosition frame) {
	if (isKey(frame))
		return frame;

	frame = prevKey(frame);
	if (frame < 0)
		frame = 0;

	return frame;
}

VDPosition VDVideoSourcePlugin::prevKey(VDPosition frame) {
	while(--frame >= mSampleFirst) {
		if (isKey(frame))
			return frame;
	}

	return -1;
}

VDPosition VDVideoSourcePlugin::nextKey(VDPosition frame) {
	while(++frame < mSampleLast) {
		if (isKey(frame))
			return frame;
	}

	return -1;
}

bool VDVideoSourcePlugin::isKeyframeOnly() {
	return 0 != (mVSInfo.mFlags & VDXVideoSourceInfo::kFlagKeyframeOnly);
}

bool VDVideoSourcePlugin::isSyncDecode() {
	return 0 != (mVSInfo.mFlags & VDXVideoSourceInfo::kFlagSyncDecode);
}

VDPosition VDVideoSourcePlugin::streamToDisplayOrder(VDPosition sample_num) {
	if (sample_num < mSampleFirst || sample_num >= mSampleLast)
		return sample_num;

	sint64 display_num;
	vdwithinputplugin(mpContext) {
		display_num = mpXVS->GetFrameNumberForSample(sample_num);
	}

	return display_num;
}

VDPosition VDVideoSourcePlugin::displayToStreamOrder(VDPosition frame) {
	if (frame < mSampleFirst || frame >= mSampleLast)
		return frame;

	sint64 stream_num;	
	vdwithinputplugin(mpContext) {
		stream_num = mpXVS->GetSampleNumberForFrame(frame);
	}

	return stream_num;
}

VDPosition VDVideoSourcePlugin::getRealDisplayFrame(VDPosition frame) {
	if (frame < mSampleFirst || frame >= mSampleLast)
		return frame;

	VDPosition pos;
	vdwithinputplugin(mpContext) {
		pos = mpXVS->GetRealFrame(frame);
	}

	return pos;
}

bool VDVideoSourcePlugin::isDecodable(VDPosition sample_num) {
	if (sample_num < mSampleFirst || sample_num >= mSampleLast)
		return false;

	bool decodable;
	vdwithinputplugin(mpContext) {
		decodable = mpXVDec->IsDecodable(sample_num);
	}

	return decodable;
}

sint64 VDVideoSourcePlugin::getSampleBytePosition(VDPosition sample_num) {
	if (sample_num < mSampleFirst || sample_num >= mSampleLast)
		return -1;

	sint64 bytepos;
	vdwithinputplugin(mpContext) {
		bytepos = mpXVS->GetSampleBytePosition(sample_num);
	}

	return bytepos;
}

///////////////////////////////////////////////////////////////////////////////
class VDAudioSourcePlugin : public AudioSource {
public:
	VDAudioSourcePlugin(IVDXAudioSource *pVS, VDInputDriverContextImpl *pContext, InputFile *pParent);
	~VDAudioSourcePlugin();

	// DubSource
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	void SetTargetFormat(const VDWaveFormat* format);
	void streamAppendReinit();

	VBRMode GetVBRMode() const { return mVBRMode; }
	VDPosition	TimeToPositionVBR(VDTime us) const;
	VDTime		PositionToTimeVBR(VDPosition samples) const;

protected:
	vdrefptr<InputFile>			mpParent;		// also holds plugin open
	vdrefptr<IVDXAudioSource>	mpXAS;
	vdrefptr<IVDXStreamSource>	mpXS;

	VBRMode	mVBRMode;

	vdrefptr<VDInputDriverContextImpl>	mpContext;

	VDXStreamSourceInfoV3	mSSInfo;
	VDXAudioSourceInfo		mASInfo;
};

VDAudioSourcePlugin::VDAudioSourcePlugin(IVDXAudioSource *pAS, VDInputDriverContextImpl *pContext, InputFile *pParent)
	: mpParent(pParent)
	, mpXAS(pAS)
	, mpXS(vdxpoly_cast<IVDXStreamSource *>(mpXAS))
	, mpContext(pContext)
{
	memset(&mSSInfo, 0, sizeof mSSInfo);
	memset(&mASInfo, 0, sizeof mASInfo);

	bool isVBR;
	vdwithinputplugin(mpContext) {
		IVDXStreamSourceV3 *xssv3 = (IVDXStreamSourceV3 *)mpXS->AsInterface(IVDXStreamSourceV3::kIID);
		if (xssv3)
			xssv3->GetStreamSourceInfoV3(mSSInfo);
		else
			mpXS->GetStreamSourceInfo(mSSInfo.mInfo);

		pAS->GetAudioSourceInfo(mASInfo);
		isVBR = mpXS->IsVBR();
	}

	mSampleFirst = 0;
	mSampleLast = mSSInfo.mInfo.mSampleCount;

	const void *format;
	vdwithinputplugin(mpContext) {
		format = mpXS->GetDirectFormat();
	}
	if (format) {
		int len;
		vdwithinputplugin(mpContext) {
			len = mpXS->GetDirectFormatLen();
		}
		memcpy(allocFormat(len), format, len);
		memcpy(allocSrcWaveFormat(len), format, len);
	} else {
		throw MyError("The audio stream has a custom format that cannot be supported.");
	}

	streamInfo.fccType			= VDAVIStreamInfo::kTypeAudio;
	streamInfo.fccHandler		= 0;
	streamInfo.dwFlags			= 0;
	streamInfo.dwCaps			= 0;
	streamInfo.wPriority		= 0;
	streamInfo.wLanguage		= 0;
	streamInfo.dwScale			= mSSInfo.mInfo.mSampleRate.mDenominator;
	streamInfo.dwRate			= mSSInfo.mInfo.mSampleRate.mNumerator;
	streamInfo.dwStart			= 0;
	streamInfo.dwLength			= VDClampToUint32(mSSInfo.mInfo.mSampleCount);
	streamInfo.dwInitialFrames	= 0;
	streamInfo.dwSuggestedBufferSize = 0;
	streamInfo.dwQuality		= (DWORD)-1;
	streamInfo.dwSampleSize		= ((const VDWaveFormat *)format)->mBlockSize;
	streamInfo.rcFrameLeft		= 0;
	streamInfo.rcFrameTop		= 0;
	streamInfo.rcFrameRight		= 0;
	streamInfo.rcFrameBottom	= 0;

	mVBRMode = (mSSInfo.mFlags & VDXStreamSourceInfoV3::kFlagVariableSizeSamples) ? kVBRModeVariableFrames : isVBR ? kVBRModeTimestamped : kVBRModeNone;
	if (mVBRMode == kVBRModeVariableFrames)
		streamInfo.dwSampleSize = 0;
}

void VDAudioSourcePlugin::streamAppendReinit() {
	vdwithinputplugin(mpContext) {
		IVDXStreamSourceV3 *xssv3 = (IVDXStreamSourceV3 *)mpXS->AsInterface(IVDXStreamSourceV3::kIID);
		if (xssv3)
			xssv3->GetStreamSourceInfoV3(mSSInfo);
		else
			mpXS->GetStreamSourceInfo(mSSInfo.mInfo);
	}

	mSampleLast = mSSInfo.mInfo.mSampleCount;
	streamInfo.dwLength	= VDClampToUint32(mSSInfo.mInfo.mSampleCount);
}

void VDAudioSourcePlugin::SetTargetFormat(const VDWaveFormat* target) {
	// check if SetTargetFormat implemented
	if (mpContext->max_api_version<7) return;

	const void *format;
	vdwithinputplugin(mpContext) {
		mpXAS->SetTargetFormat((VDXWAVEFORMATEX*)target);
		format = mpXS->GetDirectFormat();
	}
	if (format) {
		int len;
		vdwithinputplugin(mpContext) {
			len = mpXS->GetDirectFormatLen();
		}
		memcpy(allocFormat(len), format, len);
	} else {
		throw MyError("The audio stream has a custom format that cannot be supported.");
	}
}

VDAudioSourcePlugin::~VDAudioSourcePlugin() {
}

int VDAudioSourcePlugin::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *pBytesRead, uint32 *pSamplesRead) {
	uint32 actualBytes;
	uint32 actualSamples;

	bool result;
	
	vdwithinputplugin(mpContext) {
		result = mpXS->Read(lStart, lCount, lpBuffer, cbBuffer, &actualBytes, &actualSamples);
	}

	if (pBytesRead)
		*pBytesRead = actualBytes;
	if (pSamplesRead)
		*pSamplesRead = actualSamples;

	return result || !lpBuffer ? IVDStreamSource::kOK : IVDStreamSource::kBufferTooSmall;
}

VDPosition VDAudioSourcePlugin::TimeToPositionVBR(VDTime us) const {
	if (mVBRMode == kVBRModeTimestamped) {
		VDPosition pos;
		vdwithinputplugin(mpContext) {
			pos = mpXS->TimeToPositionVBR(us);
		}
		return pos;
	}

	return AudioSource::TimeToPositionVBR(us);
}

VDTime VDAudioSourcePlugin::PositionToTimeVBR(VDPosition samples) const {
	if (mVBRMode == kVBRModeTimestamped) {
		VDTime t;
		vdwithinputplugin(mpContext) {
			t = mpXS->PositionToTimeVBR(samples);
		}
		return t;
	}

	return AudioSource::PositionToTimeVBR(samples);
}

///////////////////////////////////////////////////////////////////////////////
class VDInputFileOptionsPlugin : public InputFileOptions {
public:
	VDInputFileOptionsPlugin(IVDXInputOptions *opts, VDInputDriverContextImpl *context, VDPluginDescription *desc);
	~VDInputFileOptionsPlugin();

	IVDXInputOptions *GetXObject() const { return mpXOptions; }

	int write(char *buf, int buflen) const;

protected:
	vdrefptr<IVDXInputOptions> mpXOptions;
	vdrefptr<VDInputDriverContextImpl> mpContext;
	VDPluginPtr	mpPlugin;
};

VDInputFileOptionsPlugin::VDInputFileOptionsPlugin(IVDXInputOptions *opts, VDInputDriverContextImpl *context, VDPluginDescription *desc)
	: mpXOptions(opts)
	, mpContext(context)
	, mpPlugin(desc)
{
}

VDInputFileOptionsPlugin::~VDInputFileOptionsPlugin() {
}

int VDInputFileOptionsPlugin::write(char *buf, int buflen) const {
	int result;

	vdwithinputplugin(mpContext) {
		result = mpXOptions->Write(buf, (uint32)buflen);
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
class VDInputFilePlugin : public InputFile {
public:
	VDInputFilePlugin(IVDXInputFile *p, VDPluginDescription *pDesc, VDInputDriverContextImpl *pContext);
	~VDInputFilePlugin();

	void Init(const wchar_t *szFile);
	bool Append(const wchar_t *szFile);

	void setOptions(InputFileOptions *);
	InputFileOptions *promptForOptions(VDGUIHandle);
	InputFileOptions *createOptions(const void *buf, uint32 len);
	void InfoDialog(VDGUIHandle hwndParent);

	void GetTextInfo(tFileTextInfo& info);

	bool isOptimizedForRealtime();
	bool isStreaming();

	bool GetVideoSource(int index, IVDVideoSource **ppSrc);
	bool GetAudioSource(int index, AudioSource **ppSrc);
	int GetInputDriverApiVersion(){ 
		return mpContext->max_api_version;
	}
	void GetFileTool(IFilterModFileTool **pp){
		*pp=0;
		if (mpXObject) {
			IFilterModFileTool* p = (IFilterModFileTool*)mpXObject->AsInterface(IFilterModFileTool::kIID);
			if (p) {
				p->AddRef();
				*pp = p;
			}
		}
	}

	virtual int GetFileFlags() {
		if (mpContext->max_api_version>=8) return mpXObject->GetFileFlags();
		return -1;
	}

protected:
	vdrefptr<IVDXInputFile> mpXObject;
	vdrefptr<IVDXInputOptions> mpXOptions;

	vdrefptr<VDInputDriverContextImpl>	mpContext;

	VDPluginDescription	*mpPluginDesc;
	const VDPluginInfo				*mpPluginInfo;
};

VDInputFilePlugin::VDInputFilePlugin(IVDXInputFile *p, VDPluginDescription *pDesc, VDInputDriverContextImpl *pContext)
	: mpXObject(p)
	, mpPluginDesc(pDesc)
	, mpContext(pContext)
{
	mpPluginInfo = VDLockPlugin(pDesc);
}

VDInputFilePlugin::~VDInputFilePlugin() {
	mpXOptions = NULL;
	mpXObject = NULL;

	if (mpPluginInfo)
		VDUnlockPlugin(mpPluginDesc);
}

void VDInputFilePlugin::Init(const wchar_t *szFile) {
	vdwithinputplugin(mpContext) {
		mpXObject->Init(szFile, mpXOptions);
	}

	AddFilename(szFile);
}

bool VDInputFilePlugin::Append(const wchar_t *szFile) {
	bool appended;
	
	vdwithinputplugin(mpContext) {
		appended = mpXObject->Append(szFile);
	}

	if (appended && szFile) AddFilename(szFile);

	return appended;
}

void VDInputFilePlugin::setOptions(InputFileOptions *opts) {
	mpXOptions = static_cast<VDInputFileOptionsPlugin *>(opts)->GetXObject();
}

InputFileOptions *VDInputFilePlugin::promptForOptions(VDGUIHandle hwnd) {
	vdrefptr<IVDXInputOptions> opts;

	vdwithinputplugin(mpContext) {
		mpXObject->PromptForOptions((VDXHWND)hwnd, ~opts);
	}

	if (!opts)
		return NULL;

	return new VDInputFileOptionsPlugin(opts, mpContext, mpPluginDesc);
}

InputFileOptions *VDInputFilePlugin::createOptions(const void *buf, uint32 len) {
	vdrefptr<IVDXInputOptions> opts;

	vdwithinputplugin(mpContext) {
		mpXObject->CreateOptions(buf, len, ~opts);
	}

	if (!opts)
		return NULL;

	return new VDInputFileOptionsPlugin(opts, mpContext, mpPluginDesc);
}

void VDInputFilePlugin::InfoDialog(VDGUIHandle hwndParent) {
	vdwithinputplugin(mpContext) {
		mpXObject->DisplayInfo((VDXHWND)hwndParent);
	}
}

void VDInputFilePlugin::GetTextInfo(tFileTextInfo& info) {
	info.clear();
}

bool VDInputFilePlugin::isOptimizedForRealtime() {
	return false;
}

bool VDInputFilePlugin::isStreaming() {
	return false;
}

bool VDInputFilePlugin::GetVideoSource(int index, IVDVideoSource **ppSrc) {
	vdrefptr<IVDXVideoSource> pVS;
	vdwithinputplugin(mpContext) {
		mpXObject->GetVideoSource(index, ~pVS);
	}
	if (!pVS)
		return false;

	IVDVideoSource *vs = new VDVideoSourcePlugin(pVS, mpContext, this);
	vs->AddRef();
	*ppSrc = vs;
	return true;
}

bool VDInputFilePlugin::GetAudioSource(int index, AudioSource **ppSrc) {
	vdrefptr<IVDXAudioSource> pAS;
	vdwithinputplugin(mpContext) {
		mpXObject->GetAudioSource(index, ~pAS);
	}
	if (!pAS)
		return false;

	AudioSource *as = new VDAudioSourcePlugin(pAS, mpContext, this);
	as->AddRef();
	*ppSrc = as;
	return true;
}

///////////////////////////////////////////////////////////////////////////////

class VDInputDriverPlugin : public vdrefcounted<IVDInputDriver> {
public:
	VDInputDriverPlugin(VDPluginDescription *pDesc);
	~VDInputDriverPlugin();

	int				GetDefaultPriority();
	const wchar_t *	GetSignatureName();
	uint32			GetFlags();
	const wchar_t *	GetFilenamePattern();
	bool			DetectByFilename(const wchar_t *pszFilename);
	DetectionConfidence DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		VDXMediaInfo info;
		return DetectBySignature2(info, pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize);
	}
	DetectionConfidence DetectBySignature2(VDXMediaInfo& info, const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize);
	InputFile *		CreateInputFile(uint32 flags);

protected:
	void			LoadPlugin();
	void			UnloadPlugin();

	vdrefptr<IVDXInputFileDriver> mpXObject;
	VDPluginDescription	*mpPluginDesc;
	const VDPluginInfo				*mpPluginInfo;
	const VDXInputDriverDefinition	*mpDef;
	const VDXInputDriverDefinition	*mpShadowedDef;
	vdrefptr<VDInputDriverContextImpl>	mpContext;

	VDStringW	mFilenamePattern;
};

VDInputDriverPlugin::VDInputDriverPlugin(VDPluginDescription *pDesc)
	: mpPluginDesc(pDesc)
	, mpPluginInfo(NULL)
	, mpDef(NULL)
	, mpShadowedDef(static_cast<const VDXInputDriverDefinition *>(mpPluginDesc->mpShadowedInfo->mpTypeSpecificInfo))
	, mpContext(new VDInputDriverContextImpl(mpPluginDesc))
{
	mpContext->mAPIVersion = kVDXPlugin_InputDriverAPIVersion;

	if (mpShadowedDef->mpFilenamePattern) {
		mFilenamePattern = mpShadowedDef->mpFilenamePattern;

		VDStringW::size_type pos = 0;

		// convert embedded pipes into nulls
		while(VDStringW::npos != (pos = mFilenamePattern.find('|', pos)))
			mFilenamePattern[pos++] = 0;

		// Add a null on the end. Actually, add two, just in case. We need ONE in case someone
		// forgot the filename pattern, a SECOND to end the filter, and a THIRD to end the list.
		// We get one from c_str().
		mFilenamePattern += (wchar_t)0;
		mFilenamePattern += (wchar_t)0;
	}
}

VDInputDriverPlugin::~VDInputDriverPlugin() {
	UnloadPlugin();
}

int VDInputDriverPlugin::GetDefaultPriority() {
	return mpShadowedDef->mPriority;
}

const wchar_t *VDInputDriverPlugin::GetSignatureName() {
	return mpShadowedDef->mpDriverTagName;
}

uint32 VDInputDriverPlugin::GetFlags() {
	uint32 xflags = mpShadowedDef->mFlags;
	uint32 flags = 0;

	if (xflags & VDXInputDriverDefinition::kFlagSupportsVideo)
		flags |= kF_Video;

	if (xflags & VDXInputDriverDefinition::kFlagSupportsAudio)
		flags |= kF_Audio;

	// Ugh, we can't detect this in the current API.
	if (!(xflags & VDXInputDriverDefinition::kFlagNoOptions))
		flags |= kF_SupportsOpts;

	if (xflags & VDXInputDriverDefinition::kFlagForceByName)
		flags |= kF_ForceByName;

	if (xflags & VDXInputDriverDefinition::kFlagDuplicate)
		flags |= kF_Duplicate;

	return flags;
}

const wchar_t *VDInputDriverPlugin::GetFilenamePattern() {
	return mFilenamePattern.c_str();
}

bool VDInputDriverPlugin::DetectByFilename(const wchar_t *pszFilename) {
	const wchar_t *sig = mpShadowedDef->mpFilenameDetectPattern;

	if (!sig)
		return false;

	pszFilename = VDFileSplitPath(pszFilename);

	while(const wchar_t *t = wcschr(sig, L'|')) {
		VDStringW temp(sig, t);

		if (VDFileWildMatch(temp.c_str(), pszFilename))
			return true;

		sig = t+1;
	}

	return VDFileWildMatch(sig, pszFilename);
}

VDInputDriverPlugin::DetectionConfidence VDInputDriverPlugin::DetectBySignature2(VDXMediaInfo& info, const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
	const uint8 *sig = (const uint8 *)mpShadowedDef->mpSignature;

	if (sig) {
		if (nHeaderSize < (mpShadowedDef->mSignatureLength >> 1))
			return kDC_None;

		const uint8 *data = (const uint8 *)pHeader;
		for(uint32 i=0; i<mpShadowedDef->mSignatureLength; i+=2) {
			uint8 byte = sig[0];
			uint8 mask = sig[1];

			if ((*data ^ byte) & mask)
				return kDC_None;

			sig += 2;
			++data;
		}
	}

	if (!(mpShadowedDef->mFlags & VDXInputDriverDefinition::kFlagCustomSignature))
		return kDC_Low;

	LoadPlugin();

	DetectionConfidence retval = kDC_None;
	if (mpXObject) {
		vdwithinputplugin(mpContext) {
			if (mpContext->max_api_version>=8)
				retval = (DetectionConfidence)mpXObject->DetectBySignature2(info, pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize);
			else {
				int r = mpXObject->DetectBySignature(pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize);
				retval = r < 0 ? kDC_None : r > 0 ? kDC_High : kDC_Moderate;
			}
		}
	}

	UnloadPlugin();

	return retval;
}

InputFile *VDInputDriverPlugin::CreateInputFile(uint32 flags) {
	vdrefptr<IVDXInputFile> ifile;

	LoadPlugin();

	if (!mpXObject) {
		UnloadPlugin();
		throw MyMemoryError();
	}

	vdwithinputplugin(mpContext) {
		mpXObject->CreateInputFile(flags, ~ifile);
	}

	InputFile *p = NULL;

	if (ifile)
		p = new_nothrow VDInputFilePlugin(ifile, mpPluginDesc, mpContext);

	UnloadPlugin();

	if (!p)
		throw MyMemoryError();

	return p;
}

void VDInputDriverPlugin::LoadPlugin() {
	if (!mpPluginInfo) {
		mpPluginInfo = VDLockPlugin(mpPluginDesc);
		mpDef = static_cast<const VDXInputDriverDefinition *>(mpPluginInfo->mpTypeSpecificInfo);
		vdwithinputplugin(mpContext) {
			mpDef->mpCreate(mpContext, ~mpXObject);
		}
	}
}

void VDInputDriverPlugin::UnloadPlugin() {
	mpXObject.clear();

	if (mpPluginInfo) {
		VDUnlockPlugin(mpPluginDesc);
		mpDef = NULL;
		mpPluginInfo = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

extern IVDInputDriver *VDCreateInputDriverPlugin(VDPluginDescription *desc) {
	return new VDInputDriverPlugin(desc);
}

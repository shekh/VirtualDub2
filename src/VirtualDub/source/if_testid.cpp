#include "stdafx.h"
#include <vd2/system/binary.h>
#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/Unknown.h>

///////////////////////////////////////////////////////////////////////////////

class VDVideoSourceTestId : public vdxunknown2<IVDXVideoSource, IVDXStreamSource> {
public:
	VDVideoSourceTestId();

	void		VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo&);

	bool		VDXAPIENTRY Read(sint64 lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);

	const void *VDXAPIENTRY GetDirectFormat();
	int			VDXAPIENTRY GetDirectFormatLen();

	ErrorMode	VDXAPIENTRY GetDecodeErrorMode();
	void		VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode);
	bool		VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode);

	bool		VDXAPIENTRY IsVBR();
	sint64		VDXAPIENTRY TimeToPositionVBR(sint64 us);
	sint64		VDXAPIENTRY PositionToTimeVBR(sint64 samples);

public:
	void	VDXAPIENTRY GetVideoSourceInfo(VDXVideoSourceInfo& info);

	bool	VDXAPIENTRY CreateVideoDecoderModel(IVDXVideoDecoderModel **);
	bool	VDXAPIENTRY CreateVideoDecoder(IVDXVideoDecoder **);

	void	VDXAPIENTRY GetSampleInfo(sint64 sample_num, VDXVideoFrameInfo& frameInfo);

	bool	VDXAPIENTRY IsKey(sint64 sample_num);

	sint64	VDXAPIENTRY GetFrameNumberForSample(sint64 sample_num);
	sint64	VDXAPIENTRY GetSampleNumberForFrame(sint64 frame_num);
	sint64	VDXAPIENTRY GetRealFrame(sint64 frame_num);

	sint64	VDXAPIENTRY GetSampleBytePosition(sint64 sample_num);

protected:
	VDXBITMAPINFOHEADER	mDirectFormat;
};

VDVideoSourceTestId::VDVideoSourceTestId() {
	mDirectFormat.mSize			 = sizeof(VDXBITMAPINFOHEADER);
	mDirectFormat.mWidth		 = 8;
	mDirectFormat.mHeight		 = 4;
	mDirectFormat.mPlanes		 = 1;
	mDirectFormat.mBitCount		 = 16;
	mDirectFormat.mCompression	 = VDMAKEFOURCC('M', 'S', 'V', 'C');
	mDirectFormat.mSizeImage	 = 64;
	mDirectFormat.mXPelsPerMeter = 0;
	mDirectFormat.mYPelsPerMeter = 0;
	mDirectFormat.mClrUsed		 = 0;
	mDirectFormat.mClrImportant	 = 0;
}

void VDXAPIENTRY VDVideoSourceTestId::GetStreamSourceInfo(VDXStreamSourceInfo& info) {
	info.mSampleRate.mNumerator = 10;
	info.mSampleRate.mDenominator = 1;
	info.mSampleCount = 100;
	info.mPixelAspectRatio.mNumerator = 1;
	info.mPixelAspectRatio.mDenominator = 0;
}

bool VDXAPIENTRY VDVideoSourceTestId::Read(sint64 lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	*lBytesRead = 4;
	*lSamplesRead = 1;

	if (!lpBuffer)
		return true;

	if (cbBuffer < 4)
		return false;

	uint16 *dst = (uint16 *)lpBuffer;
	uint32 i = (uint32)lStart;
	uint32 x = ((i & 15) * 17) >> 3;
	uint32 y = ((i >> 4) * 17) >> 3;
	uint16 c = (uint16)(0x8800 + (y << 5) + x);
	dst[0] = c;
	dst[1] = IsKey(lStart) ? c : 0x8400;

	return true;
}

const void *VDXAPIENTRY VDVideoSourceTestId::GetDirectFormat() {
	return &mDirectFormat;
}

int VDXAPIENTRY VDVideoSourceTestId::GetDirectFormatLen() {
	return sizeof(mDirectFormat);
}

IVDXStreamSource::ErrorMode VDXAPIENTRY VDVideoSourceTestId::GetDecodeErrorMode() {
	return kErrorModeReportAll;
}

void VDXAPIENTRY VDVideoSourceTestId::SetDecodeErrorMode(ErrorMode mode) {
}

bool VDXAPIENTRY VDVideoSourceTestId::IsDecodeErrorModeSupported(ErrorMode mode) {
	return mode == kErrorModeReportAll;
}

bool VDXAPIENTRY VDVideoSourceTestId::IsVBR() {
	return false;
}

sint64 VDXAPIENTRY VDVideoSourceTestId::TimeToPositionVBR(sint64 us) {
	return -1;
}

sint64 VDXAPIENTRY VDVideoSourceTestId::PositionToTimeVBR(sint64 samples) {
	return -1;
}

void VDXAPIENTRY VDVideoSourceTestId::GetVideoSourceInfo(VDXVideoSourceInfo& info) {
	info.mFlags			= 0;
	info.mWidth			= 8;
	info.mHeight		= 4;
	info.mDecoderModel	= VDXVideoSourceInfo::kDecoderModelDefaultIP;
}

bool VDXAPIENTRY VDVideoSourceTestId::CreateVideoDecoderModel(IVDXVideoDecoderModel **) {
	return false;
}

bool VDXAPIENTRY VDVideoSourceTestId::CreateVideoDecoder(IVDXVideoDecoder **) {
	return false;
}

void VDXAPIENTRY VDVideoSourceTestId::GetSampleInfo(sint64 sample_num, VDXVideoFrameInfo& frameInfo) {
	frameInfo.mBytePosition = -1;
	if (IsKey(sample_num)) {
		frameInfo.mFrameType = kVDXVFT_Independent;
		frameInfo.mTypeChar = 'K';
	} else {
		frameInfo.mFrameType = kVDXVFT_Predicted;
		frameInfo.mTypeChar = ' ';
	}
}

bool VDXAPIENTRY VDVideoSourceTestId::IsKey(sint64 sample_num) {
	return (int)sample_num % 5 == 0;
}

sint64 VDXAPIENTRY VDVideoSourceTestId::GetFrameNumberForSample(sint64 sample_num) {
	return sample_num;
}

sint64 VDXAPIENTRY VDVideoSourceTestId::GetSampleNumberForFrame(sint64 frame_num) {
	return frame_num;
}

sint64 VDXAPIENTRY VDVideoSourceTestId::GetRealFrame(sint64 frame_num) {
	return frame_num;
}

sint64 VDXAPIENTRY VDVideoSourceTestId::GetSampleBytePosition(sint64 sample_num) {
	return -1;
}

///////////////////////////////////////////////////////////////////////////////

class VDInputFileTestId : public vdxunknown<IVDXInputFile> {
public:
	bool VDXAPIENTRY PromptForOptions(VDXHWND, IVDXInputOptions **ppOptions);
	bool VDXAPIENTRY CreateOptions(const void *buf, uint32 len, IVDXInputOptions **ppOptions);

	void VDXAPIENTRY Init(const wchar_t *path, IVDXInputOptions *options);
	bool VDXAPIENTRY Append(const wchar_t *path);

	void VDXAPIENTRY DisplayInfo(VDXHWND hwndParent);

	bool VDXAPIENTRY GetVideoSource(int index, IVDXVideoSource **ppVS);
	bool VDXAPIENTRY GetAudioSource(int index, IVDXAudioSource **ppAS);
};

bool VDXAPIENTRY VDInputFileTestId::PromptForOptions(VDXHWND, IVDXInputOptions **ppOptions) {
	return false;
}

bool VDXAPIENTRY VDInputFileTestId::CreateOptions(const void *buf, uint32 len, IVDXInputOptions **ppOptions) {
	return false;
}

void VDXAPIENTRY VDInputFileTestId::Init(const wchar_t *path, IVDXInputOptions *options) {
}

bool VDXAPIENTRY VDInputFileTestId::Append(const wchar_t *path) {
	return false;
}

void VDXAPIENTRY VDInputFileTestId::DisplayInfo(VDXHWND hwndParent) {
}

bool VDXAPIENTRY VDInputFileTestId::GetVideoSource(int index, IVDXVideoSource **ppVS) {
	if (index)
		return false;

	*ppVS = new(std::nothrow) VDVideoSourceTestId;
	if (!*ppVS)
		return false;

	(*ppVS)->AddRef();
	return true;
}

bool VDXAPIENTRY VDInputFileTestId::GetAudioSource(int index, IVDXAudioSource **ppAS) {
	return false;
}

///////////////////////////////////////////////////////////////////////////////

class VDInputDriverTestId : public vdxunknown<IVDXInputFileDriver> {
public:
	int VDXAPIENTRY DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize);
	bool VDXAPIENTRY CreateInputFile(uint32 flags, IVDXInputFile **ppFile);
};

int VDXAPIENTRY VDInputDriverTestId::DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
	return -1;
}

bool VDXAPIENTRY VDInputDriverTestId::CreateInputFile(uint32 flags, IVDXInputFile **ppFile) {
	*ppFile = new(std::nothrow) VDInputFileTestId;
	if (!*ppFile)
		return false;

	(*ppFile)->AddRef();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool VDXAPIENTRY VDCreateInputDriverTestId(const VDXInputDriverContext *pContext, IVDXInputFileDriver **pp) {
	*pp = new(std::nothrow) VDInputDriverTestId;
	if (!*pp)
		return false;

	(*pp)->AddRef();
	return true;
}

extern const VDXInputDriverDefinition g_inputDrv_inputdef_TestId = {
	sizeof(VDXInputDriverDefinition),
	VDXInputDriverDefinition::kFlagSupportsVideo | VDXInputDriverDefinition::kFlagCustomSignature,
	-1000,
	0, NULL,
	NULL,
	NULL,
	L"TestId",
	VDCreateInputDriverTestId
};

extern const VDXPluginInfo g_inputDrv_plugindef_TestId = {
	sizeof(VDXPluginInfo),
	L"__TestId",
	NULL,
	L"Test Id loader",
	0x01000000,
	kVDXPluginType_Input,
	0,
	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDXPlugin_InputDriverAPIVersion,
	kVDXPlugin_InputDriverAPIVersion,
	&g_inputDrv_inputdef_TestId
};

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

#include <process.h>

#include <windows.h>
#include <vfw.h>

#include "InputFile.h"
#include "InputFileMP3.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "VideoSourceAVI.h"
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/Dita/resources.h>
#include <vd2/Dita/services.h>
#include <vd2/Riza/audioformat.h>
#include <vd2/VDLib/Dialog.h>
#include "AVIStripeSystem.h"
#include "AVIReadHandler.h"

#include "gui.h"
#include "oshelper.h"
#include "prefs.h"
#include "misc.h"

#include "resource.h"

extern uint32& VDPreferencesGetRenderWaveBufferSize();

//////////////////////////////////////////////////////////////////////////////

VDAudioSourceMP3::VDAudioSourceMP3(VDInputFileMP3 *parent)
	: mpParent(parent)
	, mbVBRMode(parent->IsVBRMode())
{
	mSampleFirst	= 0;

	if (mbVBRMode)
		mSampleLast = parent->GetFrameCount();
	else
		mSampleLast = parent->GetDataLength();

	const vdstructex<VDWaveFormat>& format = parent->GetFormat();

	memcpy(allocFormat(format.size()), format.data(), format.size());

	memset(&streamInfo, 0, sizeof streamInfo);
	streamInfo.fccType					= streamtypeAUDIO;
	streamInfo.fccHandler				= 0;
	streamInfo.dwFlags					= 0;
	streamInfo.wPriority				= 0;
	streamInfo.wLanguage				= 0;
	streamInfo.dwInitialFrames			= 0;

	streamInfo.dwScale					= format->mBlockSize;
	if (mbVBRMode) {
		// We want:		dwRate/dwScale = frames/second
		// Given:		dwScale = BlockSize
		//				frames/second = SamplingRate / SamplesPerFrame
		// Therefore:	dwRate = SamplingRate * BlockSize / SamplesPerFrame
		//
		// mBlockSize is always a multiple of 1152, so the division is exact.
		streamInfo.dwRate					= format->mSamplingRate * format->mBlockSize / parent->GetSamplesPerFrame();
	} else
		streamInfo.dwRate					= format->mDataRate;

	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= VDClampToUint32(mSampleLast);
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= 0xffffffff;

	// COMPAT: VideoLAN Player 0.8.6a won't handle VBR audio streams unless
	// dwSampleSize is zero.
	streamInfo.dwSampleSize				= mbVBRMode ? 0 : format->mBlockSize;
}

VDAudioSourceMP3::~VDAudioSourceMP3() {
}

int VDAudioSourceMP3::_read(VDPosition start, uint32 count, void *buffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	if (mbVBRMode) {
		if (count <= 0) {
			if (lSamplesRead)
				*lSamplesRead = 0;
			if (lBytesRead)
				*lBytesRead = 0;
			return IVDStreamSource::kOK;
		}

		const VDInputFileMP3::FrameInfo& frameInfo = mpParent->GetFrameInfo((uint32)start);
		uint32 bytes = frameInfo.mSize;

		if (lSamplesRead)
			*lSamplesRead = 1;
		if (lBytesRead)
			*lBytesRead = bytes;

		if (buffer) {
			if (cbBuffer < frameInfo.mSize)
				return IVDStreamSource::kBufferTooSmall;

			mpParent->ReadSpan(frameInfo.mPos, buffer, bytes);
		}

		if (lBytesRead)
			*lBytesRead = bytes;

		return IVDStreamSource::kOK;
	} else {
		uint32 bytes = count;

		if (bytes > cbBuffer) {
			bytes = cbBuffer;
			count = bytes;
		}
		
		if (buffer)
			mpParent->ReadSpan(start, buffer, bytes);

		if (lSamplesRead)
			*lSamplesRead = count;
		if (lBytesRead)
			*lBytesRead = bytes;

		return IVDStreamSource::kOK;
	}
}

/////////////////////////////////////////////////////////////////////

struct VDInputFileOptionsSerializedMP3 {
	uint32	mLength;
	uint32	mSignature;
	uint8	mMode;
	uint8	mPad[3];

	static const uint32 kSignature = VDMAKEFOURCC('M', 'P', '3', 'O');
};

class VDInputFileOptionsMP3 : public InputFileOptions {
public:
	VDInputFileOptionsMP3();
	~VDInputFileOptionsMP3();

	bool read(const void *buf, int buflen);
	int write(char *buf, int buflen) const;

public:
	VDInputFileMP3::BitRateMode mMode;
};

VDInputFileOptionsMP3::VDInputFileOptionsMP3()
	: mMode(VDInputFileMP3::kBRM_Autodetect)
{
}

VDInputFileOptionsMP3::~VDInputFileOptionsMP3() {
}

bool VDInputFileOptionsMP3::read(const void *buf, int buflen) {
	if (buflen < sizeof(VDInputFileOptionsSerializedMP3))
		return false;

	VDInputFileOptionsSerializedMP3 ser = {0};
	memcpy(&ser, buf, buflen);

	if (ser.mLength > buflen)
		return false;

	if (ser.mSignature != VDInputFileOptionsSerializedMP3::kSignature)
		return false;

	switch(ser.mMode) {
		case VDInputFileMP3::kBRM_Autodetect:
		case VDInputFileMP3::kBRM_CBR:
		case VDInputFileMP3::kBRM_VBR:
			mMode = (VDInputFileMP3::BitRateMode)ser.mMode;
			break;

		default:
			return false;
	}

	return true;
}

int VDInputFileOptionsMP3::write(char *buf, int buflen) const {
	if (buf) {
		if (buflen < sizeof(VDInputFileOptionsSerializedMP3))
			return 0;

		VDInputFileOptionsSerializedMP3 ser = {0};
		ser.mLength		= sizeof(VDInputFileOptionsSerializedMP3);
		ser.mSignature	= VDInputFileOptionsSerializedMP3::kSignature;
		ser.mMode		= (uint8)mMode;

		memcpy(buf, &ser, sizeof(VDInputFileOptionsSerializedMP3));
	}

	return sizeof(VDInputFileOptionsSerializedMP3);
}

/////////////////////////////////////////////////////////////////////

VDInputFileMP3::VDInputFileMP3()
	: mBufferedFile(&mFile, VDPreferencesGetRenderWaveBufferSize())
	, mBitRateMode(kBRM_Autodetect)
{
}

VDInputFileMP3::~VDInputFileMP3() {
}

void VDInputFileMP3::Init(const wchar_t *szFile) {
	mFile.open(szFile);
	mDataStart = 0;

	// Look for ID3v2 tag at beginning of the file.
	uint8 id3header[10];
	if (10 == mBufferedFile.ReadData(id3header, 10)) {
		if (id3header[0] == 'I' &&	// tag
			id3header[1] == 'D' &&	// tag
			id3header[2] == '3' &&	// tag
			id3header[3] < 0xFF &&	// version
			id3header[4] < 0xFF &&	// version
			id3header[6] < 0x80 &&	// length MSB
			id3header[7] < 0x80 &&	// length
			id3header[8] < 0x80 &&	// length
			id3header[9] < 0x80)	// length
		{
			uint32 id3length = ((uint32)id3header[6] << 21) + ((uint32)id3header[7] << 14) + ((uint32)id3header[8] << 7) + id3header[9];

			// check for footer
			if (id3header[5] & 0x10) 
				id3length += 10;

			mDataStart = 10 + id3length;
		}
	}

	mBufferedFile.Seek(mDataStart);

	uint32 currentTrackedHeader = 0;
	uint32 currentConfidence = 0;
	uint32 bestConfidence = 0;

	uint32 header = 0;
	uint32 channels = 0;
	uint32 samplingRate = 0;
	uint32 totalSamplesDiv192 = 0;
	uint32 maxFrameSize = 0;
	uint32 pads = 0;

	uint32 highestBitrate		= 0;

	uint32 framesWithLayer1		= 0;
	uint32 framesWithLayer2		= 0;
	uint32 framesWithLayer3		= 0;

	uint32 framesWithPrivate	= 0;
	uint32 framesWithProtection	= 0;
	uint32 framesWithCopyright	= 0;
	uint32 framesWithOriginal	= 0;
	uint32 framesWithMPEG1		= 0;

	uint32 framesWithMode[4] = {0};
	uint32 framesWithModeExt[4] = {0};
	uint32 framesWithEmphasis[4] = {0};

	mDataLength = 0;
	mSamplesPerFrame = 0;
	for(;;) {
		uint8 c;
		if (0 == mBufferedFile.ReadData(&c, 1))
			break;

		header = (header << 8) + c;

		// check for valid header
		if ((header & 0xFFE00000) != 0xFFE00000)
			continue;

		// MPEG-1.5 isn't valid
		if ((header & 0x00180000) == 0x00080000)
			continue;

		// check for valid bitrate
		if (!(header & 0x0000F000))
			continue;

		// we don't allow free-form
		if ((header & 0x0000F000) == 0x0000F000)
			continue;

		// check for valid sampling rate
		if ((header & 0x00000C00) == 0x00000C00)
			continue;

		// check for valid layer
		if ((header & 0x00060000) == 0)
			continue;

		// syncword			12 bits		FFF00000
		// id				1 bit		00080000
		// layer			2 bits		00060000
		// protection		1 bit		00010000
		// bitrate			4 bits		0000F000
		// sampling rate	2 bits		00000C00
		// padding			1 bit		00000200
		// private bit		1 bit		00000100
		// mode				2 bits		000000C0
		// mode extension	2 bits		00000030
		// copyright		1 bit		00000008
		// original/copy	1 bit		00000004
		// emphasis			2 bits		00000003

		// compute frame size
		static const int sBitrateTable[2][3][16]={
			{
				{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 },	// MPEG-1 layer I
				{ 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 },	// MPEG-1 layer II
				{ 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 },	// MPEG-1 layer III
			},
			{
				{ 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 },	// MPEG-2 layer I
				{ 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 },	// MPEG-2 layer II
				{ 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 },	// MPEG-2 layer III
			}
		};

		static const int sFrequencyTable[2][4]={{44100,48000,32000,0}, {22050,24000,16000,0}};

		bool is_mpeg2	= (header & 0x00080000) == 0;
		bool is_mpeg25	= (header & 0x00100000) == 0;
		int layer		= 4 - ((header>>17)&3);
		int bitrate_idx	= (header>>12)&15;
		int freq_idx	= (header>>10)&3;
		int padding		= (header>>9)&1;
		int bitrate		= sBitrateTable[is_mpeg2][layer-1][bitrate_idx];
		int freq		= sFrequencyTable[is_mpeg2][freq_idx];

		if (is_mpeg25)
			freq >>= 1;

		sint32 frameDataSize;
		uint32 frameSamplesDiv192 = 0;
		if (layer == 1) {
			frameDataSize = 4*(12000*bitrate/freq + padding);
			frameSamplesDiv192 = 2;		// 384 samples
		} else if (is_mpeg2 && layer == 3) {
			frameDataSize = (72000*bitrate/freq + padding);
			frameSamplesDiv192 = 3;		// 576 samples
		} else {
			frameDataSize = (144000*bitrate/freq + padding);
			frameSamplesDiv192 = 6;		// 1152 samples
		}

		if (currentTrackedHeader && !((currentTrackedHeader ^ header) & 0x00000CC0)) {
			++currentConfidence;
		} else {
			currentTrackedHeader = header;
			currentConfidence = 1;
		}

		static const int kLayerFrameSizes[2][3]={{384,1152,1152},{384,1152,576}};
		int samples = kLayerFrameSizes[is_mpeg2][layer - 1];

		if (currentConfidence > bestConfidence) {
			bestConfidence = currentConfidence;

			channels = (header & 0xc0) == 0xc0 ? 1 : 2;
			samplingRate = freq;
			mSamplesPerFrame = samples;
		}

		++framesWithMode[(header >> 6) & 3];
		++framesWithModeExt[(header >> 4) & 3];

		if (header & 0x00010000)
			++framesWithProtection;

		if (header & 0x00000100)
			++framesWithPrivate;

		if (header & 0x00000004)
			++framesWithOriginal;

		if (header & 0x00000008)
			++framesWithCopyright;

		if (!is_mpeg2)
			++framesWithMPEG1;

		switch(layer) {
		case 1:	++framesWithLayer1; break;
		case 2:	++framesWithLayer2; break;
		case 3:	++framesWithLayer3; break;
		}

		if (bitrate > highestBitrate)
			highestBitrate = bitrate;

		FrameInfo fi;
		fi.mPos		= mBufferedFile.Pos() - 4 - mDataStart;
		fi.mSize	= frameDataSize;
		fi.mSamples	= samples;

		// skip data payload
		frameDataSize -= 4;
		if (frameDataSize != mBufferedFile.ReadData(NULL, frameDataSize))
			break;

		mDataLength += frameDataSize + 4;
		totalSamplesDiv192 += frameSamplesDiv192;

		mFrames.push_back(fi);

		pads += padding;

		if (maxFrameSize < fi.mSize)
			maxFrameSize = fi.mSize;
	}

	if (mFrames.empty())
		throw MyError("No valid MPEG audio data was detected in file \"%ls.\"", szFile);

	// decide if we want to use CBR or VBR mode
	double seconds = (double)totalSamplesDiv192 * 192.0 / (double)samplingRate;
	double averageDataRate = (double)mDataLength / (double)seconds;
	if (mBitRateMode == kBRM_Autodetect) {

		Frames::const_iterator it(mFrames.begin()), itEnd(mFrames.end());
		sint64 localBytes = 0;
		sint64 localSamples = 0;
		double maxLocalError = 0;
		for(; it != itEnd; ++it) {
			const FrameInfo& fi = *it;

			localBytes += fi.mSize;
			localSamples += fi.mSamples;

			double localSeconds = (double)localSamples / (double)samplingRate;
			double localError = fabs(localSeconds - (double)localBytes / averageDataRate);
			if (maxLocalError < localError)
				maxLocalError = localError;
		}

		// Use VBR if local deviation exceeds 50ms.
		mbVBRMode = (maxLocalError > 0.050);
	} else {
		mbVBRMode = (mBitRateMode == kBRM_VBR);
	}

	// The critical factor for the VBR hack is that the block size has to be at least as large as
	// any frame in the audio stream. As such, using 1152 in all cases is incorrect, because some
	// combinations of sampling rate and bit rate can result in frames exceeding 1152 bytes.
	//
	// The reason that 1152 is convenient is that frames in MPEG always result in either 384, 576,
	// or 1152 samples, and therefore using a multiple of 1152 always results in an exact
	// dwRate / dwScale fraction for the stream.

	uint32 vbrBlockAlign = maxFrameSize + 1151;
	vbrBlockAlign -= vbrBlockAlign % 1152;

	int frameCount = (int)mFrames.size();
	if (framesWithLayer3 * 10 > frameCount * 9) {
		mWaveFormat.resize(sizeof(mpeglayer3waveformat_tag));
		mpeglayer3waveformat_tag& wf = *(mpeglayer3waveformat_tag *)mWaveFormat.data();
		wf.wfx.wFormatTag		= WAVE_FORMAT_MPEGLAYER3;
		wf.wfx.nChannels		= (WORD)channels;
		wf.wfx.nSamplesPerSec	= samplingRate;
		wf.wfx.nAvgBytesPerSec	= VDRoundToInt(averageDataRate);

		if (mbVBRMode)
			wf.wfx.nBlockAlign		= (WORD)vbrBlockAlign;
		else
			wf.wfx.nBlockAlign		= 1;

		double padFraction = 0;
		if (!mFrames.empty())
			padFraction = pads / (double)frameCount;

		wf.wfx.wBitsPerSample	= 0;
		wf.wfx.cbSize			= MPEGLAYER3_WFX_EXTRA_BYTES;
		wf.wID					= MPEGLAYER3_ID_MPEG;
		wf.fdwFlags				= padFraction < 0.01 ? MPEGLAYER3_FLAG_PADDING_OFF : padFraction > 0.99 ? MPEGLAYER3_FLAG_PADDING_ON : MPEGLAYER3_FLAG_PADDING_ISO;
		wf.nBlockSize			= (WORD)maxFrameSize;
		wf.nFramesPerBlock		= 1;
		wf.nCodecDelay			= 0;
	} else {
		mWaveFormat.resize(sizeof(mpeg1waveformat_tag));
		mpeg1waveformat_tag& wf = *(mpeg1waveformat_tag *)mWaveFormat.data();

		wf.wfx.wFormatTag		= WAVE_FORMAT_MPEG;
		wf.wfx.nChannels		= (WORD)channels;
		wf.wfx.nSamplesPerSec	= samplingRate;
		wf.wfx.nAvgBytesPerSec	= VDRoundToInt(averageDataRate);

		if (mbVBRMode)
			wf.wfx.nBlockAlign		= (WORD)vbrBlockAlign;
		else
			wf.wfx.nBlockAlign		= 1;

		wf.wfx.wBitsPerSample	= 0;
		wf.wfx.cbSize			= 0x16;

		if (framesWithLayer1 > framesWithLayer2) {		// 1 > 2
			if (framesWithLayer1 > framesWithLayer3)	// 1 > 2,3
				wf.fwHeadLayer = ACM_MPEG_LAYER1;
			else										// 3 > 1 > 2
				wf.fwHeadLayer = ACM_MPEG_LAYER3;
		} else {
			if (framesWithLayer2 > framesWithLayer3)	// 2 > 1,3
				wf.fwHeadLayer = ACM_MPEG_LAYER2;
			else										// 3 > 2 > 1
				wf.fwHeadLayer = ACM_MPEG_LAYER3;
		}

		wf.dwHeadBitrate	= highestBitrate * 1000;
		wf.fwHeadMode		= (WORD)(std::max_element(framesWithMode, framesWithMode + 4) - framesWithMode);
		wf.fwHeadModeExt	= (WORD)(std::max_element(framesWithModeExt, framesWithModeExt + 4) - framesWithModeExt);
		wf.wHeadEmphasis	= (WORD)(std::max_element(framesWithEmphasis, framesWithEmphasis + 4) - framesWithEmphasis);

		wf.fwHeadFlags		= 0;
		if (framesWithPrivate * 2 >= frameCount)
			wf.fwHeadFlags |= ACM_MPEG_PRIVATEBIT;

		if (framesWithProtection * 2 >= frameCount)
			wf.fwHeadFlags |= ACM_MPEG_PROTECTIONBIT;

		if (framesWithCopyright * 2 >= frameCount)
			wf.fwHeadFlags |= ACM_MPEG_COPYRIGHT;

		if (framesWithOriginal * 2 >= frameCount)
			wf.fwHeadFlags |= ACM_MPEG_ORIGINALHOME;

		if (framesWithMPEG1 * 2 >= frameCount)
			wf.fwHeadFlags |= ACM_MPEG_ID_MPEG1;

		wf.dwPTSLow			= 0;
		wf.dwPTSHigh		= 0;
	}
}

void VDInputFileMP3::setOptions(InputFileOptions *opts0) {
	VDInputFileOptionsMP3 *opts = static_cast<VDInputFileOptionsMP3 *>(opts0);

	mBitRateMode = opts->mMode;
}

class VDInputFileOptionsDialogMP3 : public VDDialogFrameW32 {
public:
	VDInputFileOptionsDialogMP3(VDInputFileOptionsMP3 *opts) : VDDialogFrameW32(IDD_EXTOPENOPTS_MP3), mOpts(*opts) {}

	bool OnLoaded();
	void OnDataExchange(bool write);

protected:
	VDInputFileOptionsMP3& mOpts;
};

bool VDInputFileOptionsDialogMP3::OnLoaded() {
	VDDialogFrameW32::OnLoaded();
	SetFocusToControl(IDC_BITRATE_AUTO);
	return true;
}

void VDInputFileOptionsDialogMP3::OnDataExchange(bool write) {
	if (write) {
		if (IsButtonChecked(IDC_BITRATE_CBR))
			mOpts.mMode = VDInputFileMP3::kBRM_CBR;
		else if (IsButtonChecked(IDC_BITRATE_VBR))
			mOpts.mMode = VDInputFileMP3::kBRM_VBR;
		else if (IsButtonChecked(IDC_BITRATE_AUTO))
			mOpts.mMode = VDInputFileMP3::kBRM_Autodetect;
	} else {
		CheckButton(IDC_BITRATE_CBR, mOpts.mMode == VDInputFileMP3::kBRM_CBR);
		CheckButton(IDC_BITRATE_VBR, mOpts.mMode == VDInputFileMP3::kBRM_VBR);
		CheckButton(IDC_BITRATE_AUTO, mOpts.mMode == VDInputFileMP3::kBRM_Autodetect);
	}
}

InputFileOptions *VDInputFileMP3::promptForOptions(VDGUIHandle hwndParent) {
	vdautoptr<VDInputFileOptionsMP3> opts(new_nothrow VDInputFileOptionsMP3);
	if (!opts)
		return NULL;

	VDInputFileOptionsDialogMP3 dlg(opts);

	if (dlg.ShowDialog(hwndParent))
		return opts.release();

	return NULL;
}

InputFileOptions *VDInputFileMP3::createOptions(const void *buf, uint32 len) {
	vdautoptr<VDInputFileOptionsMP3> opts(new_nothrow VDInputFileOptionsMP3);
	if (!opts)
		return NULL;

	if (!opts->read(buf, len))
		return NULL;

	return opts.release();
}

bool VDInputFileMP3::GetVideoSource(int index, IVDVideoSource **ppSrc) {
	return false;
}

bool VDInputFileMP3::GetAudioSource(int index, AudioSource **ppSrc) {
	if (index)
		return false;

	*ppSrc = new VDAudioSourceMP3(this);
	if (!*ppSrc)
		return false;

	(*ppSrc)->AddRef();
	return true;
}

const VDInputFileMP3::FrameInfo& VDInputFileMP3::GetFrameInfo(uint32 frame) const {
	return mFrames[frame];
}

void VDInputFileMP3::ReadSpan(sint64 pos, void *buffer, uint32 len) {
	mBufferedFile.Seek(mDataStart + pos);
	mBufferedFile.Read(buffer, len);
}

/////////////////////////////////////////////////////////////////////

class VDInputDriverMP3 : public vdrefcounted<IVDInputDriver> {
public:
	const wchar_t *GetSignatureName() { return L"MPEG audio input driver (internal)"; }

	int GetDefaultPriority() {
		return -1;
	}

	uint32 GetFlags() { return kF_Audio | kF_PromptForOpts | kF_SupportsOpts; }

	const wchar_t *GetFilenamePattern() {
		return L"MPEG audio (*.mp3,*.m2a,*.m1a,*.mpa)\0*.mp3;*.m2a;*.m1a;*.mpa\0";
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		size_t l = wcslen(pszFilename);
		if (l > 4) {
			const wchar_t *ext3 = pszFilename + l - 4;

			if (!_wcsicmp(ext3, L".mp3"))
				return true;

			if (!_wcsicmp(ext3, L".m2a"))
				return true;

			if (!_wcsicmp(ext3, L".m1a"))
				return true;

			if (!_wcsicmp(ext3, L".mpa"))
				return true;
		}

		return false;
	}

	DetectionConfidence DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		return kDC_None;
	}

	InputFile *CreateInputFile(uint32 flags) {
		VDInputFileMP3 *pf = new_nothrow VDInputFileMP3;

		if (!pf)
			throw MyMemoryError();

		return pf;
	}
};

IVDInputDriver *VDCreateInputDriverMP3() {
	return new VDInputDriverMP3;
}

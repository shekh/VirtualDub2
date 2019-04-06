//	VirtualDub - Video processing and capture application
//	Plugin headers
//	Copyright (C) 1998-2007 Avery Lee, All Rights Reserved.
//
//	The plugin headers in the VirtualDub plugin SDK are licensed differently
//	differently than VirtualDub and the Plugin SDK themselves.  This
//	particular file is thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_PLUGIN_VDINPUTDRIVER_H
#define f_VD2_PLUGIN_VDINPUTDRIVER_H

#include "vdplugin.h"
#include <string.h>

enum {
	// V1 (1.7.4.28204): Initial version
	// V2 (1.7.5): Default I/P frame model fixed.
	// V3 (1.8.0): VBR audio support.
	//   NOTE: Due to a bug, VirtualDub never properly announced this version.
	//         You must declare your input driver as V2 API compatible to work
	//         on 1.8.x and 1.9.x releases.
	// V4 (1.10.1): Added NoOptions definition flag.
	// V4 (FilterMod): Added ForceByName definition flag.
	// V5 (FilterMod): Extended StreamSource interface
	// V6 (FilterMod): Extended IFilterModFileTool interface
	// V7 (FilterMod): Extended IVDXAudioSource interface
	// V8 (FilterMod): Extended IVDXInputFile interface
	// V9 (FilterMod): Extended IVDXInputFile interface
	// V10 (FilterMod): Extended IVDXInputFile interface
	kVDXPlugin_InputDriverAPIVersion = 10
};

/// Unsigned 32-bit fraction.
struct VDXFraction {
	uint32 mNumerator;
	uint32 mDenominator;
};

typedef struct VDXHWNDStruct *VDXHWND;
typedef struct VDXBITMAPINFOHEADERStruct {
	enum { kCompressionRGB = 0 };
	uint32			mSize;
	sint32			mWidth;
	sint32			mHeight;
	uint16			mPlanes;
	uint16			mBitCount;
	uint32			mCompression;
	uint32			mSizeImage;
	sint32			mXPelsPerMeter;
	sint32			mYPelsPerMeter;
	uint32			mClrUsed;
	uint32			mClrImportant;
} VDXBITMAPINFOHEADER;

typedef struct VDXWAVEFORMATEXStruct {
	enum { kFormatPCM = 1 };
	uint16			mFormatTag;
	uint16			mChannels;
	uint32			mSamplesPerSec;
	uint32			mAvgBytesPerSec;
	uint16			mBlockAlign;
	uint16			mBitsPerSample;
	uint16			mExtraSize;
} VDXWAVEFORMATEX;

struct VDXStreamSourceInfo {
	VDXFraction		mSampleRate;
	sint64			mSampleCount;
	VDXFraction		mPixelAspectRatio;
};

// V3+ (1.8.0) only
struct VDXStreamSourceInfoV3 {
	VDXStreamSourceInfo	mInfo;

	enum {
		kFlagVariableSizeSamples	= 0x00000001
	};

	uint32			mFlags;
	uint32			mfccHandler;	///< If non-zero, specifies the FOURCC of a codec handler that should be preferred.
};

class IVDXStreamSource : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 's', 't', 's') };

	virtual void				VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo&) = 0;

	virtual bool				VDXAPIENTRY Read(sint64 lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) = 0;

	virtual const void *		VDXAPIENTRY GetDirectFormat() = 0;
	virtual int					VDXAPIENTRY GetDirectFormatLen() = 0;

	enum ErrorMode {
		kErrorModeReportAll = 0,
		kErrorModeConceal,
		kErrorModeDecodeAnyway,
		kErrorModeCount
	};

	virtual ErrorMode			VDXAPIENTRY GetDecodeErrorMode() = 0;
	virtual void				VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode) = 0;
	virtual bool				VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode) = 0;

	virtual bool				VDXAPIENTRY IsVBR() = 0;
	virtual sint64				VDXAPIENTRY TimeToPositionVBR(sint64 us) = 0;
	virtual sint64				VDXAPIENTRY PositionToTimeVBR(sint64 samples) = 0;
};

// V3+ (1.8.0)
class IVDXStreamSourceV3 : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 's', 't', '2') };

	virtual void				VDXAPIENTRY GetStreamSourceInfoV3(VDXStreamSourceInfoV3&) = 0;
};

// V5+ (FilterMod)
class IVDXStreamSourceV5 : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 's', 't', '3') };

	enum {
		// query: return false if direct copy is unsupported and should be disabled by host
		// apply: if not present, decoder can discard packets
		kStreamModeDirectCopy = 1,

		// query: return false if decode is unsupported
		// apply: if not present, decoder can bypass decoding
		kStreamModeUncompress = 2,

		// query: return false if decoder has no optimization for this mode
		// apply: if present, decoder should assume linear access (use frame threading etc), otherwise should assume random access
		kStreamModePlayForward = 4,
	};

	virtual void VDXAPIENTRY ApplyStreamMode(uint32 flags) = 0;
	virtual bool VDXAPIENTRY QueryStreamMode(uint32 flags) = 0;
};

class IVDXVideoDecoderModel : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'v', 'd', 'm') };

	virtual void				VDXAPIENTRY Reset() = 0;
	virtual void				VDXAPIENTRY SetDesiredFrame(sint64 frame_num) = 0;
	virtual sint64				VDXAPIENTRY GetNextRequiredSample(bool& is_preroll) = 0;
	virtual int					VDXAPIENTRY GetRequiredCount() = 0;
};

class IVDXVideoDecoder : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'v', 'd', 'e') };

	virtual const void *		VDXAPIENTRY DecodeFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, sint64 sampleNumber, sint64 targetFrame) = 0;
	virtual uint32				VDXAPIENTRY GetDecodePadding() = 0;
	virtual void				VDXAPIENTRY Reset() = 0;
	virtual	bool				VDXAPIENTRY IsFrameBufferValid() = 0;
	virtual const VDXPixmap&	VDXAPIENTRY GetFrameBuffer() = 0;
	virtual bool				VDXAPIENTRY SetTargetFormat(int format, bool useDIBAlignment) = 0;
	virtual bool				VDXAPIENTRY SetDecompressedFormat(const VDXBITMAPINFOHEADER *pbih) = 0;

	virtual bool				VDXAPIENTRY IsDecodable(sint64 sample_num) = 0;
	virtual const void *		VDXAPIENTRY GetFrameBufferBase() = 0;
};

class IFilterModVideoDecoder : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('F', 'M', 'd', 'e') };
	virtual const FilterModPixmapInfo&	VDXAPIENTRY GetFrameBufferInfo() = 0;
};

class IProjectState {
public:
	virtual bool GetSelection(sint64& start, sint64& end) = 0;
};

class IFilterModFileTool : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('F', 'M', 'f', 't') };
	virtual bool VDXAPIENTRY GetExportMenuInfo(int id, char* name, int name_size, bool* enabled) = 0;
	virtual bool VDXAPIENTRY ExecuteExport(int id, VDXHWND hwndParent, IProjectState* state) = 0;

	// V6
	virtual bool VDXAPIENTRY GetExportCommandName(int id, char* name, int name_size) = 0;
};

enum VDXVideoFrameType {
	kVDXVFT_Independent,
	kVDXVFT_Predicted,
	kVDXVFT_Bidirectional,
	kVDXVFT_Null,
};

struct VDXVideoFrameInfo {
	char	mTypeChar;
	uint8	mFrameType;
	sint64	mBytePosition;
};

struct VDXVideoSourceInfo {
	enum DecoderModel {
		kDecoderModelCustom,	///< A custom decoder model is provided.
		kDecoderModelDefaultIP	///< Use the default I/P decoder model.
	};

	enum Flags {
		kFlagNone			= 0,
		kFlagKeyframeOnly	= 0x00000001,
		kFlagSyncDecode		= 0x00000002, ///< Driver cannot split read/decode in asynchronous way.
		kFlagAll			= 0xFFFFFFFF
	};

public:
	uint32	mFlags;
	uint32	mWidth;
	uint32	mHeight;
	uint8	mDecoderModel;
	uint8	unused[3];
};

class IVDXVideoSource : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'v', 'd', 's') };

	virtual void	VDXAPIENTRY GetVideoSourceInfo(VDXVideoSourceInfo& info) = 0;

	virtual bool	VDXAPIENTRY CreateVideoDecoderModel(IVDXVideoDecoderModel **) = 0;
	virtual bool	VDXAPIENTRY CreateVideoDecoder(IVDXVideoDecoder **) = 0;

	virtual void	VDXAPIENTRY GetSampleInfo(sint64 sample_num, VDXVideoFrameInfo& frameInfo) = 0;

	virtual bool	VDXAPIENTRY IsKey(sint64 sample_num) = 0;

	virtual sint64	VDXAPIENTRY GetFrameNumberForSample(sint64 sample_num) = 0;
	virtual sint64	VDXAPIENTRY GetSampleNumberForFrame(sint64 frame_num) = 0;
	virtual sint64	VDXAPIENTRY GetRealFrame(sint64 frame_num) = 0;

	virtual sint64	VDXAPIENTRY GetSampleBytePosition(sint64 sample_num) = 0;
};

struct VDXAudioSourceInfo {
public:
	uint32	mFlags;
};

class IVDXAudioSource : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'a', 'd', 's') };

	virtual void VDXAPIENTRY GetAudioSourceInfo(VDXAudioSourceInfo& info) = 0;

	// V7
	virtual void VDXAPIENTRY SetTargetFormat(const VDXWAVEFORMATEX* format) = 0;
};

class IVDXInputOptions : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'i', 'o', 'p') };

	virtual uint32		VDXAPIENTRY Write(void *buf, uint32 buflen) = 0;
};

class IVDXInputFile : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'i', 'f', 'l') };

	virtual bool	VDXAPIENTRY PromptForOptions(VDXHWND, IVDXInputOptions **ppOptions) = 0;
	virtual bool	VDXAPIENTRY CreateOptions(const void *buf, uint32 len, IVDXInputOptions **ppOptions) = 0;

	virtual void	VDXAPIENTRY Init(const wchar_t *path, IVDXInputOptions *options) = 0;
	virtual bool	VDXAPIENTRY Append(const wchar_t *path) = 0;

	virtual void	VDXAPIENTRY DisplayInfo(VDXHWND hwndParent) = 0;

	virtual bool	VDXAPIENTRY GetVideoSource(int index, IVDXVideoSource **ppVS) = 0;
	virtual bool	VDXAPIENTRY GetAudioSource(int index, IVDXAudioSource **ppAS) = 0;

	// V8
	virtual int VDXAPIENTRY GetFileFlags(){ return -1; }

	// V10
	// flags meaning : as in CreateInputFile
	virtual bool	VDXAPIENTRY Append2(const wchar_t *path, int flags, IVDXInputOptions *options){ return Append(path); }
};

struct VDXMediaInfo {
	int width;
	int height;
	int pixmapFormat;
	wchar_t format_name[100];
	wchar_t vcodec_name[100];

	VDXMediaInfo() {
		width = 0;
		height = 0;
		pixmapFormat = 0;
		format_name[0] = 0;
		vcodec_name[0] = 0;
	}
};

///////////////////////////////////////////////////////////////////////////////
// IVDXInputFileDriver
//
class IVDXInputFileDriver : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'i', 'f', 'd') };

	enum DetectionConfidence {
		kDC_None,
		kDC_VeryLow,
		kDC_Low,
		kDC_Moderate,
		kDC_High
	};

	virtual int		VDXAPIENTRY DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) = 0;
	virtual bool	VDXAPIENTRY CreateInputFile(uint32 flags, IVDXInputFile **ppFile) = 0;
	
	// V8
	// Signature2 return value is different: enum DetectionConfidence
	virtual int		VDXAPIENTRY DetectBySignature2(VDXMediaInfo& info, const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		int r = DetectBySignature(pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize);
		if (r<0) return kDC_None;
		if (r==0) return kDC_Moderate;
		return kDC_High;
	}
	
	// V9
	virtual int		VDXAPIENTRY DetectBySignature3(VDXMediaInfo& info, const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize, const wchar_t* fileName) {
		return DetectBySignature2(info, pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize);
	}
};

struct VDXInputDriverContext {
	uint32	mAPIVersion;
	IVDPluginCallbacks *mpCallbacks;
};

typedef bool (VDXAPIENTRY *VDXInputDriverCreateProc)(const VDXInputDriverContext *pContext, IVDXInputFileDriver **);

struct VDXInputDriverDefinition {
	enum {
		kFlagNone				= 0x00000000,
		kFlagSupportsVideo		= 0x00000001,
		kFlagSupportsAudio		= 0x00000002,
		kFlagNoOptions			= 0x00000004,	// (V4+)
		kFlagCustomSignature	= 0x00010000,
		kFlagForceByName		= 0x00020000,
		kFlagDuplicate			= 0x00040000,
		kFlagAll				= 0xFFFFFFFF
	};
	uint32		mSize;				// size of this structure in bytes
	uint32		mFlags;
	sint32		mPriority;
	uint32		mSignatureLength;
	const void *mpSignature;
	const wchar_t *mpFilenameDetectPattern;
	const wchar_t *mpFilenamePattern;
	const wchar_t *mpDriverTagName;

	VDXInputDriverCreateProc		mpCreate;
};

///////////////////////////////////////////////////////////////////////////////

enum VDXVideoStreamType {
	kVDXST_Video,
	kVDXST_Audio,
	kVDXST_Subtitle,
	kVDXST_Data,
};

struct VDXAVIStreamHeader {
	uint32		fccType;
	uint32		fccHandler;
	uint32		dwFlags;
	uint16		wPriority;
	uint16		wLanguage;
	uint32		dwInitialFrames;
	uint32		dwScale;	
	uint32		dwRate;
	uint32		dwStart;
	uint32		dwLength;
	uint32		dwSuggestedBufferSize;
	uint32		dwQuality;
	uint32		dwSampleSize;
	struct {
		sint16	left;
		sint16	top;
		sint16	right;
		sint16	bottom;
	} rcFrame;
};

struct VDXStreamControl {
	int version; // this struct version

	// AVFMT_GLOBALHEADER
	bool global_header;

	// streams preserve relative timing
	bool use_offsets;
	sint64 timebase_num;
	sint64 timebase_den;

	VDXStreamControl() {
		version = 2;
		global_header = false;
		use_offsets = false;
		timebase_num = 0;
		timebase_den = 0;
	}
};

struct VDXStreamInfo {
	VDXAVIStreamHeader aviHeader;

	int version; // this struct version

	int avcodec_version; // LIBAVCODEC_VERSION_INT

	// fields below correspond to AVCodecParameters
	// all enum codes, meaning and default values match those of AVCodecParameters

	// not used: set explicitly
	//enum AVMediaType codec_type; 

	// not used: derived from bitmap (maybe needed for avi-incompatible formats)
	//enum AVCodecID   codec_id;
	//uint32_t         codec_tag;
	//uint8_t *extradata;
	//int      extradata_size;

	int format;
	sint64 bit_rate;
	int bits_per_coded_sample;
	int bits_per_raw_sample;
	int profile;
	int level;

	// not used: derived from bitmap
	//int width;
	//int height;

	// video only
	int sar_width;
	int sar_height;
	int field_order;
	int color_range;
	int color_primaries;
	int color_trc;
	int color_space;
	int chroma_location;
	int video_delay;

	// audio only
	int frame_size;
	int block_align;
	int initial_padding;
	int trailing_padding;

	// version >= 2
	sint64 start_num;
	sint64 start_den;

	VDXStreamInfo() {
		memset(&aviHeader,0,sizeof(aviHeader));
		version = 2;
		avcodec_version = 0;

		format = -1;
		bit_rate = 0;
		bits_per_coded_sample = 0;
		bits_per_raw_sample = 0;
		profile = -99;
		level = -99;
		sar_width = 0;
		sar_height = 1;
		field_order = 0;
		color_range = 0;
		color_primaries = 2;
		color_trc = 2;
		color_space = 2;
		chroma_location = 0;
		video_delay = 0;

		frame_size = 0;
		block_align = 0;
		initial_padding = 0;
		trailing_padding = 0;

		start_num = 0;
		start_den = 1;
	}
};

// same as AV_NOPTS_VALUE
const sint64 VDX_NOPTS_VALUE = ((sint64)uint64(0x8000000000000000));

struct VDXPictureCompress {
	int version;
	const VDXPixmapLayout* px;

	sint64 pts;
	sint64 dts;
	sint64 duration;

	VDXPictureCompress() {
		px = 0;
		version = 1;
		pts = VDX_NOPTS_VALUE;
		dts = VDX_NOPTS_VALUE;
		duration = 0;
	}
};

enum {
	kFormatCaps_UseVideo = 1,
	kFormatCaps_UseAudio = 2,
	kFormatCaps_Wildcard = 4,
	kFormatCaps_OffsetStreams = 8,
};

class IVDXOutputFile : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'o', 'f', 'l') };

	struct PacketInfo{
		uint32 flags;
		uint32 samples;
		sint64 pcm_samples;
		sint64 pts;
		sint64 dts;

		PacketInfo(){
			flags = 0;
			samples = 0;
			pcm_samples = -1; 
			pts = VDX_NOPTS_VALUE;
			dts = VDX_NOPTS_VALUE;
		}
	};

	virtual void	VDXAPIENTRY Init(const wchar_t *path, const char* format) = 0;
	virtual uint32 VDXAPIENTRY CreateStream(int type) = 0;
	virtual void VDXAPIENTRY SetVideo(uint32 index, const VDXStreamInfo& si, const void *pFormat, int cbFormat) = 0;
	virtual void VDXAPIENTRY SetAudio(uint32 index, const VDXStreamInfo& si, const void *pFormat, int cbFormat) = 0;
	virtual void VDXAPIENTRY Write(uint32 index, const void *pBuffer, uint32 cbBuffer, PacketInfo& info) = 0;
	virtual void Finalize() = 0;

	// V2
	// format name is from same namespace as EnumFormats, SuggestFileFormat, GetElementaryFormat
	// finally it is just short format name from ffmpeg
	// format name is also passed to init but init can change it
	virtual const char* GetFormatName() = 0;
	virtual bool Begin(){ return true; }
};

///////////////////////////////////////////////////////////////////////////////
// IVDXOutputFileDriver
//

class IVDXOutputFileDriver : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'o', 'f', 'd') };

	virtual bool	VDXAPIENTRY CreateOutputFile(IVDXOutputFile **ppFile) = 0;
	virtual bool	VDXAPIENTRY EnumFormats(int i, wchar_t* filter, wchar_t* ext, char* name) = 0; // all buffers are 128 chars
	virtual bool	VDXAPIENTRY GetStreamControl(const wchar_t *path, const char* format, VDXStreamControl& sc){ return true; }

	// V2
	virtual uint32	VDXAPIENTRY GetFormatCaps(int i){ return kFormatCaps_UseVideo; }
};

typedef bool (VDXAPIENTRY *VDXOutputDriverCreateProc)(const VDXInputDriverContext *pContext, IVDXOutputFileDriver **);

struct VDXOutputDriverDefinition {
	uint32		mSize;				// size of this structure in bytes
	uint32		mFlags;       // reserved

	const wchar_t *mpDriverName;
	const wchar_t *mpDriverTagName;

	VDXOutputDriverCreateProc		mpCreate;
};

enum {
	// V1 (FilterMod): Initial version
	kVDXPlugin_OutputDriverAPIVersion = 2
};

///////////////////////////////////////////////////////////////////////////////

namespace vd2{
	enum FormatConfidence {
		kFormat_Unknown = -1,
		kFormat_Reject = 0,
		kFormat_Unwise = 1,
		kFormat_Good = 2,
		kFormat_Elementary = 255,
	};
};

class IVDXAudioEnc : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'a', 'e', 'n') };

	virtual bool HasAbout() = 0;
	virtual bool HasConfig() = 0;
	virtual void ShowAbout(VDXHWND parent) = 0;
	virtual void ShowConfig(VDXHWND parent) = 0;
	virtual size_t GetConfigSize() = 0;
	virtual void* GetConfig() = 0;
	virtual void SetConfig(void* data, size_t size) = 0;

	virtual void SetInputFormat(VDXWAVEFORMATEX* format) = 0;
	virtual void Shutdown() = 0;
	virtual bool IsEnded() const = 0;

	virtual unsigned	GetInputLevel() const = 0;
	virtual unsigned	GetInputSpace() const = 0;
	virtual unsigned	GetOutputLevel() const = 0;
	virtual const VDXWAVEFORMATEX *GetOutputFormat() const = 0;
	virtual unsigned	GetOutputFormatSize() const = 0;
	virtual void		GetStreamInfo(VDXStreamInfo& si) const = 0;

	virtual void		Restart() = 0;
	virtual bool		Convert(bool flush, bool requireOutput) = 0;

	virtual void		*LockInputBuffer(unsigned& bytes) = 0;
	virtual void		UnlockInputBuffer(unsigned bytes) = 0;
	virtual const void	*LockOutputBuffer(unsigned& bytes) = 0;
	virtual void		UnlockOutputBuffer(unsigned bytes) = 0;
	virtual unsigned	CopyOutput(void *dst, unsigned bytes, sint64& duration) = 0;

	// V2
	virtual int	SuggestFileFormat(const char* name) { return vd2::kFormat_Unknown; }
	virtual const char* GetElementaryFormat() { return 0; }
};

typedef bool (VDXAPIENTRY *VDXAudioEncCreateProc)(const VDXInputDriverContext *pContext, IVDXAudioEnc **);

struct VDXAudioEncDefinition {
	uint32		mSize;				// size of this structure in bytes
	uint32		mFlags;       // reserved

	const wchar_t *mpDriverName;
	const char *mpDriverTagName;

	VDXAudioEncCreateProc		mpCreate;
};

enum {
	// V1: Initial version
	// V2: Added SuggestFileFormat
	kVDXPlugin_AudioEncAPIVersion = 2
};

///////////////////////////////////////////////////////////////////////////////
// video encoding - on top of vfw model

// same as kVDLogInfo .. kVDLogError
enum {
	VD_LOG_INFO,
	VD_LOG_MARKER,
	VD_LOG_WARNING,
	VD_LOG_ERROR
};

typedef void (*VDLogProc)(int severity, const wchar_t *format);

enum {
	VDICM_COMPRESS_INPUT_FORMAT = 1,  // query which format to use as input
	VDICM_COMPRESS_QUERY = 2,         // same as compress_query but src is PixmapLayout
	VDICM_COMPRESS_GET_FORMAT = 3,
	VDICM_COMPRESS_GET_SIZE = 4,
	VDICM_COMPRESS_BEGIN = 5,
	VDICM_COMPRESS = 6,
	VDICM_COMPRESS_MATRIX_INFO = 7,   // set vui options
	VDICM_ENUMFORMATS = 8,            // list included fourcc codes
	VDICM_LOGPROC = 9,                // attach log output
	VDICM_COMPRESS2 = 10,             // same as compress but with more room to return extra data
	VDICM_COMPRESS_INPUT_INFO = 11,   // test if format is known to encoder, irrespective of current settings
	VDICM_GETSTREAMINFO = 12,         // fill values in VDXStreamInfo
	VDICM_STREAMCONTROL = 13,         // adjust encoding options for given container
	VDICM_COMPRESS_TRUNCATE = 14,     // do not receive more frames, just flush delayed
	VDICM_GETHANDLER = 15,            // fourcc to identify codec in AVI (instead of icinfo)
};

enum {
	VDCOMPRESS_WAIT = 0x10000000L,
};

#endif

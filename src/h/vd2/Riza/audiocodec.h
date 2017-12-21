//	VirtualDub - Video processing and capture application
//	A/V interface library
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

#ifndef f_VD2_RIZA_AUDIOCODEC_H
#define f_VD2_RIZA_AUDIOCODEC_H

#ifdef _MSC_VER
	#pragma once
#endif

#ifndef f_VD2_SYSTEM_VDTYPES_H
	#include <vd2/system/vdtypes.h>
#endif

#ifdef _MSC_VER
	#pragma pack(push, 2)
#endif

struct VDXStreamInfo;

struct VDWaveFormat {
	enum {
		kTagPCM			= 1,
		kTagCCITTALaw	= 6,
		kTagCCITTMuLaw	= 7,
		kTagMPEG1		= 80,
		kTagMPEGLayer3	= 85
	};

	uint16		mTag;
	uint16		mChannels;
	uint32		mSamplingRate;
	uint32		mDataRate;
	uint16		mBlockSize;
	uint16		mSampleBits;
	uint16		mExtraSize;
};

#ifdef _MSC_VER
	#pragma pack(pop)
#endif

class VDINTERFACE IVDAudioCodec {
public:
	virtual ~IVDAudioCodec() {}
	virtual void Shutdown() = 0;

	virtual bool IsEnded() const = 0;

	virtual unsigned	GetInputLevel() const = 0;
	virtual unsigned	GetInputSpace() const = 0;
	virtual unsigned	GetOutputLevel() const = 0;
	virtual const VDWaveFormat *GetOutputFormat() const = 0;
	virtual unsigned	GetOutputFormatSize() const = 0;
	virtual void		GetStreamInfo(VDXStreamInfo& si) const {}

	virtual void		Restart() = 0;
	virtual bool		Convert(bool flush, bool requireOutput) = 0;

	virtual void		*LockInputBuffer(unsigned& bytes) = 0;
	virtual void		UnlockInputBuffer(unsigned bytes) = 0;
	virtual const void	*LockOutputBuffer(unsigned& bytes) = 0;
	virtual void		UnlockOutputBuffer(unsigned bytes) = 0;
	virtual unsigned	CopyOutput(void *dst, unsigned bytes) = 0;
	virtual unsigned	CopyOutput(void *dst, unsigned bytes, sint64& duration) {
		duration = -1;
		return CopyOutput(dst,bytes);
	}
};

IVDAudioCodec *VDLocateAudioDecompressor(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, bool preferInternalCodecs, const char *pShortNameDriverHint = NULL);

IVDAudioCodec *VDCreateAudioDecompressor(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat);
IVDAudioCodec *VDCreateAudioDecompressorALaw(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat);
IVDAudioCodec *VDCreateAudioDecompressorMuLaw(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat);
IVDAudioCodec *VDCreateAudioDecompressorMPEG(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat);

IVDAudioCodec *VDCreateAudioCompressorW32(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, const char *pShortNameDriverHint, bool throwIfNotFound);
IVDAudioCodec *VDCreateAudioDecompressorW32(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, const char *pShortNameDriverHint, bool throwIfNotFound);

bool is_audio_pcm(const VDWaveFormat *wfex);
bool is_audio_pcm8(const VDWaveFormat *wfex);
bool is_audio_pcm16(const VDWaveFormat *wfex);
bool is_audio_float(const VDWaveFormat *wfex);
int get_audio_sampleBits(const VDWaveFormat *wfex);
int default_channel_mask(int n);

#endif	// f_VD2_RIZA_AUDIOCODEC_H

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

#ifndef f_VIDEOSOURCEAVI_H
#define f_VIDEOSOURCEAVI_H

#include "VideoSource.h"
#include "InputFileAVI.h"

#ifdef _MSC_VER
	#pragma once
#endif

class VideoSourceAVI : public VideoSource, public VDAVIStreamSource {
private:
	InputFileAVI *mpParent;
	IAVIReadHandler *pAVIFile;
	IAVIReadStream *pAVIStream;
	VDPosition		lLastFrame;

	VDPixmapLayout	mSourceLayout;
	int				mSourceVariant;
	uint32			mSourceFrameSize;

	bool		fAllKeyFrames;
	bool		bIsType1;
	bool		bDirectDecompress;
	bool		bInvertFrames;

	const uint32 *mpKeyFlags;
	bool		use_internal;
	int			mjpeg_mode;
	void		*mjpeg_reorder_buffer;
	int			mjpeg_reorder_buffer_size;
	long		*mjpeg_splits;
	VDPosition	mjpeg_last;
	long		mjpeg_last_size;
	uint32		fccForceVideo;
	uint32		fccForceVideoHandler;

	ErrorMode	mErrorMode;
	bool		mbMMXBrokenCodecDetected;
	bool		mbConcealingErrors;
	bool		mbDecodeStarted;
	bool		mbDecodeRealTime;

	vdautoptr<IVDVideoDecompressor>	mpDecompressor;

	VDStringW	mDriverName;
	char		szCodecName[128];

	bool _construct(int streamIndex);
	void _destruct();

	~VideoSourceAVI();

public:
	VideoSourceAVI(InputFileAVI *pParent, IAVIReadHandler *pAVI, AVIStripeSystem *stripesys, IAVIReadHandler **stripe_files, bool use_internal, int mjpeg_mode, uint32 fccForceVideo, uint32 fccForceVideoHandler, const uint32 *key_flags);

	bool Init(int stream_index);
	void Reinit();
	void redoKeyFlags(vdfastvector<uint32>& newFlags);

	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	bool _isKey(VDPosition samp);
	VDPosition nearestKey(VDPosition lSample);
	VDPosition prevKey(VDPosition lSample);
	VDPosition nextKey(VDPosition lSample);

	bool setTargetFormat(int format);
	bool setDecompressedFormat(int depth) { return VideoSource::setDecompressedFormat(depth); }
	bool setDecompressedFormat(const VDAVIBitmapInfoHeader *pbih);
	void invalidateFrameBuffer();
	bool isFrameBufferValid();
	bool isStreaming();

	void streamBegin(bool fRealTime, bool bForceReset);
	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_sample);
	void streamEnd();

	// I really hate doing this, but an awful lot of codecs are sloppy about their
	// Huffman or VLC decoding and read a few bytes beyond the end of the stream.
	uint32 streamGetDecodePadding() { return 16; }

	// This is to work around an XviD decode bug. From squid_80:
	// "When decompressing a b-frame, Xvid reads past the end of the input buffer looking for a resync
	//  marker. This is the nasty bit - if it sees what it thinks is a resync marker it toddles off the
	//  end of the input buffer merrily decoding garbage. Unfortunately it doesn't stay merry for long.
	//  Best case = artifacts in the decompressed frame, worst case = heap corruption which lets the
	//  encode continue but with a borked result, normal case = plain old access violation."
	void streamFillDecodePadding(void *inputBuffer, uint32 data_len);

	const void *getFrame(VDPosition frameNum);

	const wchar_t *getDecompressorName() const {
		return mpDecompressor ? mpDecompressor->GetName() : NULL;
	}

	char getFrameTypeChar(VDPosition lFrameNum);
	eDropType getDropType(VDPosition lFrameNum);
	bool isKeyframeOnly();
	bool isType1();
	bool isDecodable(VDPosition sample_num);

	ErrorMode getDecodeErrorMode() { return mErrorMode; }
	void setDecodeErrorMode(ErrorMode mode);
	bool isDecodeErrorModeSupported(ErrorMode mode);

	VDPosition	getRealDisplayFrame(VDPosition display_num);
	sint64 getSampleBytePosition(VDPosition pos);
};

#endif

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

#ifndef f_VIDEOSOURCE_H
#define f_VIDEOSOURCE_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/VDString.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Riza/videocodec.h>
#include <vd2/Riza/avi.h>

#include "DubSource.h"

class IVDStreamSource;

class IVDVideoSource : public IVDRefCount {
public:
	virtual IVDStreamSource *asStream() = 0;

	virtual VDAVIBitmapInfoHeader *getImageFormat() = 0;

	virtual const void *getFrameBuffer() = 0;
	virtual const VDFraction getPixelAspectRatio() const = 0;

	virtual VDPixmapFormatEx getDefaultFormat() = 0;
	virtual VDPixmapFormatEx getSourceFormat() = 0;
	virtual const VDPixmap& getTargetFormat() = 0;
	virtual bool		setTargetFormat(VDPixmapFormatEx format) = 0;
	virtual bool		setDecompressedFormat(int depth) = 0;
	virtual bool		setDecompressedFormat(const VDAVIBitmapInfoHeader *pbih) = 0;

	virtual VDAVIBitmapInfoHeader *getDecompressedFormat() = 0;
	virtual uint32		getDecompressedFormatLen() = 0;

	virtual void		streamSetDesiredFrame(VDPosition frame_num) = 0;
	virtual VDPosition	streamGetNextRequiredFrame(bool& is_preroll) = 0;
	virtual int			streamGetRequiredCount(uint32 *totalsize) = 0;
	virtual const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_sample) = 0;
	virtual uint32		streamGetDecodePadding() = 0;
	virtual void		streamFillDecodePadding(void *buffer, uint32 data_len) = 0;

	virtual bool		streamOwn(void *owner) = 0;
	virtual void		streamDisown(void *owner) = 0;
	virtual void		streamBegin(bool fRealTime, bool bForceReset) = 0;
	virtual void		streamRestart() = 0;
	virtual void		streamAppendReinit() = 0;

	virtual void		invalidateFrameBuffer() = 0;
	virtual	bool		isFrameBufferValid() = 0;

	virtual const void *getFrame(VDPosition frameNum) = 0;

	virtual char		getFrameTypeChar(VDPosition lFrameNum) = 0;

	enum eDropType {
		kDroppable		= 0,
		kDependant,
		kIndependent,
	};

	virtual eDropType	getDropType(VDPosition lFrameNum) = 0;

	virtual bool isKey(VDPosition lSample) = 0;
	virtual VDPosition nearestKey(VDPosition lSample) = 0;
	virtual VDPosition prevKey(VDPosition lSample) = 0;
	virtual VDPosition nextKey(VDPosition lSample) = 0;

	virtual bool		isKeyframeOnly() = 0;
	virtual bool		isSyncDecode() = 0;
	virtual bool		isType1() = 0;

	virtual VDPosition	streamToDisplayOrder(VDPosition sample_num) = 0;
	virtual VDPosition	displayToStreamOrder(VDPosition display_num) = 0;
	virtual VDPosition	getRealDisplayFrame(VDPosition display_num) = 0;

	virtual bool		isDecodable(VDPosition sample_num) = 0;

	virtual sint64		getSampleBytePosition(VDPosition sample_num) = 0;
};

class VideoSource : public DubSource, public IVDVideoSource {
protected:
	void		*mpFrameBuffer;
	uint32		mFrameBufferSize;

	vdstructex<VDAVIBitmapInfoHeader> mpTargetFormatHeader;
	VDPixmap	mTargetFormat;
	int			mTargetFormatVariant;
	VDPixmapFormatEx	mDefaultFormat;
	VDPixmapFormatEx	mSourceFormat;
	VDPosition	stream_desired_frame;
	VDPosition	stream_current_frame;

	void		*mpStreamOwner;

	uint32		mPalette[256];

	void *AllocFrameBuffer(long size);
	void FreeFrameBuffer();

	bool setTargetFormatVariant(VDPixmapFormatEx format, int variant);
	virtual bool _isKey(VDPosition lSample);

	VideoSource();

public:
	enum {
		IFMODE_NORMAL		=0,
		IFMODE_SWAP			=1,
		IFMODE_SPLIT1		=2,
		IFMODE_SPLIT2		=3,
		IFMODE_DISCARD1		=4,
		IFMODE_DISCARD2		=5,
	};

	virtual ~VideoSource();

	IVDStreamSource *asStream() { return this; }

	int AddRef() { return DubSource::AddRef(); }
	int Release() { return DubSource::Release(); }

	VDAVIBitmapInfoHeader *getImageFormat() {
		return (VDAVIBitmapInfoHeader *)getFormat();
	}

	virtual const VDFraction getPixelAspectRatio() const;

	virtual const void *getFrameBuffer() {
		return mpFrameBuffer;
	}

	virtual VDPixmapFormatEx getDefaultFormat() { return mDefaultFormat; }
	virtual VDPixmapFormatEx getSourceFormat() { return mSourceFormat; }
	virtual const VDPixmap& getTargetFormat() { return mTargetFormat; }
	virtual bool setTargetFormat(VDPixmapFormatEx format);
	virtual bool setDecompressedFormat(int depth);
	virtual bool setDecompressedFormat(const VDAVIBitmapInfoHeader *pbih);

	VDAVIBitmapInfoHeader *getDecompressedFormat() {
		return mpTargetFormatHeader.empty() ? NULL : mpTargetFormatHeader.data();
	}

	uint32 getDecompressedFormatLen() {
		return mpTargetFormatHeader.size();
	}

	virtual void streamSetDesiredFrame(VDPosition frame_num);
	virtual VDPosition streamGetNextRequiredFrame(bool& is_preroll);
	virtual int	streamGetRequiredCount(uint32 *totalsize);
	virtual uint32 streamGetDecodePadding() { return 0; }
	virtual void streamFillDecodePadding(void *inputBuffer, uint32 data_len) {}

	virtual bool streamOwn(void *owner);
	virtual void streamDisown(void *owner);
	virtual void streamBegin(bool fRealTime, bool bForceReset);
	virtual void streamRestart();
	virtual void streamAppendReinit(){}

	virtual void invalidateFrameBuffer();
	virtual	bool isFrameBufferValid() = NULL;

	virtual const void *getFrame(VDPosition frameNum) = NULL;

	virtual bool isKey(VDPosition lSample);
	virtual VDPosition nearestKey(VDPosition lSample);
	virtual VDPosition prevKey(VDPosition lSample);
	virtual VDPosition nextKey(VDPosition lSample);

	virtual bool isKeyframeOnly();
	virtual bool isSyncDecode();
	virtual bool isType1();

	virtual VDPosition	streamToDisplayOrder(VDPosition sample_num) { return sample_num; }
	virtual VDPosition	displayToStreamOrder(VDPosition display_num) { return display_num; }
	virtual VDPosition	getRealDisplayFrame(VDPosition display_num) { return display_num; }

	virtual sint64		getSampleBytePosition(VDPosition sample_num) { return -1; }
};

#endif

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

#ifndef f_AUDIO_H
#define f_AUDIO_H

#include <vd2/system/vdalloc.h>
#include <vd2/system/fraction.h>
#include <vd2/Riza/audiocodec.h>
#include "AudioFilterSystem.h"

#include "FrameSubset.h"

typedef void (*AudioFormatConverter)(void *, void *, long);
typedef long (*AudioPointSampler)(void *, void *, long, long, long);
typedef long (*AudioUpSampler)(void *, void *, long, long, long);
typedef long (*AudioDownSampler)(void *, void *, long *, int, long, long, long);

class VDFraction;
class AudioSource;
class VDAudioFilterSystem;
class IVDAudioFilterInstance;
class IVDAudioFilterSink;
class IVDStreamSource;

///////

class AudioStream {
protected:
	VDWaveFormat *format;
	long format_len;

	AudioStream *source;
	sint64 samples_read;
	sint64 stream_len;
	sint64 stream_limit;

	AudioStream();

	VDWaveFormat *AllocFormat(long len);
public:
	virtual ~AudioStream();

	virtual void UpdateFormat() {}
	virtual VDWaveFormat *GetFormat() const;
	virtual long GetFormatLen() const;
	virtual sint64 GetSampleCount() const;
	virtual sint64 GetLength() const;
	virtual const VDFraction GetSampleRate() const;
	virtual bool IsVBR() const = 0;
	virtual void GetStreamInfo(VDXStreamInfo& si) const {}

	virtual long _Read(void *buffer, long max_samples, long *lplBytes);
	virtual long Read(void *buffer, long max_samples, long *lplBytes);
	virtual long _ReadVBR(void *buffer, long size, long *lplBytes, sint64 *duration);
	virtual long ReadVBR(void *buffer, long size, long *lplBytes, sint64 *duration);
	virtual bool Skip(sint64 samples);
	virtual void SetSource(AudioStream *source);
	virtual void SetLimit(sint64 limit);
	virtual bool isEnd();
	virtual bool _isEnd();

	virtual void Seek(VDPosition pos);
};

class AudioStreamSource : public AudioStream {
private:
	AudioSource *aSrc;
	sint64 cur_samp;
	sint64 end_samp;
	sint64 mPreskip;
	sint64 mPrefill;
	sint64 mOffset;
	sint64 mBasePos;
	bool fZeroRead;
	bool fStart;
	bool mbSourceIsVBR;

	vdautoptr<IVDAudioCodec>	mpCodec;

public:
	AudioStreamSource(AudioSource *src, sint64 max_sample, bool allow_decompression, sint64 offset);
	~AudioStreamSource();

	const VDFraction GetSampleRate() const;

	bool IsVBR() const;
	long _Read(void *buffer, long max_samples, long *lplBytes);
	bool Skip(sint64 samples);
	bool _isEnd();
	void Seek(VDPosition);
};

class AudioStreamConverter : public AudioStream {
private:
	AudioFormatConverter convRout;
	void *cbuffer;
	int bytesPerInputSample, bytesPerOutputSample;
	int offset;

	enum { BUFFER_SIZE=4096 };

public:
	AudioStreamConverter(AudioStream *src, bool to_16bit, bool to_stereo_or_right, bool single_only);
	~AudioStreamConverter();

	bool IsVBR() const { return false; }
	long _Read(void *buffer, long max_samples, long *lplBytes);
	bool _isEnd();

	bool Skip(sint64);
};

class AudioStreamResampler : public AudioStream {
private:
	AudioPointSampler ptsampleRout;
	AudioUpSampler upsampleRout;
	AudioDownSampler dnsampleRout;
	void *cbuffer;
	int bytesPerSample;
	long samp_frac;
	long accum;
	int holdover;
	long *filter_bank;
	int filter_width;
	bool fHighQuality;

	uint32	mBufferSize;

	enum { MIN_BUFFER_SIZE=512 };

	long Upsample(void *buffer, long samples, long *lplBytes);
	long Downsample(void *buffer, long samples, long *lplBytes);

public:
	AudioStreamResampler(AudioStream *source, long new_rate, bool high_quality);
	~AudioStreamResampler();

	bool IsVBR() const { return false; }
	long _Read(void *buffer, long max_samples, long *lplBytes);
	bool _isEnd();
};

class AudioCompressor : public AudioStream {
private:
	vdautoptr<IVDAudioCodec>	mpCodec;
	bool fStreamEnded;
	bool fVBR;
	long bytesPerInputSample;
	long bytesPerOutputSample;
	sint64 mPrefill;

	char mDriverName[64];

	enum { INPUT_BUFFER_SIZE = 16384 };

public:
	AudioCompressor(AudioStream *src, const VDWaveFormat *dst_format, long dst_format_len, const char *shortNameHint, vdblock<char>& config);
	~AudioCompressor();
	void UpdateFormat();
	void SkipSource(long samples);
	bool IsVBR() const;
	void GetStreamInfo(VDXStreamInfo& si) const;
	const VDFraction GetSampleRate() const;
	long _Read(void *buffer, long samples, long *lplBytes);
	long _ReadVBR(void *buffer, long size, long *lplBytes, sint64 *duration);
	bool	isEnd();

protected:
	bool Process();
};

class AudioL3Corrector {
private:
	long samples, frame_bytes, read_left, frames;
	bool header_mode;
	char hdr_buffer[4];

public:
	AudioL3Corrector();
	long ComputeByterate(long sample_rate) const;
	double ComputeByterateDouble(long sample_rate) const;

	sint32 GetFrameCount() const { return frames; }

	void Process(void *buffer, long bytes);
};

class AudioStreamL3Corrector : public AudioStream, public AudioL3Corrector {
public:
	AudioStreamL3Corrector(AudioStream *src);
	~AudioStreamL3Corrector();

	const VDFraction GetSampleRate() const { return source->GetSampleRate(); }
	bool IsVBR() const { return source->IsVBR(); }
	void GetStreamInfo(VDXStreamInfo& si) const { source->GetStreamInfo(si); }

	long _Read(void *buffer, long max_samples, long *lplBytes);
	long _ReadVBR(void *buffer, long size, long *lplBytes, sint64 *duration);
	bool isEnd(){ return source->isEnd(); }
	bool Skip(sint64 n){ return source->Skip(n); }
};

class AudioStats : public AudioStream {
public:
	uint64 packets, total_bytes, max_sample, total_duration;

	AudioStats(AudioStream *src);
	const VDFraction GetSampleRate() const { return source->GetSampleRate(); }
	bool IsVBR() const { return source->IsVBR(); }
	void GetStreamInfo(VDXStreamInfo& si) const { source->GetStreamInfo(si); }
	void UpdateFormat() { source->UpdateFormat(); }
	VDWaveFormat *GetFormat() const { return source->GetFormat(); }
	long GetFormatLen() const { return source->GetFormatLen(); }

	long _ReadVBR(void *buffer, long size, long *lplBytes, sint64 *duration);
	bool isEnd(){ return source->isEnd(); }
	bool Skip(sint64 n){ return source->Skip(n); }
	long ComputeByterate() const;
	double ComputeByterateDouble() const;
};

class AudioSubset : public AudioStream {
private:
	FrameSubset subset;
	FrameSubset::const_iterator pfsnCur;
	sint64 mOffset;
	sint64 mSrcPos;
	int mSkipSize;
	int mCurrentSource;
	AudioStream *mpCurrentSource;
	bool mbLimited;
	bool mbEnded;

	typedef vdfastvector<AudioStream *> Sources;
	Sources mSources;

	enum { kSkipBufferSize = 512 };

public:
	AudioSubset(const vdfastvector<AudioStream *>& sources, const FrameSubset *pfs, const VDFraction& videoFrameRate, sint64 preskew, bool appendTail);
	~AudioSubset();

	const VDFraction GetSampleRate() const;

	bool IsVBR() const;
	long _Read(void *, long, long *);
	bool _isEnd();
};

class AudioStreamAmplifier : public AudioStream {
private:
	int mFactor;

public:
	AudioStreamAmplifier(AudioStream *src, float factor);
	~AudioStreamAmplifier();

	bool IsVBR() const { return false; }
	long _Read(void *buffer, long max_samples, long *lplBytes);
	bool _isEnd();
	bool Skip(sint64);
};

class AudioFilterSystemStream : public AudioStream {
public:
	AudioFilterSystemStream(const VDAudioFilterGraph& graph, sint64 start_us);
	~AudioFilterSystemStream();

	bool IsVBR() const { return false; }
	long _Read(void *buffer, long max_samples, long *lplBytes);
	bool _isEnd();
	bool Skip(sint64);
	void Seek(VDPosition);

protected:
	IVDAudioFilterSink *mpFilterIF;

	VDScheduler mFilterSystemScheduler;
	VDAudioFilterSystem mFilterSystem;
	sint64		mStartTime;
	sint64		mSamplePos;
};

sint64 AudioTranslateVideoSubset(FrameSubset& dst, const FrameSubset& src, const VDFraction& videoFrameRate, const VDWaveFormat *pwfex, sint64 appendTail, IVDStreamSource *pVBRAudio);

#endif

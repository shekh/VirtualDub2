//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#include <stdafx.h>
#include <vd2/system/thread.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/math.h>
#include <vd2/system/profile.h>
#include <vd2/VDCapture/capaudiocomp.h>
#include <vd2/Riza/w32audiocodec.h>
#include <vd2/Priss/convert.h>

///////////////////////////////////////////////////////////////////////////

class VDCaptureAudioCompFilter : public IVDCaptureAudioCompFilter {
public:
	VDCaptureAudioCompFilter();
	~VDCaptureAudioCompFilter();

	void SetChildCallback(IVDCaptureDriverCallback *pChild);
	void SetSourceSplit(bool enable);

	void Init(const WAVEFORMATEX *srcFormat, const WAVEFORMATEX *dstFormat, const char *pShortNameHint);

	void GetStatus(VDCaptureAudioCompStatus&);

	void CapBegin(sint64 global_clock);
	void CapEnd(const MyError *pError);
	bool CapEvent(nsVDCapture::DriverEvent event, int data);
	void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);

protected:
	IVDCaptureDriverCallback *mpCB;
	bool	mbSplitSource;

	VDAudioCodecW32	mCodec;
	VDRTProfileChannel	mProfileChannel;
};

VDCaptureAudioCompFilter::VDCaptureAudioCompFilter()
	: mpCB(NULL)
	, mProfileChannel("Audio compressor")
{
}

VDCaptureAudioCompFilter::~VDCaptureAudioCompFilter() {
}

IVDCaptureAudioCompFilter *VDCreateCaptureAudioCompFilter() {
	return new VDCaptureAudioCompFilter;
}

void VDCaptureAudioCompFilter::SetChildCallback(IVDCaptureDriverCallback *pChild) {
	mpCB = pChild;
}

void VDCaptureAudioCompFilter::SetSourceSplit(bool enable) {
	mbSplitSource = enable;
}

void VDCaptureAudioCompFilter::Init(const WAVEFORMATEX *srcFormat, const WAVEFORMATEX *dstFormat, const char *pShortNameHint) {
	mCodec.Init(srcFormat, dstFormat, true, pShortNameHint, true);
}

void VDCaptureAudioCompFilter::GetStatus(VDCaptureAudioCompStatus& status) {
}

void VDCaptureAudioCompFilter::CapBegin(sint64 global_clock) {
	mCodec.Restart();

	mpCB->CapBegin(global_clock);
}

void VDCaptureAudioCompFilter::CapEnd(const MyError *pError) {
	mpCB->CapEnd(pError);
}

bool VDCaptureAudioCompFilter::CapEvent(nsVDCapture::DriverEvent event, int data) {
	return mpCB->CapEvent(event, data);
}

void VDCaptureAudioCompFilter::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock)  {
	if (stream != 1) {
		mpCB->CapProcessData(stream, data, size, timestamp, key, global_clock);
		return;
	}

	if (mbSplitSource)
		mpCB->CapProcessData(-2, data, size, timestamp, key, global_clock);

	while(size > 0) {
		unsigned bytes;
		void *dst = mCodec.LockInputBuffer(bytes);

		if (bytes > 0) {
			if (bytes > size)
				bytes = size;

			memcpy(dst, data, bytes);
			data = (const char *)data + bytes;

			size -= bytes;
		}

		mCodec.UnlockInputBuffer(bytes);

		mProfileChannel.Begin(0xffe0e0, "Compress");
		bool progressMade = mCodec.Convert(false, !bytes);
		mProfileChannel.End();
		if (!progressMade)
			break;

		const void *src = mCodec.LockOutputBuffer(bytes);
		if (bytes > 0)
			mpCB->CapProcessData(1, src, bytes, 0, true, global_clock);
		mCodec.UnlockOutputBuffer(bytes);
	}
}

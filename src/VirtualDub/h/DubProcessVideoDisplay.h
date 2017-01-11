//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef f_VD2_DUBPROCESSVIDEODISPLAY_H
#define f_VD2_DUBPROCESSVIDEODISPLAY_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/atomic.h>
#include <vd2/Kasumi/pixmaputils.h>

class VDLoopThrottle;
class VDRenderOutputBuffer;
class VDRenderOutputBufferTracker;
class IVDAsyncBlitter;
class IVDVideoDisplay;
class DubOptions;
class IVDVideoCompressor;
class IVDVideoDecompressor;

class VDDubVideoProcessorDisplay {
	VDDubVideoProcessorDisplay(const VDDubVideoProcessorDisplay&);
	VDDubVideoProcessorDisplay& operator=(const VDDubVideoProcessorDisplay&);
public:
	VDDubVideoProcessorDisplay();
	~VDDubVideoProcessorDisplay();

	void SetThreadInfo(VDLoopThrottle *throttle);
	void SetOptions(const DubOptions *opts);
	void SetInputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetOutputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetBlitter(IVDAsyncBlitter *blitter);
	void SetVideoCompressor(IVDVideoCompressor *pCompressor);
	void SetVideoSource(IVDVideoSource *pVideo);

	sint32 GetLatency() const;
	uint32 GetDisplayClock() const;
	void AdvanceFrame();

	bool TryLockInputChannel(sint32 timeout);
	void UnlockInputChannel();

	bool TryRevokeOutputBuffer(VDRenderOutputBuffer **buffer);

	void UnlockAndDisplay(bool forceDisplay, VDRenderOutputBuffer *pBuffer, bool outputValid);

	void ScheduleUpdate();
	void CheckForDecompressorSwitch();
	void UpdateDecompressedVideo(const void *data, uint32 size, bool isKey);

protected:
	static bool AsyncReinitDisplayCallback(int pass, void *pThisAsVoid, void *, bool aborting);
	static bool AsyncUpdateCallback(int pass, void *pDisplayAsVoid, void *pInterlaced, bool aborting);
	static bool StaticAsyncUpdateOutputCallback(int pass, void *pThisAsVoid, void *pBuffer, bool aborting);
	bool AsyncUpdateOutputCallback(int pass, VDRenderOutputBuffer *pBuffer, bool aborting);

	const DubOptions	*mpOptions;
	IVDVideoCompressor	*mpVideoCompressor;
	VDLoopThrottle		*mpLoopThrottle;
	bool				mbInputLocked;

	// DISPLAY
	vdrefptr<IVDVideoSource>	mpVideoSource;
	uint32				mFramesToDrop;
	IVDAsyncBlitter		*mpBlitter;
	IVDVideoDisplay		*mpInputDisplay;
	IVDVideoDisplay		*mpOutputDisplay;
	VDAtomicInt			mRefreshFlag;

	// DECOMPRESSION PREVIEW
	vdautoptr<IVDVideoDecompressor>	mpVideoDecompressor;
	bool				mbVideoDecompressorEnabled;
	bool				mbVideoDecompressorPending;
	bool				mbVideoDecompressorErrored;
	VDPixmapBuffer		mVideoDecompBuffer;
};

#endif	// f_VD2_DUBPROCESSVIDEODISPLAY_H

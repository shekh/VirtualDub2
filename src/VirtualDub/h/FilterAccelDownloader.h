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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#ifndef f_VD2_FILTERACCELDOWNLOADER_H
#define f_VD2_FILTERACCELDOWNLOADER_H

#include <vd2/system/thread.h>
#include <vd2/Kasumi/blitter.h>
#include "FilterInstance.h"
#include "FilterFrameManualSource.h"
#include "FilterAccelEngine.h"
#include "FilterFrameRequest.h"

class IVDFilterSystemScheduler;

class VDFilterAccelDownloader : public VDFilterFrameManualSource {
	VDFilterAccelDownloader(const VDFilterAccelDownloader&);
	VDFilterAccelDownloader& operator=(const VDFilterAccelDownloader&);
public:
	VDFilterAccelDownloader();
	~VDFilterAccelDownloader();

	void Init(VDFilterAccelEngine *engine, IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const VDPixmapLayout *sourceLayoutOverride);
	void Start(IVDFilterFrameEngine *frameEngine);
	void Stop();

	bool GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex);
	sint64 GetSourceFrame(sint64 outputFrame);
	sint64 GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source);
	sint64 GetNearestUniqueFrame(sint64 outputFrame);

	RunResult RunRequests(const uint32 *batchNumberLimit, int index);

protected:
	struct CallbackMsg;

	bool InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable, uint32 batchNumber);

	static void StaticInitCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	static void StaticShutdownCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	static void StaticCleanupCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);

	VDFilterAccelEngine *mpEngine;
	IVDFilterFrameSource *mpSource;
	IVDFilterFrameEngine *mpFrameEngine;
	VDFilterAccelReadbackBuffer *mpReadbackBuffer;
	vdrefptr<VDFilterFrameRequest> mpRequest;

	VDPixmapLayout		mSourceLayout;

	VDSignal	mCompletedSignal;

	struct DownloadMsg : public VDFilterAccelEngineDownloadMsg {
		VDFilterAccelDownloader *mpDownloader;
	};

	DownloadMsg mDownloadMsg;
};

#endif

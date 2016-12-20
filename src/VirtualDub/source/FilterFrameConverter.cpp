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

#include "stdafx.h"
#include <vd2/Kasumi/pixmaputils.h>
#include <../Kasumi/h/uberblit_rgb64.h>
#include <../Kasumi/h/uberblit_16f.h>
#include "FilterFrameConverter.h"

class VDFilterFrameConverterNode {
public:
	VDFilterFrameConverterNode();
	void Stop();
	void EndFrame(bool success);
	IVDFilterFrameSource::RunResult RunProcess();

	VDFilterFrameConverter* source;
	vdautoptr<IVDPixmapBlitter> mpBlitter;
	vdrefptr<VDFilterFrameRequest> mpRequest;

	VDPixmap	mPixmapSrc;
	VDPixmap	mPixmapDst;

	VDAtomicInt	mbRequestPending;
	VDAtomicInt	mbRequestSuccess;
};

VDFilterFrameConverterNode::VDFilterFrameConverterNode() 
	: mbRequestPending(false)
{
	source = 0;
}

VDFilterFrameConverter::VDFilterFrameConverter()
{
	node = 0;
	node_count = 0;
}

VDFilterFrameConverter::~VDFilterFrameConverter() {
	delete[] node;
}

void VDFilterFrameConverter::Init(IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const VDPixmapLayout *sourceLayoutOverride, bool normalize16) {
	mpSource = source;
	mSourceLayout = sourceLayoutOverride ? *sourceLayoutOverride : source->GetOutputLayout();
	mNormalize16 = normalize16;
	SetOutputLayout(outputLayout);
}

int VDFilterFrameConverter::AllocateNodes(int threads) {
	node_count = threads;
	node = new VDFilterFrameConverterNode[threads];

	IVDPixmapExtraGen* extraDst = 0;
	if (mNormalize16) {
		switch (mLayout.format) {
		case nsVDPixmap::kPixFormat_XRGB64:
			extraDst = new ExtraGen_X16R16G16B16_Normalize;
			break;
		case nsVDPixmap::kPixFormat_YUV420_Planar16:
		case nsVDPixmap::kPixFormat_YUV422_Planar16:
		case nsVDPixmap::kPixFormat_YUV444_Planar16:
			extraDst = new ExtraGen_YUV_Normalize;
			break;
		}
	}

	{for(int i=0; i<threads; i++){
		node[i].source = this;
		node[i].mpBlitter = VDPixmapCreateBlitter(mLayout, mSourceLayout, extraDst);
	}}

	delete extraDst;

	return threads;
}

void VDFilterFrameConverter::Start(IVDFilterFrameEngine *engine) {
	mpEngine = engine;
}

void VDFilterFrameConverter::Stop() {
	{for(int i=0; i<node_count; i++){
		node[i].Stop();
	}}
}

bool VDFilterFrameConverter::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	return mpSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 VDFilterFrameConverter::GetSourceFrame(sint64 outputFrame) {
	return outputFrame;
}

sint64 VDFilterFrameConverter::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	if (source == this)
		return outputFrame;

	return mpSource->GetSymbolicFrame(outputFrame, source);
}

sint64 VDFilterFrameConverter::GetNearestUniqueFrame(sint64 outputFrame) {
	return mpSource->GetNearestUniqueFrame(outputFrame);
}

IVDFilterFrameSource::RunResult VDFilterFrameConverter::RunRequests(const uint32 *batchNumberLimit, int index) {
	if (index>=node_count) return kRunResult_Idle;

	bool activity = false;

	VDFilterFrameConverterNode& node = this->node[index];

	if (node.mpRequest) {
		if (node.mbRequestPending)
			return kRunResult_Blocked;

		node.EndFrame(node.mbRequestSuccess != 0);
		activity = true;
	}

	if (!node.mpRequest) {
		if (!GetNextRequest(batchNumberLimit, ~node.mpRequest))
			return activity ? kRunResult_IdleWasActive : kRunResult_Idle;
	}

	VDFilterFrameBuffer *srcbuf = node.mpRequest->GetSource(0);
	if (!srcbuf) {
		node.mpRequest->MarkComplete(false);
		CompleteRequest(node.mpRequest, false);
		node.mpRequest = NULL;
		return kRunResult_Running;
	}

	if (!AllocateRequestBuffer(node.mpRequest)) {
		node.mpRequest->MarkComplete(false);
		CompleteRequest(node.mpRequest, false);
		node.mpRequest = NULL;
		return kRunResult_Running;
	}

	VDFilterFrameBuffer *dstbuf = node.mpRequest->GetResultBuffer();
	node.mPixmapSrc = VDPixmapFromLayout(mSourceLayout, (void *)srcbuf->LockRead());
	node.mPixmapSrc.info = srcbuf->info;
	node.mPixmapDst = VDPixmapFromLayout(mLayout, dstbuf->LockWrite());

	node.mbRequestPending = true;
	mpEngine->ScheduleProcess(index);

	return activity ? kRunResult_BlockedWasActive : kRunResult_Blocked;
}

bool VDFilterFrameConverter::InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable, uint32 batchNumber) {
	vdrefptr<IVDFilterFrameClientRequest> creq;
	if (!mpSource->CreateRequest(outputFrame, false, batchNumber, ~creq))
		return false;

	req->SetSourceCount(1);
	req->SetSourceRequest(0, creq);
	return true;
}

IVDFilterFrameSource::RunResult VDFilterFrameConverter::RunProcess(int index) {
	if (index>=node_count) return kRunResult_Idle;
	return node[index].RunProcess(); 
}

IVDFilterFrameSource::RunResult VDFilterFrameConverterNode::RunProcess() {
	if (!mbRequestPending)
		return IVDFilterFrameSource::kRunResult_Idle;

	VDPROFILEBEGINEX("Convert", (uint32)mpRequest->GetTiming().mOutputFrame);
	mpBlitter->Blit(mPixmapDst, mPixmapSrc);
	VDPROFILEEND();

	VDFilterFrameBuffer *dstbuf = mpRequest->GetResultBuffer();
	dstbuf->info = mPixmapDst.info;

	mbRequestSuccess = true;
	mbRequestPending = false;
	source->mpEngine->Schedule();
	return IVDFilterFrameSource::kRunResult_IdleWasActive;
}

void VDFilterFrameConverterNode::EndFrame(bool success) {
	VDASSERT(!mbRequestPending);

	if (mpRequest) {
		VDFilterFrameBuffer *srcbuf = mpRequest->GetSource(0);
		VDFilterFrameBuffer *dstbuf = mpRequest->GetResultBuffer();
		dstbuf->Unlock();
		srcbuf->Unlock();
		mpRequest->MarkComplete(success);
		source->CompleteRequest(mpRequest, success);
		mpRequest = NULL;
	}
}

void VDFilterFrameConverterNode::Stop() {
	mbRequestPending = false;
	EndFrame(false);
}


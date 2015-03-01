#include "stdafx.h"
#include <vd2/Tessa/Context.h>
#include "FilterAccelReadbackBuffer.h"

VDFilterAccelReadbackBuffer::VDFilterAccelReadbackBuffer()
	: mpReadbackBuffer(NULL)
	, mpReadbackRT(NULL)
{
}

VDFilterAccelReadbackBuffer::~VDFilterAccelReadbackBuffer() {
	Shutdown();
}

bool VDFilterAccelReadbackBuffer::Init(IVDTContext *ctx, uint32 w, uint32 h, bool yuvMode) {
	uint32 rw = w;
	uint32 rh = h;

	if (yuvMode) {
		rw = (rw + 3) >> 2;
		rh *= 3;
	}

	if (!ctx->CreateReadbackBuffer(rw, rh, kVDTF_B8G8R8A8, &mpReadbackBuffer)) {
		Shutdown();
		return false;
	}

	if (!ctx->CreateSurface(rw, rh, kVDTF_B8G8R8A8, kVDTUsage_Render, &mpReadbackRT)) {
		Shutdown();
		return false;
	}

	return true;
}

void VDFilterAccelReadbackBuffer::Shutdown() {
	if (mpReadbackBuffer) {
		mpReadbackBuffer->Release();
		mpReadbackBuffer = NULL;
	}

	if (mpReadbackRT) {
		mpReadbackRT->Release();
		mpReadbackRT = NULL;
	}
}

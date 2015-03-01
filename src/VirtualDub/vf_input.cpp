//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#include "stdafx.h"
#include "vf_base.h"
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>
#include "VBitmap.h"
#include "VideoSource.h"

class VDVideoFilterInput : public VDVideoFilterBase {
public:
	static void *Create(const VDVideoFilterContext *pContext) {
		return new VDVideoFilterInput(pContext);
	}

	VDVideoFilterInput(const VDVideoFilterContext *pContext) : VDVideoFilterBase(pContext), mpSource((IVDVideoSource *)pContext->mpFilterData) {}

	sint32 Run();
	void Prefetch(sint64 frame);
	sint32 Prepare();
	void Start();
	void Stop();
	unsigned Suspend(void *dst, unsigned size);
	void Resume(const void *src, unsigned size);

protected:
	IVDVideoSource			*mpSource;
};

sint32 VDVideoFilterInput::Run() {
	mpSource->getFrame(mpContext->mpOutput->mFrameNum);

	mpContext->AllocFrame();

	VDPixmapBlt(*mpContext->mpDstFrame->mpPixmap, mpSource->getTargetFormat());

	return kVFVRun_OK;
}

void VDVideoFilterInput::Prefetch(sint64 frame) {
}

sint32 VDVideoFilterInput::Prepare() {
	VDPixmap& outformat = *mpContext->mpOutput->mpFormat;
	const VDAVIBitmapInfoHeader& srcformat = *mpSource->getDecompressedFormat();

	outformat.w			= srcformat.biWidth;
	outformat.h			= srcformat.biHeight;
	outformat.format	= nsVDPixmap::kPixFormat_XRGB8888;

	const VDFraction rate(mpSource->asStream()->getRate());

	mpContext->mpOutput->mFrameRateHi	= rate.getHi();
	mpContext->mpOutput->mFrameRateLo	= rate.getLo();
	mpContext->mpOutput->mLength		= mpSource->asStream()->getLength();
	mpContext->mpOutput->mStart			= 0;

	return 0;
}

void VDVideoFilterInput::Start() {
	mpSource->setDecompressedFormat(32);
}

void VDVideoFilterInput::Stop() {
}

unsigned VDVideoFilterInput::Suspend(void *dst, unsigned size) {
	return 0;
}

void VDVideoFilterInput::Resume(const void *src, unsigned size) {
}

extern const struct VDVideoFilterDefinition vfilterDef_input = {
	sizeof(VDVideoFilterDefinition),
	0,
	0,	1,
	NULL,
	NULL,
	VDVideoFilterInput::Create,
	VDVideoFilterInput::MainProc,
};

extern const VDPluginInfo vpluginDef_input = {
	sizeof(VDPluginInfo),
	L"input",
	NULL,
	L"Reads video from a source.",
	0,
	kVDPluginType_Video,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_VideoAPIVersion,
	kVDPlugin_VideoAPIVersion,

	&vfilterDef_input
};

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterTest : public VDVideoFilterBase {
public:
	static void *Create(const VDVideoFilterContext *pContext) {
		return new VDVideoFilterTest(pContext);
	}

	VDVideoFilterTest(const VDVideoFilterContext *pContext) : VDVideoFilterBase(pContext) {}

	sint32 Run();
	void Prefetch(sint64 frame);
	sint32 Prepare();
	void Start();
	void Stop();
	unsigned Suspend(void *dst, unsigned size);
	void Resume(const void *src, unsigned size);

protected:
};

sint32 VDVideoFilterTest::Run() {
	mpContext->AllocFrame();

	VDPixmapStretchBltBilinear(*mpContext->mpDstFrame->mpPixmap, *mpContext->mpSrcFrames[0]->mpPixmap);

	return kVFVRun_OK;
}

void VDVideoFilterTest::Prefetch(sint64 frame) {
	mpContext->Prefetch(0, frame, 0);
}

sint32 VDVideoFilterTest::Prepare() {
	VDPixmap& informat = *mpContext->mpInputs[0]->mpFormat;
	VDPixmap& outformat = *mpContext->mpOutput->mpFormat;

	outformat = informat;
	outformat.w = (outformat.w * 3) / 2;
	outformat.h = (outformat.h * 3) / 2;

	return 0;
}

void VDVideoFilterTest::Start() {
}

void VDVideoFilterTest::Stop() {
}

unsigned VDVideoFilterTest::Suspend(void *dst, unsigned size) {
	return 0;
}

void VDVideoFilterTest::Resume(const void *src, unsigned size) {
}

extern const struct VDVideoFilterDefinition vfilterDef_test = {
	sizeof(VDVideoFilterDefinition),
	0,
	1,	1,
	NULL,
	NULL,
	VDVideoFilterTest::Create,
	VDVideoFilterTest::MainProc,
};

extern const VDPluginInfo vpluginDef_test = {
	sizeof(VDPluginInfo),
	L"test",
	NULL,
	L"Does a 3:2 stretch.",
	0,
	kVDPluginType_Video,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_VideoAPIVersion,
	kVDPlugin_VideoAPIVersion,

	&vfilterDef_test
};

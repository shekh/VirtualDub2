//	VDXFrame - Helper library for VirtualDub plugins
//	Copyright (C) 2008 Avery Lee
//
//	The plugin headers in the VirtualDub plugin SDK are licensed differently
//	differently than VirtualDub and the Plugin SDK themselves.  This
//	particular file is thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include "stdafx.h"
#include <vd2/VDXFrame/VideoFilter.h>
#include <stdio.h>

///////////////////////////////////////////////////////////////////////////

uint32 VDXVideoFilter::sAPIVersion;
uint32 VDXVideoFilter::FilterModVersion;

VDXVideoFilter::VDXVideoFilter() {
	fma = 0;
}

VDXVideoFilter::~VDXVideoFilter() {
}

void VDXVideoFilter::SetHooks(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	this->fa = fa;
	this->ff = ff;
}

void VDXVideoFilter::SetAPIVersion(uint32 apiVersion) {
	sAPIVersion = apiVersion;
}

void VDXVideoFilter::SetFilterModVersion(uint32 version) {
	FilterModVersion = version;
}

///////////////////////////////////////////////////////////////////////////

vd2::VDXPixmapFormat VDXVideoFilter::ExtractBaseFormat(sint32 format) {
	using namespace vd2;

	switch (format) {
	case kPixFormat_Y8_FR:
		return kPixFormat_Y8;

	case kPixFormat_YUV444_Planar_709_FR:
	case kPixFormat_YUV444_Planar_FR:
	case kPixFormat_YUV444_Planar_709:
		return kPixFormat_YUV444_Planar;

	case kPixFormat_YUV422_Planar_709_FR:
	case kPixFormat_YUV422_Planar_FR:
	case kPixFormat_YUV422_Planar_709:
		return kPixFormat_YUV422_Planar;

	case kPixFormat_YUV420_Planar_709_FR:
	case kPixFormat_YUV420_Planar_FR:
	case kPixFormat_YUV420_Planar_709:
		return kPixFormat_YUV420_Planar;

	case kPixFormat_YUV410_Planar_709_FR:
	case kPixFormat_YUV410_Planar_FR:
	case kPixFormat_YUV410_Planar_709:
		return kPixFormat_YUV410_Planar;

	case kPixFormat_YUV411_Planar_709_FR:
	case kPixFormat_YUV411_Planar_FR:
	case kPixFormat_YUV411_Planar_709:
		return kPixFormat_YUV411_Planar;

	case kPixFormat_YUV422_YUYV_709_FR:
	case kPixFormat_YUV422_YUYV_FR:
	case kPixFormat_YUV422_YUYV_709:
		return kPixFormat_YUV422_YUYV;

	case kPixFormat_YUV422_UYVY_709_FR:
	case kPixFormat_YUV422_UYVY_FR:
	case kPixFormat_YUV422_UYVY_709:
		return kPixFormat_YUV422_UYVY;

	case kPixFormat_YUV420i_Planar_FR:
	case kPixFormat_YUV420i_Planar_709:
	case kPixFormat_YUV420i_Planar_709_FR:
		return kPixFormat_YUV420i_Planar;

	case kPixFormat_YUV420it_Planar_FR:
	case kPixFormat_YUV420it_Planar_709:
	case kPixFormat_YUV420it_Planar_709_FR:
		return kPixFormat_YUV420it_Planar;

	case kPixFormat_YUV420ib_Planar_FR:
	case kPixFormat_YUV420ib_Planar_709:
	case kPixFormat_YUV420ib_Planar_709_FR:
		return kPixFormat_YUV420ib_Planar;
	}

	return (vd2::VDXPixmapFormat)format;
}

vd2::ColorSpaceMode VDXVideoFilter::ExtractColorSpace(sint32 format) {
	using namespace vd2;

	switch (format) {
	case kPixFormat_XRGB1555:
	case kPixFormat_RGB565:
	case kPixFormat_RGB888:
	case kPixFormat_XRGB8888:
	case kPixFormat_XRGB64:
		return kColorSpaceMode_None;

	case kPixFormat_Y8:
	case kPixFormat_Y8_FR:
	case kPixFormat_Y16:
		return kColorSpaceMode_None;

	case kPixFormat_YUV444_Planar_709_FR:
	case kPixFormat_YUV444_Planar_709:
	case kPixFormat_YUV422_Planar_709_FR:
	case kPixFormat_YUV422_Planar_709:
	case kPixFormat_YUV420_Planar_709_FR:
	case kPixFormat_YUV420_Planar_709:
	case kPixFormat_YUV410_Planar_709_FR:
	case kPixFormat_YUV410_Planar_709:
	case kPixFormat_YUV411_Planar_709_FR:
	case kPixFormat_YUV411_Planar_709:
	case kPixFormat_YUV422_YUYV_709_FR:
	case kPixFormat_YUV422_YUYV_709:
	case kPixFormat_YUV422_UYVY_709_FR:
	case kPixFormat_YUV422_UYVY_709:
	case kPixFormat_YUV420i_Planar_709:
	case kPixFormat_YUV420i_Planar_709_FR:
	case kPixFormat_YUV420it_Planar_709:
	case kPixFormat_YUV420it_Planar_709_FR:
	case kPixFormat_YUV420ib_Planar_709:
	case kPixFormat_YUV420ib_Planar_709_FR:
		return kColorSpaceMode_709;
	}

	return kColorSpaceMode_601;
}

vd2::ColorRangeMode VDXVideoFilter::ExtractColorRange(sint32 format) {
	using namespace vd2;

	switch (format) {
	case kPixFormat_XRGB1555:
	case kPixFormat_RGB565:
	case kPixFormat_RGB888:
	case kPixFormat_XRGB8888:
	case kPixFormat_XRGB64:
		return kColorRangeMode_None;

	case kPixFormat_Y8_FR:
		return kColorRangeMode_Full;

	case kPixFormat_YUV444_Planar_709_FR:
	case kPixFormat_YUV444_Planar_FR:
	case kPixFormat_YUV422_Planar_709_FR:
	case kPixFormat_YUV422_Planar_FR:
	case kPixFormat_YUV420_Planar_709_FR:
	case kPixFormat_YUV420_Planar_FR:
	case kPixFormat_YUV410_Planar_709_FR:
	case kPixFormat_YUV410_Planar_FR:
	case kPixFormat_YUV411_Planar_709_FR:
	case kPixFormat_YUV411_Planar_FR:
	case kPixFormat_YUV422_YUYV_709_FR:
	case kPixFormat_YUV422_YUYV_FR:
	case kPixFormat_YUV422_UYVY_709_FR:
	case kPixFormat_YUV422_UYVY_FR:
	case kPixFormat_YUV420i_Planar_FR:
	case kPixFormat_YUV420i_Planar_709_FR:
	case kPixFormat_YUV420it_Planar_FR:
	case kPixFormat_YUV420it_Planar_709_FR:
	case kPixFormat_YUV420ib_Planar_FR:
	case kPixFormat_YUV420ib_Planar_709_FR:
		return kColorRangeMode_Full;
	}

	return kColorRangeMode_Limited;
}

vd2::ColorSpaceMode VDXVideoFilter::ExtractColorSpace(const VDXFBitmap* bitmap) {
	if (fma && fma->fmpixmap) {
		FilterModPixmapInfo* info = fma->fmpixmap->GetPixmapInfo(bitmap->mpPixmap);
		if (info->colorSpaceMode!=vd2::kColorSpaceMode_None) return info->colorSpaceMode;
	}

	return ExtractColorSpace(bitmap->mpPixmapLayout->format);
}

vd2::ColorRangeMode VDXVideoFilter::ExtractColorRange(const VDXFBitmap* bitmap) {
	if (fma && fma->fmpixmap) {
		FilterModPixmapInfo* info = fma->fmpixmap->GetPixmapInfo(bitmap->mpPixmap);
		if (info->colorRangeMode!=vd2::kColorRangeMode_None) return info->colorRangeMode;
	}

	return ExtractColorRange(bitmap->mpPixmapLayout->format);
}

int VDXVideoFilter::ExtractWidth2(sint32 format, sint32 w) {
	using namespace vd2;

	switch (ExtractBaseFormat(format)) {
	case kPixFormat_YUV422_UYVY:
	case kPixFormat_YUV422_YUYV:
	case kPixFormat_YUV422_Planar:
	case kPixFormat_YUV422_Alpha_Planar:
	case kPixFormat_YUV422_V210:
	case kPixFormat_YUV422_Planar16:
	case kPixFormat_YUV422_Alpha_Planar16:
	case kPixFormat_YUV422_P210:
	case kPixFormat_YUV422_P216:
		return (w+1)/2;
	case kPixFormat_YUV420_Planar:
	case kPixFormat_YUV420_Alpha_Planar:
	case kPixFormat_YUV420_NV12:
	case kPixFormat_YUV420i_Planar:
	case kPixFormat_YUV420it_Planar:
	case kPixFormat_YUV420ib_Planar:
	case kPixFormat_YUV420_P010:
	case kPixFormat_YUV420_P016:
	case kPixFormat_YUV420_Planar16:
	case kPixFormat_YUV420_Alpha_Planar16:
		return (w+1)/2;
	}
	return w;
}

int VDXVideoFilter::ExtractHeight2(sint32 format, sint32 w) {
	using namespace vd2;

	switch (ExtractBaseFormat(format)) {
	case kPixFormat_YUV420_Planar:
	case kPixFormat_YUV420_Alpha_Planar:
	case kPixFormat_YUV420_NV12:
	case kPixFormat_YUV420i_Planar:
	case kPixFormat_YUV420it_Planar:
	case kPixFormat_YUV420ib_Planar:
	case kPixFormat_YUV420_P010:
	case kPixFormat_YUV420_P016:
	case kPixFormat_YUV420_Planar16:
	case kPixFormat_YUV420_Alpha_Planar16:
		return (w+1)/2;
	}
	return w;
}

///////////////////////////////////////////////////////////////////////////

bool VDXVideoFilter::Init() {
	return true;
}

void VDXVideoFilter::Start() {
}

void VDXVideoFilter::Run() {
}

void VDXVideoFilter::End() {
}

bool VDXVideoFilter::Configure(VDXHWND hwnd) {
	return hwnd != NULL;
}

void VDXVideoFilter::GetSettingString(char *buf, int maxlen) {
}

void VDXVideoFilter::GetScriptString(char *buf, int maxlen) {
}

int VDXVideoFilter::Serialize(char *buf, int maxbuf) {
	return 0;
}

int VDXVideoFilter::Deserialize(const char *buf, int maxbuf) {
	return 0;
}

sint64 VDXVideoFilter::Prefetch(sint64 frame) {
	return frame;
}

bool VDXVideoFilter::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	prefetcher->PrefetchFrame(0, Prefetch(frame), 0);
	return true;
}

void VDXVideoFilter::StartAccel(IVDXAContext *vdxa) {
}

void VDXVideoFilter::RunAccel(IVDXAContext *vdxa) {
}

void VDXVideoFilter::StopAccel(IVDXAContext *vdxa) {
}

bool VDXVideoFilter::OnEvent(uint32 event, const void *eventData) {
	switch(event) {
		case kVDXVFEvent_InvalidateCaches:
			return OnInvalidateCaches();

		default:
			return false;
	}
}

bool VDXVideoFilter::OnInvalidateCaches() {
	return false;
}

///////////////////////////////////////////////////////////////////////////

void __cdecl VDXVideoFilter::FilterDeinit   (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	(*reinterpret_cast<VDXVideoFilter **>(fa->filter_data))->~VDXVideoFilter();
}

int  __cdecl VDXVideoFilter::FilterRun      (const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);
	pThis->Run();
	return 0;
}

long __cdecl VDXVideoFilter::FilterParam    (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= fa;

	return pThis->GetParams();
}

int  __cdecl VDXVideoFilter::FilterConfig   (VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hwnd) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= fa;

	return !pThis->Configure(hwnd);
}

int  __cdecl VDXVideoFilter::FilterStart    (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= fa;

	if (sAPIVersion >= 15 && fa->mpVDXA)
		pThis->StartAccel(fa->mpVDXA);
	else
		pThis->Start();

	return 0;
}

int  __cdecl VDXVideoFilter::FilterEnd      (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= fa;

	if (sAPIVersion >= 15 && fa->mpVDXA)
		pThis->StopAccel(fa->mpVDXA);
	else
		pThis->End();

	return 0;
}

void __cdecl VDXVideoFilter::FilterString   (const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);

	pThis->GetScriptString(buf, 80);
}

bool __cdecl VDXVideoFilter::FilterScriptStr(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= fa;

	buf[0] = 0;
	pThis->GetScriptString(buf, buflen);

	return buf[0] != 0;
}

void __cdecl VDXVideoFilter::FilterString2  (const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);

	pThis->GetSettingString(buf, maxlen);
}

int  __cdecl VDXVideoFilter::FilterSerialize    (VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxbuf) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= fa;

	return pThis->Serialize(buf, maxbuf);
}

void __cdecl VDXVideoFilter::FilterDeserialize  (VDXFilterActivation *fa, const VDXFilterFunctions *ff, const char *buf, int maxbuf) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= fa;

	pThis->Deserialize(buf, maxbuf);
}

sint64 __cdecl VDXVideoFilter::FilterPrefetch(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, sint64 frame) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);

	return pThis->Prefetch(frame);
}

bool __cdecl VDXVideoFilter::FilterPrefetch2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);

	return pThis->Prefetch2(frame, prefetcher);
}

bool __cdecl VDXVideoFilter::FilterEvent(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, uint32 event, const void *eventData) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);

	return pThis->OnEvent(event, eventData);
}

void  __cdecl VDXVideoFilter::FilterAccelRun(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);

	pThis->RunAccel(fa->mpVDXA);
}

bool VDXVideoFilter::StaticAbout(VDXHWND parent) {
	return false;
}

bool VDXVideoFilter::StaticConfigure(VDXHWND parent) {
	return false;
}

void  __cdecl VDXVideoFilter::FilterModActivate(FilterModActivation *fma, const VDXFilterFunctions *ff) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fma->filter_data);

	pThis->fma = fma;
}

long __cdecl VDXVideoFilter::FilterModParam(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXVideoFilter *pThis = *reinterpret_cast<VDXVideoFilter **>(fa->filter_data);

	pThis->fa		= fa;

	return pThis->GetFilterModParams();
}

void VDXVideoFilter::SafePrintf(char *buf, int maxbuf, const char *format, ...) {
	if (maxbuf <= 0)
		return;

	va_list val;
	va_start(val, format);
	if ((unsigned)_vsnprintf(buf, maxbuf, format, val) >= (unsigned)maxbuf)
		buf[maxbuf - 1] = 0;
	va_end(val);
}

const VDXScriptFunctionDef VDXVideoFilter::sScriptMethods[1]={0};

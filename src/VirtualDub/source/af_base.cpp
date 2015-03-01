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
#include "af_base.h"

uint32 VDAudioFilterBase::Run() {
	return kVFARun_Finished;
}

sint64 VDAudioFilterBase::Seek(sint64 microsecs) {
	return microsecs;
}

uint32 VDAudioFilterBase::Prepare() {
	return 0;
}

void VDAudioFilterBase::Start() {
}

void VDAudioFilterBase::Stop() {
}

unsigned VDAudioFilterBase::Suspend(void *dst, unsigned size) {
	return 0;
}

void VDAudioFilterBase::Resume(const void *src, unsigned size) {
}

const nsVDAudioFilterBase::ConfigEntryExt *VDAudioFilterBase::GetParamEntry(const unsigned idx) {
	const VDXPluginConfigEntry *pEnt = mpContext->mpDefinition->mpConfigInfo;

	if (pEnt)
		for(; pEnt->next; pEnt=pEnt->next) {
			if (pEnt->idx == idx)
				return (nsVDAudioFilterBase::ConfigEntryExt *)(pEnt);
		}

	return NULL;
}

unsigned VDAudioFilterBase::GetParam(unsigned idx, void *dst, unsigned size) {
	const nsVDAudioFilterBase::ConfigEntryExt *pEnt = GetParamEntry(idx);

	if (pEnt) {
		char *ptr = (char *)GetConfigPtr() + pEnt->objoffset;

		using namespace nsVDAudioFilterBase;
		unsigned l;

		switch(pEnt->info.type) {
		case VDXPluginConfigEntry::kTypeU32:
			if (size >= sizeof(uint32))
				*(Type_U32 *)dst = *(Type_U32 *)ptr;
			return sizeof(uint32);
		case VDXPluginConfigEntry::kTypeS32:
			if (size >= sizeof(sint32))
				*(Type_S32 *)dst = *(Type_S32 *)ptr;
			return sizeof(sint32);
		case VDXPluginConfigEntry::kTypeU64:
			if (size >= sizeof(uint64))
				*(Type_U64 *)dst = *(Type_U64 *)ptr;
			return sizeof(uint64);
		case VDXPluginConfigEntry::kTypeS64:
			if (size >= sizeof(sint64))
				*(Type_S64 *)dst = *(Type_S64 *)ptr;
			return sizeof(sint64);
		case VDXPluginConfigEntry::kTypeDouble:
			if (size >= sizeof(double))
				*(Type_Double *)dst = *(Type_Double *)ptr;
			return sizeof(double);
		case VDXPluginConfigEntry::kTypeAStr:
			{
				Type_AStr& str = *(Type_AStr *)ptr;
				l = str.size() + 1;
				if (size >= l)
					memcpy(dst, str.c_str(), l);
				return l;
			}

		case VDXPluginConfigEntry::kTypeWStr:
			{
				Type_WStr& str = *(Type_WStr *)ptr;
				l = (str.size() + 1) * sizeof(wchar_t);
				if (size >= l)
					memcpy(dst, str.c_str(), l);
				return l;
			}

		case VDXPluginConfigEntry::kTypeBlock:
			{
				Type_Block& blk = *(Type_Block *)ptr;
				if (size >= blk.size())
					memcpy(dst, &blk[0], size);
				return blk.size();
			}

		}
	}

	return 0;
}

void VDAudioFilterBase::SetParam(unsigned idx, const void *src, unsigned size) {
	const nsVDAudioFilterBase::ConfigEntryExt *pEnt = GetParamEntry(idx);

	if (pEnt) {
		char *ptr = (char *)GetConfigPtr() + pEnt->objoffset;

		using namespace nsVDAudioFilterBase;

		switch(pEnt->info.type) {
		case VDXPluginConfigEntry::kTypeU32:		*(Type_U32 *)ptr = *(Type_U32 *)src; break;
		case VDXPluginConfigEntry::kTypeS32:		*(Type_S32 *)ptr = *(Type_S32 *)src; break;
		case VDXPluginConfigEntry::kTypeU64:		*(Type_U64 *)ptr = *(Type_U64 *)src; break;
		case VDXPluginConfigEntry::kTypeS64:		*(Type_S64 *)ptr = *(Type_S64 *)src; break;
		case VDXPluginConfigEntry::kTypeDouble:	*(Type_Double *)ptr = *(Type_Double *)src; break;
		case VDXPluginConfigEntry::kTypeAStr: 	*(Type_AStr *)ptr = (const char *)src; break;
		case VDXPluginConfigEntry::kTypeWStr: 	*(Type_WStr *)ptr = (const wchar_t *)src; break;
		case VDXPluginConfigEntry::kTypeBlock:
			{
				Type_Block& blk = *(Type_Block *)ptr;
				blk.resize(size);
				memcpy(&blk[0], src, size);
			}
			break;
		}
	}
}

bool VDAudioFilterBase::Config(HWND hwnd) {
	return false;
}

uint32 VDAudioFilterBase::Read(unsigned pin, void *dst, uint32 samples) {
	return 0;
}

///////////////////////////////////////////////////////////////////////////

void VDAPIENTRY VDAudioFilterBase::DestroyProc(const VDAudioFilterContext *pContext) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	((VDAudioFilterBase *)pContext->mpFilterData)->~VDAudioFilterBase();
}

uint32 VDAPIENTRY VDAudioFilterBase::RunProc(const VDAudioFilterContext *pContext) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	return ((VDAudioFilterBase *)pContext->mpFilterData)->Run();
}

sint64 VDAPIENTRY VDAudioFilterBase::SeekProc(const VDAudioFilterContext *pContext, sint64 microsecs) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	return ((VDAudioFilterBase *)pContext->mpFilterData)->Seek(microsecs);
}

uint32 VDAPIENTRY VDAudioFilterBase::PrepareProc(const VDAudioFilterContext *pContext) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	return ((VDAudioFilterBase *)pContext->mpFilterData)->Prepare();
}

void VDAPIENTRY VDAudioFilterBase::StartProc(const VDAudioFilterContext *pContext) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	((VDAudioFilterBase *)pContext->mpFilterData)->Start();
}

void VDAPIENTRY VDAudioFilterBase::StopProc(const VDAudioFilterContext *pContext) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	((VDAudioFilterBase *)pContext->mpFilterData)->Stop();
}

unsigned VDAPIENTRY VDAudioFilterBase::SuspendProc(const VDAudioFilterContext *pContext, void *dst, unsigned size) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	return ((VDAudioFilterBase *)pContext->mpFilterData)->Suspend(dst, size);
}

void VDAPIENTRY VDAudioFilterBase::ResumeProc(const VDAudioFilterContext *pContext, const void *src, unsigned size) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	((VDAudioFilterBase *)pContext->mpFilterData)->Resume(src, size);
}

unsigned VDAPIENTRY VDAudioFilterBase::GetParamProc(const VDAudioFilterContext *pContext, unsigned idx, void *dst, unsigned size) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	return ((VDAudioFilterBase *)pContext->mpFilterData)->GetParam(idx, dst, size);
}

void VDAPIENTRY VDAudioFilterBase::SetParamProc(const VDAudioFilterContext *pContext, unsigned idx, const void *src, unsigned size) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	((VDAudioFilterBase *)pContext->mpFilterData)->SetParam(idx, src, size);
}

bool VDAPIENTRY VDAudioFilterBase::ConfigProc(const VDAudioFilterContext *pContext, HWND hwnd) {
	((VDAudioFilterBase *)pContext->mpFilterData)->mpContext = pContext;
	return ((VDAudioFilterBase *)pContext->mpFilterData)->Config(hwnd);
}

///////////////////////////////////////////////////////////////////////////

const VDAudioFilterVtbl VDAudioFilterBase::sVtbl = {
	sizeof(VDAudioFilterVtbl),
	DestroyProc,
	PrepareProc,
	StartProc,
	StopProc,
	RunProc,
	SeekProc,
	SuspendProc,
	ResumeProc,
	GetParamProc,
	SetParamProc,
	ConfigProc,
};

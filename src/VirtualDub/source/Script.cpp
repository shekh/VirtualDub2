//	VirtualDub - Video processing and capture application
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

#include "stdafx.h"

#include <windows.h>
#include <mmsystem.h>
#include <vfw.h>

#include "indeo_if.h"

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"
#include <vd2/system/error.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/file.h>
#include <vd2/system/log.h>
#include <vd2/system/VDString.h>
#include <vd2/system/filesys.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Dita/services.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/VDLib/ParameterCurve.h>
#include "AVIOutputCLI.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "AudioFilterSystem.h"
#include "InputFile.h"
#include "project.h"
#include "filters.h"
#include "FilterInstance.h"
#include "AVIOutputPlugin.h"

#include "command.h"
#include "dub.h"
#include "DubOutput.h"
#include "gui.h"
#include "prefs.h"
#include "resource.h"
#include "script.h"
#include "plugins.h"
#include "oshelper.h"

extern DubOptions g_dubOpts;
extern HWND g_hWnd;

extern const char g_szError[];
extern bool g_fJobMode;
extern int g_returnCode;

extern VDProject *g_project;

extern HINSTANCE g_hInst;
extern std::list<class VDExternalModule *>		g_pluginModules;

extern vdrefptr<IVDVideoSource>	inputVideo;
extern vdrefptr<AudioSource> inputAudio;
extern COMPVARS2 g_Vcompression;

extern const char *VDGetStartupArgument(int index);

static VDScriptValue RootHandler(IVDScriptInterpreter *isi, char *szName, void *lpData);

extern bool VDPreferencesGetBatchShowStatusWindow();
extern IVDInputDriver *VDCreateInputDriverTest();

///////////////////////////////////////////////

void RunScript(const wchar_t *name, void *hwnd) {
	static const wchar_t fileFilters[]=
				L"All scripts\0"							L"*.vdscript;*.vcf;*.syl;*.jobs;*.vdproject\0"
				L"VirtualDub configuration file\0"			L"*.vcf\0"
				L"VirtualDub script\0"						L"*.vdscript;*.syl\0"
				L"VirtualDub job queue\0"					L"*.jobs\0"
				L"VirtualDub project\0"						L"*.vdproject\0"
				L"All Files (*.*)\0"						L"*.*\0"
				;

	VDStringW filenameW;

	if (!name) {
		filenameW = VDGetLoadFileName(VDFSPECKEY_SCRIPT, (VDGUIHandle)hwnd, L"Load configuration script", fileFilters, L"vdscript", 0, 0);

		if (filenameW.empty())
			return;

		name = filenameW.c_str();
	}

	const char *line = NULL;
	int lineno = 1;

	VDTextInputFile f(name);

	vdautoptr<IVDScriptInterpreter> isi(VDCreateScriptInterpreter());

	g_project->BeginTimelineUpdate();

	try {
		isi->SetRootHandler(RootHandler, NULL);

		while(line = f.GetNextLine())
			isi->ExecuteLine(line);
	} catch(const VDScriptError& cse) {
		int pos = isi->GetErrorLocation();
		int prelen = std::min<int>(pos, 50);
		const char *s = line ? line : "";

		throw MyError("Error during script execution at line %d, column %d: %s\n\n"
						"    %.*s<!>%.50s"
					, lineno
					, pos+1
					, isi->TranslateScriptError(cse)
					, prelen
					, s + pos - prelen
					, s + pos);
	}

	g_project->EndTimelineUpdate();
	g_project->UpdateFilterList();
}

void RunProject(const wchar_t *name, void *hwnd) {
	static const wchar_t projectFilters[]=
				L"Project scripts\0"						L"*.jobs;*.vdproject\0"
				L"VirtualDub job queue\0"					L"*.jobs\0"
				L"VirtualDub project\0"						L"*.vdproject\0"
				L"All Files (*.*)\0"						L"*.*\0"
				;

	VDStringW filenameW;

	if (!name) {
		filenameW = VDGetLoadFileName('proj', (VDGUIHandle)hwnd, L"Load Project", projectFilters, L"vdproject", 0, 0);

		if (filenameW.empty())
			return;

		name = filenameW.c_str();
	}

	g_project->OpenProject(name);
}

void RunScriptMemory(const char *mem, bool stopAtReloadMarker) {
	g_project->BeginLoading();
	g_project->CloseAVI();
	g_project->CloseWAV();

	vdautoptr<IVDScriptInterpreter> isi(VDCreateScriptInterpreter());
	const char *errorLineStart = mem;
	int errorLine = 1;

	try {
		vdfastvector<char> linebuffer;
		const char *s = mem, *t;

		isi->SetRootHandler(RootHandler, NULL);

		while(*s) {
			t = s;
			while(*t && *t!='\n') ++t;

			// check for reload marker
			if (stopAtReloadMarker) {
				const char *u = s;
				while(isspace((unsigned char)*u))
					++u;

				if (u[0] == '/' && u[1] == '/' && strstr(s, "$reloadstop"))
					break;
			}

			errorLineStart = s;

			linebuffer.resize(t+1-s);
			memcpy(linebuffer.data(), s, t-s);
			linebuffer[t-s] = 0;

			isi->ExecuteLine(linebuffer.data());

			s = t;
			if (*s=='\n') ++s;
			++errorLine;
		}

	} catch(const VDScriptError& cse) {
		int pos = isi->GetErrorLocation();
		int prelen = std::min<int>(pos, 50);

		throw MyError("Error during script execution at line %d, column %d: %s\n\n"
						"    %.*s<!>%.50s"
					, errorLine
					, pos+1
					, isi->TranslateScriptError(cse)
					, prelen
					, errorLineStart + pos - prelen
					, errorLineStart + pos);
					
	} catch(const MyError& e) {
		g_project->EndLoading();
		throw e;
	}

	g_project->EndLoading();
}

///////////////////////////////////////////////////////////////////////////
//
//	General script-handler helper routines.
//
///////////////////////////////////////////////////////////////////////////

bool strfuzzycompare(const char *s, const char *t) {
	char c,d;

	// Collapse spaces to one, convert symbols to _, and letters to uppercase...

	do {
		c = *s++;

		if (isalpha((unsigned char)c))
			c=(char)toupper((unsigned char)c);

		else if (isspace((unsigned char)c)) {
			c = ' ';
			while(*s && isspace((unsigned char)*s)) ++s;
		} else if (c)
			c = '_';

		d = *t++;

		if (isalpha((unsigned char)d))
			d=(char)toupper((unsigned char)d);
		else if (isspace((unsigned char)d)) {
			d = ' ';
			while(*t && isspace((unsigned char)*t)) ++t;
		} else if (d)
			d = '_';

		if (c!=d) break;
	} while(c && d);

	return c==d;
}

static char base64[]=
	"ABCDEFGHIJKLMNOP"
	"QRSTUVWXYZabcdef"
	"ghijklmnopqrstuv"
	"wxyz0123456789+/"
	"=";

long memunbase64(char *t, const char *s, long cnt) {
	char *t0 = t;
	char c1, c2, c3, c4, *ind, *limit = t+cnt;
	long v;

	for(;;) {
		while((c1=*s++) && !(ind = strchr(base64,c1)));
		if (!c1) break;
		c1 = (char)(ind-base64);

		while((c2=*s++) && !(ind = strchr(base64,c2)));
		if (!c2) break;
		c2 = (char)(ind-base64);

		while((c3=*s++) && !(ind = strchr(base64,c3)));
		if (!c3) break;
		c3 = (char)(ind-base64);

		while((c4=*s++) && !(ind = strchr(base64,c4)));
		if (!c4) break;
		c4 = (char)(ind-base64);

		// [c1,c4] valid -> 24 bits (3 bytes)
		// [c1,c3] valid -> 18 bits (2 bytes)
		// [c1,c2] valid -> 12 bits (1 byte)
		// [c1] valid    ->  6 bits (0 bytes)

		v = ((c1 & 63)<<18) | ((c2 & 63)<<12) | ((c3 & 63)<<6) | (c4 & 63);

		if (c1!=64 && c2!=64) {
			*t++ = (char)(v >> 16);

			if (t >= limit)
				break;

			if (c3!=64) {
				*t++ = (char)(v >> 8);
				if (t >= limit)
					break;

				if (c4!=64) {
					*t++ = (char)v;
					if (t >= limit)
						break;

					continue;
				}
			}
		}
		break;
	}

	return t-t0;
}

void membase64(char *t, const char *s, long l) {

	unsigned char c1, c2, c3;

	while(l>0) {
		c1 = (unsigned char)s[0];
		if (l>1) {
			c2 = (unsigned char)s[1];
			if (l>2)
				c3 = (unsigned char)s[2];
		}

		t[0] = base64[(c1 >> 2) & 0x3f];
		if (l<2) {
			t[1] = base64[(c1<<4)&0x3f];
			t[2] = t[3] = '=';
		} else {
			t[1] = base64[((c1<<4)|(c2>>4))&0x3f];
			if (l<3) {
				t[2] = base64[(c2<<2)&0x3f];
				t[3] = '=';
			} else {
				t[2] = base64[((c2<<2) | (c3>>6))&0x3f];
				t[3] = base64[c3 & 0x3f];
			}
		}

		l-=3;
		t+=4;
		s+=3;
	}
	*t=0;
}

///////////////////////////////////////////////////////////////////////////
//
//	Object: VDParameterCurve
//
///////////////////////////////////////////////////////////////////////////

static void func_VDParameterCurve_AddPoint(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDParameterCurve *obj = (VDParameterCurve *)argv[-1].asObjectPtr();
	VDParameterCurve::PointList& pts = obj->Points();
	VDParameterCurvePoint pt;

	pt.mX = argv[0].asDouble();
	pt.mY = argv[1].asDouble();
	pt.mbLinear = argv[2].asInt() != 0;

	VDParameterCurve::PointList::iterator it(obj->UpperBound(argv[0].asDouble()));

	pts.insert(it, pt);
}

static const VDScriptFunctionDef obj_VDParameterCurve_functbl[]={
	{ func_VDParameterCurve_AddPoint, "AddPoint", "0ddi" },
	{ NULL }
};

static const VDScriptObject obj_VDParameterCurve={
	"VDParameterCurve", NULL, obj_VDParameterCurve_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.video.filters
//
///////////////////////////////////////////////////////////////////////////

static void func_VDVFiltInst_Remove(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	for(VDFilterChainDesc::Entries::iterator it(g_filterChain.mEntries.begin()), itEnd(g_filterChain.mEntries.end());
		it != itEnd;
		++it)
	{
		if (*it == ent) {
			g_project->StopFilters();
			g_filterChain.mEntries.erase(it);
			ent->Release();
			break;
		}
	}
}

static void func_VDVFiltInst_SetClipping(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	int x1 = std::max<int>(0, argv[0].asInt());
	int y1 = std::max<int>(0, argv[1].asInt());
	int x2 = std::max<int>(0, argv[2].asInt());
	int y2 = std::max<int>(0, argv[3].asInt());

	bool precise = true;
	if (argc >= 5)
		precise = (0 != argv[4].asInt());

	ent->mpInstance->SetCrop(x1, y1, x2, y2, precise);
}

static void func_VDVFiltInst_GetClipping(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	const vdrect32& r = ent->mpInstance->GetCropInsets();

	switch(argv[0].asInt()) {
	case 0:	argv[0] = r.left; break;
	case 1: argv[0] = r.top; break;
	case 2: argv[0] = r.right; break;
	case 3: argv[0] = r.bottom; break;
	case 4: argv[0] = ent->mpInstance->IsPreciseCroppingEnabled(); break;
	default:
		argv[0] = VDScriptValue(0);
	}
}

static void func_VDVFiltInst_SetOpacityClipping(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	int x1 = std::max<int>(0, argv[0].asInt());
	int y1 = std::max<int>(0, argv[1].asInt());
	int x2 = std::max<int>(0, argv[2].asInt());
	int y2 = std::max<int>(0, argv[3].asInt());

	ent->mpInstance->SetOpacityCrop(x1, y1, x2, y2);
}

static void func_VDVFiltInst_AddOpacityCurve(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	VDParameterCurve *curve = new VDParameterCurve;
	curve->SetYRange(0, 1);
	ent->mpInstance->SetAlphaParameterCurve(curve);

	argv[0] = VDScriptValue(curve, &obj_VDParameterCurve);
}

static void func_VDVFiltInst_SetEnabled(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	ent->mpInstance->SetEnabled(argv[0].asInt() != 0);
}

static void func_VDVFiltInst_SetForceSingleFBEnabled(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	ent->mpInstance->SetForceSingleFBEnabled(argv[0].asInt() != 0);
}

static void func_VDVFiltInst_SetOutputName(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	ent->mOutputName = *argv[0].asString();
}

static void func_VDVFiltInst_AddInput(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	ent->mSources.push_back_as(*argv[0].asString());
}

static void func_VDVFiltInst_DataPrefix(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();

	ent->mpInstance->fmProject.dataPrefix = *argv[0].asString();
}

static VDScriptValue obj_VDVFiltInst_lookup(IVDScriptInterpreter *isi, const VDScriptObject *thisPtr, void *lpVoid, char *szName) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)lpVoid;
	FilterInstance *pfi = ent->mpInstance;

	if (!(strcmp(szName, "__srcwidth"))) {
		g_project->PrepareFilters();
		return VDScriptValue((int)pfi->GetSourceDesc().mLayout.w);
	}

	if (!(strcmp(szName, "__srcheight"))) {
		g_project->PrepareFilters();
		return VDScriptValue((int)pfi->GetSourceDesc().mLayout.h);
	}

	if (!(strcmp(szName, "__srcrate"))) {
		g_project->PrepareFilters();
		return VDScriptValue(pfi->GetSourceDesc().mFrameRate.asDouble());
	}

	if (!(strcmp(szName, "__srcframes"))) {
		g_project->PrepareFilters();
		return VDScriptValue((sint64)pfi->GetSourceDesc().mFrameCount);
	}

	if (!(strcmp(szName, "__dstwidth"))) {
		g_project->PrepareFilters();
		return VDScriptValue((int)pfi->GetOutputFrameWidth());
	}

	if (!(strcmp(szName, "__dstheight"))) {
		g_project->PrepareFilters();
		return VDScriptValue((int)pfi->GetOutputFrameHeight());
	}

	if (!(strcmp(szName, "__dstrate"))) {
		g_project->PrepareFilters();
		return VDScriptValue(pfi->GetOutputFrameRate().asDouble());
	}

	if (!(strcmp(szName, "__dstframes"))) {
		g_project->PrepareFilters();
		return VDScriptValue((sint64)pfi->GetOutputFrameCount());
	}

	const VDScriptObject *scriptObj = pfi->GetScriptObject();
	if (scriptObj)
		return isi->LookupObjectMember(scriptObj, lpVoid, szName);

	return VDScriptValue();
}

static const VDScriptFunctionDef obj_VDVFiltInst_functbl[]={
	{ func_VDVFiltInst_Remove			, "Remove", "0" },
	{ func_VDVFiltInst_SetClipping		, "SetClipping", "0iiii" },
	{ func_VDVFiltInst_SetClipping		, NULL, "0iiiii" },
	{ func_VDVFiltInst_GetClipping		, "GetClipping", "ii" },
	{ func_VDVFiltInst_SetOpacityClipping, "SetOpacityClipping", "0iiii" },
	{ func_VDVFiltInst_AddOpacityCurve	, "AddOpacityCurve", "v" },
	{ func_VDVFiltInst_SetEnabled		, "SetEnabled", "0i" },
	{ func_VDVFiltInst_SetForceSingleFBEnabled, "SetForceSingleFBEnabled", "0i" },
	{ func_VDVFiltInst_SetOutputName	, "SetOutputName", "0s" },
	{ func_VDVFiltInst_AddInput			, "AddInput", "0s" },
	{ func_VDVFiltInst_DataPrefix		, "DataPrefix", "0s" },
	{ NULL }
};

extern const VDScriptObject obj_VDVFiltInst={
	"VDVideoFilterInstance", obj_VDVFiltInst_lookup, obj_VDVFiltInst_functbl, NULL	
};

///////////////////

static void func_VDVFilters_instance(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	int index = argv[0].asInt();

	VDFilterChainDesc& desc = g_filterChain;

	if (index < 0 || (uint32)index >= desc.mEntries.size()) {
		VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);
	}

	argv[0] = VDScriptValue(desc.mEntries[index], &obj_VDVFiltInst);
}

static const VDScriptFunctionDef obj_VDVFilters_instance_functbl[]={
	{ func_VDVFilters_instance		, "[]", "vi" },
	{ NULL }
};

static const VDScriptObject obj_VDVFilters_instance={
	"VDVideoFilterList", NULL, obj_VDVFilters_instance_functbl, NULL	
};

static void func_VDVFilters_Clear(IVDScriptInterpreter *, VDScriptValue *, int) {
	g_project->StopFilters();

	g_filterChain.Clear();
}

static void func_VDVFilters_Add(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	std::list<FilterBlurb>	filterList;

	FilterEnumerateFilters(filterList);

	g_project->StopFilters();

	const char *name = *argv[0].asString();

	for(std::list<FilterBlurb>::const_iterator it(filterList.begin()), itEnd(filterList.end()); it!=itEnd; ++it) {
		const FilterBlurb& fb = *it;

		if (strfuzzycompare(fb.name.c_str(), name)) {
			vdrefptr<VDFilterChainEntry> ent(new_nothrow VDFilterChainEntry);
			if (!ent) VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

			vdrefptr<FilterInstance> fa(new_nothrow FilterInstance(fb.key));
			if (!fa) VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

			ent->mpInstance = fa;

			g_filterChain.AddEntry(ent);

			argv[0] = (int)g_filterChain.mEntries.size() - 1;
			return;
		}
	}

	throw MyError("Cannot add filter '%s': no such filter loaded", *argv[0].asString());
}

static void func_VDVFilters_BeginUpdate(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	g_project->BeginFilterUpdates();
}

static void func_VDVFilters_EndUpdate(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	g_project->EndFilterUpdates();
}

static VDScriptValue obj_VDVFilters_lookup(IVDScriptInterpreter *isi, const VDScriptObject *thisPtr, void *lpVoid, char *szName) {
	if (!strcmp(szName, "instance"))
		return VDScriptValue(lpVoid, &obj_VDVFilters_instance);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptFunctionDef obj_VDVFilters_functbl[]={
	{ func_VDVFilters_Clear			, "Clear", "0" },
	{ func_VDVFilters_Add			, "Add", "is", },
	{ func_VDVFilters_BeginUpdate	, "BeginUpdate", "0" },
	{ func_VDVFilters_EndUpdate		, "EndUpdate", "0" },
	{ NULL }
};

static const VDScriptObject obj_VDVFilters={
	"VDVideoFilters", obj_VDVFilters_lookup, obj_VDVFilters_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.video
//
///////////////////////////////////////////////////////////////////////////

static void func_VDVideo_GetDepth(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int format;

	if (arglist[0].asInt())
		format = g_dubOpts.video.mOutputFormat;
	else
		format = g_dubOpts.video.mInputFormat;

	switch(format) {
		case nsVDPixmap::kPixFormat_XRGB1555:
		case nsVDPixmap::kPixFormat_RGB565:
			arglist[0] = VDScriptValue(16);
			break;
		default:
			arglist[0] = VDScriptValue(24);
			break;
	}
}

static void func_VDVideo_SetDepth(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int new_depth1, new_depth2;

	switch(arglist[0].asInt()) {
	case 16:	new_depth1 = nsVDPixmap::kPixFormat_XRGB1555; break;
	case 24:	new_depth1 = nsVDPixmap::kPixFormat_RGB888; break;
	case 32:	new_depth1 = nsVDPixmap::kPixFormat_XRGB8888; break;
	default:
		return;
	}

	switch(arglist[1].asInt()) {
	case 16:	new_depth2 = nsVDPixmap::kPixFormat_XRGB1555; break;
	case 24:	new_depth2 = nsVDPixmap::kPixFormat_RGB888; break;
	case 32:	new_depth2 = nsVDPixmap::kPixFormat_XRGB8888; break;
	default:
		return;
	}

	g_dubOpts.video.mInputFormat = new_depth1;
	g_dubOpts.video.mOutputFormat = new_depth2;
	g_project->MarkTimelineRateDirty();
}

static void func_VDVideo_SetInputFormat(IVDScriptInterpreter *, VDScriptValue *argv, int argc) {
	g_dubOpts.video.mInputFormat = argv[0].asInt();
	if (g_dubOpts.video.mInputFormat >= nsVDPixmap::kPixFormat_Max_Standard)
		g_dubOpts.video.mInputFormat = nsVDPixmap::kPixFormat_RGB888;
	g_project->MarkTimelineRateDirty();
}

static void func_VDVideo_SetOutputFormat(IVDScriptInterpreter *, VDScriptValue *argv, int argc) {
	g_dubOpts.video.mOutputFormat = argv[0].asInt();
	if (g_dubOpts.video.mOutputFormat >= nsVDPixmap::kPixFormat_Max_Standard)
		g_dubOpts.video.mOutputFormat = nsVDPixmap::kPixFormat_RGB888;
	g_project->MarkTimelineRateDirty();
}

static void func_VDVideo_SetInputMatrix(IVDScriptInterpreter *, VDScriptValue *argv, int argc) {
	int colorSpace = argv[0].asInt();
	int colorRange = argv[1].asInt();
	if (colorSpace >= nsVDXPixmap::kColorSpaceModeCount)	colorSpace = nsVDXPixmap::kColorSpaceMode_None;
	if (colorRange >= nsVDXPixmap::kColorRangeModeCount)	colorRange = nsVDXPixmap::kColorRangeMode_None;
	g_dubOpts.video.mInputFormat.colorSpaceMode = (nsVDXPixmap::ColorSpaceMode)colorSpace;
	g_dubOpts.video.mInputFormat.colorRangeMode = (nsVDXPixmap::ColorRangeMode)colorRange;
	g_project->MarkTimelineRateDirty();
}

static void func_VDVideo_SetOutputMatrix(IVDScriptInterpreter *, VDScriptValue *argv, int argc) {
	int colorSpace = argv[0].asInt();
	int colorRange = argv[1].asInt();
	if (colorSpace >= nsVDXPixmap::kColorSpaceModeCount)	colorSpace = nsVDXPixmap::kColorSpaceMode_None;
	if (colorRange >= nsVDXPixmap::kColorRangeModeCount)	colorRange = nsVDXPixmap::kColorRangeMode_None;
	g_dubOpts.video.mOutputFormat.colorSpaceMode = (nsVDXPixmap::ColorSpaceMode)colorSpace;
	g_dubOpts.video.mOutputFormat.colorRangeMode = (nsVDXPixmap::ColorRangeMode)colorRange;
	g_project->MarkTimelineRateDirty();
}

static void func_VDVideo_GetMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = VDScriptValue(g_dubOpts.video.mode);
}

static void func_VDVideo_SetMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int new_mode = arglist[0].asInt();

	if (new_mode>=0 && new_mode<4) {
		g_dubOpts.video.mode = (char)new_mode;
		g_project->MarkTimelineRateDirty();
	}
}

static void func_VDVideo_SetSmartRendering(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.mbUseSmartRendering = !!arglist[0].asInt();
}

static void func_VDVideo_GetSmartRendering(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = (int)g_dubOpts.video.mbUseSmartRendering;
}

static void func_VDVideo_SetPreserveEmptyFrames(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.mbPreserveEmptyFrames = !!arglist[0].asInt();
}

static void func_VDVideo_GetPreserveEmptyFrames(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = (int)g_dubOpts.video.mbPreserveEmptyFrames;
}

static void func_VDVideo_GetFrameRate(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0: arglist[0] = VDScriptValue(g_dubOpts.video.frameRateDecimation); return;
	case 1:
		if (g_dubOpts.video.mFrameRateAdjustLo)
			arglist[0] = VDScriptValue((int)VDRoundToInt((double)g_dubOpts.video.mFrameRateAdjustLo / (double)g_dubOpts.video.mFrameRateAdjustHi * 1000000.0));
		else if (g_dubOpts.video.mFrameRateAdjustHi == DubVideoOptions::kFrameRateAdjustSameLength)
			arglist[0] = VDScriptValue(-1);
		else
			arglist[0] = VDScriptValue(0);
		return;
	case 2:
		arglist[0] = VDScriptValue(0);		// was IVTC enable
		return;
	}
	arglist[0] = VDScriptValue(0);
}

static void func_VDVideo_SetFrameRate(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int newrate = arglist[0].asInt();
	if (newrate < 0) {
		g_dubOpts.video.mFrameRateAdjustHi = DubVideoOptions::kFrameRateAdjustSameLength;
		g_dubOpts.video.mFrameRateAdjustLo = 0;
	} else if (newrate > 0) {
		g_dubOpts.video.mFrameRateAdjustHi = 1000000;
		g_dubOpts.video.mFrameRateAdjustLo = newrate;
	} else {
		g_dubOpts.video.mFrameRateAdjustHi = 0;
		g_dubOpts.video.mFrameRateAdjustLo = 0;
	}

	g_dubOpts.video.frameRateDecimation = std::max<int>(1, arglist[1].asInt());
	g_dubOpts.video.frameRateTargetLo = 0;
	g_dubOpts.video.frameRateTargetHi = 0;

	if (arg_count > 2) {
		if (arglist[2].asInt())
			throw MyError("Inverse telecine (IVTC) is no longer supported as a pipeline parameter and has been moved to a video filter.");
	}

	g_project->MarkTimelineRateDirty();
}

static void func_VDVideo_SetFrameRate2(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.mFrameRateAdjustHi = (uint32)arglist[0].asLong();
	g_dubOpts.video.mFrameRateAdjustLo = (uint32)arglist[1].asLong();
	g_dubOpts.video.frameRateDecimation = std::max<int>(1, arglist[2].asInt());
	g_dubOpts.video.frameRateTargetLo = 0;
	g_dubOpts.video.frameRateTargetHi = 0;
	g_project->MarkTimelineRateDirty();
}

static void func_VDVideo_SetTargetFrameRate(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.video.frameRateDecimation = 1;
	g_dubOpts.video.frameRateTargetLo = arglist[1].asInt();
	g_dubOpts.video.frameRateTargetHi = arglist[0].asInt();
	g_project->MarkTimelineRateDirty();
}

static void func_VDVideo_GetRange(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	if (!inputVideo)
		VDSCRIPT_EXT_ERROR(FCALL_OUT_OF_RANGE);

	const VDPosition timelineLen = g_project->GetTimeline().GetLength();
	const VDFraction& frameRate = inputVideo->asStream()->getRate();

	arglist[0] = VDScriptValue(arglist[0].asInt() ? g_dubOpts.video.mSelectionEnd.ResolveToMS(timelineLen, frameRate, true) : g_dubOpts.video.mSelectionStart.ResolveToMS(timelineLen, frameRate, false));
}

static void func_VDVideo_SetRangeEmpty(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_project->ClearSelection();
}

static void func_VDVideo_SetRange(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (!inputVideo)
		return;

	// Note that these must be in SOURCE units for compatibility with 1.7.x. In particular, frame
	// rate adjustment must NOT be applied.
	int startOffset = arglist[0].asInt();
	int endOffset = arglist[1].asInt();
	double msToFrames = inputVideo->asStream()->getRate().asDouble() / 1000.0;

	g_project->UpdateTimelineRate();
	g_project->SetSelectionStart(VDRoundToInt64((double)startOffset * msToFrames));
	g_project->SetSelectionEnd(g_project->GetFrameCount() - VDRoundToInt64((double)endOffset * msToFrames));
}

static void func_VDVideo_SetRangeFrames(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (!inputVideo)
		return;

	VDPosition startOffset = arglist[0].asLong();
	VDPosition endOffset = arglist[1].asLong();

	g_project->UpdateTimelineRate();
	g_project->SetSelectionStart(startOffset);
	g_project->SetSelectionEnd(endOffset);
}

static void func_VDVideo_AddMarker(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (!inputVideo)
		return;

	VDPosition p = arglist[0].asLong();
	g_project->GetTimeline().SetMarkerSrc(p);
}

static void func_VDVideo_GetCompression(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (g_Vcompression.dwFlags & ICMF_COMPVARS_VALID) {
		switch(arglist[0].asInt()) {
		case 0:	arglist[0] = VDScriptValue((int)g_Vcompression.fccHandler); return;
		case 1: arglist[0] = VDScriptValue(g_Vcompression.lKey); return;
		case 2: arglist[0] = VDScriptValue(g_Vcompression.lQ); return;
		case 3: arglist[0] = VDScriptValue(g_Vcompression.lDataRate); return;
		}
	}

	arglist[0] = VDScriptValue(0);
}

static void func_VDVideo_SetCompression(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
//	ICCompressorFree(&g_Vcompression);
	FreeCompressor(&g_Vcompression);

	g_Vcompression.clear();

	if (!arg_count) return;

	g_Vcompression.cbSize	= sizeof(COMPVARS);
	g_Vcompression.dwFlags |= ICMF_COMPVARS_VALID;

	g_Vcompression.fccType		= ICTYPE_VIDEO;

	if (arglist[0].isString()) {
		g_Vcompression.fccHandler	= 0x20202020;
		memcpy(&g_Vcompression.fccHandler, *arglist[0].asString(), std::min<unsigned>(4,strlen(*arglist[0].asString())));
	} else
		g_Vcompression.fccHandler	= arglist[0].asInt();

	g_Vcompression.lKey			= arglist[1].asInt();
	g_Vcompression.lQ			= arglist[2].asInt();
	g_Vcompression.lDataRate	= arglist[3].asInt();

	if (arg_count==5) {
		VDStringW fileName(VDTextU8ToW(VDStringA(*arglist[4].asString())));

		std::list<class VDExternalModule *>::const_iterator it(g_pluginModules.begin()),
				itEnd(g_pluginModules.end());

		for(; it!=itEnd; ++it) {
			VDExternalModule *pModule = *it;
			const VDStringW& path = pModule->GetFilename();
			const wchar_t* name = VDFileSplitPath(path.c_str());
			if (_wcsicmp(name,fileName.c_str())!=0) continue;
			g_Vcompression.driver = EncoderHIC::load(path, g_Vcompression.fccType, g_Vcompression.fccHandler, ICMODE_COMPRESS);
			break;
		}
	} else {
		g_Vcompression.driver = EncoderHIC::open(g_Vcompression.fccType, g_Vcompression.fccHandler, ICMODE_COMPRESS);
	}
}

static void func_VDVideo_SetCompData(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	void *mem;
	long l = ((strlen(*arglist[1].asString())+3)/4)*3;

	if (!(g_Vcompression.dwFlags & ICMF_COMPVARS_VALID))
		return;

	if (!g_Vcompression.driver)
		return;

	if (arglist[0].asInt() > l) return;

	l = arglist[0].asInt();

	if (!(mem = allocmem(l)))
		VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

	_CrtCheckMemory();
	memunbase64((char *)mem, *arglist[1].asString(), l);
	_CrtCheckMemory();

	g_Vcompression.driver->setState(mem, l);

	freemem(mem);
}

static void func_VDVideo_EnableIndeoQC(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	R4_ENC_SEQ_DATA r4enc;

	if (!(g_Vcompression.dwFlags & ICMF_COMPVARS_VALID))
		return;

	if ((g_Vcompression.fccHandler & 0xFFFF) != 'VI')
		return;

	if (!g_Vcompression.driver)
		return;

	r4enc.dwSize			= sizeof(R4_ENC_SEQ_DATA);
	r4enc.dwFourCC			= g_Vcompression.fccHandler;
	r4enc.dwVersion			= SPECIFIC_INTERFACE_VERSION;
	r4enc.mtType			= MT_ENCODE_SEQ_VALUE;
	r4enc.oeEnvironment		= OE_32;
	r4enc.dwFlags			= ENCSEQ_VALID | ENCSEQ_QUICK_COMPRESS;
	r4enc.fQuickCompress	= !!arglist[0].asInt();

	g_Vcompression.driver->setState(&r4enc, sizeof r4enc);
}

static void func_VDVideo_SetIVTC(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (!!arglist[0].asInt())
		throw MyError("Inverse telecine (IVTC) is no longer supported as a pipeline parameter and has been moved to a video filter.");
}

static void func_VDVideo_ScanForErrors(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	if (!inputVideo || !g_project)
		VDSCRIPT_EXT_ERROR(FCALL_OUT_OF_RANGE);

	g_project->ScanForErrors();
}

static void func_VDVideo_intGetFramePrefix(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	if (!inputVideo)
		VDSCRIPT_EXT_ERROR(FCALL_OUT_OF_RANGE);

	VDPosition pos = argv[0].asInt();
	vdfastvector<uint8> buf;
	IVDStreamSource *stream = inputVideo->asStream();
	uint32 bytes, samples;
	stream->read(pos, 1, NULL, 0, &bytes, &samples);

	buf.resize(bytes);
	stream->read(pos, 1, buf.data(), buf.size(), &bytes, &samples);

	char c[4] = {0};
	memcpy(c, buf.data(), std::min<size_t>(buf.size(), 4));

	argv[0] = (int)VDReadUnalignedLEU32(c);
}

static void func_VDVideo_intIsKeyFrame(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	if (!inputVideo)
		VDSCRIPT_EXT_ERROR(FCALL_OUT_OF_RANGE);

	VDPosition pos = argv[0].asInt();

	argv[0] = (int)inputVideo->isKey(pos);
}

namespace {
	int GetFrameId(VDPosition pos) {
		vdfastvector<uint8> buf;
		IVDStreamSource *stream = inputVideo->asStream();

		if (pos < 0 || pos >= stream->getLength())
			return -1;

		uint32 bytes, samples;
		stream->read(pos, 1, NULL, 0, &bytes, &samples);

		buf.resize(bytes);
		stream->read(pos, 1, buf.data(), buf.size(), &bytes, &samples);

		char c[4] = {0};
		memcpy(c, buf.data(), std::min<size_t>(buf.size(), 4));

		uint32 id = VDReadUnalignedLEU32(c);
		int decframe = ((id & 0x1e) >> 1) + ((id & 0x3c0) >> 2);

		if (decframe >= 100 || (id & 0xfc00) != 0x8800)
			return -1;

		uint32 lo = id & 0xffff;
		uint32 hi = id >> 16;

		if (lo == hi) {
			if (!inputVideo->isKey(pos))
				decframe = -1;
		} else if (hi == 0x8400) {
			if (inputVideo->isKey(pos))
				decframe = -1;
		} else {
			decframe = -1;
		}

		return decframe;
	}

	int GetFrameId2(VDPosition pos) {
		IVDStreamSource *stream = inputVideo->asStream();

		if (pos < 0 || pos >= stream->getLength())
			return -1;

		const VDPixmap& px = inputVideo->getTargetFormat();
		if (px.w != 8 || px.h != 4 || !px.format)
			return -1;

		if (!inputVideo->getFrame(pos))
			return -1;

		uint16 tex[4][8];
		VDPixmap pxdst={};
		pxdst.w = 8;
		pxdst.h = 4;
		pxdst.pitch = sizeof tex[0];
		pxdst.data = tex;
		pxdst.format = nsVDPixmap::kPixFormat_XRGB1555;

		if (!VDPixmapBlt(pxdst, px))
			return -1;

		uint32 id = tex[0][0];
//		throw MyError("%x\n", id);
		int decframe = ((id & 0x1e) >> 1) + ((id & 0x3c0) >> 2);

		if (decframe >= 100)
			return -1;

		return decframe;
	}
}

static void func_VDVideo_intDetectIdFrame(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	if (!inputVideo)
		VDSCRIPT_EXT_ERROR(FCALL_OUT_OF_RANGE);

	VDPosition pos = argv[0].asInt();

	argv[0] = GetFrameId(pos);
}

static void func_VDVideo_intDetectIdFrame2(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	if (!inputVideo)
		VDSCRIPT_EXT_ERROR(FCALL_OUT_OF_RANGE);

	VDPosition pos = argv[0].asInt();

	argv[0] = GetFrameId2(pos);
}

static void func_VDVideo_intValidateFrames(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	if (!inputVideo)
		VDSCRIPT_EXT_ERROR(FCALL_OUT_OF_RANGE);

	for(int i=0; i<argc; ++i) {
		if (!argv[i].isInt())
			VDSCRIPT_EXT_ERROR(TYPE_INT_REQUIRED);

		int frame = argv[i].asInt();
		int id = GetFrameId(i);

		if (id != frame)
			throw MyError("Frame mismatch: frame[%d] has id %d, expected %d", i, id, frame);
	}

	VDPosition len = inputVideo->asStream()->getLength();
	if (argc != len)
		throw MyError("Length mismatch: expected %d frames, found %d frames", argc, (int)len);
}

static void func_VDVideo_intValidateFrames2(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	if (!inputVideo)
		VDSCRIPT_EXT_ERROR(FCALL_OUT_OF_RANGE);

	for(int i=0; i<argc; ++i) {
		if (!argv[i].isInt())
			VDSCRIPT_EXT_ERROR(TYPE_INT_REQUIRED);

		int frame = argv[i].asInt();
		int id = GetFrameId2(i);

		if (id != frame)
			throw MyError("Frame mismatch: frame[%d] has id %d, expected %d", i, id, frame);
	}

	VDPosition len = inputVideo->asStream()->getLength();
	if (argc != len)
		throw MyError("Length mismatch: expected %d frames, found %d frames", argc, (int)len);
}

static const VDScriptFunctionDef obj_VDVideo_functbl[]={
	{ func_VDVideo_GetDepth			, "GetDepth",		"ii" },
	{ func_VDVideo_SetDepth			, "SetDepth",		"0ii" },
	{ func_VDVideo_GetMode			, "GetMode",		"i" },
	{ func_VDVideo_SetMode			, "SetMode",		"0i" },
	{ func_VDVideo_GetFrameRate		, "GetFrameRate",	"ii" },
	{ func_VDVideo_SetFrameRate		, "SetFrameRate",	"0ii" },
	{ func_VDVideo_SetFrameRate		, NULL,				"0iii" },
	{ func_VDVideo_SetFrameRate2	, "SetFrameRate2",	"0lli" },
	{ func_VDVideo_SetTargetFrameRate, "SetTargetFrameRate", "0ii" },
	{ func_VDVideo_GetRange			, "GetRange",		"ii" },
	{ func_VDVideo_SetRangeEmpty	, "SetRange",		"0" },
	{ func_VDVideo_SetRange			, NULL,				"0ii" },
	{ func_VDVideo_AddMarker		, "AddMarker",		"0l" },
	{ func_VDVideo_SetRangeFrames	, "SetRangeFrames",	"0ll" },
	{ func_VDVideo_GetCompression	, "GetCompression",	"ii" },
	{ func_VDVideo_SetCompression	, "SetCompression",	"0siii" },
	{ func_VDVideo_SetCompression	, NULL,				"0iiii" },
	{ func_VDVideo_SetCompression	, NULL,				"0siiis" },
	{ func_VDVideo_SetCompression	, NULL,				"0iiiis" },
	{ func_VDVideo_SetCompression	, NULL,				"0" },
	{ func_VDVideo_SetCompData		, "SetCompData",	"0is" },
	{ func_VDVideo_EnableIndeoQC	, "EnableIndeoQC",	"0i" },
	{ func_VDVideo_SetIVTC			, "SetIVTC",		"0iiii" },
	{ func_VDVideo_SetInputFormat	, "SetInputFormat",	"0i" },
	{ func_VDVideo_SetOutputFormat	, "SetOutputFormat", "0i" },
	{ func_VDVideo_SetInputMatrix	, "SetInputMatrix", "0ii" },
	{ func_VDVideo_SetOutputMatrix	, "SetOutputMatrix", "0ii" },
	{ func_VDVideo_GetSmartRendering, "GetSmartRendering", "i" },
	{ func_VDVideo_SetSmartRendering, "SetSmartRendering", "0i" },
	{ func_VDVideo_GetPreserveEmptyFrames, "GetPreserveEmptyFrames", "i" },
	{ func_VDVideo_SetPreserveEmptyFrames, "SetPreserveEmptyFrames", "0i" },
	{ func_VDVideo_ScanForErrors	, "ScanForErrors", "0" },
	{ func_VDVideo_intGetFramePrefix, "__GetFramePrefix", "ii" },
	{ func_VDVideo_intIsKeyFrame, "__IsKeyFrame", "ii" },
	{ func_VDVideo_intDetectIdFrame, "__DetectIdFrame", "ii" },
	{ func_VDVideo_intDetectIdFrame2, "__DetectIdFrame2", "ii" },
	{ func_VDVideo_intValidateFrames, "__ValidateFrames", "0." },
	{ func_VDVideo_intValidateFrames2, "__ValidateFrames2", "0." },
	{ NULL }
};

static const VDScriptObjectDef obj_VDVideo_objtbl[]={
	{ "filters", &obj_VDVFilters },
	{ NULL }
};

static VDScriptValue obj_VirtualDub_video_lookup(IVDScriptInterpreter *isi, const VDScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "width"))
		return VDScriptValue(inputVideo ? inputVideo->getImageFormat()->biWidth : 0);
	else if (!strcmp(szName, "height"))
		return VDScriptValue(inputVideo ? abs(inputVideo->getImageFormat()->biHeight) : 0);
	else if (!strcmp(szName, "length"))
		return VDScriptValue(inputVideo ? inputVideo->asStream()->getLength() : 0);
	else if (!strcmp(szName, "framerate"))
		return VDScriptValue(inputVideo ? inputVideo->asStream()->getRate().asDouble() : 0.0);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptObject obj_VDVideo={
	"VDVideo", obj_VirtualDub_video_lookup, obj_VDVideo_functbl, obj_VDVideo_objtbl
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio.filters.instance
//
///////////////////////////////////////////////////////////////////////////

namespace {
	const VDXPluginConfigEntry *GetFilterParamEntry(const VDXPluginConfigEntry *pEnt, const unsigned idx) {
		if (pEnt)
			for(; pEnt->next; pEnt=pEnt->next) {
				if (pEnt->idx == idx)
					return pEnt;
			}

		return NULL;
	}

	void SetFilterParam(void *lpVoid, unsigned idx, const VDPluginConfigVariant& v) {
		VDAudioFilterGraph::FilterEntry *pFilt = (VDAudioFilterGraph::FilterEntry *)lpVoid;

		VDPluginDescription *pDesc = VDGetPluginDescription(pFilt->mFilterName.c_str(), kVDXPluginType_Audio);

		if (!pDesc)
			throw MyError("VDAFiltInst: Unknown audio filter: \"%s\"", VDTextWToA(pFilt->mFilterName).c_str());

		VDPluginPtr lock(pDesc);

		const VDXPluginConfigEntry *pEnt = GetFilterParamEntry(((VDAudioFilterDefinition *)lock->mpInfo->mpTypeSpecificInfo)->mpConfigInfo, idx);

		if (!pEnt)
			throw MyError("VDAFiltInst: Audio filter \"%s\" does not have a parameter with id %d", VDTextWToA(pFilt->mFilterName).c_str(), idx);

		VDPluginConfigVariant& var = pFilt->mConfig[idx];

		switch(pEnt->type) {
		case VDXPluginConfigEntry::kTypeU32:
			switch(v.GetType()) {
			case VDPluginConfigVariant::kTypeU32:	var.SetU32(v.GetU32()); return;
			case VDPluginConfigVariant::kTypeS32:	var.SetU32(v.GetS32()); return;
			case VDPluginConfigVariant::kTypeU64:	var.SetU32((uint32)v.GetU64()); return;
			case VDPluginConfigVariant::kTypeS64:	var.SetU32((uint32)v.GetS64()); return;
			}
			break;
		case VDXPluginConfigEntry::kTypeS32:
			switch(v.GetType()) {
			case VDPluginConfigVariant::kTypeU32:	var.SetS32(v.GetU32()); return;
			case VDPluginConfigVariant::kTypeS32:	var.SetS32(v.GetS32()); return;
			case VDPluginConfigVariant::kTypeU64:	var.SetS32((sint32)v.GetU64()); return;
			case VDPluginConfigVariant::kTypeS64:	var.SetS32((sint32)v.GetS64()); return;
			}
			break;
		case VDXPluginConfigEntry::kTypeU64:
			switch(v.GetType()) {
			case VDPluginConfigVariant::kTypeU32:	var.SetU64(v.GetU32()); return;
			case VDPluginConfigVariant::kTypeS32:	var.SetU64(v.GetS32()); return;
			case VDPluginConfigVariant::kTypeU64:	var.SetU64(v.GetU64()); return;
			case VDPluginConfigVariant::kTypeS64:	var.SetU64(v.GetS64()); return;
			}
			break;
		case VDXPluginConfigEntry::kTypeS64:
			switch(v.GetType()) {
			case VDPluginConfigVariant::kTypeU32:	var.SetS64(v.GetU32()); return;
			case VDPluginConfigVariant::kTypeS32:	var.SetS64(v.GetS32()); return;
			case VDPluginConfigVariant::kTypeU64:	var.SetS64(v.GetU64()); return;
			case VDPluginConfigVariant::kTypeS64:	var.SetS64(v.GetS64()); return;
			}
			break;
		case VDXPluginConfigEntry::kTypeDouble:
			if (v.GetType() == VDXPluginConfigEntry::kTypeDouble) {
				var = v;
				return;
			}
			break;
		case VDXPluginConfigEntry::kTypeAStr:
			if (v.GetType() == VDXPluginConfigEntry::kTypeWStr) {
				var.SetAStr(VDTextWToA(v.GetWStr()).c_str());
				return;
			}
			break;
		case VDXPluginConfigEntry::kTypeWStr:
			if (v.GetType() == VDXPluginConfigEntry::kTypeWStr) {
				var = v;
				return;
			}
			break;
		case VDXPluginConfigEntry::kTypeBlock:
			if (v.GetType() == VDXPluginConfigEntry::kTypeBlock) {
				var = v;
				return;
			}
		}

		pFilt->mConfig.erase(idx);

		throw MyError("VDAFiltInst: Type mismatch on audio filter \"%s\" param %d (\"%s\")", VDTextWToA(pFilt->mFilterName).c_str(), idx, VDTextWToA(pEnt->name).c_str());
	}
};

static void func_VDAFiltInst_SetInt(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;
	
	v.SetS32(argv[1].asInt());

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetLong(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;
	
	if (argc == 2)
		v.SetU64(argv[1].asLong());
	else
		v.SetU64((uint32)argv[2].asInt() + ((uint64)(uint32)argv[1].asInt() << 32));

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetDouble(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;

	if (argc == 2)
		v.SetDouble(argv[1].asDouble());
	else {
		union {
			struct {
				uint32 lo;
				uint32 hi;
			} bar;
			double d;
		} foo;

		foo.bar.lo = argv[2].asInt();
		foo.bar.hi = argv[1].asInt();
	
		v.SetDouble(foo.d);
	}

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetString(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;
	
	v.SetWStr(VDTextU8ToW(*argv[1].asString(), -1).c_str());

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static void func_VDAFiltInst_SetRaw(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDPluginConfigVariant v;

	int len = argv[1].asInt();
	vdfastvector<char> mem(len);
	long l = ((strlen(*argv[2].asString())+3)/4)*3;

	if (len > l)
		return;

	memunbase64(mem.data(), *argv[2].asString(), len);
	v.SetBlock(&mem.front(), len);

	SetFilterParam(argv[-1].asObjectPtr(), argv[0].asInt(), v);
}

static const VDScriptFunctionDef obj_VDAFiltInst_functbl[]={
	{ func_VDAFiltInst_SetInt,			"SetInt",		"0ii" },
	{ func_VDAFiltInst_SetLong,			"SetLong",		"0iii" },
	{ func_VDAFiltInst_SetLong,			NULL,			"0il" },
	{ func_VDAFiltInst_SetDouble,		"SetDouble",	"0iii" },
	{ func_VDAFiltInst_SetDouble,		NULL,			"0id" },
	{ func_VDAFiltInst_SetString,		"SetString",	"0is" },
	{ func_VDAFiltInst_SetRaw,			"SetRaw",		"0iis" },
	{ NULL }
};

static const VDScriptObject obj_VDAFiltInst={
	"VDAudio", NULL, obj_VDAFiltInst_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio.filters
//
///////////////////////////////////////////////////////////////////////////

static void func_VDAFilters_instance(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	int index = argv[0].asInt();

	if (index < 0 || index >= g_audioFilterGraph.mFilters.size()) {
		VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);
	}

	VDAudioFilterGraph::FilterList::iterator it(g_audioFilterGraph.mFilters.begin());

	std::advance(it, index);

	argv[0] = VDScriptValue(static_cast<VDAudioFilterGraph::FilterEntry *>(&*it), &obj_VDAFiltInst);
}

static const VDScriptFunctionDef obj_VDAFilters_instance_functbl[]={
	{ func_VDAFilters_instance		, "[]", "vi" },
	{ NULL }
};

static const VDScriptObject obj_VDAFilters_instance={
	"VDAudioFilterList", NULL, obj_VDAFilters_instance_functbl, NULL	
};

static void func_VDAFilters_Clear(IVDScriptInterpreter *, VDScriptValue *, int) {
	g_audioFilterGraph.mFilters.clear();
	g_audioFilterGraph.mConnections.clear();
}

static void func_VDAFilters_Add(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDAudioFilterGraph::FilterEntry filt;

	filt.mFilterName = VDTextU8ToW(*argv[0].asString(), -1);

	VDPluginDescription *pDesc = VDGetPluginDescription(filt.mFilterName.c_str(), kVDXPluginType_Audio);

	if (!pDesc)
		throw MyError("VDAFilters.Add(): Unknown audio filter: \"%s\"", VDTextWToA(filt.mFilterName).c_str());

	const VDAudioFilterDefinition *pDef = reinterpret_cast<const VDAudioFilterDefinition *>(pDesc->mpInfo->mpTypeSpecificInfo);

	filt.mInputPins = pDef->mInputPins;
	filt.mOutputPins = pDef->mOutputPins;

	g_audioFilterGraph.mFilters.push_back(filt);

	VDAudioFilterGraph::FilterConnection conn = {-1,-1};

	for(unsigned i=0; i<filt.mInputPins; ++i)
		g_audioFilterGraph.mConnections.push_back(conn);

	argv[0] = (int)(g_audioFilterGraph.mFilters.size()-1);
}

static void func_VDAFilters_Connect(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	int srcfilt	= argv[0].asInt();
	int srcpin	= argv[1].asInt();
	int dstfilt	= argv[2].asInt();
	int dstpin	= argv[3].asInt();
	int nfilts	= g_audioFilterGraph.mFilters.size();

	if (srcfilt<0 || srcfilt>=nfilts)
		throw MyError("VDAFilters.Connect(): Invalid source filter number %d (should be 0-%d)", srcfilt, nfilts-1);
	if (dstfilt<=srcfilt || dstfilt>=nfilts)
		throw MyError("VDAFilters.Connect(): Invalid target filter number %d (should be %d-%d)", dstfilt, srcfilt+1, nfilts-1);

	// #&*$(
	VDAudioFilterGraph::FilterList::const_iterator itsrc = g_audioFilterGraph.mFilters.begin();
	VDAudioFilterGraph::FilterList::const_iterator itdst = g_audioFilterGraph.mFilters.begin();
	int dstconnidx = 0;

	while(dstfilt-->0) {
		dstconnidx += (*itdst).mInputPins;
		++itdst;
	}

	std::advance(itsrc, srcfilt);

	VDASSERT(dstconnidx < g_audioFilterGraph.mConnections.size());

	const VDAudioFilterGraph::FilterEntry& fesrc = *itsrc;
	const VDAudioFilterGraph::FilterEntry& fedst = *itdst;

	if (srcpin<0 || srcpin>=fesrc.mOutputPins)
		throw MyError("VDAFilters.Connect(): Invalid source pin %d (should be 0-%d)", srcpin, fesrc.mOutputPins-1);
	if (dstpin<0 || dstpin>=fedst.mInputPins)
		throw MyError("VDAFilters.Connect(): Invalid target pin %d (should be 0-%d)", dstpin, fedst.mInputPins-1);

	VDAudioFilterGraph::FilterConnection& conn = g_audioFilterGraph.mConnections[dstconnidx + dstpin];

	conn.filt = srcfilt;
	conn.pin = srcpin;
}

static VDScriptValue obj_VDAFilters_lookup(IVDScriptInterpreter *isi, const VDScriptObject *thisPtr, void *lpVoid, char *szName) {
	if (!strcmp(szName, "instance"))
		return VDScriptValue(lpVoid, &obj_VDAFilters_instance);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptFunctionDef obj_VDAFilters_functbl[]={
	{ func_VDAFilters_Clear			, "Clear", "0" },
	{ func_VDAFilters_Add			, "Add", "is", },
	{ func_VDAFilters_Connect		, "Connect", "0iiii", },
	{ NULL }
};

static const VDScriptObject obj_VDAFilters={
	"VDAudio", obj_VDAFilters_lookup, obj_VDAFilters_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.audio
//
///////////////////////////////////////////////////////////////////////////

static void func_VDAudio_GetMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = VDScriptValue(g_dubOpts.audio.mode);
}

static void func_VDAudio_SetMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int new_mode = arglist[0].asInt();

	if (new_mode>=0 && new_mode<2)
		g_dubOpts.audio.mode = (char)new_mode;
}

static void func_VDAudio_GetInterleave(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:	arglist[0] = VDScriptValue(g_dubOpts.audio.enabled); return;
	case 1:	arglist[0] = VDScriptValue(g_dubOpts.audio.preload); return;
	case 2:	arglist[0] = VDScriptValue(g_dubOpts.audio.interval); return;
	case 3:	arglist[0] = VDScriptValue(g_dubOpts.audio.is_ms); return;
	case 4: arglist[0] = VDScriptValue(g_dubOpts.audio.offset); return;
	}

	arglist[0] = VDScriptValue(0);
}

static void func_VDAudio_SetInterleave(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.enabled		= !!arglist[0].asInt();
	g_dubOpts.audio.preload		= arglist[1].asInt();
	g_dubOpts.audio.interval	= arglist[2].asInt();
	g_dubOpts.audio.is_ms		= !!arglist[3].asInt();
	g_dubOpts.audio.offset		= arglist[4].asInt();
}

static void func_VDAudio_GetClipMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:		arglist[0] = VDScriptValue(g_dubOpts.audio.fStartAudio); return;
	case 1:		arglist[0] = VDScriptValue(g_dubOpts.audio.fEndAudio); return;
	}

	arglist[0] = VDScriptValue(0);
}

static void func_VDAudio_SetClipMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.fStartAudio	= !!arglist[0].asInt();
	g_dubOpts.audio.fEndAudio	= !!arglist[1].asInt();
}

static void func_VDAudio_GetEditMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	arglist[0] = VDScriptValue((int)g_dubOpts.audio.mbApplyVideoTimeline);
}

static void func_VDAudio_SetEditMode(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.mbApplyVideoTimeline = !!arglist[0].asInt();
	g_dubOpts.audio.fStartAudio	= !!arglist[0].asInt();
	g_dubOpts.audio.fEndAudio	= !!arglist[1].asInt();
}

static void func_VDAudio_GetConversion(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	switch(arglist[0].asInt()) {
	case 0:		arglist[0] = VDScriptValue(g_dubOpts.audio.new_rate); return;
	case 1:		arglist[0] = VDScriptValue(g_dubOpts.audio.newPrecision); return;
	case 2:		arglist[0] = VDScriptValue(g_dubOpts.audio.newChannels); return;
	}

	arglist[0] = VDScriptValue(0);
}

static void func_VDAudio_SetConversion(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.new_rate		= arglist[0].asInt();
	g_dubOpts.audio.newPrecision	= (char)arglist[1].asInt();
	g_dubOpts.audio.newChannels		= (char)arglist[2].asInt();
	g_project->SetAudioSource();

	if (arg_count >= 5) {
		if (arglist[3].asInt())
			throw MyError("The \"integral_rate\" feature of the audio.SetConversion() function is no longer supported.");
		g_dubOpts.audio.fHighQuality	= !!arglist[4].asInt();
	}
}

static void func_VDAudio_SetSource(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	int baseMode = arglist[0].asInt();

	if (baseMode) {
		int streamIndex = 0;

		if (arg_count >= 2)
			streamIndex = arglist[1].asInt();

		g_project->SetAudioSourceNormal(streamIndex);
	} else
		g_project->SetAudioSourceNone();
}

static void func_VDAudio_SetSourceExternal(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	IVDInputDriver *pDriver = NULL;
	VDStringW s(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	VDStringW fileName(g_project->ExpandProjectPath(s.c_str()));

	if (arg_count >= 2) {
		const VDStringW driverName(VDTextU8ToW(*arglist[1].asString(), -1));

		if (driverName.empty())
			pDriver = VDAutoselectInputDriverForFile(fileName.c_str(), IVDInputDriver::kF_Audio);
		else
			pDriver = VDGetInputDriverByName(driverName.c_str());

		if (!pDriver)
			throw MyError("Unable to find input driver with name: %ls", driverName.c_str());
	}

	if (arg_count >= 3) {
		long l = ((strlen(*arglist[2].asString())+3)/4)*3;
		vdfastvector<char> buf(l);

		l = memunbase64(buf.data(), *arglist[2].asString(), l);

		g_project->OpenWAV(fileName.c_str(), pDriver, true, false, buf.data(), l);
	} else {
		g_project->OpenWAV(fileName.c_str(), pDriver, true, false, NULL, 0);
	}
}

static void func_VDAudio_SetCompressionPCM(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	PCMWAVEFORMAT *pwf = (PCMWAVEFORMAT *)allocmem(sizeof(PCMWAVEFORMAT));

	if (!pwf) VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

	pwf->wf.wFormatTag			= WAVE_FORMAT_PCM;
	pwf->wf.nSamplesPerSec		= arglist[0].asInt();
	pwf->wf.nChannels			= (WORD)arglist[1].asInt();
	pwf->   wBitsPerSample		= (WORD)arglist[2].asInt();
	pwf->wf.nBlockAlign			= (WORD)((pwf->wBitsPerSample/8) * pwf->wf.nChannels);
	pwf->wf.nAvgBytesPerSec		= pwf->wf.nSamplesPerSec * pwf->wf.nBlockAlign;
	g_ACompressionFormatSize	= sizeof(PCMWAVEFORMAT);
	freemem(g_ACompressionFormat);
	g_ACompressionFormat = (VDWaveFormat *)pwf;
}

// VirtualDub.audio.SetCompression();
// VirtualDub.audio.SetCompression(tag, sampling_rate, channels, bits, bytes/sec, blockalign);
// VirtualDub.audio.SetCompression(tag, sampling_rate, channels, bits, bytes/sec, blockalign, exdatalen, exdata);

static void func_VDAudio_SetCompression(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	WAVEFORMATEX *wfex;
	long ex_data=0;

	if (!arg_count) {
		freemem(g_ACompressionFormat);
		g_ACompressionFormat = NULL;
		return;
	}

	if (arg_count > 6)
		ex_data = arglist[6].asInt();

	if (!(wfex = (WAVEFORMATEX *)allocmem(sizeof(WAVEFORMATEX) + ex_data)))
		VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

	wfex->wFormatTag		= (WORD)arglist[0].asInt();
	wfex->nSamplesPerSec	= arglist[1].asInt();
	wfex->nChannels			= (WORD)arglist[2].asInt();
	wfex->wBitsPerSample	= (WORD)arglist[3].asInt();
	wfex->nAvgBytesPerSec	= arglist[4].asInt();
	wfex->nBlockAlign		= (WORD)arglist[5].asInt();
	wfex->cbSize			= (WORD)ex_data;

	if (arg_count > 6) {
		long l = ((strlen(*arglist[7].asString())+3)/4)*3;

		if (ex_data > l) {
			freemem(wfex);
			return;
		}

		memunbase64((char *)(wfex+1), *arglist[7].asString(), ex_data);
	}

	_CrtCheckMemory();

	if (g_ACompressionFormat)
		freemem(g_ACompressionFormat);

	g_ACompressionFormat = (VDWaveFormat *)wfex;

	if (wfex->wFormatTag == WAVE_FORMAT_PCM)
		g_ACompressionFormatSize = sizeof(PCMWAVEFORMAT);
	else
		g_ACompressionFormatSize = sizeof(WAVEFORMATEX)+ex_data;

	g_ACompressionConfig.clear();

	_CrtCheckMemory();
}

static void func_VDAudio_SetCompressionWithHint(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	VDASSERT(arg_count > 0);

	func_VDAudio_SetCompression(isi, arglist, arg_count - 1);

	if (g_ACompressionFormat)
		g_ACompressionFormatHint.assign(*arglist[arg_count - 1].asString());
}

static void func_VDAudio_SetCompData(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	g_ACompressionConfig.clear();

	void *mem;
	long l = ((strlen(*arglist[1].asString())+3)/4)*3;

	if (arglist[0].asInt() > l) return;

	l = arglist[0].asInt();

	if (!(mem = allocmem(l)))
		VDSCRIPT_EXT_ERROR(OUT_OF_MEMORY);

	_CrtCheckMemory();
	memunbase64((char *)mem, *arglist[1].asString(), l);
	_CrtCheckMemory();

	g_ACompressionConfig.resize(l);
	memcpy(g_ACompressionConfig.data(), mem, l);

	freemem(mem);
}

static void func_VDAudio_SetVolume(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	if (arg_count)
		g_dubOpts.audio.mVolume = arglist[0].asInt() * (1.0f / 256.0f);
	else
		g_dubOpts.audio.mVolume = -1.0f;
}

static void func_VDAudio_GetVolume(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (g_dubOpts.audio.mVolume < 0)
		arglist[0] = VDScriptValue(0);
	else
		arglist[0] = VDScriptValue(VDRoundToInt(g_dubOpts.audio.mVolume * 256.0f));
}

static void func_VDAudio_EnableFilterGraph(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	g_dubOpts.audio.bUseAudioFilterGraph = (arglist[0].asInt() != 0);
	g_project->SetAudioSource();
}

static VDScriptFunctionDef obj_VDAudio_functbl[]={
	{ func_VDAudio_GetMode			, "GetMode"				, "i"		},
	{ func_VDAudio_SetMode			, "SetMode"				, "0i"		},
	{ func_VDAudio_GetInterleave		, "GetInterleave"		, "ii"		},
	{ func_VDAudio_SetInterleave		, "SetInterleave"		, "0iiiii"	},
	{ func_VDAudio_GetClipMode		, "GetClipMode"			, "ii"		},
	{ func_VDAudio_SetClipMode		, "SetClipMode"			, "0ii"		},
	{ func_VDAudio_GetEditMode		, "GetEditMode"			, "i"		},
	{ func_VDAudio_SetEditMode		, "SetEditMode"			, "0i"		},
	{ func_VDAudio_GetConversion		, "GetConversion"		, "ii"		},
	{ func_VDAudio_SetConversion		, "SetConversion"		, "0iii"	},
	{ func_VDAudio_SetConversion		, NULL					, "0iiiii"	},
	{ func_VDAudio_SetSource			, "SetSource"			, "0i"		},
	{ func_VDAudio_SetSource			, NULL					, "0ii"		},
	{ func_VDAudio_SetSourceExternal	, NULL					, "0s"		},
	{ func_VDAudio_SetSourceExternal	, NULL					, "0ss"		},
	{ func_VDAudio_SetSourceExternal	, NULL					, "0sss"	},
	{ func_VDAudio_SetCompressionPCM	, "SetCompressionPCM"	, "0iii"	},
	{ func_VDAudio_SetCompression	, "SetCompression"		, "0"		},
	{ func_VDAudio_SetCompression	, NULL					, "0iiiiii" },
	{ func_VDAudio_SetCompression	, NULL					, "0iiiiiiis" },
	{ func_VDAudio_SetCompressionWithHint	, "SetCompressionWithHint"					, "0iiiiiis" },
	{ func_VDAudio_SetCompressionWithHint	, NULL					, "0iiiiiiiss" },
	{ func_VDAudio_SetCompData				, "SetCompData"			, "0is" },
	{ func_VDAudio_SetVolume			, "SetVolume"			, "0" },
	{ func_VDAudio_SetVolume			, NULL					, "0i" },
	{ func_VDAudio_GetVolume			, "GetVolume"			, "i" },
	{ func_VDAudio_EnableFilterGraph	, "EnableFilterGraph"	, "0i" },
	{ NULL }
};

static const VDScriptObjectDef obj_VDAudio_objtbl[]={
	{ "filters", &obj_VDAFilters },
	{ NULL }
};

static VDScriptValue obj_VirtualDub_audio_lookup(IVDScriptInterpreter *isi, const VDScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "samplerate"))
		return VDScriptValue(inputAudio ? (int)inputAudio->getWaveFormat()->mSamplingRate : 0);
	else if (!strcmp(szName, "blockrate")) {
		if (!inputAudio)
			return VDScriptValue(0);

		const VDWaveFormat& wfex = *inputAudio->getWaveFormat();
		return VDScriptValue((double)wfex.mDataRate / (double)wfex.mBlockSize);
	} else if (!strcmp(szName, "length"))
		return VDScriptValue(inputAudio ? (int)inputAudio->getLength() : 0);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptObject obj_VDAudio={
	"VDAudio", obj_VirtualDub_audio_lookup, obj_VDAudio_functbl, obj_VDAudio_objtbl
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.subset
//
///////////////////////////////////////////////////////////////////////////

static void func_VDSubset_Delete(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	g_project->ResetTimeline();
}

static void func_VDSubset_Clear(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	g_project->ResetTimeline();
	FrameSubset& s = g_project->GetTimeline().GetSubset();
	s.clear();
}

static void func_VDSubset_AddRange(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	FrameSubset& s = g_project->GetTimeline().GetSubset();
	s.addRange(arglist[0].asLong(), arglist[1].asLong(), false, 0);
}

static void func_VDSubset_AddMaskedRange(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	FrameSubset& s = g_project->GetTimeline().GetSubset();
	s.addRange(arglist[0].asLong(), arglist[1].asLong(), true, 0);
}

static void func_VDSubset_LookupFrameAtFilter(IVDScriptInterpreter *isi, VDScriptValue *argv, int arg_count) {
	int index = argv[0].asInt();

	if (index < 0 || (unsigned)index >= g_filterChain.mEntries.size())
		VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);

	FilterInstance *fi = g_filterChain.mEntries[index]->mpInstance;

	if (!fi)
		VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);

	VDProject *p = g_project;
	
	if (!p)
		VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);

	p->StartFilters();
	FrameSubset& s = p->GetTimeline().GetSubset();

	VDPosition frame = argv[1].asInt();
	VDPosition n = s.getTotalFrames();

	if (frame >= n)
		frame = s.lookupFrame(n - 1) + (frame - n);
	else if (frame < 0)
		frame = s.lookupFrame(0) + frame;
	else
		frame = s.lookupFrame(frame);

	argv[0] = VDScriptValue(filters.GetSymbolicFrame(frame, fi));
}

static const VDScriptFunctionDef obj_VDSubset_functbl[]={
	{ func_VDSubset_Delete			, "Delete"				, "0"		},
	{ func_VDSubset_Clear			, "Clear"				, "0"		},
	{ func_VDSubset_AddRange		, "AddFrame"			, "0ll"		},	// DEPRECATED
	{ func_VDSubset_AddRange		, "AddRange"			, "0ll"		},
	{ func_VDSubset_AddMaskedRange	, "AddMaskedRange"		, "0ll"		},
	{ func_VDSubset_LookupFrameAtFilter, "LookupFrameAtFilter"	, "lil"		},

	{ NULL }
};

static VDScriptValue obj_VDSubset_lookup(IVDScriptInterpreter *isi, const VDScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "length"))
		return VDScriptValue(g_project->GetTimeline().GetLength());

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptObject obj_VDSubset={
	"VDSubset", obj_VDSubset_lookup, obj_VDSubset_functbl
};


///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.params
//
///////////////////////////////////////////////////////////////////////////

static void func_VDParams(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	const int index = argv[0].asInt();
	const char *s = VDGetStartupArgument(index);

	if (!s)
		VDSCRIPT_EXT_ERROR(ARRAY_INDEX_OUT_OF_BOUNDS);

	const long l = strlen(s);
	char **h = isi->AllocTempString(l);

	strcpy(*h, s);

	argv[0] = VDScriptValue(h);
}

static const VDScriptFunctionDef obj_VDParams_functbl[]={
	{ func_VDParams		, "[]", "vi" },
	{ NULL }
};

static const VDScriptObject obj_VDParams={
	"VDParams", NULL, obj_VDParams_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub.project
//
///////////////////////////////////////////////////////////////////////////

static void func_VDProject_ClearTextInfo(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDProject::tTextInfo& textInfo = g_project->GetTextInfo();

	textInfo.clear();
}

static void func_VDProject_AddTextInfo(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDProject::tTextInfo& textInfo = g_project->GetTextInfo();
	union {
		char buf[4];
		uint32 id;
	} conv;

	strncpy(conv.buf, *argv[0].asString(), 4);

	textInfo.push_back(VDProject::tTextInfo::value_type(conv.id, VDStringA(*argv[1].asString())));
}

static const VDScriptFunctionDef obj_VDProject_functbl[]={
	{ func_VDProject_ClearTextInfo,	"ClearTextInfo", "0" },
	{ func_VDProject_AddTextInfo,	"AddTextInfo", "0ss" },
	{ NULL }
};

static const VDScriptObject obj_VDProject={
	"VDProject", NULL, obj_VDProject_functbl, NULL	
};

///////////////////////////////////////////////////////////////////////////
//
//	Object: VirtualDub
//
///////////////////////////////////////////////////////////////////////////

static void func_VirtualDub_SetStatus(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	guiSetStatus("%s", 255, *arglist[0].asString());
}

static void func_VirtualDub_OpenOld(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextAToW(*arglist[0].asString()));
	IVDInputDriver *pDriver = VDGetInputDriverForLegacyIndex(arglist[1].asInt());

	if (arg_count > 3) {
		long l = ((strlen(*arglist[3].asString())+3)/4)*3;
		vdfastvector<char> buf(l);

		l = memunbase64(buf.data(), *arglist[3].asString(), l);

		g_project->Open(filename.c_str(), pDriver, !!arglist[2].asInt(), true, 0, buf.data(), l);
	} else
		g_project->Open(filename.c_str(), pDriver, !!arglist[2].asInt(), true, 0);
}

static void func_VirtualDub_Open(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	IVDInputDriver *pDriver = NULL;
	bool extopen = false;
	VDStringW signature;
	
	if (arg_count > 1) {
		signature = VDTextAToW(*arglist[1].asString());
		pDriver = VDGetInputDriverByName(signature.c_str());

		if (arg_count > 2)
			extopen = !!arglist[2].asInt();
	}

	if (arg_count > 3) {
		long l = ((strlen(*arglist[3].asString())+3)/4)*3;
		vdfastvector<char> buf(l);

		l = memunbase64(buf.data(), *arglist[3].asString(), l);

		vdrefptr<IVDInputDriver> pTest;
		if (filename.empty() && !pDriver) {
			pTest = VDCreateInputDriverTest();
			if (signature==pTest->GetSignatureName())
			pDriver = pTest;
		}

		g_project->Open(filename.c_str(), pDriver, extopen, true, 0, buf.data(), l);
	} else
		g_project->Open(filename.c_str(), pDriver, extopen, true, 0);
}

static void func_VirtualDub_intOpenTest(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	vdrefptr<IVDInputDriver> pDriver(VDCreateInputDriverTest());

	g_project->Open(L"", pDriver, false);
}

static void func_VirtualDub_Append(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	AppendAVI(filename.c_str());
}

static void func_VirtualDub_Close(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_project->Close();
}

static void func_VirtualDub_SaveFormat(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	if (arg_count > 1) {
		g_FileOutDriver = VDTextAToW(*arglist[0].asString());
		g_FileOutFormat = *arglist[1].asString();
	} else {
		g_FileOutDriver.clear();
		g_FileOutFormat.clear();
	}
}

static void func_VirtualDub_Preview(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	DubOptions opts(g_dubOpts);
	opts.fShowStatus			= false;
	g_project->Preview(&opts);
}

static void func_VirtualDub_RunNullVideoPass(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_project->RunNullVideoPass();
}

namespace {
	void InitBatchOptions(DubOptions& opts) {
		opts.fShowStatus			= false;
		opts.mbForceShowStatus		= VDPreferencesGetBatchShowStatusWindow();

		if (g_fJobMode) {
			opts.fMoveSlider			= true;
			opts.video.fShowInputFrame	= false;
			opts.video.fShowOutputFrame	= false;
			opts.video.fShowDecompressedFrame	= false;
		}
	}
}

static void func_VirtualDub_SaveAVI(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	if (g_FileOutDriver.empty()) {
		SaveAVI(filename.c_str(), true, &opts, false);
		return;
	}

	IVDOutputDriver *driver = VDGetOutputDriverByName(g_FileOutDriver.c_str());
	if (!driver) {
		VDString drv = VDTextWToA(g_FileOutDriver);
		throw MyError("Cannot save video with '%s': no such driver loaded", drv.c_str());
	}
	for(int i=0; ; i++){
		wchar_t filter[128];
		wchar_t ext[128];
		char name[128];
		if (!driver->GetDriver()->EnumFormats(i,filter,ext,name)) {
			VDString drv = VDTextWToA(g_FileOutDriver);
			throw MyError("Cannot save video with '%s / %s': no such format supported by driver", drv.c_str(), g_FileOutFormat.c_str());
		}

		if (g_FileOutFormat == name) {
			SavePlugin(filename.c_str(), driver, name, &opts);
			return;
		}
	}
}

static void func_VirtualDub_SaveCompatibleAVI(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	SaveAVI(filename.c_str(), true, &opts, true);
}

static void func_VirtualDub_SaveSegmentedAVI(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	int digits = 2;
	if (arg_count >= 4)
		digits = arglist[3].asInt();

	SaveSegmentedAVI(filename.c_str(), true, &opts, arglist[1].asInt(), arglist[2].asInt(), digits);
}

static void func_VirtualDub_SaveImageSequence(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW prefix(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	const VDStringW suffix(VDTextU8ToW(VDStringA(*arglist[1].asString())));

	int q = 95;

	if (arg_count >= 5)
		q = arglist[4].asInt();

	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	SaveImageSequence(prefix.c_str(), suffix.c_str(), arglist[2].asInt(), true, &opts, arglist[3].asInt(), q);
}

static void func_VirtualDub_SaveWAV(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	SaveWAV(filename.c_str(), true, &opts);
}

static void func_VirtualDub_SaveRawAudio(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	g_project->SaveRawAudio(filename.c_str(), true, &opts);
}

static void func_VirtualDub_SaveRawVideo(IVDScriptInterpreter *isi, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	VDAVIOutputRawVideoFormat format;
	format.mOutputFormat = arglist[1].asInt();

	if (!format.mOutputFormat || format.mOutputFormat >= nsVDPixmap::kPixFormat_Max_Standard
		|| format.mOutputFormat <= nsVDPixmap::kPixFormat_Pal8)
	{
		EXT_SCRIPT_ERROR(FCALL_OUT_OF_RANGE);
	}

	format.mScanlineAlignment = arglist[2].asInt();
	if (format.mScanlineAlignment == 0 || format.mScanlineAlignment > 1024) {
		EXT_SCRIPT_ERROR(FCALL_OUT_OF_RANGE);
	}

	format.mbSwapChromaPlanes = arglist[3].asInt() != 0;
	format.mbBottomUp = arglist[4].asInt() != 0;

	g_project->SaveRawVideo(filename.c_str(), format, true, &opts);
}

static void func_VirtualDub_SaveAnimatedGIF(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	g_project->SaveAnimatedGIF(filename.c_str(), arglist[1].asInt(), true, &opts);
}

static void func_VirtualDub_SaveAnimatedPNG(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	g_project->SaveAnimatedPNG(filename.c_str(), arglist[1].asInt(), arglist[2].asInt(), arglist[3].asInt(), true, &opts);
}

static void func_VirtualDub_ExportViaEncoderSet(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW filename(VDTextU8ToW(VDStringA(*arglist[0].asString())));
	const VDStringW encSetName(VDTextU8ToW(VDStringA(*arglist[1].asString())));
	DubOptions opts(g_dubOpts);
	InitBatchOptions(opts);

	g_project->ExportViaEncoder(filename.c_str(), encSetName.c_str(), true, &opts);
}

static void func_VirtualDub_Log(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	const VDStringW text(VDTextU8ToW(VDStringA(*arglist[0].asString())));

	VDLog(kVDLogInfo, text);
}

static void func_VirtualDub_Exit(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_returnCode = arglist[0].asInt();
	PostQuitMessage(0);
}

static void func_VirtualDub_StartServer(IVDScriptInterpreter *, VDScriptValue *arglist, int arg_count) {
	g_project->StartServer(*arglist[0].asString());
}

extern "C" unsigned long version_num;

static VDScriptValue obj_VirtualDub_lookup(IVDScriptInterpreter *isi, const VDScriptObject *obj, void *lpVoid, char *szName) {
	if (!strcmp(szName, "version")) {
		const VDStringA& s = VDTextWToA(VDLoadStringW32(IDS_TITLE_INITIAL, true));

		return isi->DupCString(s.c_str());
	} else if (!strcmp(szName, "video"))
		return VDScriptValue(NULL, &obj_VDVideo);
	else if (!strcmp(szName, "audio"))
		return VDScriptValue(NULL, &obj_VDAudio);
	else if (!strcmp(szName, "subset"))
		return VDScriptValue(NULL, &obj_VDSubset);
	else if (!strcmp(szName, "project"))
		return VDScriptValue(NULL, &obj_VDProject);
	else if (!strcmp(szName, "params"))
		return VDScriptValue(NULL, &obj_VDParams);

	VDSCRIPT_EXT_ERROR(MEMBER_NOT_FOUND);
}

static const VDScriptFunctionDef obj_VirtualDub_functbl[]={
	{ func_VirtualDub_SetStatus,			"SetStatus",			"0s" },
	{ func_VirtualDub_OpenOld,			"Open",					"0sii" },
	{ func_VirtualDub_OpenOld,			NULL,					"0siis" },
	{ func_VirtualDub_Open,				NULL,					"0s" },
	{ func_VirtualDub_Open,				NULL,					"0ssi" },
	{ func_VirtualDub_Open,				NULL,					"0ssis" },
	{ func_VirtualDub_intOpenTest,		"__OpenTest",			"0i" },
	{ func_VirtualDub_Append,			"Append",				"0s" },
	{ func_VirtualDub_Close,				"Close",				"0" },
	{ func_VirtualDub_SaveFormat,			"SaveFormatAVI",		"0" },
	{ func_VirtualDub_SaveFormat,			"SaveFormat",			"0ss" },
	{ func_VirtualDub_Preview,			"Preview",				"0" },
	{ func_VirtualDub_RunNullVideoPass,	"RunNullVideoPass",		"0" },
	{ func_VirtualDub_SaveAVI,			"SaveAVI",				"0s" },
	{ func_VirtualDub_SaveCompatibleAVI, "SaveCompatibleAVI",	"0s" },
	{ func_VirtualDub_SaveSegmentedAVI,	"SaveSegmentedAVI",		"0sii" },
	{ func_VirtualDub_SaveSegmentedAVI,	NULL,					"0siii" },
	{ func_VirtualDub_SaveImageSequence,	"SaveImageSequence",	"0ssii" },
	{ func_VirtualDub_SaveImageSequence,	NULL,					"0ssiii" },
	{ func_VirtualDub_SaveWAV,			"SaveWAV",				"0s" },
	{ func_VirtualDub_SaveRawAudio,		"SaveRawAudio",			"0s" },
	{ func_VirtualDub_SaveRawVideo,		"SaveRawVideo",			"0siiii" },
	{ func_VirtualDub_SaveAnimatedGIF,	"SaveAnimatedGIF",		"0si" },
	{ func_VirtualDub_SaveAnimatedPNG,	"SaveAnimatedPNG",		"0siii" },
	{ func_VirtualDub_ExportViaEncoderSet,	"ExportViaEncoderSet",	"0ss" },
	{ func_VirtualDub_Log,				"Log",					"0s" },
	{ func_VirtualDub_Exit,				"Exit",					"0i" },
	{ func_VirtualDub_StartServer,		"StartServer",			"0s" },
	{ NULL }
};

static const VDScriptObject obj_VirtualDub={
	"VDApplication", &obj_VirtualDub_lookup, obj_VirtualDub_functbl
};

static VDScriptValue RootHandler(IVDScriptInterpreter *isi, char *szName, void *lpData) {
	if (!strcmp(szName, "VirtualDub"))
		return VDScriptValue(NULL, &obj_VirtualDub);

	VDSCRIPT_EXT_ERROR(VAR_NOT_FOUND);
}

 

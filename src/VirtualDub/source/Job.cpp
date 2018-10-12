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

#include "stdafx.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <vfw.h>

#include "resource.h"

#include <vd2/system/list.h>
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/registry.h>
#include <vd2/system/VDString.h>
#include <vd2/system/log.h>
#include <vd2/system/text.h>
#include <vd2/system/file.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/strutil.h>
#include <vd2/Dita/services.h>
#include "InputFile.h"
#include "AudioFilterSystem.h"
#include "DubOutput.h"

#include "uiframe.h"
#include "gui.h"
#include "job.h"
#include "command.h"
#include "dub.h"
#include "DubOutput.h"
#include "script.h"
#include "misc.h"
#include "project.h"
#include "filters.h"
#include "FilterInstance.h"
#include "oshelper.h"
#include "projectui.h"

#include "JobControl.h"

///////////////////////////////////////////////////////////////////////////

extern HINSTANCE g_hInst;
extern wchar_t g_szInputAVIFile[];
extern wchar_t g_szInputWAVFile[];
extern InputFileOptions *g_pInputOpts;
extern VDProject *g_project;
extern COMPVARS2 g_Vcompression;
extern VDJobQueue g_VDJobQueue;
extern vdrefptr<VDProjectUI> g_projectui;

///////////////////////////////////////////////////////////////////////////

static INT_PTR CALLBACK JobCtlDlgProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////////////////////////

VDStringA VDEncodeBase64A(const void *src, unsigned len) {
	unsigned enclen = (len+2)/3*4;
	std::vector<char> buf(enclen);

	membase64(&buf.front(), (const char *)src, len);

	VDStringA str(&buf.front(), enclen);

	return str;
}

///////////////////////////////////////////////////////////////////////////

class JobScriptOutput {
public:
	typedef vdfastvector<char> Script;

	JobScriptOutput();
	~JobScriptOutput();

	void clear();
	void write(const char *s, long l);
	void adds(const char *s);
	void addf(const char *fmt, ...);

	const char *data() const { return mScript.data(); }
	size_t size() const { return mScript.size(); }

	const Script& getscript();

protected:
	Script		mScript;
};

///////

JobScriptOutput::JobScriptOutput() {
	clear();
}

JobScriptOutput::~JobScriptOutput() {
	clear();
}

void JobScriptOutput::clear() {
	mScript.clear();
}

void JobScriptOutput::write(const char *s, long l) {
	mScript.insert(mScript.end(), s, s+l);
}

void JobScriptOutput::adds(const char *s) {
	write(s, strlen(s));
	write("\r\n",2);
}

void JobScriptOutput::addf(const char *fmt, ...) {
	char buf[256];
	char *bufptr = buf;

	va_list val;
	va_start(val, fmt);

	int len = _vsnprintf(buf, (sizeof buf) - 1, fmt, val);

	if ((unsigned)len >= sizeof buf) {
		len = _vscprintf(fmt, val);

		if (len >= 0) {
			bufptr = (char *)malloc(len + 1);

			len = _vsnprintf(bufptr, len, fmt, val);
		}
	}

	va_end(val);

	if (len < 0)
		throw MyInternalError("Unable to add formatted line to script.");

	bufptr[len]=0;

	adds(bufptr);

	if (bufptr && bufptr != buf)
		free(bufptr);
}

const JobScriptOutput::Script& JobScriptOutput::getscript() {
	return mScript;
}

///////////////////////////////////////////////////////////////////////////

namespace {
	enum VDJobEditListMode {
		kVDJobEditListMode_Omit,
		kVDJobEditListMode_Include,
		kVDJobEditListMode_Reset
	};
}

void JobCreateScript(JobScriptOutput& output, bool project_relative, const DubOptions *opt, VDJobEditListMode editListMode = kVDJobEditListMode_Include, bool bIncludeTextInfo = true) {
	char *mem= NULL;

	int audioSourceMode = g_project->GetAudioSourceMode();
	if (opt->removeAudio) audioSourceMode = kVDAudioSourceMode_None;

	switch(audioSourceMode) {

	case kVDAudioSourceMode_External:
		{
			VDStringW s(g_szInputWAVFile);
			if (project_relative) s = g_project->BuildProjectPath(g_szInputWAVFile);
			const VDStringA& encodedFileName = VDEncodeScriptString(s);
			const VDStringA& encodedDriverName = VDEncodeScriptString(VDTextWToU8(g_project->GetAudioSourceDriverName(), -1));

			// check if we have options to write out
			const InputFileOptions *opts = g_project->GetAudioSourceOptions();
			if (opts) {
				// get raw required length for options
				const int rawlen = opts->write(NULL, 0);

				if (rawlen >= 0) {
					// compute buffer size to needed for both raw and base64 string
					const size_t base64len = (((size_t)rawlen + 2) / 3) * 4 + 1;
					const size_t buflen = (size_t)rawlen + base64len;

					char buf[256];
					char *bufp = buf;

					vdblock<char> bufalloc;

					if (buflen > sizeof(buf)) {
						bufalloc.resize(buflen);
						bufp = bufalloc.data();
					}

					int len = opts->write(bufp, rawlen);

					if (len) {
						membase64(bufp+len, bufp, len);

						output.addf("VirtualDub.audio.SetSource(\"%s\", \"%s\", \"%s\");", encodedFileName.c_str(), encodedDriverName.c_str(), bufp+len);
						break;
					}
				}
			}

			// no options
			output.addf("VirtualDub.audio.SetSource(\"%s\", \"%s\");", encodedFileName.c_str(), encodedDriverName.c_str());
		}
		break;

	default:
		if (audioSourceMode >= kVDAudioSourceMode_Source) {
			int index = audioSourceMode - kVDAudioSourceMode_Source;

			if (!index)
				output.addf("VirtualDub.audio.SetSource(1);");
			else
				output.addf("VirtualDub.audio.SetSource(1,%d);", index);
			break;
		}
		// fall through
	case kVDAudioSourceMode_None:
		output.addf("VirtualDub.audio.SetSource(0);");
		break;
	
	}

	output.addf("VirtualDub.audio.SetMode(%d);", opt->audio.mode);

	output.addf("VirtualDub.audio.SetInterleave(%d,%d,%d,%d,%d);",
			opt->audio.enabled,
			opt->audio.preload,
			opt->audio.interval,
			opt->audio.is_ms,
			opt->audio.offset);

	output.addf("VirtualDub.audio.SetClipMode(%d,%d);",
			opt->audio.fStartAudio,
			opt->audio.fEndAudio);

	output.addf("VirtualDub.audio.SetEditMode(%d);", opt->audio.mbApplyVideoTimeline);

	output.addf("VirtualDub.audio.SetConversion(%d,%d,%d,0,%d);",
			opt->audio.new_rate,
			opt->audio.newPrecision,
			opt->audio.newChannels,
			opt->audio.fHighQuality);

	if (opt->audio.mVolume >= 0.0f)
		output.addf("VirtualDub.audio.SetVolume(%d);", VDRoundToInt(256.0f * opt->audio.mVolume));
	else
		output.addf("VirtualDub.audio.SetVolume();");

	if (g_ACompressionFormat) {
		if (g_ACompressionFormat->mExtraSize) {
			mem = (char *)allocmem(((g_ACompressionFormat->mExtraSize+2)/3)*4 + 1);
			if (!mem) throw MyMemoryError();

			membase64(mem, (char *)(g_ACompressionFormat+1), g_ACompressionFormat->mExtraSize);
			output.addf("VirtualDub.audio.SetCompressionWithHint(%d,%d,%d,%d,%d,%d,%d,\"%s\",\"%s\");"
						,g_ACompressionFormat->mTag
						,g_ACompressionFormat->mSamplingRate
						,g_ACompressionFormat->mChannels
						,g_ACompressionFormat->mSampleBits
						,g_ACompressionFormat->mDataRate
						,g_ACompressionFormat->mBlockSize
						,g_ACompressionFormat->mExtraSize
						,mem
						,VDEncodeScriptString(g_ACompressionFormatHint).c_str()
						);

			freemem(mem);
		} else
			output.addf("VirtualDub.audio.SetCompressionWithHint(%d,%d,%d,%d,%d,%d,\"%s\");"
						,g_ACompressionFormat->mTag
						,g_ACompressionFormat->mSamplingRate
						,g_ACompressionFormat->mChannels
						,g_ACompressionFormat->mSampleBits
						,g_ACompressionFormat->mDataRate
						,g_ACompressionFormat->mBlockSize
						,VDEncodeScriptString(g_ACompressionFormatHint).c_str()
						);

		long l = g_ACompressionConfig.size();

		if (l>0) {
			mem = (char *)allocmem(l + ((l+2)/3)*4 + 1);
			if (!mem) throw MyMemoryError();
			memcpy(mem,g_ACompressionConfig.data(),l);
			membase64(mem+l, mem, l);
			VDStringA line;
			line.sprintf("VirtualDub.audio.SetCompData(%d,\"", l);
			line += (mem+l);
			line += "\");";
			output.adds(line.c_str());
			freemem(mem);
		}
  
	} else
		output.addf("VirtualDub.audio.SetCompression();");

	output.addf("VirtualDub.audio.EnableFilterGraph(%d);", opt->audio.bUseAudioFilterGraph);

	output.addf("VirtualDub.video.SetInputFormat(%d);", opt->video.mInputFormat.format);
	if (!opt->video.mInputFormat.defaultMode())
		output.addf("VirtualDub.video.SetInputMatrix(%d,%d);", opt->video.mInputFormat.colorSpaceMode, opt->video.mInputFormat.colorRangeMode);
	output.addf("VirtualDub.video.SetOutputFormat(%d);", opt->video.mOutputFormat.format);
	if (!opt->video.mOutputFormat.defaultMode())
		output.addf("VirtualDub.video.SetOutputMatrix(%d,%d);", opt->video.mOutputFormat.colorSpaceMode, opt->video.mOutputFormat.colorRangeMode);
	if (opt->video.outputReference!=1)
		output.addf("VirtualDub.video.SetOutputReference(%d);", opt->video.outputReference);

	output.addf("VirtualDub.video.SetMode(%d);", opt->video.mode);
	output.addf("VirtualDub.video.SetSmartRendering(%d);", opt->video.mbUseSmartRendering);
	output.addf("VirtualDub.video.SetPreserveEmptyFrames(%d);", opt->video.mbPreserveEmptyFrames);

	output.addf("VirtualDub.video.SetFrameRate2(%u,%u,%d);",
			opt->video.mFrameRateAdjustHi,
			opt->video.mFrameRateAdjustLo,
			opt->video.frameRateDecimation);

	if (opt->video.frameRateTargetLo) {
		output.addf("VirtualDub.video.SetTargetFrameRate(%u,%u);",
				opt->video.frameRateTargetHi,
				opt->video.frameRateTargetLo);
	}

	output.addf("VirtualDub.video.SetIVTC(0, 0, 0, 0);");

	if ((g_Vcompression.dwFlags & ICMF_COMPVARS_VALID) && g_Vcompression.fccHandler) {
		if (g_Vcompression.driver && !g_Vcompression.driver->path.empty()) {
			VDStringW name = VDFileSplitPathRight(g_Vcompression.driver->path);
			output.addf("VirtualDub.video.SetCompression(0x%08lx,%d,%d,%d,\"%s\");",
					g_Vcompression.fccHandler,
					g_Vcompression.lKey,
					g_Vcompression.lQ,
					g_Vcompression.lDataRate,
					strCify(VDTextWToU8(name).c_str()));
		} else {
			output.addf("VirtualDub.video.SetCompression(0x%08lx,%d,%d,%d);",
					g_Vcompression.fccHandler,
					g_Vcompression.lKey,
					g_Vcompression.lQ,
					g_Vcompression.lDataRate);
		}

		long l = 0;
		if (g_Vcompression.driver)
			l = g_Vcompression.driver->getStateSize();

		if (l>0) {
			mem = (char *)allocmem(l + ((l+2)/3)*4 + 1);
			if (!mem) throw MyMemoryError();

			if (g_Vcompression.driver->getState(mem, l)<0) {
				freemem(mem);
//				throw MyError("Bad state data returned from compressor");

				// Fine then, be that way.  Stupid Pinnacle DV200 driver.
				mem = NULL;
			}

			if (mem) {
				membase64(mem+l, mem, l);
				// urk... Windows Media 9 VCM uses a very large configuration struct (~7K pre-BASE64).
				VDStringA line;
				line.sprintf("VirtualDub.video.SetCompData(%d,\"", l);
				line += (mem+l);
				line += "\");";
				output.adds(line.c_str());
				freemem(mem);
			}
		}

	} else
		output.addf("VirtualDub.video.SetCompression();");

	if (!g_FileOutDriver.empty()) {
		const VDStringA driver(strCify(VDTextWToU8(g_FileOutDriver).c_str()));
		output.addf("VirtualDub.SaveFormat(\"%s\",\"%s\");"
			,driver.c_str()
			,g_FileOutFormat.c_str()
			);

	} else
		output.addf("VirtualDub.SaveFormatAVI();");

	output.addf("VirtualDub.video.filters.BeginUpdate();");
	output.addf("VirtualDub.video.filters.Clear();");

	// Add video filters

	int iFilter = 0;
	for(VDFilterChainDesc::Entries::const_iterator it(g_filterChain.mEntries.begin()), itEnd(g_filterChain.mEntries.end());
		it != itEnd;
		++it)
	{
		VDFilterChainEntry *ent = *it;
		FilterInstance *fa = ent->mpInstance;

		output.addf("VirtualDub.video.filters.Add(\"%s\");", strCify(fa->GetName()));

		if (fa->IsCroppingEnabled()) {
			const vdrect32& cropInsets = fa->GetCropInsets();

			output.addf("VirtualDub.video.filters.instance[%d].SetClipping(%d,%d,%d,%d%s);"
						, iFilter
						, cropInsets.left
						, cropInsets.top
						, cropInsets.right
						, cropInsets.bottom
						, fa->IsPreciseCroppingEnabled() ? "" : ",0"
						);
		}
		
		if (fa->IsForceSingleFBEnabled()) {
			output.addf("VirtualDub.video.filters.instance[%d].SetForceSingleFBEnabled(true);", iFilter);
		}

		VDStringA dataPrefix = fa->fmProject.dataPrefix;
		if (!dataPrefix.empty())
			output.addf("VirtualDub.video.filters.instance[%d].DataPrefix(\"%s\");", iFilter, dataPrefix.c_str());

		const VDStringA& scriptStr = fa->mConfigString;
		if (!scriptStr.empty())
			output.addf("VirtualDub.video.filters.instance[%d].%s;", iFilter, scriptStr.c_str());

		if (!fa->IsEnabled())
			output.addf("VirtualDub.video.filters.instance[%d].SetEnabled(false);", iFilter);

		if (!ent->mOutputName.empty())
			output.addf("VirtualDub.video.filters.instance[%d].SetOutputName(\"%s\");", iFilter, VDEncodeScriptString(ent->mOutputName).c_str());

		if (!ent->mSources.empty()) {
			for(vdvector<VDStringA>::const_iterator it2(ent->mSources.begin()), it2End(ent->mSources.end());
				it2 != it2End;
				++it2)
			{
				const VDStringA& name = *it2;
				output.addf("VirtualDub.video.filters.instance[%d].AddInput(\"%s\");", iFilter, VDEncodeScriptString(name).c_str());
			}
		}

		if (fa->IsOpacityCroppingEnabled()) {
			const vdrect32& cropInsets = fa->GetOpacityCropInsets();

			output.addf("VirtualDub.video.filters.instance[%d].SetOpacityClipping(%d,%d,%d,%d);"
						, iFilter
						, cropInsets.left
						, cropInsets.top
						, cropInsets.right
						, cropInsets.bottom
						);
		}

		VDPosition rangeStart;
		VDPosition rangeEnd;
		fa->GetRangeFrames(rangeStart, rangeEnd);
		if (rangeEnd!=-1) {
			output.addf("VirtualDub.video.filters.instance[%d].SetRangeFrames(%I64d,%I64d);"
						, iFilter
						, rangeStart
						, rangeEnd
						);
		}
		
		VDParameterCurve *pc = fa->GetAlphaParameterCurve();
		if (pc) {
			output.addf("declare curve = VirtualDub.video.filters.instance[%d].AddOpacityCurve();", iFilter);

			const VDParameterCurve::PointList& pts = pc->Points();
			for(VDParameterCurve::PointList::const_iterator it(pts.begin()), itEnd(pts.end()); it!=itEnd; ++it) {
				const VDParameterCurvePoint& pt = *it;

				output.addf("curve.AddPoint(%.12g, %.12g, %d);", pt.mX, pt.mY, pt.mbLinear);
			}
		}

		++iFilter;
	}

	// trigger rescaling of timeline if needed (needed if we don't have subset info included).
	output.addf("VirtualDub.video.filters.EndUpdate();");

	// Add audio filters

	{
		VDAudioFilterGraph::FilterList::const_iterator it(g_audioFilterGraph.mFilters.begin()), itEnd(g_audioFilterGraph.mFilters.end());
		int connidx = 0;
		int srcfilt = 0;

		output.addf("VirtualDub.audio.filters.Clear();");

		for(; it!=itEnd; ++it, ++srcfilt) {
			const VDAudioFilterGraph::FilterEntry& fe = *it;

			output.addf("VirtualDub.audio.filters.Add(\"%s\");", strCify(VDTextWToU8(fe.mFilterName).c_str()));

			for(unsigned i=0; i<fe.mInputPins; ++i) {
				const VDAudioFilterGraph::FilterConnection& conn = g_audioFilterGraph.mConnections[connidx++];
				output.addf("VirtualDub.audio.filters.Connect(%d, %d, %d, %d);", conn.filt, conn.pin, srcfilt, i);
			}

			VDPluginConfig::const_iterator itc(fe.mConfig.begin()), itcEnd(fe.mConfig.end());

			for(; itc!=itcEnd; ++itc) {
				const unsigned idx = (*itc).first;
				const VDPluginConfigVariant& var = (*itc).second;

				switch(var.GetType()) {
				case VDPluginConfigVariant::kTypeU32:
					output.addf("VirtualDub.audio.filters.instance[%d].SetInt(%d, %d);", srcfilt, idx, var.GetU32());
					break;
				case VDPluginConfigVariant::kTypeS32:
					output.addf("VirtualDub.audio.filters.instance[%d].SetInt(%d, %d);", srcfilt, idx, var.GetS32());
					break;
				case VDPluginConfigVariant::kTypeU64:
					output.addf("VirtualDub.audio.filters.instance[%d].SetLong(%d, %I64d);", srcfilt, idx, var.GetU64());
					break;
				case VDPluginConfigVariant::kTypeS64:
					output.addf("VirtualDub.audio.filters.instance[%d].SetLong(%d, %I64d);", srcfilt, idx, var.GetS64());
					break;
				case VDPluginConfigVariant::kTypeDouble:
					output.addf("VirtualDub.audio.filters.instance[%d].SetDouble(%d, %g);", srcfilt, idx, var.GetDouble());
					break;
				case VDPluginConfigVariant::kTypeAStr:
					output.addf("VirtualDub.audio.filters.instance[%d].SetString(%d, \"%s\");", srcfilt, idx, strCify(VDTextWToU8(VDTextAToW(var.GetAStr())).c_str()));
					break;
				case VDPluginConfigVariant::kTypeWStr:
					output.addf("VirtualDub.audio.filters.instance[%d].SetString(%d, \"%s\");", srcfilt, idx, strCify(VDTextWToU8(var.GetWStr(), -1).c_str()));
					break;
				case VDPluginConfigVariant::kTypeBlock:
					output.addf("VirtualDub.audio.filters.instance[%d].SetBlock(%d, %d, \"%s\");", srcfilt, idx, var.GetBlockLen(), VDEncodeBase64A(var.GetBlockPtr(), var.GetBlockLen()).c_str());
					break;
				}
			}
		}
	}

	// Add subset information

	switch(editListMode) {
		case kVDJobEditListMode_Include:
			{
				const FrameSubset& fs = g_project->GetTimeline().GetSubset();

				output.addf("VirtualDub.subset.Clear();");

				for(FrameSubset::const_iterator it(fs.begin()), itEnd(fs.end()); it!=itEnd; ++it)
					output.addf("VirtualDub.subset.Add%sRange(%I64d,%I64d);", it->bMask ? "Masked" : "", it->start, it->len);

				// Note that this must be AFTER the subset (we used to place it before, which was a bug).
				if (g_project->IsSelectionPresent()) {
					output.addf("VirtualDub.video.SetRangeFrames(%I64d,%I64d);",
						g_project->GetSelectionStartFrame(),
						g_project->GetSelectionEndFrame());
				} else {
					output.addf("VirtualDub.video.SetRange();");
				}

				VDPosition z0,z1;
				if (g_project->GetZoomRange(z0,z1)) {
					output.addf("VirtualDub.video.SetZoomFrames(%I64d,%I64d);", z0, z1);
				}

				const vdfastvector<sint64>& marker = g_project->GetTimeline().GetMarker();
				{for(int i=0; i<marker.size(); i++){
					output.addf("VirtualDub.video.AddMarker(%I64d);", marker[i]);
				}}
			}
			break;

		case kVDJobEditListMode_Reset:
			output.addf("VirtualDub.subset.Delete();");
			break;

		case kVDJobEditListMode_Omit:
			break;
	}

	// Add text information
	if (bIncludeTextInfo) {
		typedef std::list<std::pair<uint32, VDStringA> > tTextInfo;
		const tTextInfo& textInfo = g_project->GetTextInfo();

		output.addf("VirtualDub.project.ClearTextInfo();");
		for(tTextInfo::const_iterator it(textInfo.begin()), itEnd(textInfo.end()); it!=itEnd; ++it) {
			char buf[5]={0};
			
			memcpy(buf, &(*it).first, 4);

			output.addf("VirtualDub.project.AddTextInfo(\"%s\", \"%s\");", buf, VDEncodeScriptString((*it).second).c_str());
		}
	}
}

void JobAddConfigurationInputs(JobScriptOutput& output, const VDProject* project, const wchar_t *szFileInput, const wchar_t *pszInputDriver, int inputFlags, List2<InputFilenameNode> *pListAppended) {
	do {
		VDStringW s(szFileInput);
		if (project) s = project->BuildProjectPath(szFileInput);
		const VDStringA filename(strCify(VDTextWToU8(s).c_str()));
		const char* funcName = "VirtualDub.Open";
		if (inputFlags!=-1 && (inputFlags & IVDInputDriver::kFF_Sequence))
		funcName = "VirtualDub.OpenSequence";

		if (pszInputDriver && g_pInputOpts) {
			int req = g_pInputOpts->write(NULL, 0);

			vdfastvector<char> srcbuf(req);

			int srcsize = g_pInputOpts->write(srcbuf.data(), req);

			if (srcsize) {
				vdfastvector<char> encbuf((srcsize + 2) / 3 * 4 + 1);
				membase64(encbuf.data(), srcbuf.data(), srcsize);

				output.addf("%s(\"%s\",\"%s\",0,\"%s\");", funcName, filename.c_str(), pszInputDriver?strCify(VDTextWToU8(VDStringW(pszInputDriver)).c_str()):"", encbuf.data());
				break;
			}
		}

		output.addf("%s(\"%s\",\"%s\",0);", funcName, filename.c_str(), pszInputDriver?strCify(VDTextWToU8(VDStringW(pszInputDriver)).c_str()):"");

	} while(false);

	if (pListAppended) {
		InputFilenameNode *ifn = pListAppended->AtHead(), *ifn_next;

		if (ifn = ifn->NextFromHead())
			while(ifn_next = ifn->NextFromHead()) {
				VDStringW s(ifn->name);
				if (project) s = project->BuildProjectPath(ifn->name);
				output.addf("VirtualDub.Append(\"%s\");", strCify(VDTextWToU8(s).c_str()));
				ifn = ifn_next;
			}
	}
}

static void JobAddReloadMarker(JobScriptOutput& output) {
	output.adds("  // -- $reloadstop --");
}

static void JobAddClose(JobScriptOutput& output) {
	output.adds("VirtualDub.audio.SetSource(1);");		// required to close a WAV file
	output.adds("VirtualDub.Close();");
}

static void JobCreateEntry(JobScriptOutput& output, const VDProject* project, const VDStringW& dataSubdir, const wchar_t *inputPath, const wchar_t *outputPath) {
	vdautoptr<VDJob> vdj(new VDJob);
	vdj->SetInputFile(inputPath);

	if (outputPath)
		vdj->SetOutputFile(outputPath);

	if (project) {
		vdj->SetName(project->mProjectName.c_str());
		vdj->SetProjectSubdir(VDTextWToU8(dataSubdir).c_str());
	}

	const JobScriptOutput::Script& script = output.getscript();
	vdj->SetScript(script.data(), script.size(), true);
	g_VDJobQueue.Add(vdj, false);
	vdj.release();
}

void JobFlushFilterConfig() {
	for(VDFilterChainDesc::Entries::const_iterator it(g_filterChain.mEntries.begin()), itEnd(g_filterChain.mEntries.end());
		it != itEnd;
		++it)
	{
		VDFilterChainEntry *ent = *it;
		FilterInstance *fa = ent->mpInstance;

		VDStringA scriptStr;
		if (fa->GetScriptString(scriptStr)) {
			fa->mConfigString = scriptStr;
		} else {
			fa->mConfigString.clear();
		}
	}
}

void JobSaveProjectData(const VDProject* project, VDStringW& dataSubdir) {
	if (project) {
		VDStringW filename(g_VDJobQueue.GetJobFilePath());
		dataSubdir = L"job";
		project->SaveData(filename, dataSubdir, true);
	} else {
		JobFlushFilterConfig();
	}
}

void JobAddConfiguration(const VDProject* project, const DubOptions *opt, const wchar_t *szFileInput, const wchar_t *pszInputDriver, int inputFlags, const wchar_t *szFileOutput, bool fCompatibility, List2<InputFilenameNode> *pListAppended, long lSpillThreshold, long lSpillFrameThreshold, bool bIncludeEditList, int digits) {
	JobScriptOutput output;

	VDStringW dataSubdir;
	JobSaveProjectData(project,dataSubdir);

	JobAddConfigurationInputs(output, 0, szFileInput, pszInputDriver, inputFlags, pListAppended);
	JobCreateScript(output, false, opt, bIncludeEditList ? kVDJobEditListMode_Include : kVDJobEditListMode_Reset);
	JobAddReloadMarker(output);

	// Add actual run option
	if (lSpillThreshold)
		output.addf("VirtualDub.SaveSegmentedAVI(\"%s\", %d, %d, %d);", strCify(VDTextWToU8(VDStringW(szFileOutput)).c_str()), lSpillThreshold, lSpillFrameThreshold, digits);
	else
		output.addf("VirtualDub.Save%sAVI(\"%s\");", fCompatibility ? "Compatible" : "", strCify(VDTextWToU8(VDStringW(szFileOutput)).c_str()));

	JobAddClose(output);
	JobCreateEntry(output, project, dataSubdir, szFileInput, szFileOutput);
}

void JobAddConfigurationImages(const VDProject* project, const DubOptions *opt, const wchar_t *szFileInput, const wchar_t *pszInputDriver, int inputFlags, const wchar_t *szFilePrefix, const wchar_t *szFileSuffix, int minDigits, int imageFormat, int quality, List2<InputFilenameNode> *pListAppended) {
	JobScriptOutput output;

	VDStringW dataSubdir;
	JobSaveProjectData(project,dataSubdir);

	JobAddConfigurationInputs(output, 0, szFileInput, pszInputDriver, inputFlags, pListAppended);
	JobCreateScript(output, false, opt);
	JobAddReloadMarker(output);

	// Add actual run option
	VDStringA s(strCify(VDTextWToU8(VDStringW(szFilePrefix)).c_str()));

	output.addf("VirtualDub.SaveImageSequence(\"%s\", \"%s\", %d, %d, %d);", s.c_str(), strCify(VDTextWToU8(VDStringW(szFileSuffix)).c_str()), minDigits, imageFormat, quality);

	JobAddClose(output);

	VDStringW outputFile;
	outputFile.sprintf(L"%ls*%ls", szFilePrefix, szFileSuffix);
	JobCreateEntry(output, project, dataSubdir, szFileInput, outputFile.c_str());
}

void JobAddConfigurationSaveAudio(const VDProject* project, const DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, int inputFlags, List2<InputFilenameNode> *pListAppended, const wchar_t *dstFile, bool raw, bool includeEditList, bool auto_w64) {
	JobScriptOutput output;

	VDStringW dataSubdir;
	JobSaveProjectData(project,dataSubdir);

	JobAddConfigurationInputs(output, 0, srcFile, srcInputDriver, inputFlags, pListAppended);
	JobCreateScript(output, false, opt, includeEditList ? kVDJobEditListMode_Include : kVDJobEditListMode_Reset);
	JobAddReloadMarker(output);

	// Add actual run option
	VDString name = strCify(VDTextWToU8(VDStringW(dstFile)));
	if (raw) {
		output.addf("VirtualDub.SaveRawAudio(\"%s\");", name);
	} else {
		if (auto_w64)
			output.addf("VirtualDub.SaveWAV(\"%s\");", name);
		else
			output.addf("VirtualDub.SaveWAV(\"%s\", 0);", name);
	}

	JobAddClose(output);
	JobCreateEntry(output, project, dataSubdir, srcFile, dstFile);
}


void JobAddConfigurationSaveVideo(const VDProject* project, const DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, int inputFlags, List2<InputFilenameNode> *pListAppended, const wchar_t *dstFile, bool includeEditList, const VDAVIOutputRawVideoFormat& format) {
	JobScriptOutput output;

	VDStringW dataSubdir;
	JobSaveProjectData(project,dataSubdir);

	JobAddConfigurationInputs(output, 0, srcFile, srcInputDriver, inputFlags, pListAppended);
	JobCreateScript(output, false, opt, includeEditList ? kVDJobEditListMode_Include : kVDJobEditListMode_Reset);
	JobAddReloadMarker(output);

	// Add actual run option
	output.addf("VirtualDub.SaveRawVideo(\"%s\", %u, %u, %u, %u);"
		, strCify(VDTextWToU8(VDStringW(dstFile)).c_str())
		, format.mOutputFormat
		, format.mScanlineAlignment
		, format.mbSwapChromaPlanes
		, format.mbBottomUp
		);

	JobAddClose(output);
	JobCreateEntry(output, project, dataSubdir, srcFile, dstFile);
}

void JobAddConfigurationExportViaEncoder(const VDProject* project, const DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, int inputFlags, List2<InputFilenameNode> *pListAppended, const wchar_t *dstFile, bool includeEditList, const wchar_t *encSetName) {
	JobScriptOutput output;

	VDStringW dataSubdir;
	JobSaveProjectData(project,dataSubdir);

	JobAddConfigurationInputs(output, 0, srcFile, srcInputDriver, inputFlags, pListAppended);
	JobCreateScript(output, false, opt, includeEditList ? kVDJobEditListMode_Include : kVDJobEditListMode_Reset);
	JobAddReloadMarker(output);

	// Add actual run option
	output.addf("VirtualDub.ExportViaEncoderSet(\"%s\", \"%s\");"
		, VDEncodeScriptString(VDTextWToU8(VDStringW(dstFile))).c_str()
		, VDEncodeScriptString(VDTextWToU8(VDStringW(encSetName))).c_str()
		);

	JobAddClose(output);
	JobCreateEntry(output, project, dataSubdir, srcFile, dstFile);
}

void JobAddConfigurationRunVideoAnalysisPass(const VDProject* project, const DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, int inputFlags, List2<InputFilenameNode> *pListAppended, bool includeEditList) {
	JobScriptOutput output;

	VDStringW dataSubdir;
	JobSaveProjectData(project,dataSubdir);

	JobAddConfigurationInputs(output, 0, srcFile, srcInputDriver, inputFlags, pListAppended);
	JobCreateScript(output, false, opt, includeEditList ? kVDJobEditListMode_Include : kVDJobEditListMode_Reset);
	JobAddReloadMarker(output);

	// Add actual run option
	output.adds("VirtualDub.RunNullVideoPass();");

	JobAddClose(output);
	JobCreateEntry(output, project, dataSubdir, srcFile, NULL);
}

void JobWriteProjectScript(VDFile& f, const VDProject* project, bool project_relative, const VDStringW& dataSubdir, DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, int inputFlags, List2<InputFilenameNode> *pListAppended) {
	JobScriptOutput output;

	output.adds("// VirtualDub project (Sylia script format)");
	output.adds("// This is a program generated file -- edit at your own risk.");
	output.adds("");
	output.addf("// $job \"%s\"", project->mProjectName.c_str());
 	output.addf("// $data \"%s\"", VDTextWToU8(dataSubdir).c_str());
	output.adds("// $script");
	output.adds("");

	if (srcFile) JobAddConfigurationInputs(output, project_relative ? project:0, srcFile, srcInputDriver, inputFlags, pListAppended);
	JobCreateScript(output, project_relative, opt, kVDJobEditListMode_Include, true);

	output.adds("");
	output.adds("// $endjob");

	f.write(output.data(), output.size());
}

void JobWriteConfiguration(const wchar_t *filename, DubOptions *opt, bool bIncludeEditList, bool bIncludeTextInfo) {
	JobScriptOutput output;

	JobFlushFilterConfig();
	JobCreateScript(output, false, opt, bIncludeEditList ? kVDJobEditListMode_Include : kVDJobEditListMode_Omit, bIncludeTextInfo);

	VDFile f(filename, nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kCreateAlways);
	f.write(output.data(), output.size());
}

///////////////////////////////////////////////////////////////////////////

bool InitJobSystem() {
	g_VDJobQueue.SetDefaultJobFilePath(VDMakePath(VDGetDataPath(), L"VirtualDub.jobs").c_str());
	g_VDJobQueue.SetJobFilePath(NULL, false, false);

	return true;
}

void DeinitJobSystem() {
	try {
		if (g_VDJobQueue.IsModified())
			g_VDJobQueue.Flush();
	} catch(const MyError&) {
		// eat flush errors
	}

	g_VDJobQueue.Shutdown();
}

void JobLockDubber() {
	g_VDJobQueue.SetBlocked(true);
}

void JobUnlockDubber() {
	g_VDJobQueue.SetBlocked(false);
}

void JobClearList() {
	g_VDJobQueue.ListClear();
}

bool JobRunList() {
	g_VDJobQueue.RunAllStart();

	while(g_VDJobQueue.IsRunAllInProgress()) {
		MSG msg;

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				g_VDJobQueue.RunAllStop();
				PostQuitMessage(msg.wParam);
				return false;
			}

			if (guiCheckDialogs(&msg))
				continue;

			if (VDUIFrame::TranslateAcceleratorMessage(msg))
				continue;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!JobPollAutoRun()) {
			VDClearEvilCPUStates();		// clear evil CPU states set by Borland DLLs

			WaitMessage();
		}
	}
	return true;
}

bool JobPollAutoRun() {
	return g_VDJobQueue.PollAutoRun();
}

void JobSetQueueFile(const wchar_t *filename, bool distributed, bool autorun) {
	g_VDJobQueue.SetAutoRunEnabled(false);
	g_VDJobQueue.SetJobFilePath(filename, distributed, distributed);
	g_VDJobQueue.SetAutoRunEnabled(autorun);
}

void JobAddBatchFile(const wchar_t *lpszSrc, const wchar_t *lpszDst) {
	JobAddConfiguration(0, &g_dubOpts, lpszSrc, 0, 0, lpszDst, false, NULL, 0, 0, false, true);
}

void JobAddBatchDirectory(const wchar_t *lpszSrc, const wchar_t *lpszDst) {
	// Scan source directory

	HANDLE				h;
	WIN32_FIND_DATA		wfd;
	wchar_t *s, *t;
	wchar_t szSourceDir[MAX_PATH], szDestDir[MAX_PATH];

	wcsncpyz(szSourceDir, lpszSrc, MAX_PATH);
	wcsncpyz(szDestDir, lpszDst, MAX_PATH);

	s = szSourceDir;
	t = szDestDir;

	if (*s) {

		// If the path string is just \ or starts with x: or ends in a slash
		// then don't append a slash

		while(*s) ++s;

		if ((s==szSourceDir || s[-1]!=L'\\') && (!isalpha(szSourceDir[0]) || szSourceDir[1]!=L':' || szSourceDir[2]))
			*s++ = L'\\';

	}
	
	if (*t) {

		// If the path string is just \ or starts with x: or ends in a slash
		// then don't append a slash

		while(*t) ++t;

		if ((t==szDestDir || t[-1]!=L'\\') && (!isalpha(szDestDir[0]) || szDestDir[1]!=L':' || szDestDir[2]))
			*t++ = L'\\';

	}

	wcscpy(s, L"*.*");

	h = FindFirstFile(VDTextWToA(szSourceDir).c_str(),&wfd);

	if (INVALID_HANDLE_VALUE != h) {
		do {
			if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				wchar_t *t2, *dot = NULL;

				VDTextAToW(s, (szSourceDir+MAX_PATH) - s, wfd.cFileName, -1);
				VDTextAToW(t, (szDestDir+MAX_PATH) - t, wfd.cFileName, -1);

				// Replace extension with .avi

				t2 = t;
				while(*t2) if (*t2++ == '.') dot = t2;

				if (dot)
					wcscpy(dot, L"avi");
				else
					wcscpy(t2, L".avi");

				// Add job!

				JobAddConfiguration(0, &g_dubOpts, szSourceDir, 0, 0, szDestDir, false, NULL, 0, 0, false, true);
			}
		} while(FindNextFile(h,&wfd));
		FindClose(h);
	}
}

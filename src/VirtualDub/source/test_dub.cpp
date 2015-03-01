#include "stdafx.h"
#if 0

#include <windows.h>
#include <math.h>
#include <vd2/system/memory.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/error.h>
#include <vd2/system/binary.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/plugin/vdvideofiltold.h>
#include "InputFile.h"
#include "VBitmap.h"
#include "VideoFilterSystem.h"
#include "FrameSubset.h"
#include "Dub.h"
#include "DubOutput.h"

namespace {
	bool pump() {
		MSG msg;
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				return false;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return true;
	}

	class FooThread : public VDThread {
		void ThreadRun() { for(;;) Sleep(1); }
	};
}

extern const VDPluginInfo		vpluginDef_input;
extern const VDPluginInfo		vpluginDef_test;
extern const VDPluginInfo		vpluginDef_adapter;
extern const VDPluginInfo		vpluginDef_avsadapter;
extern FilterDefinition filterDef_resize;

void VDTestVideoFilters() {
	CPUEnableExtensions(CPUCheckForExtensions());
	VDFastMemcpyAutodetect();

	try {
		vdrefptr<InputFile> pFile1;

		VDOpenMediaFile(L"d:\\anime-dl\\The Melancholy of Suzumiya Haruhi OP.avi", 0, ~pFile1);
		vdrefptr<IVDVideoSource> pSource1;
		pFile1->GetVideoSource(0, ~pSource1);
		pSource1->setDecompressedFormat(32);

		VDPosition len1 = pSource1->asStream()->getLength();

		FrameSubset fs;

		for(VDPosition i=0; i<len1; i += 240)
			fs.addRange(i, std::min<VDPosition>(len1-i, 120), false, 0);

		vdautoptr<VDAVIOutputFileSystem> os(new VDAVIOutputFileSystem);
		os->SetFilename(L"e:\\test\\test.avi");
		os->SetBuffer(1048576);

		DubOptions opts(g_dubOpts);

		opts.video.mbUseSmartEncoding = true;

		vdautoptr<IDubber> dubber(CreateDubber(&opts));

		COMPVARS vars;

		vars.cbSize = sizeof(COMPVARS);
		vars.cbState = 0;
		vars.dwFlags = ICMF_COMPVARS_VALID;
		vars.fccHandler = VDMAKEFOURCC('d','i','v', 'x');
		vars.fccType = VDMAKEFOURCC('v', 'i', 'd', 'c');
		vars.hic = ICOpen(vars.fccType, vars.fccHandler, ICMODE_COMPRESS);
		vars.lDataRate = 0;
		vars.lFrame = 0;
		vars.lKey = 0;
		vars.lKeyCount = 0;
		vars.lpbiIn = NULL;
		vars.lpbiOut = NULL;
		vars.lpBitsOut = NULL;
		vars.lpBitsPrev = NULL;
		vars.lpState = NULL;
		vars.lQ = 0;

		IVDVideoSource *sources[1]={pSource1};
		vdrefptr<AudioSource> asrc;
		pFile1->GetAudioSource(0, ~asrc);
		dubber->Init(sources, 1, &asrc, 1, os, &vars, &fs);
		dubber->Go();

		FooThread blah;
		blah.ThreadStart();

		while(dubber->isRunning()) {
			if (!pump())
				break;

			WaitMessage();
		}

		dubber->Stop();

	} catch(const MyError& e) {
		e.post(NULL, "shimatta...");
	}
}

extern void (*g_pPostInitRoutine)();

struct runtests {
	runtests() {
		g_pPostInitRoutine = VDTestVideoFilters;
	}
} g_runtests;

#endif

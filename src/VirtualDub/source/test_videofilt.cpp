#include "stdafx.h"
#if 0

#include <windows.h>
#include <math.h>
#include "VideoDisplay.h"
#include <vd2/system/memory.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/error.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/plugin/vdvideofiltold.h>
#include "InputFile.h"
#include "VBitmap.h"
#include "VideoFilterSystem.h"

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
}

extern const VDPluginInfo		vpluginDef_input;
extern const VDPluginInfo		vpluginDef_test;
extern const VDPluginInfo		vpluginDef_adapter;
extern const VDPluginInfo		vpluginDef_avsadapter;
extern FilterDefinition filterDef_resize;

void VDTestVideoFilters() {
	CPUEnableExtensions(CPUCheckForExtensions());
	VDFastMemcpyAutodetect();

	VDRegisterVideoDisplayControl();

	HWND hwndDisp = CreateWindow(VIDEODISPLAYCONTROLCLASS, "Kasumi onee-sama", WS_VISIBLE|WS_POPUP, 0, 0, 1024, 768, NULL, NULL, GetModuleHandle(NULL), NULL);
	IVDVideoDisplay *pDisp = VDGetIVideoDisplay(hwndDisp);

	IVDInputDriver *pInputDriver = VDGetInputDriverByName(L"MPEG-1 input driver (internal)");

	InputFile *pFile = pInputDriver->CreateInputFile(0);
	pFile->Init(L"e:\\anime\\Vandread OP - Trust.mpg");
	vdrefptr<IVDVideoSource> pSource;
	pFile->GetVideoSource(0, ~pSource);
	pSource->setDecompressedFormat(32);

//	VBitmap src(pSource->getFrameBuffer(), pSource->getDecompressedFormat());
//	pDisp->SetSourcePersistent(VDAsPixmap(src));

	VDPosition len = pSource->asStream()->getLength();

	vdautoptr<IVDVideoFilterSystem> pfiltsys(VDCreateVideoFilterSystem());
	VDScheduler scheduler;

	class SchedulerThread : public VDThread {
	public:
		SchedulerThread(VDScheduler& s) : VDThread("Video filter thread"), mScheduler(s), mbRunning(true) {}
		~SchedulerThread() {
			Stop();
		}

		void ThreadRun() {
			while(mbRunning) {
				if (!mScheduler.Run())
					Sleep(1);
			}
		}

		void Stop() {
			mbRunning = false;
			ThreadWait();
		}

	protected:
		VDScheduler& mScheduler;
		VDAtomicInt		mbRunning;
	} schthread(scheduler);

	schthread.ThreadStart();

	try {
		pfiltsys->SetScheduler(&scheduler);
		IVDVideoFilterInstance *pInputFilter = pfiltsys->CreateFilter(&vpluginDef_input, pSource);
		IVDVideoFilterInstance *pFilter = pfiltsys->CreateFilter(&vpluginDef_adapter, &filterDef_resize);
	//	IVDVideoFilterInstance *pFilter = pfiltsys->CreateFilter(&vpluginDef_avsadapter, L"c:\\avsfiltsrc\\debug\\tweak.dll");

		pfiltsys->Connect(pInputFilter, pFilter, 0);

		pFilter->Config((VDGUIHandle)hwndDisp);

		pfiltsys->Prepare();

		const VDPixmap& pxf = pFilter->GetFormat();

		SetWindowPos(hwndDisp, NULL, 0, 0, pxf.w, pxf.h, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);

		pfiltsys->Start();

	//	for(VDPosition i=0; i<len; ++i) {
		double t = 0;

		for(;;) {
			if (!pump())
				break;

			VDPosition i = (VDPosition)(8.0 * (1.0 + sin(t)));

			IVDVideoFrameRequest *pReq = pFilter->RequestFrame(i, NULL, 0);

			while(!pReq->IsReady())
				Sleep(1);

			VDVideoFilterFrame *pFrame = pReq->GetFrame();

			pDisp->SetSource(true, *pFrame->mpPixmap);

			pReq->Release();
			Sleep(30);

			t += 0.1;
		}

		pfiltsys->Stop();
		pfiltsys->Clear();
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

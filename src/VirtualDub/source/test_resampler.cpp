#include <stdafx.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/time.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/resample.h>
#include <vd2/Dita/interface.h>

namespace {
	void CreateTextPixmap(VDPixmap& dst) {
		uint32 *p = (uint32 *)dst.data;

		for(unsigned h=0; h<dst.h; ++h) {
			for(unsigned w=0; w<dst.h; ++w)
				p[w] = ((w^h)&1?0xff00:0) + (w*255/(dst.w-1)) + ((h*255/(dst.h-1))<<16);

			vdptrstep(p, dst.pitch);
		}
	}
}

void VDBenchmarkResampler(VDGUIHandle h) {
	CPUEnableExtensions(CPUCheckForExtensions());

	// benchmark
	VDPixmapBuffer src(320, 240, nsVDPixmap::kPixFormat_XRGB8888);
	VDPixmapBuffer dst(1024, 768, nsVDPixmap::kPixFormat_XRGB8888);

	CreateTextPixmap(src);

	uint64 results[IVDPixmapResampler::kFilterCount];
	vdautoptr<IVDPixmapResampler> resampler(VDCreatePixmapResampler());

	for(unsigned i=0; i<IVDPixmapResampler::kFilterCount; ++i) {
		IVDPixmapResampler::FilterMode mode = (IVDPixmapResampler::FilterMode)i;

		uint64 fastest = (uint64)(sint64)-1;

		for(unsigned attempt=0; attempt<5; ++attempt) {
			uint64 start = VDGetPreciseTick();
			resampler->SetFilters(mode, mode, false);
			resampler->Init(dst.w, dst.h, dst.format, src.w, src.h, src.format);
			resampler->Process(dst, src);
			uint64 time = VDGetPreciseTick() - start;

			if (time < fastest)
				fastest = time;
		}

		results[i] = fastest;
	}

	VDStringW s;

	static const char *const kModeNames[]={
		"Point",
		"Linear",
		"Cubic",
		"Lanczos3"
	};

	const double rate = VDGetPreciseTicksPerSecond();
	const double invrate = 1.0 / rate;
	for(unsigned j=0; j<IVDPixmapResampler::kFilterCount; ++j) {
		const char *name = kModeNames[j];
		const double ms = results[j] * invrate * 1000.0;
		const double mpixels = (dst.w * dst.h) / 1000000.0 / (results[j] * invrate);
		s += VDswprintf(L"%hs: %.2f ms, %.1fMpixels/sec\n", 3, &name, &ms, &mpixels);
	}

	// display results
	vdautoptr<IVDUIWindow> pPeer(VDUICreatePeer(h));
	IVDUIWindow *pWin = VDCreateDialogFromResource(9000, pPeer);
	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);
	pBase->GetControl(100)->SetCaption(s.c_str());
	pBase->DoModal();
	pWin->Shutdown();
}
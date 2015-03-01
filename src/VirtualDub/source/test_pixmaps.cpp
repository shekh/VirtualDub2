#include "stdafx.h"
#if 0

#include <windows.h>
#include "VideoDisplay.h"
#include <vd2/system/memory.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/triblt.h>
#include <vd2/Kasumi/resample.h>

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

	double randdbl() { return rand() / (double)RAND_MAX; }

	struct bouncer {
		bouncer(double x1_, double y1_, double x2_, double y2_, double mx_)
			: x1(x1_)
			, y1(y1_)
			, x2(x2_)
			, y2(y2_)
			, x(x1_ + (x2_ - x1_) * randdbl())
			, y(y1_ + (y2_ - y1_) * randdbl())
			, dx((randdbl()*2.0-1.0) * mx_)
			, dy((randdbl()*2.0-1.0) * mx_)
		{
		}

		int xpos() const { return (int)floor(0.5 + x); }
		int ypos() const { return (int)floor(0.5 + y); }
		int xposf() const { return (int)floor(0.5 + x * 65536.0); }
		int yposf() const { return (int)floor(0.5 + y * 65536.0); }

		void advance() {
			x += dx;
			y += dy;

			if (x < x1) {
				x = x1;
				dx = fabs(dx);
			}

			if (y < y1) {
				y = y1;
				dy = fabs(dy);
			}

			if (x > x2) {
				x = x2;
				dx = -fabs(dx);
			}

			if (y > y2) {
				y = y2;
				dy = -fabs(dy);
			}
		}

		const double x1, y1, x2, y2;
		double x, y, dx, dy;
	};
}

void VDTestPixmaps() {
	CPUEnableExtensions(CPUCheckForExtensions());
	VDFastMemcpyAutodetect();

	VDRegisterVideoDisplayControl();

	HWND hwndDisp = CreateWindow(VIDEODISPLAYCONTROLCLASS, "Kasumi onee-sama", WS_VISIBLE|WS_POPUP, 0, 0, 1024, 768, NULL, NULL, GetModuleHandle(NULL), NULL);

	IVDVideoDisplay *pDisp = VDGetIVideoDisplay(hwndDisp);

	const int srcw = 80;
	const int srch = 60;

	VDPixmapBuffer image(srcw, srch, nsVDPixmap::kPixFormat_XRGB8888);

	for(int y=0; y<srch; ++y) {
		for(int x=0; x<srcw; ++x) {
			int x2 = x - (srcw>>1);
			int y2 = y - (srch>>1);

			uint32 v = (int)((1.0 + sin((x2*x2 + y2*y2) / 50.0)) * 255.0 / 2.0 + 0.5);

			uint32 r = (255-v)<<16;

			if ((x^y)&1)
				v = r = 0;

			((uint32 *)((char *)image.data + image.pitch * y))[x] = (v*x/srcw) + (((v*y)/srch)<<8) + r;
		}
	}

	VDPixmapBuffer sprite(srcw, srch, nsVDPixmap::kPixFormat_XRGB8888);
	VDPixmapBuffer buffer(1024, 768, nsVDPixmap::kPixFormat_XRGB8888);

	VDPixmapBlt(sprite, image);

	pDisp->SetSourcePersistent(true, buffer);

	bouncer p1(-64, -48, 1024+64, 768+48, 1.0);
	bouncer p2(-64, -48, 1024+64, 768+48, 0.5);

	sint64 freq;
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq);

	sint64 start;
	QueryPerformanceCounter((LARGE_INTEGER *)&start);
	int blits = 0;
	double th = 0;

	VDPixmapTextureMipmapChain mipchain(sprite);

	vdautoptr<IVDPixmapResampler> pResampler(VDCreatePixmapResampler());

	while(pump()) {
		int x1 = p1.xposf();
		int y1 = p1.yposf();
		int x2 = p2.xposf();
		int y2 = p2.yposf();

//		VDPixmapBlt(buffer, xp, yp, image, 0, 0, 320, 240);
//		VDPixmapStretchBltNearest(buffer, x1, y1, x2, y2, sprite, -32<<16, -32<<16, (srcw+32)<<16, (srch+32)<<16);
//		VDPixmapStretchBltBilinear(buffer, x1, y1, x2, y2, sprite, 0, 0, srcw<<16, srch<<16);

		double fx1 = x1 / 65536.0;
		double fy1 = y1 / 65536.0;
		double fx2 = x2 / 65536.0;
		double fy2 = y2 / 65536.0;

		if (fx2 < fx1)
			std::swap(fx1, fx2);
		if (fy2 < fy1)
			std::swap(fy1, fy2);

		pResampler->Init(fx2-fx1, fy2-fy1, buffer.format, sprite.w, sprite.h, sprite.format, IVDPixmapResampler::kFilterLanczos3, IVDPixmapResampler::kFilterLanczos3, false);
		pResampler->Process(&buffer, fx1, fy1, fx2, fy2, &sprite, 0, 0);

#if 0
		float mx[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};

		mx[0] = cos(th) / 512.0f;
		mx[1] = sin(th) / 512.0f;
		mx[5] = cos(th) / 384.0f;
		mx[4] = -sin(th) / 384.0f;

		mx[13] = -6.0f / 384.0f;
		mx[15] = 1.0f;

		VDTriBltVertex vx[4]={
			{ -100, -100, 0, 0, 0 },
			{ +100, -100, 0, 0, 60 },
			{ +100, +100, 0, 80, 60 },
			{ -100, +100, 0, 80, 0 },
		};

		const int idx[6]={0,1,2,0,2,3};

		VDPixmap buffer_cropped(VDPixmapOffset(buffer, 160, 120));

		buffer_cropped.w -= 320;
		buffer_cropped.h -= 240;

		VDPixmapTriBlt(buffer_cropped, mipchain.Mips(), mipchain.Levels(), vx, 4, idx, 6, kTriBltFilterTrilinear, 0.0f, mx);

		th += 0.01;
#endif

		pDisp->Update();
		++blits;

		p1.advance();
		p2.advance();

		sint64 last;
		QueryPerformanceCounter((LARGE_INTEGER *)&last);

		if (last-start >= freq) {
			start += freq;
			VDDEBUG2("%d blits/sec\n", blits);
			blits = 0;
		}
	}
}

extern void (*g_pPostInitRoutine)();

struct runtests {
	runtests() {
		g_pPostInitRoutine = VDTestPixmaps;
	}
} g_runtests;

#endif
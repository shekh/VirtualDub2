#include <stdafx.h>
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/time.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/resample.h>
#include <vd2/Dita/services.h>
#include "ProgressDialog.h"
#include "InputFile.h"
#include "VideoSource.h"
#include "AVIOutput.h"
#include "AVIOutputFile.h"
#include <vector>

extern const char g_szError[];

namespace {
	static const uint8 ditherred[16][16]={
		 35, 80,227,165, 64,199, 42,189,138, 74,238,111, 43,153, 13,211,
		197,135, 20, 99,244,  4,162,105, 25,210, 38,134,225, 78,242, 87,
		 63,249,126,192, 50,174, 82,251,116,148, 97,176, 19,167, 52,163,
		187, 30, 85,142,219, 71,194, 45,169, 11,241, 58,216,106,204,  5,
		 94,151,235,  9,112,155, 17,224, 91,206, 84,188,120, 36,132,233,
		177, 48,124,201, 40,239,125, 66,180, 51,160,  7,152,255, 89, 56,
		 16,209, 72,161,121, 59,208,150, 28,248, 75,229,101, 26,140,220,
		170,110,226, 22,252,139,  1,109,195,115,172, 39,200,114,191, 68,
		136, 34, 96,183, 44,175, 95,234, 81, 15,143,217, 62,164,  2,237,
		 57,245,154, 61,203, 70,213, 37,137,243, 98, 23,179, 86,198,103,
		184, 12,123,221,  6,129,156, 88,185, 53,127,228, 49,250, 31,130,
		 77,205, 83,145,107,247, 29,223, 10,212,159, 79,168, 73,146,232,
		173, 41,240, 24,190, 54,178,102,149,118, 33,202,  8,215,119, 18,
		 92,158, 67,166, 76,207,133, 47,254, 65,230,100,131,157, 69,193,
		253,  3,214,117,231, 14, 93,171, 21,182,144, 55,246, 27,222, 46,
		141,181,104, 32,147,113,236, 60,218,122,  0,196, 90,186,128,108,
	};

	static const uint8 dithergrn[16][16]={
		130,239, 48,211, 19,242, 33, 85,120,252, 72,207,107, 26,179, 39,
		 67,112,184, 92,156, 75,164,213, 17,138,188,  2,245,140, 87,225,
		198,147, 11,254, 41,219,129, 60,194, 49,115,176, 95, 45,170, 15,
		109, 51,177, 84,165,117,  6,247,132, 90,201, 56,155,215, 80,250,
		136,229, 65,206, 28,189, 97,169, 36,223, 24,241,124,  8,181, 34,
		212,  1,143,104,234, 70,220, 57,121,159,102,144, 42,232,133, 96,
		 71,123,246, 22,149,128, 16,137,237, 10,208, 77,191,111, 53,195,
		158,187, 58,175, 44,193,110,202, 82,183, 61,255, 21,151,226, 18,
		 94, 32,214, 88,228, 64,249, 30,172, 47,162, 89,173, 63,106,200,
		236,141, 68,166,  3,161, 93,153, 74,231,  0,216, 38,240,139, 46,
		118, 13,253,114,205, 52,186, 14,199,113,142,101,192,125,  7,178,
		222,103,154, 37,131,244, 79,221,134, 54,251, 27,157, 83,209, 78,
		 29,190, 73,217, 98, 25,174,108, 35,185, 91,210, 66,243, 50,171,
		248, 55,168,  9,146,227, 69,148,238,127, 12,122,180, 20,145,116,
		100,203, 86,235,119, 43,197,  5, 81,204, 99,230, 59,126,196,  4,
		150, 23,163, 62,182,105,135,224,160, 31,167, 40,152,233, 76,218,
	};

	static const uint8 ditherblu[16][16]={
		184, 97,174, 29,118,218, 69,249,152,  5,172,126, 31,120,164, 59,
		135, 12,140,214, 76,161,122, 26, 93,205, 80,242,106,220, 82,250,
		108,228, 91, 40,182,  1,202,142,236, 55,160, 44,167,  9,179, 34,
		204, 56,128,255, 86,231, 99, 49,130,187, 20,201, 78,209, 73,158,
		  4,153,192, 18,170, 35,131,193, 10, 87,253,116,151, 28,245,123,
		234, 43,103,212, 74,156,221, 61,230,163,102, 50,223,105,143, 68,
		 89,183,136, 52,243,113, 22,146,121, 30,206,132,  3,196, 48,173,
		217, 25,203,119,  7,188, 92,248, 72,171, 83,240,147, 63,238, 15,
		 79,166, 64,235,138, 58,208, 38,197, 13,186, 36,110,177, 94,154,
		251,104,190, 41,175,100,150,124, 81,219, 66,127,210, 19,222, 37,
		117, 11,134,215, 24,252,  2,226,159, 27,144,246, 75,162, 88,194,
		141,227, 54,114,165, 77,180, 53,115,233, 98,  8,199, 51,176, 70,
		 33,169, 85,239, 47,211,139, 90,195, 46,189,133,107,225,  0,244,
		207,101,155,  6,185,112, 32,241, 14,168, 84,254, 42,149,129, 60,
		145, 23,198,125, 62,224, 96,137,200, 57,157, 21,181, 65,191,111,
		 45,232, 67,247,148, 17,178, 39,109,229, 71,213, 95,237, 16,216,
	};
}

void VDCreateTestPal8Video(VDGUIHandle h) {
	CPUEnableExtensions(CPUCheckForExtensions());

	try {
		tVDInputDrivers inputDrivers;
		std::vector<int> xlat;

		VDGetInputDrivers(inputDrivers, IVDInputDriver::kF_Video);

		const VDStringW filter(VDMakeInputDriverFileFilter(inputDrivers, xlat));

		const VDFileDialogOption opt[]={
			{ VDFileDialogOption::kSelectedFilter },
			0
		};

		int optval[1]={0};

		const VDStringW srcfile(VDGetLoadFileName('pl8s', h, L"Choose source file", filter.c_str(), NULL, opt, optval));

		if (srcfile.empty())
			return;

		IVDInputDriver *pDrv;
		int filtidx = xlat[optval[0] - 1];
		if (filtidx < 0)
			pDrv = VDAutoselectInputDriverForFile(srcfile.c_str(), IVDInputDriver::kF_Video);
		else {
			tVDInputDrivers::iterator itDrv(inputDrivers.begin());
			std::advance(itDrv, filtidx);

			pDrv = *itDrv;
		}

		vdrefptr<InputFile> pIF(pDrv->CreateInputFile(0));

		pIF->Init(srcfile.c_str());

		const VDStringW dstfile(VDGetSaveFileName('pl8d', h, L"Choose destination 8-bit file", L"Audio-video interleaved (*.avi)\0*.avi\0All files\0*.*", L"avi", NULL, NULL));
		if (dstfile.empty())
			return;

		vdrefptr<IVDVideoSource> pVS;
		pIF->GetVideoSource(0, ~pVS);
		IVDStreamSource *pVSS = pVS->asStream();
		const VDPosition frames = pVSS->getLength();

		if (!pVS->setTargetFormat(nsVDPixmap::kPixFormat_XRGB8888))
			throw MyError("Cannot set decompression format to 32-bit.");

		vdautoptr<IVDMediaOutputAVIFile> pOut(VDCreateMediaOutputAVIFile());

		IVDMediaOutputStream *pVSOut = pOut->createVideoStream();

		const VDPixmap& pxsrc = pVS->getTargetFormat();
		const uint32 rowbytes = (pxsrc.w+3) & ~3;

		AVIStreamHeader_fixed hdr;

		hdr.fccType		= 'sdiv';
		hdr.fccHandler	= 0;
		hdr.dwFlags		= 0;
		hdr.wPriority	= 0;
		hdr.wLanguage	= 0;
		hdr.dwScale		= pVSS->getStreamInfo().dwScale;
		hdr.dwRate		= pVSS->getStreamInfo().dwRate;
		hdr.dwStart		= 0;
		hdr.dwLength	= 0;
		hdr.dwInitialFrames = 0;
		hdr.dwSuggestedBufferSize = 0;
		hdr.dwQuality = -1;
		hdr.dwSampleSize = 0;
		hdr.rcFrame.left	= 0;
		hdr.rcFrame.top		= 0;
		hdr.rcFrame.right	= (short)pxsrc.w;
		hdr.rcFrame.bottom	= (short)pxsrc.h;

		pVSOut->setStreamInfo(hdr);

		vdstructex<BITMAPINFOHEADER> bih;

		bih.resize(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*252);

		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biWidth = pxsrc.w;
		bih->biHeight = pxsrc.h;
		bih->biPlanes = 1;
		bih->biBitCount = 8;
		bih->biCompression = BI_RGB;
		bih->biSizeImage = rowbytes*pxsrc.h;
		bih->biXPelsPerMeter = 0;
		bih->biYPelsPerMeter = 0;
		bih->biClrUsed = 252;
		bih->biClrImportant = 252;

		RGBQUAD *pal = (RGBQUAD *)((char *)bih.data() + sizeof(BITMAPINFOHEADER));
		for(int i=0; i<252; ++i) {
			pal[i].rgbRed		= (BYTE)((i/42)*51);
			pal[i].rgbGreen		= (BYTE)((((i/6)%7)*85)>>1);
			pal[i].rgbBlue		= (BYTE)((i%6)*51);
			pal[i].rgbReserved	= 0;
		}

		pVSOut->setFormat(bih.data(), bih.size());

		pOut->init(dstfile.c_str());

		ProgressDialog dlg((HWND)h, "Processing video stream", "Palettizing frames", (long)frames, true);

		vdblock<uint8> outbuf(rowbytes * pxsrc.h);

		const vdpixsize w = pxsrc.w;
		const vdpixsize h = pxsrc.h;

		try {
			for(uint32 frame=0; frame<frames; ++frame) {
				pVS->getFrame(frame);

				const uint8 *src = (const uint8 *)pxsrc.data;
				ptrdiff_t srcpitch = pxsrc.pitch;
				uint8 *dst = &outbuf[rowbytes * (pxsrc.h - 1)];

				for(int y=0; y<h; ++y) {
					const uint8 *dr = ditherred[y & 15];
					const uint8 *dg = dithergrn[y & 15];
					const uint8 *db = ditherblu[y & 15];

					for(int x=0; x<w; ++x) {
						const uint8 b = (uint8)((((src[0] * 1286)>>8) + dr[x&15]) >> 8);
						const uint8 g = (uint8)((((src[1] * 1543)>>8) + dg[x&15]) >> 8);
						const uint8 r = (uint8)((((src[2] * 1286)>>8) + db[x&15]) >> 8);
						src += 4;

						dst[x] = (uint8)(r*42 + g*6 + b);
					}

					vdptrstep(dst, -(ptrdiff_t)rowbytes);
					vdptrstep(src, srcpitch - w*4);
				}

				pVSOut->write(AVIOutputStream::kFlagKeyFrame, outbuf.data(), outbuf.size(), 1);

				dlg.advance(frame);
				dlg.check();
			}
		} catch(const MyUserAbortError&) {
		}

		pVSOut->flush();
		pOut->finalize();

	} catch(const MyError& e) {
		e.post((HWND)h, g_szError);
	}
}

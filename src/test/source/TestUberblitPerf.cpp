#include "test.h"
#include <intrin.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/time.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "../../Kasumi/h/uberblit.h"

namespace {
	struct PerfEntry {
		int srcFormat;
		int dstFormat;
		double bltSpeed;
		double uberSpeed;
		double ratio;
	};

	struct SortPerfEntriesByRatio {
		bool operator()(const PerfEntry& x, const PerfEntry& y) {
			return x.ratio < y.ratio;
		}
	};
}

DEFINE_TEST_NONAUTO(UberblitPerf) {
	static const int kFormats[]={
		nsVDPixmap::kPixFormat_Pal1,
		nsVDPixmap::kPixFormat_Pal2,
		nsVDPixmap::kPixFormat_Pal4,
		nsVDPixmap::kPixFormat_Pal8,
		nsVDPixmap::kPixFormat_XRGB1555,		// 4
		nsVDPixmap::kPixFormat_RGB565,
		nsVDPixmap::kPixFormat_RGB888,
		nsVDPixmap::kPixFormat_XRGB8888,
		nsVDPixmap::kPixFormat_Y8,				// 8
		nsVDPixmap::kPixFormat_YUV422_UYVY,
		nsVDPixmap::kPixFormat_YUV422_YUYV,
		nsVDPixmap::kPixFormat_YUV444_Planar,
		nsVDPixmap::kPixFormat_YUV422_Planar,	// 12
		nsVDPixmap::kPixFormat_YUV422_Planar_16F,
		nsVDPixmap::kPixFormat_YUV420_Planar,
		nsVDPixmap::kPixFormat_YUV411_Planar,
		nsVDPixmap::kPixFormat_YUV410_Planar,
		nsVDPixmap::kPixFormat_YUV422_V210,
		nsVDPixmap::kPixFormat_YUV422_UYVY_709,
		nsVDPixmap::kPixFormat_YUV420_NV12,
		nsVDPixmap::kPixFormat_YUV422_YUYV_709,
		nsVDPixmap::kPixFormat_YUV444_Planar_709,
		nsVDPixmap::kPixFormat_YUV422_Planar_709,
		nsVDPixmap::kPixFormat_YUV420_Planar_709,
		nsVDPixmap::kPixFormat_YUV411_Planar_709,
		nsVDPixmap::kPixFormat_YUV410_Planar_709,
		nsVDPixmap::kPixFormat_YUV422_UYVY_FR,
		nsVDPixmap::kPixFormat_YUV422_YUYV_FR,
		nsVDPixmap::kPixFormat_YUV444_Planar_FR,
		nsVDPixmap::kPixFormat_YUV422_Planar_FR,
		nsVDPixmap::kPixFormat_YUV420_Planar_FR,
		nsVDPixmap::kPixFormat_YUV411_Planar_FR,
		nsVDPixmap::kPixFormat_YUV410_Planar_FR,
		nsVDPixmap::kPixFormat_YUV422_UYVY_709_FR,
		nsVDPixmap::kPixFormat_YUV422_YUYV_709_FR,
		nsVDPixmap::kPixFormat_YUV444_Planar_709_FR,
		nsVDPixmap::kPixFormat_YUV422_Planar_709_FR,
		nsVDPixmap::kPixFormat_YUV420_Planar_709_FR,
		nsVDPixmap::kPixFormat_YUV411_Planar_709_FR,
		nsVDPixmap::kPixFormat_YUV410_Planar_709_FR,
		nsVDPixmap::kPixFormat_XRGB64,
		nsVDPixmap::kPixFormat_YUV420_Planar16,
		nsVDPixmap::kPixFormat_YUV422_Planar16,
		nsVDPixmap::kPixFormat_YUV444_Planar16,
	};

	enum {
#if 1
		kSrcStart = 4,
		kDstStart = 4,
#else
		kSrcStart = 9,
		kDstStart = 7,
#endif
		kFormatCount = sizeof(kFormats)/sizeof(kFormats[0])
	};

	static const int kSize = 512;

	vdautoarrayptr<char> dstbuf(new char[kSize*kSize*8]);
	vdautoarrayptr<char> srcbuf(new char[kSize*kSize*8]);
	uint32 palette[256]={0};

	memset(srcbuf.get(), 0, kSize*kSize*8);

	VDPixmap srcPixmaps[kFormatCount];

	for(int srcformat = kSrcStart; srcformat < kFormatCount; ++srcformat) {
		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, kFormats[srcformat], kSize, kSize, 16);
		srcPixmaps[srcformat] = VDPixmapFromLayout(layout, srcbuf.get());
		srcPixmaps[srcformat].palette = palette;
		srcPixmaps[srcformat].info.ref_r = 0xFFFF;
		srcPixmaps[srcformat].info.ref_g = 0xFFFF;
		srcPixmaps[srcformat].info.ref_b = 0xFFFF;
		srcPixmaps[srcformat].info.ref_a = 0xFFFF;
	}

	vdfastvector<PerfEntry> perfEntries;
	for(int dstformat = kDstStart; dstformat < kFormatCount; ++dstformat) {
//		dstformat = 5;

		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, kFormats[dstformat], kSize, kSize, 16);
		VDPixmap dstPixmap = VDPixmapFromLayout(layout, dstbuf.get());
		dstPixmap.palette = palette;

		const char *dstName = VDPixmapGetInfo(kFormats[dstformat]).name;

		for(int srcformat = kSrcStart; srcformat < kFormatCount; ++srcformat) {
//		srcformat = 15;
			const VDPixmap& srcPixmap = srcPixmaps[srcformat];
			const char *srcName = VDPixmapGetInfo(kFormats[srcformat]).name;
      bool blt_fail = false;

			uint64 bltTime = (uint64)(sint64)-1;
			for(int i=0; i<10; ++i) {
				uint64 t1 = VDGetPreciseTick();
				if (!VDPixmapBlt(dstPixmap, srcPixmap)) {
					blt_fail = true;
					bltTime = 0;
					break;
				}
				t1 = VDGetPreciseTick() - t1;

				if (bltTime > t1)
					bltTime = t1;
			}

			vdautoptr<IVDPixmapBlitter> blitter(VDPixmapCreateBlitter(dstPixmap, srcPixmap));

			uint64 uberTime = (uint64)(sint64)-1;
//			for(;;)
			{
			for(int i=0; i<10; ++i) {
				uint64 t1 = VDGetPreciseTick();
				blitter->Blit(dstPixmap, srcPixmap);
				t1 = VDGetPreciseTick() - t1;

				if (uberTime > t1)
					uberTime = t1;
			}
			}

			double bltSpeed = (double)(kSize*kSize) / 1000000.0 / (double)bltTime * VDGetPreciseTicksPerSecond();
			double uberSpeed = (double)(kSize*kSize) / 1000000.0 / (double)uberTime * VDGetPreciseTicksPerSecond();

			if (blt_fail)
				printf("%-18s <- %-18s %14s %8.2fMP/sec\n", dstName, srcName, "not impl.", uberSpeed);
			else
				printf("%-18s <- %-18s %8.2fMP/sec %8.2fMP/sec\n", dstName, srcName, bltSpeed, uberSpeed);

			PerfEntry& ent = perfEntries.push_back();

			ent.srcFormat = srcformat;
			ent.dstFormat = dstformat;
			ent.bltSpeed = bltSpeed;
			ent.uberSpeed = uberSpeed;
			ent.ratio = uberSpeed / bltSpeed;
			if (blt_fail) {
  				ent.bltSpeed = 0;
				ent.ratio = 0;
			}
		}
	}

	std::sort(perfEntries.begin(), perfEntries.end(), SortPerfEntriesByRatio());

	printf("\nSlower blitters:\n");

	vdfastvector<PerfEntry>::const_iterator it(perfEntries.begin()), itEnd(perfEntries.end());
	int idx;
	for(idx=0; it != itEnd; ++it, ++idx) {
		const PerfEntry& ent = *it;

		const char *srcName = VDPixmapGetInfo(kFormats[ent.srcFormat]).name;
		const char *dstName = VDPixmapGetInfo(kFormats[ent.dstFormat]).name;

		if(!ent.bltSpeed)
			printf("%-18s <- %-18s %14s %8.2fMP/sec\n", dstName, srcName, "not impl.", ent.uberSpeed);
		else
			printf("%-18s <- %-18s %8.2fMP/sec %8.2fMP/sec (%.1f%%)\n", dstName, srcName, ent.bltSpeed, ent.uberSpeed, ent.ratio*100.0);

		if (ent.ratio >= 1.0)
			break;
	}

	printf("%d/%d blitters slower.\n", idx, (int)perfEntries.size());

	return 0;
}

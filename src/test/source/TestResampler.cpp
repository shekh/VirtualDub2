#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/memory.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/resample.h>
#include "test.h"

DEFINE_TEST(Resampler) {
	static const uint32 kCPUModes[]={
		CPUF_SUPPORTS_FPU,
		CPUF_SUPPORTS_FPU | CPUF_SUPPORTS_MMX,
		CPUF_SUPPORTS_FPU | CPUF_SUPPORTS_MMX | CPUF_SUPPORTS_INTEGER_SSE,
		CPUF_SUPPORTS_FPU | CPUF_SUPPORTS_MMX | CPUF_SUPPORTS_INTEGER_SSE | CPUF_SUPPORTS_SSE2,
		CPUF_SUPPORTS_FPU | CPUF_SUPPORTS_MMX | CPUF_SUPPORTS_INTEGER_SSE | CPUF_SUPPORTS_SSE2 | CPUF_SUPPORTS_SSSE3,
		CPUF_SUPPORTS_FPU | CPUF_SUPPORTS_MMX | CPUF_SUPPORTS_INTEGER_SSE | CPUF_SUPPORTS_SSE2 | CPUF_SUPPORTS_SSSE3 | CPUF_SUPPORTS_SSE41,
	};

	uint32 availModes = CPUCheckForExtensions();

	for(int cpumode=0; cpumode<sizeof(kCPUModes)/sizeof(kCPUModes[0]); ++cpumode) {
		uint32 desiredModes = kCPUModes[cpumode];

		if (desiredModes & ~availModes)
			continue;

		CPUEnableExtensions(desiredModes);

		uint8 dst[32*32];
		uint8 src[33*32];

		VDPixmap pxdst;
		pxdst.pitch = 32;
		pxdst.format = nsVDPixmap::kPixFormat_Y8;

		VDPixmap pxsrc;
		pxsrc.pitch = 33;
		pxsrc.format = nsVDPixmap::kPixFormat_Y8;

		static const IVDPixmapResampler::FilterMode kFilterModes[]={
			IVDPixmapResampler::kFilterLinear,
			IVDPixmapResampler::kFilterCubic,
			IVDPixmapResampler::kFilterLanczos3,
		};

		for(int fmodei = 0; fmodei < sizeof(kFilterModes)/sizeof(kFilterModes[0]); ++fmodei) {
			IVDPixmapResampler::FilterMode filterMode = kFilterModes[fmodei];

			pxdst.h = 16;
			pxsrc.h = 16;
			for(int dalign = 0; dalign < 4; ++dalign) {
				for(int dx = 4; dx <= 16; ++dx) {
					pxdst.data = dst + 4*32 + 4 + dalign;
					pxdst.w = dx;

					for(int sx = 4; sx <= 16; ++sx) {
						VDMemset8(src, 0x40, sizeof src);
						VDMemset8(dst, 0xCD, sizeof dst);

						pxsrc.w = sx;
						pxsrc.data = src + 4*32 + 4;

						for(int i=0; i<16; ++i)
							VDMemset8((char *)pxsrc.data + pxsrc.pitch*i, 0xA0, sx);

						VDPixmapResample(pxdst, pxsrc, filterMode);

						TEST_ASSERT(!VDMemCheck8(dst+32*3, 0xCD, 32));
						for(int i=0; i<16; ++i) {
							TEST_ASSERT(!VDMemCheck8(dst+32*(4+i), 0xCD, dalign + 4));
							TEST_ASSERT(!VDMemCheck8(dst+32*(4+i)+dalign+4, 0xA0, dx));
							TEST_ASSERT(!VDMemCheck8(dst+32*(4+i)+dalign+4+dx, 0xCD, 32 - (4+dalign+dx)));
						}
						TEST_ASSERT(!VDMemCheck8(dst+32*20, 0xCD, 32));
					}
				}
			}
		}
	}

	return 0;
}

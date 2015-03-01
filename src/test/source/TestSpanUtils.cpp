#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/memory.h>
#include "..\..\Kasumi\h\blt_spanutils.h"
#include "..\..\Kasumi\h\blt_spanutils_x86.h"
#include "test.h"

DEFINE_TEST(SpanUtils) {
	uint32 ext = CPUCheckForExtensions();
	CPUEnableExtensions(ext);

	// horiz_expand2x_coaligned
	for(int pass = 0; pass < 2; ++pass) {
		void (*hfunc)(uint8 *dst, const uint8 *src, sint32 w);

		if (pass == 0)
			hfunc = nsVDPixmapSpanUtils::horiz_expand2x_coaligned;
		else if (pass == 1) {
#ifdef _M_IX86
			if (!(ext & CPUF_SUPPORTS_INTEGER_SSE))
				continue;

			hfunc = nsVDPixmapSpanUtils::horiz_expand2x_coaligned_ISSE;
#else
			continue;
#endif
		}

		uint8 dst[64];
		uint8 src[32];

		for(int w = 1; w <= 32; ++w) {
			memset(src, 0x40, sizeof src);
			memset(dst, 0xCD, sizeof dst);

			int sw = (w + 1) >> 1;
			for(int j=0; j<sw; ++j)
				src[4 + j] = j << 4;

			hfunc(dst+4, src+4, w);

			TEST_ASSERT(!VDMemCheck8(dst, 0xCD, 4));
			TEST_ASSERT(!VDMemCheck8(dst + 4 + w, 0xCD, 60 - w));

			for(int j=0; j<w-1; ++j) {
				TEST_ASSERT(dst[4 + j] == (j << 3));
			}
			TEST_ASSERT(dst[3 + w] == ((sw - 1) << 4));
		}
	}

	// vert_expand2x_centered
	for(int pass = 0; pass < 2; ++pass) {
		void (*hfunc)(uint8 *dst, const uint8 *const *srcs, sint32 w, uint8 phase);

		if (pass == 0)
			hfunc = nsVDPixmapSpanUtils::vert_expand2x_centered;
		else if (pass == 1) {
#ifdef _M_IX86
			if (!(ext & CPUF_SUPPORTS_INTEGER_SSE))
				continue;

			hfunc = nsVDPixmapSpanUtils::vert_expand2x_centered_ISSE;
#else
			continue;
#endif
		}

		uint8 dst[64];
		uint8 src1[64];
		uint8 src2[64];

		for(int inv = 0; inv < 2; ++inv) {
			uint8 phase = inv ? 0xc0 : 0x40;

			const uint8 *const srcs[2]={src1 + 4, src2 + 4};

			for(int w = 1; w <= 32; ++w) {
				memset(src1, 0x40, sizeof src1);
				memset(src2, 0x40, sizeof src2);
				memset(dst, 0xCD, sizeof dst);

				for(int j=0; j<w; ++j) {
					src1[4 + j] = (uint8)(rand() & 0xff);
					src2[4 + j] = (uint8)(rand() & 0xff);
				}

				hfunc(dst + 4, srcs, w, phase);

				TEST_ASSERT(!VDMemCheck8(dst, 0xCD, 4));
				TEST_ASSERT(!VDMemCheck8(dst + 4 + w, 0xCD, 60 - w));

				const uint8 *chk1 = inv ? src1 : src2;
				const uint8 *chk3 = inv ? src2 : src1;

				for(int j=0; j<w-1; ++j) {
					uint32 c3 = chk3[4 + j];
					uint32 c1 = chk1[4 + j];
					uint32 cc = (3*c3 + c1 + 2) >> 2;
					uint32 cr = dst[4 + j];

					TEST_ASSERT(cr == cc);
				}
			}
		}
	}

	return 0;
}

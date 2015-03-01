#include <vd2/system/filesys.h>
#include <vd2/Priss/convert.h>
#include "test.h"

void testint(tpVDConvertPCM fnScalar, tpVDConvertPCM fnMMX) {
	char buf1[256];
	char buf2[256];
	char buf3[256];
	int i;

	for(i=0; i<256; ++i)
		buf1[i] = rand();

	for(i=0; i<64; ++i) {
		memcpy(buf2, buf1, sizeof buf2);
		memcpy(buf3, buf1, sizeof buf3);

		fnScalar(buf2+16, buf1+16, i);
		fnMMX   (buf3+16, buf1+16, i);

		for(int j=0; j<256; ++j)
			TEST_ASSERT(buf2[j] == buf3[j]);
	}
}

void testfp1(tpVDConvertPCM fnScalar, tpVDConvertPCM fnSSE) {
	float __declspec(align(16)) buf1[64];
	char buf2[256];
	char buf3[256];
	int i;

	for(i=0; i<64; ++i)
		buf1[i] = (float)((((double)rand() / RAND_MAX) - 0.5) * 2.2);

	for(int p=0; p<16; ++p) {
		for(int o=0; o<4; ++o) {
			for(i=0; i<32; ++i) {
				memcpy(buf2, buf1, sizeof buf2);
				memcpy(buf3, buf1, sizeof buf3);

				fnScalar(buf2+4+p, buf1+4+o, i);
				fnSSE   (buf3+4+p, buf1+4+o, i);

				for(int j=0; j<256; ++j)
					TEST_ASSERT(buf2[j] == buf3[j]);
			}
		}
	}
}

void testfp2(tpVDConvertPCM fnScalar, tpVDConvertPCM fnSSE) {
	float buf0[64];
	char buf1[256];
	float buf2[64];
	float buf3[64];
	int i;

	for(i=0; i<64; ++i)
		buf0[i] = (float)rand();

	for(i=0; i<256; ++i)
		buf1[i] = rand();

	for(int p=0; p<4; ++p) {
		for(int o=0; o<16; ++o) {
			for(i=0; i<32; ++i) {
				memcpy(buf2, buf0, sizeof buf2);
				memcpy(buf3, buf0, sizeof buf3);

				fnScalar(buf2+4+p, buf1+4+o, i);
				fnSSE   (buf3+4+p, buf1+4+o, i);

				for(int j=0; j<64; ++j)
					TEST_ASSERT(buf2[j] == buf3[j]);
			}
		}
	}
}

DEFINE_TEST(AudioConvert) {
#ifdef _M_IX86
	testint(VDConvertPCM8ToPCM16, VDConvertPCM8ToPCM16_MMX);
	testint(VDConvertPCM16ToPCM8, VDConvertPCM16ToPCM8_MMX);
	testfp1(VDConvertPCM32FToPCM16, VDConvertPCM32FToPCM16_SSE);
	testfp1(VDConvertPCM32FToPCM8, VDConvertPCM32FToPCM8_SSE);
	testfp2(VDConvertPCM16ToPCM32F, VDConvertPCM16ToPCM32F_SSE);
	testfp2(VDConvertPCM8ToPCM32F, VDConvertPCM8ToPCM32F_SSE);
#endif
	return 0;
}

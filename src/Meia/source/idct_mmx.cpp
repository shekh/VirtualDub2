#include <string.h>
#include <math.h>

#include <vd2/system/vdtypes.h>
#include <vd2/Meia/MPEGIDCT.h>
#include "tables.h"

#ifdef _M_AMD64
extern "C" void _mm_sfence();
#endif

extern "C" void IDCT_mmx(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int pos);
extern "C" void IDCT_isse(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int pos);
extern "C" void IDCT_sse2(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int pos);

using namespace nsVDMPEGTables;

//#define VERIFY_ROW_SHORTCUT

namespace nsVDMPEGIDCTMMX {

#ifndef _M_AMD64
		static void mmx_idct_intra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
			IDCT_mmx(tmp, dst, pitch, 1, last_pos);
		}

		static void mmx_idct_nonintra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
			IDCT_mmx(tmp, dst, pitch, 0, last_pos);
		}

		static void mmx_idct_test(short *tmp, int last_pos) {
			IDCT_mmx(tmp, NULL, 0, 2, last_pos);
			__asm emms
		}

		static void isse_idct_intra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
	#ifdef VERIFY_ROW_SHORTCUT
			short test1[64], test2[64];
			memcpy(test1, tmp, 128);
			memcpy(test2, tmp, 128);
			IDCT_isse(test1, 0, 0, 2, last_pos?last_pos:1);
			IDCT_isse(test2, 0, 0, 2, 63);
			VDASSERT(!memcmp(test1, test2, 128));
	#endif
			
			IDCT_isse(tmp, dst, pitch, 1, last_pos);
		}

		static void isse_idct_nonintra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
	#ifdef VERIFY_ROW_SHORTCUT
			short test1[64], test2[64];
			memcpy(test1, tmp, 128);
			memcpy(test2, tmp, 128);
			IDCT_isse(test1, 0, 0, 2, last_pos?last_pos:1);
			IDCT_isse(test2, 0, 0, 2, 63);
			VDASSERT(!memcmp(test1, test2, 128));
	#endif

			IDCT_isse(tmp, dst, pitch, 0, last_pos);
		}

		static void isse_idct_test(short *tmp, int last_pos) {
			IDCT_isse(tmp, NULL, 0, 2, last_pos);
			__asm emms
			__asm sfence
		}
#endif

	static void sse2_idct_intra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
#ifdef VERIFY_ROW_SHORTCUT
		short test1[64], test2[64];
		memcpy(test1, tmp, 128);
		memcpy(test2, tmp, 128);
		IDCT_sse2(test1, 0, 0, 2, last_pos?last_pos:1);
		IDCT_sse2(test2, 0, 0, 2, 63);
		VDASSERT(!memcmp(test1, test2, 128));
#endif
		
		IDCT_sse2(tmp, dst, pitch, 1, last_pos);
	}

	static void sse2_idct_nonintra(unsigned char *dst, int pitch, short *tmp, int last_pos) {
#ifdef VERIFY_ROW_SHORTCUT
		short test1[64], test2[64];
		memcpy(test1, tmp, 128);
		memcpy(test2, tmp, 128);
		IDCT_sse2(test1, 0, 0, 2, last_pos?last_pos:1);
		IDCT_sse2(test2, 0, 0, 2, 63);
		VDASSERT(!memcmp(test1, test2, 128));
#endif

		IDCT_sse2(tmp, dst, pitch, 0, last_pos);
	}

	static void sse2_idct_test(short *tmp, int last_pos) {
		__declspec(align(16)) short tmp2[64];

		memcpy(tmp2, tmp, 64*2);
		IDCT_sse2(tmp2, NULL, 0, 2, last_pos);
		memcpy(tmp, tmp2, 64*2);

#ifdef _M_AMD64
		_mm_sfence();
#else
		__asm emms
		__asm sfence
#endif
	}

	///////////////////////////////////////////////////////////////////////

	static const int zigzag_reordered[64]={
		 0,  2,  8, 16, 10,  4,  6, 12,
		18, 24, 32, 26, 20, 14,  1,  3,
		 9, 22, 28, 34, 40, 48, 42, 36,
		30, 17, 11,  5,  7, 13, 19, 25,
		38, 44, 50, 56, 58, 52, 46, 33,
		27, 21, 15, 23, 29, 35, 41, 54,
		60, 62, 49, 43, 37, 31, 39, 45,
		51, 57, 59, 53, 47, 55, 61, 63,
	};

	static const int zigzag_sse2[64]={
		 0,  4,  8, 16, 12,  1,  5,  9,
		20, 24, 32, 28, 17, 13,  2,  6,
		10, 21, 25, 36, 40, 48, 44, 33,
		29, 18, 14,  3,  7, 11, 22, 26,
		37, 41, 52, 56, 60, 49, 45, 34,
		30, 19, 15, 23, 27, 38, 42, 53,
		57, 61, 50, 46, 35, 31, 39, 43,
		54, 58, 62, 51, 47, 55, 59, 63,
	};

};

#ifndef _M_AMD64
const struct VDMPEGIDCTSet g_VDMPEGIDCT_mmx = {
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::mmx_idct_intra,
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::mmx_idct_nonintra,
	(tVDMPEGIDCTTest)nsVDMPEGIDCTMMX::mmx_idct_test,
	NULL,
	nsVDMPEGIDCTMMX::zigzag_reordered,
};

const struct VDMPEGIDCTSet g_VDMPEGIDCT_isse = {
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::isse_idct_intra,
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::isse_idct_nonintra,
	(tVDMPEGIDCTTest)nsVDMPEGIDCTMMX::isse_idct_test,
	NULL,
	nsVDMPEGIDCTMMX::zigzag_reordered,
};
#endif

const struct VDMPEGIDCTSet g_VDMPEGIDCT_sse2 = {
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::sse2_idct_intra,
	(tVDMPEGIDCT)nsVDMPEGIDCTMMX::sse2_idct_nonintra,
	(tVDMPEGIDCTTest)nsVDMPEGIDCTMMX::sse2_idct_test,
	NULL,
	nsVDMPEGIDCTMMX::zigzag_sse2,
};

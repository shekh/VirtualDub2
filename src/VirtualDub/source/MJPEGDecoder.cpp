//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include <vd2/system/error.h>
#include "MJPEGDecoder.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/memory.h>
#include <vd2/system/binary.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>

#ifdef VD_COMPILER_MSVC
	#include <vd2/system/win32/intrin.h>
	#pragma intrinsic(__ll_lshift)
#endif


#if defined(VD_COMPILER_MSVC)
	#pragma warning(disable: 4324)	// warning C4324: '': structure was padded due to __declspec(align())
#endif

///////////////////////////////////////////////////////////////////////////
//
//		Externs
//
///////////////////////////////////////////////////////////////////////////

class MJPEGBlockDef {
public:
	const uint8 *huff_dc;
	const uint8 *huff_ac;
	const uint8 (*huff_ac_quick)[2];
	const uint8 (*huff_ac_quick2)[2];
	const int *quant;
	int *dc_ptr;
	int	ac_last;
};
extern "C" void asm_mb_decode(uint32& bitbuf, int& bitcnt, const uint8 *& ptr, int mcu_length, MJPEGBlockDef *pmbd, short **dctarray);
void mb_decode(uint32& bitbuf, int& bitcnt, const uint8 *& ptr, int mcu_length, MJPEGBlockDef *pmbd, short **dctarray);

extern "C" void IDCT_mmx(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int ac_last);
extern "C" void IDCT_isse(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int ac_last);
extern "C" void IDCT_sse2(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int ac_last);

///////////////////////////////////////////////////////////////////////////
//
//		Tables
//
///////////////////////////////////////////////////////////////////////////

static const char MJPEG_zigzag_mmx[64] = {		// the reverse zigzag scan order
	 0,  2,  8, 16, 10,  4,  6, 12,
	18, 24, 32, 26, 20, 14,  1,  3,
	 9, 22, 28, 34, 40, 48, 42, 36,
	30, 17, 11,  5,  7, 13, 19, 25,
	38, 44, 50, 56, 58, 52, 46, 33,
	27, 21, 15, 23, 29, 35, 41, 54,
	60, 62, 49, 43, 37, 31, 39, 45,
	51, 57, 59, 53, 47, 55, 61, 63,
};

static const char MJPEG_zigzag_sse2[64]={
	 0,  4,  8, 16, 12,  1,  5,  9,
	20, 24, 32, 28, 17, 13,  2,  6,
	10, 21, 25, 36, 40, 48, 44, 33,
	29, 18, 14,  3,  7, 11, 22, 26,
	37, 41, 52, 56, 60, 49, 45, 34,
	30, 19, 15, 23, 27, 38, 42, 53,
	57, 61, 50, 46, 35, 31, 39, 43,
	54, 58, 62, 51, 47, 55, 59, 63,
};

// Huffman tables

static const uint8 huff_dc_0[] = {	// DC table 0
	0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	// counts by bit length
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,	// values
};

static const uint8 huff_dc_1[] = {	// DC table 1
	0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,
};

static const uint8 huff_ac_0_quick[][2]={
#if 0
	0x01,2,	// 0000-3FFF
	0x01,2,
	0x01,2,
	0x01,2,
	0x02,2,	// 4000-7FFF
	0x02,2,
	0x02,2,
	0x02,2,
	0x03,3,	// 8000-9FFF
	0x03,3,
	0x00,4,	// A000-AFFF
#endif

/* 00-0F */ 0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,
/* 10-1F */ 0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,
/* 20-2F */ 0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,
/* 30-3F */ 0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,
/* 40-4F */ 0xFD,4,0xFD,4,0xFD,4,0xFD,4,0xFD,4,0xFD,4,0xFD,4,0xFD,4,
/* 50-5F */ 0xFE,4,0xFE,4,0xFE,4,0xFE,4,0xFE,4,0xFE,4,0xFE,4,0xFE,4,
/* 60-6F */ 0x02,4,0x02,4,0x02,4,0x02,4,0x02,4,0x02,4,0x02,4,0x02,4,
/* 70-7F */	0x03,4,0x03,4,0x03,4,0x03,4,0x03,4,0x03,4,0x03,4,0x03,4,
/* 80-8F */ 0xF9,6,0xF9,6,0xFA,6,0xFA,6,0xFB,6,0xFB,6,0xFC,6,0xFC,6,
/* 90-9F */ 0x04,6,0x04,6,0x05,6,0x05,6,0x06,6,0x06,6,0x07,6,0x07,6,
/* A0-AF */	0x00,4,0x00,4,0x00,4,0x00,4,0x00,4,0x00,4,0x00,4,0x00,4,
};

static uint8 huff_ac_0_quick2[0x1000 - 0xB00][2];

static const uint8 huff_ac_0[]={		// AC table 0
//	0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,	// 0xe2 values

/*
	0x01,0x02,					// (00-01) 0000-7FFF
	0x03,						// (02)    8000-9FFF
	0x00,0x04,0x11,				// (03-05) A000-CFFF
	0x05,0x12,0x21,				// (06-08) D000-E7FF
	0x31,0x41,					// (09-0A) E800-EFFF
	0x06,0x13,0x51,0x61,		// (0B-0E) F000-F7FF
	0x07,0x22,0x71,				// (0F-11) F800-FAFF
	0x14,0x32,0x81,0x91,0xA1,	// (12-16) FB00-FD7F
	0x08,0x23,0x42,0xB1,0xC1,	// (17-1B) FD80-FEBF
	0x15,0x52,0xD1,0xF0,		// (1C-1F) FEC0-FF3F
	0x24,0x33,0x62,0x72,		// (20-23) FF40-FF7F
*/
	0x82,15,
	0x82,15,

	0x09,16,0x0A,16,0x16,16,0x17,16,0x18,16,0x19,16,0x1A,16,0x25,16,0x26,16,0x27,16,0x28,16,0x29,16,0x2A,16,0x34,16,0x35,16,0x36,16,
	0x37,16,0x38,16,0x39,16,0x3A,16,0x43,16,0x44,16,0x45,16,0x46,16,0x47,16,0x48,16,0x49,16,0x4A,16,0x53,16,0x54,16,0x55,16,0x56,16,
	0x57,16,0x58,16,0x59,16,0x5A,16,0x63,16,0x64,16,0x65,16,0x66,16,0x67,16,0x68,16,0x69,16,0x6A,16,0x73,16,0x74,16,0x75,16,0x76,16,
	0x77,16,0x78,16,0x79,16,0x7A,16,0x83,16,0x84,16,0x85,16,0x86,16,0x87,16,0x88,16,0x89,16,0x8A,16,0x92,16,0x93,16,0x94,16,0x95,16,
	0x96,16,0x97,16,0x98,16,0x99,16,0x9A,16,0xA2,16,0xA3,16,0xA4,16,0xA5,16,0xA6,16,0xA7,16,0xA8,16,0xA9,16,0xAA,16,0xB2,16,0xB3,16,
	0xB4,16,0xB5,16,0xB6,16,0xB7,16,0xB8,16,0xB9,16,0xBA,16,0xC2,16,0xC3,16,0xC4,16,0xC5,16,0xC6,16,0xC7,16,0xC8,16,0xC9,16,0xCA,16,
	0xD2,16,0xD3,16,0xD4,16,0xD5,16,0xD6,16,0xD7,16,0xD8,16,0xD9,16,0xDA,16,0xE1,16,0xE2,16,0xE3,16,0xE4,16,0xE5,16,0xE6,16,0xE7,16,
	0xE8,16,0xE9,16,0xEA,16,0xF1,16,0xF2,16,0xF3,16,0xF4,16,0xF5,16,0xF6,16,0xF7,16,0xF8,16,0xF9,16,0xFA,16,
};

static const uint8 huff_ac_0_src[]={		// AC table 0
	0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,	// 0xe2 values

	0x01,0x02,					// (00-01) 0000-7FFF
	0x03,						// (02)    8000-9FFF
	0x00,0x04,0x11,				// (03-05) A000-CFFF
	0x05,0x12,0x21,				// (06-08) D000-E7FF
	0x31,0x41,					// (09-0A) E800-EFFF
	0x06,0x13,0x51,0x61,		// (0B-0E) F000-F7FF
	0x07,0x22,0x71,				// (0F-11) F800-FAFF
	0x14,0x32,0x81,0x91,0xA1,	// (12-16) FB00-FD7F
	0x08,0x23,0x42,0xB1,0xC1,	// (17-1B) FD80-FEBF
	0x15,0x52,0xD1,0xF0,		// (1C-1F) FEC0-FF3F
	0x24,0x33,0x62,0x72,		// (20-23) FF40-FF7F
};

static const uint8 huff_ac_1_quick[][2]={
#if 0
	0x00,2,	// 0000-0FFF
	0x00,2,	// 1000-1FFF
	0x00,2,	// 2000-2FFF
	0x00,2,	// 3000-3FFF
	0x01,2,	// 4000-4FFF
	0x01,2,	// 5000-5FFF
	0x01,2,	// 6000-6FFF
	0x01,2,	// 7000-7FFF
	0x02,3,	// 8000-8FFF
	0x02,3,	// 9000-9FFF
	0x03,4,	// A000-AFFF
#endif

/* 00-0F */ 0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,
/* 10-1F */ 0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,
/* 20-2F */ 0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,
/* 30-3F */ 0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,
/* 40-4F */ 0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,
/* 50-5F */ 0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,
/* 60-6F */ 0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,
/* 70-7F */ 0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,
/* 80-8F */ 0xFD,5,0xFD,5,0xFD,5,0xFD,5,0xFE,5,0xFE,5,0xFE,5,0xFE,5,
/* 90-9F */ 0x02,5,0x02,5,0x02,5,0x02,5,0x03,5,0x03,5,0x03,5,0x03,5,
/* A0-AF */	0xF9,7,0xFA,7,0xFB,7,0xFC,7,0x04,7,0x05,7,0x06,7,0x07,7,
};

static const uint8 huff_ac_1[]={		// AC table 1
//	0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,

/*
	0x00,0x01,								// (00-01) 4000 0000-7FFF
	0x02,									// (02)    2000 8000-9FFF
	0x03,0x11,								// (03-04) 1000 A000-BFFF
	0x04,0x05,0x21,0x31,					// (05-08) 0800 C000-DFFF
	0x06,0x12,0x41,0x51,					// (09-0C) 0400 E000-EFFF
	0x07,0x61,0x71,							// (0D-0F) 0200 F000-F5FF
	0x13,0x22,0x32,0x81,					// (10-13) 0100 F600-F9FF
	0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,		// (14-1B) 0080 FA00-FD7F
	0x09,0x23,0x33,0x52,0xF0,				// (1C-20) 0040 FD80-FEBF
	0x15,0x62,0x72,0xD1,					// (21-24) 0020 FEC0-FF3F
	0x0A,0x16,0x24,0x34,					// (25-28) 0010 FF40-FF80
*/
	0xE1,14,
	0xE1,14,
	0xE1,14,
	0xE1,14,

	0x25,15,0x25,15,
	0xF1,15,0xF1,15,

	0x17,16,0x18,16,0x19,16,0x1A,16,0x26,16,0x27,16,0x28,16,0x29,16,0x2A,16,0x35,16,0x36,16,0x37,16,0x38,16,0x39,16,0x3A,16,0x43,16,
	0x44,16,0x45,16,0x46,16,0x47,16,0x48,16,0x49,16,0x4A,16,0x53,16,0x54,16,0x55,16,0x56,16,0x57,16,0x58,16,0x59,16,0x5A,16,0x63,16,
	0x64,16,0x65,16,0x66,16,0x67,16,0x68,16,0x69,16,0x6A,16,0x73,16,0x74,16,0x75,16,0x76,16,0x77,16,0x78,16,0x79,16,0x7A,16,0x82,16,
	0x83,16,0x84,16,0x85,16,0x86,16,0x87,16,0x88,16,0x89,16,0x8A,16,0x92,16,0x93,16,0x94,16,0x95,16,0x96,16,0x97,16,0x98,16,0x99,16,
	0x9A,16,0xA2,16,0xA3,16,0xA4,16,0xA5,16,0xA6,16,0xA7,16,0xA8,16,0xA9,16,0xAA,16,0xB2,16,0xB3,16,0xB4,16,0xB5,16,0xB6,16,0xB7,16,
	0xB8,16,0xB9,16,0xBA,16,0xC2,16,0xC3,16,0xC4,16,0xC5,16,0xC6,16,0xC7,16,0xC8,16,0xC9,16,0xCA,16,0xD2,16,0xD3,16,0xD4,16,0xD5,16,
	0xD6,16,0xD7,16,0xD8,16,0xD9,16,0xDA,16,0xE2,16,0xE3,16,0xE4,16,0xE5,16,0xE6,16,0xE7,16,0xE8,16,0xE9,16,0xEA,16,0xF2,16,0xF3,16,
	0xF4,16,0xF5,16,0xF6,16,0xF7,16,0xF8,16,0xF9,16,0xFA,16
};

static const uint8 huff_ac_1_src[]={		// AC table 1
	0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,

	0x00,0x01,								// (00-01) 4000 0000-7FFF
	0x02,									// (02)    2000 8000-9FFF
	0x03,0x11,								// (03-04) 1000 A000-BFFF
	0x04,0x05,0x21,0x31,					// (05-08) 0800 C000-DFFF
	0x06,0x12,0x41,0x51,					// (09-0C) 0400 E000-EFFF
	0x07,0x61,0x71,							// (0D-0F) 0200 F000-F5FF
	0x13,0x22,0x32,0x81,					// (10-13) 0100 F600-F9FF
	0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,		// (14-1B) 0080 FA00-FD7F
	0x09,0x23,0x33,0x52,0xF0,				// (1C-20) 0040 FD80-FEBF
	0x15,0x62,0x72,0xD1,					// (21-24) 0020 FEC0-FF3F
	0x0A,0x16,0x24,0x34,					// (25-28) 0010 FF40-FF80
};

static uint8 huff_ac_1_quick2[0x1000 - 0xB00][2];

static const uint8 *huff_dc[2] = { huff_dc_0, huff_dc_1 };
static const uint8 *huff_ac[2] = { huff_ac_0, huff_ac_1 };
static const uint8 *huff_ac_src[2] = { huff_ac_0_src, huff_ac_1_src };
static const uint8 (*huff_ac_quick[2])[2] = { huff_ac_0_quick, huff_ac_1_quick };
static const uint8 (*huff_ac_quick2[2])[2] = { huff_ac_0_quick2, huff_ac_1_quick2 };

///////////////////////////////////////////////////////////////////////////
//
//		Class definitions
//
///////////////////////////////////////////////////////////////////////////


class MJPEGDecoder : public IMJPEGDecoder, public VDAlignedObject<16> {
public:
	MJPEGDecoder(int w, int h);
	~MJPEGDecoder();

	void decodeFrameRGB15(uint32 *output, uint8 *input, int len);
	void decodeFrameRGB32(uint32 *output, uint8 *input, int len);
	void decodeFrameUYVY(uint32 *output, uint8 *input, int len);
	void decodeFrameYUY2(uint32 *output, uint8 *input, int len);

private:
	int quant[4][128];				// quantization matrices
	int width, height, field_height;
	int mcu_width, mcu_height;		// size of frame when blocked into MCUs
	int mcu_length;
	int mcu_count;
	int mcu_size_x;
	int mcu_size_y;
	int raw_width, raw_height;
	int clip_row, clip_lines;
	int restart_interval;
	void *pixdst;

	int *comp_quant[3];
	int comp_mcu_x[3], comp_mcu_y[3], comp_mcu_length[4];
	int comp_last_dc[3];
	int comp_id[3];
	int comp_start[3];

	MJPEGBlockDef blocks[24];
	__declspec(align(16)) short dct_coeff[24][64];
	short *dct_coeff_ptrs[24];

	bool interlaced;

	enum {
		kYCrCb444,
		kYCrCb422,
		kYCrCb420,

		kChromaModeCount
	} mChromaMode;

	enum {
		kDecodeRGB15,
		kDecodeRGB32,
		kDecodeUYVY,
		kDecodeYUY2,

		kDecodeModeCount
	} mDecodeMode;
	int mBPP;

	VDPixmapBuffer	mImageBuffer;

	void decodeFrame(uint32 *output, const uint8 *input, int len);
	const uint8 *decodeQuantTables(const uint8 *psrc);
	const uint8 *decodeFrameInfo(const uint8 *psrc);
	const uint8 *decodeScan(const uint8 *ptr, bool odd_field);
	uint8 __forceinline huffDecodeDC(uint32& bitbuf, int& bitcnt, const uint8 * const table);
	uint8 __forceinline huffDecodeAC(uint32& bitbuf, int& bitcnt, const uint8 * const table);
	const uint8 *decodeMCUs(const uint8 *ptr, bool odd_field);
};

enum {
	MARKER_SOF0			= 0xc0,		// start-of-frame, baseline scan
	MARKER_SOI			= 0xd8,		// start of image
	MARKER_EOI			= 0xd9,		// end of image
	MARKER_SOS			= 0xda,		// start of scan
	MARKER_DQT			= 0xdb,		// define quantization tables
	MARKER_DRI			= 0xdd,		// define restart interval
	MARKER_APP_FIRST	= 0xe0,
	MARKER_APP_LAST		= 0xef,
	MARKER_COMMENT		= 0xfe,
};

///////////////////////////////////////////////////////////////////////////
//
//		Construction/destruction
//
///////////////////////////////////////////////////////////////////////////

MJPEGDecoder::MJPEGDecoder(int w, int h)
	: width(w)
	, height(h)
	, restart_interval(0)
{
	for(int tbl=0; tbl<2; tbl++) {
		int base=0;
		uint8 *ptr = (uint8 *)huff_ac_quick2[tbl];
		const uint8 *countptr = huff_ac_src[tbl];
		const uint8 *codeptr = huff_ac_src[tbl] + 16;

		for(int bits=1; bits<=12; bits++) {
			for(int cnt=0; cnt<*countptr; cnt++) {
				int first, last;

				first = base;
				last = base + (0x1000 >> bits);

				if (first < 0xB00)
					first = 0xB00;

				while(first < last) {
					*ptr++ = *codeptr;
					*ptr++ = (uint8)bits;
					++first;
				}

				base = last;

				++codeptr;
			}

			++countptr;
		}

		_RPT2(0,"Code length for table %d: %04x\n", tbl, base);
	}
}

MJPEGDecoder::~MJPEGDecoder() {
}

IMJPEGDecoder *CreateMJPEGDecoder(int w, int h) {
	return new MJPEGDecoder(w, h);
}

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////


int __inline getshort(const uint8 *p) {
	return ((int)p[0]<<8) + (int)p[1];
}



void MJPEGDecoder::decodeFrameRGB15(uint32 *output, uint8 *ptr, int size) {
	mDecodeMode = kDecodeRGB15;
	mBPP = 2;
	decodeFrame(output, ptr, size);
}

void MJPEGDecoder::decodeFrameRGB32(uint32 *output, uint8 *ptr, int size) {
	mDecodeMode = kDecodeRGB32;
	mBPP = 4;
	decodeFrame(output, ptr, size);
}

void MJPEGDecoder::decodeFrameUYVY(uint32 *output, uint8 *ptr, int size) {
	mDecodeMode = kDecodeUYVY;
	mBPP = 2;
	decodeFrame(output, ptr, size);
}

void MJPEGDecoder::decodeFrameYUY2(uint32 *output, uint8 *ptr, int size) {
	mDecodeMode = kDecodeYUY2;
	mBPP = 2;
	decodeFrame(output, ptr, size);
}

void MJPEGDecoder::decodeFrame(uint32 *output, const uint8 *ptr, int size) {
	const uint8 *limit = ptr+size-1;
	uint8 tag;
	bool odd_field = true;
	int field_count = 0;
	bool bFirstField = true;

	do {
//		_RPT1(0,"Decoding %s field\n", odd_field ? "odd" : "even");

		// scan for SOI tag

		while(ptr < limit)
			if (*ptr++ == 0xff) {
				if (ptr >= limit)
					break;
				if ((tag = *ptr++) == MARKER_SOI)
					break;
				else if (tag == 0xff) {
					while(ptr<limit && *ptr == 0xff)
						++ptr;
					--ptr;
				} else {
//					_RPT0(0,"Error: markers found before SOI tag\n");
//					return;
					break;		// happens with dmb1
				}
			}

		if (ptr >= limit) {
//			_RPT0(0,"Error: SOI mark not found\n");
			return;
		}

		// parse out chunks

		while(ptr < limit) {
			if (*ptr++ == 0xff)
				switch(tag = *ptr++) {
				case MARKER_EOI:
//					_RPT1(0,"Note: EOI tag found at %p\n", ptr-2);
					goto next_field;
				case MARKER_DQT:
					ptr = decodeQuantTables(ptr);
					break;
				case MARKER_SOF0:
					ptr = decodeFrameInfo(ptr);

					// dmb1 thinks it's interlaced all the time...

					interlaced = raw_height <= ((height+15)&~15)/2;
					field_height = interlaced ? height/2 : height;

					if (mcu_height*mcu_size_y > field_height) {
						mcu_height	= (field_height + mcu_size_y - 1)/mcu_size_y;
						clip_row = field_height / mcu_size_y;
						clip_lines = field_height % mcu_size_y;
					}

					break;
				case MARKER_SOS:

					pixdst = output;
					ptr = decodeScan(ptr, odd_field);

					bFirstField = false;
					break;
				case MARKER_APP_FIRST:
					// Here we interpret 1 as odd and 2 as even. This seems backwards,
					// but that's the way the DX8 MJPEG decoder does it, so....
					if (bFirstField)
						odd_field = (ptr[6] == 1);
					else
						odd_field = !odd_field;

					ptr += getshort(ptr);
					break;
				case MARKER_DRI:
					restart_interval = getshort(ptr+2);
					ptr += 4;
					break;
				case 0xff:
					while(ptr<limit && *ptr == 0xff)
						++ptr;
					--ptr;
					break;
				case 0:
					break;
				default:
					if ((tag >= MARKER_APP_FIRST && tag <= MARKER_APP_LAST) || tag == MARKER_COMMENT) {

						ptr += getshort(ptr);
						break;
					}
//					_RPT1(0,"Warning: Unknown tag %02x\n", tag);
				}
		}
next_field:
		;
	} while(interlaced && field_count<2);

//	_RPT0(0,"Warning: No EOI tag found\n");
}

const uint8 *MJPEGDecoder::decodeQuantTables(const uint8 *psrc) {
	int *dst;
	int n;

	const char *zigzag = SSE2_enabled ? MJPEG_zigzag_sse2 : MJPEG_zigzag_mmx;

	psrc += 2;	// skip length
	while(*psrc != 0xff) {
		n = psrc[0] & 15;
		if (n>3)
			throw MyError("Error: Illegal quantization table # in DQT chunk");

		dst = quant[n];
		++psrc;

		if (psrc[-1] & 0xf0) {
			// 16-bit quantization tables

			for(n=0; n<64; n++) {
				dst[n*2+0] = getshort(psrc + n*2);
				dst[n*2+1] = zigzag[n]*2;
			}
			psrc += 128;
		} else {
			// 8-bit quantization tables

			for(n=0; n<64; n++) {
				dst[n*2+0] = psrc[n];
				dst[n*2+1] = zigzag[n]*2;
			}
			psrc += 64;
		}

//		MJPEG_IDCT_norm(dst);
	}

	return psrc;
}

const uint8 *MJPEGDecoder::decodeFrameInfo(const uint8 *psrc) {
	int i, n;

	if (psrc[2] != 8)
		throw MyError("Can only decode 8-bit images");

	raw_height = getshort(psrc + 3);
	raw_width = getshort(psrc + 5);

	if (raw_width > width || raw_height > height)
		throw MyError("Error: The size of this frame is inconsistent with the video stream (frame is %dx%d, stream is %dx%d)", raw_width, raw_height, width, height);

	if (psrc[7] != 3)
		throw MyError("Error: picture must be 3 component (YCC)");

	// parse component data

	if (psrc[12] != psrc[15])
		throw MyError("Error: chrominance subsampling factors must be the same");

	mcu_length = 0;
	for(i=0; i<3; i++) {
		n = psrc[10 + 3*i];
		if (n>3)
			throw MyError("Error: component specifies quantization table other than 0-3");

//		_RPT2(0,"Component %d uses quant %d\n", i, n);

		comp_quant[i] = quant[n];
		comp_mcu_x[i] = psrc[9 + 3*i] >> 4;
		comp_mcu_y[i] = psrc[9 + 3*i] & 15;
		comp_mcu_length[i] = comp_mcu_x[i] * comp_mcu_y[i];
		comp_id[i] = psrc[8 + 3*i];
		comp_start[i] = mcu_length;

		mcu_length += comp_mcu_length[i];
	}

	if (mcu_length > 10)
		throw MyError("Error: macroblocks per MCU > 10");

	// check subsampling format

	if (comp_mcu_x[1] != 1 || comp_mcu_y[1] != 1)
		throw MyError("Error: multiple chroma blocks not supported");

	do {

		// 4:4:4 (PICVideo's 1/1/1)

		if (comp_mcu_x[0] == 1 && comp_mcu_y[0] == 1) {
			mChromaMode = kYCrCb444;
			mcu_width	= (raw_width + 7)/8;
			mcu_height	= (raw_height + 7)/8;
			break;
		}

		// 4:2:2

		if (comp_mcu_x[0] == 2 && comp_mcu_y[0] == 1) {
			mChromaMode = kYCrCb422;
			mcu_width	= (raw_width + 15)/16;
			mcu_height	= (raw_height + 7)/8;
			break;
		}

		// 4:2:0 (PICVideo's 4/1/1)

		if (comp_mcu_x[0] == 2 && comp_mcu_y[0] == 2) {
			mChromaMode = kYCrCb420;
			mcu_width	= (raw_width + 15)/16;
			mcu_height	= (raw_height + 15)/16;
			break;
		}

		throw MyError("Error: Chroma subsampling mode not supported (must be 4:4:4, 4:2:2, or 4:2:0)");
	} while(false);

	mcu_count = mcu_width * mcu_height;
	mcu_size_x = comp_mcu_x[0] * 8;
	mcu_size_y = comp_mcu_y[0] * 8;

	// reinit frame buffer

	int format = 0;
	switch(mChromaMode) {
		case kYCrCb444:
			format = nsVDPixmap::kPixFormat_YUV444_Planar;
			break;

		case kYCrCb422:
			format = nsVDPixmap::kPixFormat_YUV422_Planar_Centered;
			break;

		case kYCrCb420:
			format = nsVDPixmap::kPixFormat_YUV420_Planar_Centered;
			break;
	}

	if (!mImageBuffer.base() || mImageBuffer.format != format) {
		// Allocate an image with space for 16x16 blocks, but set the width and height
		// to the MCUs. This promotes proper chroma handling at the edges.
		mImageBuffer.init((width + 15) & ~15, (height + 15) & ~15, format);
		mImageBuffer.w = comp_mcu_x[0] * 8 * mcu_width;
		mImageBuffer.h = comp_mcu_y[0] * 8 * mcu_height;
	}

	return psrc + 8 + 3*3;
}

const uint8 *MJPEGDecoder::decodeScan(const uint8 *ptr, bool odd_field) {
	int mb=0;
	int i,j;

	// Ns (components in scan) must be 3

	if (ptr[2] != 3)
		throw MyError("Error: scan must have 3 interleaved components");

	if (ptr[9] != 0 || ptr[10] != 63)
		throw MyError("Error: DCT coefficients must run from 0-63");

	if (ptr[11] != 0)
		throw MyError("Error: Successive approximation not allowed");

	// decode component order (indices 3, 5, 7)

	// select entropy (Huffman) coders (indices 4, 6, 8)

	for(i=0; i<3; i++) {
		for(j=0; j<3; j++)
			if (ptr[3+2*i] == comp_id[j])
				break;

		if (j>=3)
			throw MyError("Error: MJPEG scan has mislabeled component");

		mb = comp_start[j];

		// Add the macroblocks *vertically* -- this makes 4:2:0 considerably easier.

		for(j=0; j<comp_mcu_x[i]*comp_mcu_y[i]; j++) {
			blocks[mb].huff_dc	= huff_dc[ptr[4+2*i]>>4];
			blocks[mb].huff_ac	= huff_ac[ptr[4+2*i]&15];
			blocks[mb].huff_ac_quick = huff_ac_quick[ptr[4+2*i]&15];
			blocks[mb].huff_ac_quick2 = huff_ac_quick2[ptr[4+2*i]&15];
			blocks[mb].quant	= comp_quant[i];
			blocks[mb].dc_ptr	= &comp_last_dc[i];
			++mb;
		}

//		comp_last_dc[i] = 128*8;
	}

	static const char translator_444_422[] = { 0,1,2,3 };
	static const char translator_420[] = { 0,1,2,3,4,5 };

	for(i=0; i<mcu_length; i++) {
		blocks[i+mcu_length] = blocks[i];
		blocks[i+mcu_length*2] = blocks[i];
		blocks[i+mcu_length*3] = blocks[i];

		dct_coeff_ptrs[i] = &dct_coeff[i][0];
		dct_coeff_ptrs[i+mcu_length] = &dct_coeff[i+mcu_length][0];
		dct_coeff_ptrs[i+mcu_length*2] = &dct_coeff[i+mcu_length*2][0];
		dct_coeff_ptrs[i+mcu_length*3] = &dct_coeff[i+mcu_length*3][0];
	}

	comp_last_dc[0] = 128*8;
	comp_last_dc[1] = 128*8;
	comp_last_dc[2] = 128*8;

	ptr += 12;

	return decodeMCUs(ptr, odd_field);
}

///////////////////////////////////////////////////////////////////////////

// 320x240 -> 20x30 -> 600 MCUs
// 304x228 -> 19x29 -> 551 MCUs 

const uint8 *MJPEGDecoder::decodeMCUs(const uint8 *ptr, bool odd_field) {
	int mcu;
	uint32 bitbuf = 0;
	int bitcnt = 24;	// 24 - bits in buffer
	int mb_x = 0, mb_y = 0;

	const ptrdiff_t pitchY = mImageBuffer.pitch;
	const ptrdiff_t pitchCb = mImageBuffer.pitch2;
	const ptrdiff_t pitchCr = mImageBuffer.pitch3;
	uint8 *dstrowY = (uint8 *)mImageBuffer.data;
	uint8 *dstrowCb = (uint8 *)mImageBuffer.data2;
	uint8 *dstrowCr = (uint8 *)mImageBuffer.data3;
	uint8 *dstY = dstrowY;
	uint8 *dstCb = dstrowCb;
	uint8 *dstCr = dstrowCr;

	// Decode!!!

	for(mcu=0; mcu<mcu_length*4; mcu++)
		memset(dct_coeff[mcu], 0, 128);

	int nRestartCounter = restart_interval;
	int nRestartOffset = 0;

	if (!nRestartCounter)
		nRestartCounter = 0x7FFFFFFF;

	void (*pIDCT)(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int ac_last);

#ifdef _M_IX86
	if (SSE2_enabled)
		pIDCT = IDCT_sse2;
	else if (ISSE_enabled)
		pIDCT = IDCT_isse;
	else
		pIDCT = IDCT_mmx;
#else
	pIDCT = IDCT_sse2;
#endif

	mcu = 0;
	__try {
		while(mcu<mcu_count) {
			int mcus = 4;

			if (mcu >= mcu_count-4)
				mcus = mcu_count - mcu;

			if (mcus > nRestartCounter)
				mcus = nRestartCounter;

			mb_decode(bitbuf, bitcnt, ptr, mcu_length*mcus, blocks, dct_coeff_ptrs);

			mcu += mcus;

			nRestartCounter -= mcus;
			if (!nRestartCounter && mcu < mcu_count) {
				unsigned tag = 0xd0ff + nRestartOffset;
				int i;

				for(i=0; i<8; ++i) {
					if (*(unsigned short *)ptr  == tag)
						break;

					--ptr;
				}

				if (i >= 8)
					return ptr;

				ptr += 2;

				bitcnt = 24;
				bitbuf = 0;
				nRestartCounter = restart_interval;
				nRestartOffset = (nRestartOffset + 0x0100) & 0xd7ff;

				// Reset all DC coefficients

				comp_last_dc[0] = 128*8;
				comp_last_dc[1] = 128*8;
				comp_last_dc[2] = 128*8;
			}

			short **dct_src = dct_coeff_ptrs;
			const MJPEGBlockDef *block_src = blocks;

			for(int i=0; i<mcus; ++i) {
				// transform luma blocks
				switch(mChromaMode) {
					case kYCrCb444:
						pIDCT(dct_src[0], dstY, pitchY, 1, block_src[0].ac_last);
						++dct_src;
						++block_src;
						dstY += 8;
						break;

					case kYCrCb422:
						pIDCT(dct_src[0], dstY, pitchY, 1, block_src[0].ac_last);
						pIDCT(dct_src[1], dstY + 8, pitchY, 1, block_src[1].ac_last);
						dct_src += 2;
						block_src += 2;
						dstY += 16;
						break;

					case kYCrCb420:
						pIDCT(dct_src[0], dstY, pitchY, 1, block_src[0].ac_last);
						pIDCT(dct_src[1], dstY + 8, pitchY, 1, block_src[1].ac_last);
						pIDCT(dct_src[2], dstY + pitchY * 8, pitchY, 1, block_src[2].ac_last);
						pIDCT(dct_src[3], dstY + pitchY * 8 + 8, pitchY, 1, block_src[3].ac_last);
						dct_src += 4;
						block_src += 4;
						dstY += 16;
						break;
				}

				// transform chroma blocks
				pIDCT(dct_src[0], dstCb, pitchCb, 1, block_src[0].ac_last);
				pIDCT(dct_src[1], dstCr, pitchCr, 1, block_src[1].ac_last);
				dct_src += 2;
				block_src += 2;
				dstCr += 8;
				dstCb += 8;

				if (++mb_x >= mcu_width) {
					mb_x = 0;
					++mb_y;

					switch(mChromaMode) {
						case kYCrCb444:
						case kYCrCb422:
							dstrowY += pitchY * 8;
							break;

						case kYCrCb420:
							dstrowY += pitchY * 16;
							break;
					}

					dstrowCb += pitchCb * 8;
					dstrowCr += pitchCr * 8;

					dstY = dstrowY;
					dstCb = dstrowCb;
					dstCr = dstrowCr;
				}
			}

			for(int i=0; i<mcu_length*mcus; i++)
				memset(dct_coeff[i], 0, 128);
		}
	} _except(1) {
		// This ain't good, folks
#ifdef _M_IX86
		__asm emms

		if (ISSE_enabled)
			__asm sfence
#endif
		throw MyError("MJPEG decoder: Access violation caught.  Source may be corrupted.");
	}

#ifdef _M_IX86
	__asm emms

	if (ISSE_enabled)
		__asm sfence
#endif

	VDPixmap pxdst = {0};

	pxdst.w = width;
	pxdst.h = height;
	pxdst.pitch = (width * mBPP + 3) & ~3;
	pxdst.data = pixdst;

	switch(mDecodeMode) {
		case kDecodeRGB15:
			pxdst.format = nsVDPixmap::kPixFormat_XRGB1555;
			pxdst.data = (char *)pxdst.data + pxdst.pitch * (pxdst.h - 1);
			pxdst.pitch = -pxdst.pitch;
			break;

		case kDecodeRGB32:
			pxdst.format = nsVDPixmap::kPixFormat_XRGB8888;
			pxdst.data = (char *)pxdst.data + pxdst.pitch * (pxdst.h - 1);
			pxdst.pitch = -pxdst.pitch;
			break;

		case kDecodeUYVY:
			pxdst.format = nsVDPixmap::kPixFormat_YUV422_UYVY;
			break;

		case kDecodeYUY2:
			pxdst.format = nsVDPixmap::kPixFormat_YUV422_YUYV;
			break;
	}


	if (interlaced)
		VDPixmapBlt(VDPixmapExtractField(pxdst, odd_field), mImageBuffer);
	else
		VDPixmapBlt(pxdst, mImageBuffer);

//	return ptr - ((31-bitcnt)>>3);
	return ptr - 8;
}

















void mb_decode(uint32& bitheap0, int& bitcnt0, const uint8 *& src0, int mcu_length, MJPEGBlockDef *block, short **dctarray) {
	const uint8 * VDRESTRICT src = src0;
	uint32 bitheap = bitheap0;
	int bitcnt = bitcnt0;

	bitcnt -= 8;

	for(int mb=0; mb<mcu_length; ++mb) {
		const int *quantptr = block->quant + 2;
		const uint8 (*const acquick)[2] = block->huff_ac_quick;
		const uint8 (*const acquick2)[2] = block->huff_ac_quick2;
		const uint8 *const huffac = block->huff_ac;
		short *dctptr = *dctarray++;
		const int *quantlimit = quantptr + 63*2;

		// fill bit heap
		if (bitcnt >= -8) {
			do {
				uint32 c = *src;
				src += ((c + 0x101) >> 8);
				bitheap += c << (bitcnt + 8);
				bitcnt -= 8;
			} while(bitcnt >= 0);
		}

		// decode DC coefficient
		int i = 0;
		int code = 0;
		sint32 acc = bitheap >> 30;
		bitheap <<= 2;
		while(i < 15) {
			uint32 count = block->huff_dc[i + 1];
			if (acc < count)
				break;
			acc -= count;
			acc = (acc + acc) + (bitheap >> 31);
			bitheap += bitheap;
			code += count;
			++i;
		}

		bitcnt += i + 2;
		code += acc;

		VDASSERT(bitcnt < 32);
		VDASSERT(code <= 16);
		
		sint32 coeff = 0;
		if (code) {
			// sign-extend DC difference

			sint32 sign = ~((sint32)bitheap >> 31);
			union {
				uint32 d[2];
				uint64 q;
			} converter = { { bitheap, sign } };

#ifdef VD_COMPILER_MSVC
			converter.q = __ll_lshift(converter.q, code);
#else
			converter.q <<= code;
#endif

			bitheap = converter.d[0];
			coeff = converter.d[1] - sign;
			bitcnt += code;
		}

		coeff *= quantptr[-2];
		coeff += *block->dc_ptr;
		*block->dc_ptr = coeff;
		*dctptr = (short)coeff;

		VDASSERT(bitcnt < 32);

		// BEGIN DECODING AC COEFFICIENTS

		for(;;) {
			VDASSERT(bitcnt < 32);
			if (bitcnt >= 0) {
				uint32 c = VDSwizzleU32(*(uint16 *)src) >> 16;
				src += 2;

				if (~(c + 0x01010101) & (c & 0x80808080)) {
					c = src[-2];
					src += (c + 1) >> 8;

					uint32 d = src[-1];
					c = (c << 8) + d;
					src += (d + 1) >> 8;
				}

				bitheap += c << bitcnt;
				bitcnt -= 16;
			}

			if (bitheap < 0xb0000000) {
				// short decode
				// table-based decode for short AC coefficients

				const uint8 *acdata = acquick[bitheap >> 25];
				int code = (sint8)acdata[0];
				uint8 len = acdata[1];

				bitheap <<= len;
				bitcnt += len;

				if (!code)
					break;

				// multiply coefficient by quant. matrix entry and store
				int idx = quantptr[1];
				*(short *)((char *)dctptr + idx) = (short)(code * quantptr[0]);
				quantptr += 2;

				if (idx >= 63*2)
					break;

				continue;
			}

			uint8 code;
			if (bitheap >= 0xff800000) {
				// long decode
				// start long AC decode
				//
				//	16: 00-7F
				//	15: 00-3F
				//	14: 00-1F

				const uint8 *acdata = &huffac[((bitheap >> 16) - 0xFF80) * 2];
				code = acdata[0];
				int len = acdata[1];
				bitheap <<= len;
				bitcnt += len;

				if (bitcnt > 0) {
					uint32 c = VDSwizzleU32(*(uint16 *)src) >> 16;
					src += 2;
					if (~(c + 0x01010101) & (c & 0x80808080)) {
						c = src[-2];
						src += (c + 1) >> 8;
						uint32 d = src[-1];
						c = (c << 8) + d;
						src += (d + 1) >> 8;
					}
					bitheap += (c << bitcnt);
					bitcnt -= 16;
				}
			} else {
				// medium decode
				const uint8 *acdata = acquick2[(bitheap >> 20) - 0xB00];
				code = acdata[0];
				int len = acdata[1];

				bitheap <<= len;
				bitcnt += len;

				if (bitcnt > 0) {
					uint32 c = VDSwizzleU32(*(uint16 *)src) >> 16;
					src += 2;
					if (~(c + 0x01010101) & (c & 0x80808080)) {
						c = src[-2];
						src += (c + 1) >> 8;
						uint32 d = src[-1];
						c = (c << 8) + d;
						src += (d + 1) >> 8;
					}
					bitheap += (c << bitcnt);
					bitcnt -= 16;
				}

				if (code == 0xf0) {
					quantptr += 16*2;
					if (quantptr > quantlimit) {
						quantptr = quantlimit;
						break;
					}
					continue;
				}
			}

			int skip = code >> 4;
			int len = code & 15;

			quantptr += skip*2 + 2;
			if (quantptr > quantlimit) {
				quantptr = quantlimit;
				break;
			}

			sint32 sign = ~((sint32)bitheap >> 31);
			union {
				uint32 d[2];
				uint64 q;
			} converter = { { bitheap, sign } };

#ifdef VD_COMPILER_MSVC
			converter.q = __ll_lshift(converter.q, len);
#else
			converter.q <<= len;
#endif

			bitheap = converter.d[0];
			coeff = converter.d[1] - sign;
			bitcnt += len;

			int idx = quantptr[-1];
			*(short *)((char *)dctptr + idx) = (short)(coeff*quantptr[-2]);

			if (idx >= 63*2)
				break;
		};

		block->ac_last = (((quantptr - block->quant) >> 1) - 1) & 63;
		++block;
	}

	bitcnt += 8;

	bitheap0 = bitheap;
	bitcnt0 = bitcnt;
	src0 = src;
}

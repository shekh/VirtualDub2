//	Priss (NekoAmp 2.0) - MPEG-1/2 audio decoding library
//	Copyright (C) 2003 Avery Lee
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

#include <math.h>
#include "engine.h"
#include "bitreader.h"

// The format of an MPEG layer II frame:
//
// 1) bit allocations for separate subbands (2..4 bits per channel per subband)
// 2) bit allocations for joint subbands (2..4 bits per subband)
// 3) scale factor selector indices (2 bits per channel per subband that has
//    bits allocated)
// 4) scale factors (6-18 bits per channel per subband that has bits allocated,
//                   depending on scale factor selector)
// 5) repeated 12 times:
//    a) samples for seperate subbands (3 x n bits per channel per subband)
//    b) samples for joint subbands (3 x n bits per subband)
//
// There are up to two channels and always 32 subbands.  Channels in the joint
// stereo range share the same samples but may have different scale factors.

namespace {
	// bit allocation table rows (ISO 11172-3 Table B.2)
	//
	// These tables tell us how many bits (or which grouping type) to use to
	// decode a subband sample in a subband.  Different subbands have different
	// selections according to the table.
	//
	static const uint8 g_L2BitAllocTableRows[6][16] = {
		{  0, 17,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16 },	// table B.2a, subbands 0-2; table B.2b, subbands 0-2
		{  0, 17, 18,  3, 19,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 16 },	// table B.2a, subbands 3-10; table B.2b, subbands 3-10
		{  0, 17, 18,  3, 19,  4,  5, 16 },									// table B.2a, subbands 11-22; table B.2b, subbands 11-22
		{  0, 17, 18, 16 },													// table B.2a, subbands 23-26; table B.2b, subbands 23-29
		{  0, 17, 18, 19,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },	// table B.2c, subbands 0-7; table B.2d, subbands 0-11; 13818-3 table B.1, subbands 4-29
		{  0, 17, 18,  3, 19,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14 },	// 13818-3 table B.1, subbands 0-3
	};

	// bit allocation table row pointers
	static const struct L2BitAllocTableBand {
		uint8 table_row;		// row from above table to use
		uint8 subband_count;	// number of subbands
		uint8 bits;				// number of bits per subband to index into row
	} g_L2BitAllocTables[5][4]={
		{ { 0, 3, 4 }, { 1, 8, 4 }, { 2, 12, 3 }, { 3, 4, 2 } },	// MPEG-1 table A (27 subbands / 88 bits)
		{ { 0, 3, 4 }, { 1, 8, 4 }, { 2, 12, 3 }, { 3, 7, 2 } },	// MPEG-1 table B (30 subbands / 94 bits)
		{ { 4, 2, 4 }, { 4, 6, 3 } },								// MPEG-1 table C ( 8 subbands / 26 bits)
		{ { 4, 2, 4 }, { 4, 10, 3 } },								// MPEG-1 table D (12 subbands / 38 bits)
		{ { 5, 4, 4 }, { 4, 7, 3 }, { 4, 19, 2 } },					// MPEG-2 table   (30 subbands / 75 bits)
	};

	// bit allocation table selectors, by channels, frequency and bitrate
	static const sint8 g_L2BitAllocTableSelector[2][3][16]={
//			inv, 32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384,ff
		{
			{-1,  2,  2,  0,  0,  0,  1,  1,  1,  1,  1, -1, -1, -1, -1, 1 },		// 44.1KHz mono
			{-1,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, 0 },		// 48KHz mono
			{-1,  3,  3,  0,  0,  0,  1,  1,  1,  1,  1, -1, -1, -1, -1, 1 },		// 32KHz mono
		},
		{
			{-1, -1, -1, -1,  2, -1,  2,  0,  0,  0,  1,  1,  1,  1,  1, 0 },		// 44.1KHz stereo
			{-1, -1, -1, -1,  2, -1,  2,  0,  0,  0,  0,  0,  0,  0,  0, 0 },		// 48KHz stereo
			{-1, -1, -1, -1,  3, -1,  3,  0,  0,  0,  1,  1,  1,  1,  1, 0 },		// 32KHz stereo
		}
	};
};

bool VDMPEGAudioDecoder::DecodeLayerII() {
	unsigned	bitalloc[32][2] = {0};		// bit allocations
	uint8		scfsi[32][2];				// scale factor selector indices (32 subbands x 2 channels)
	float		scalefac[3][32][2];			// scale factors (3 parts x 32 subbands x 2 channels)
	unsigned	sb;							// subband
	unsigned	sblimit;					// subband limit for decoding
	unsigned	bound = 32;					// start of joint-stereo or mono decoding
	bool		stereo = mMode != 3;
	bool		is_mpeg2 = mHeader.nMPEGVer > 1;

	if (mMode == 1)
		bound = mModeExtension * 4 + 4;

	if (!stereo)
		bound = 0;

	// reservoir analysis
	//
	// Worst case for layer II is 8Kbps, 48KHz stereo, giving a frame size of
	// 24 bytes, or a data payload of 160 bits.  Well, the MPEG-2 bit allocation
	// table takes 150 bits for two channels, so MPEG-2 bit allocation reading
	// will never overflow.  For MPEG-1, the bound goes up to 32Kbps, raising
	// the payload to 736 bits.  We definitely won't hit that!

	VDMPEGAudioBitReader bits(mFrameBuffer, mFrameDataSize);

	// select bit allocation table
	int bitalloc_table = is_mpeg2 ? 4 : g_L2BitAllocTableSelector[stereo][mSamplingRateIndex][mBitrateIndex];

	if (bitalloc_table < 0)
		throw (int)ERR_INVALIDDATA;		// combination not permitted by standard

	// read in bit allocations (max: 188 bits)
	//
	// The bit allocation table has been broken down into four ranges of
	// subbands, each of which has a row type and a number of bits to
	// fetch per subband to index into that row type.

	static const uint8 bits_per_sample_triad[]={0,3*1,3*2,3*3,3*4,3*5,3*6,3*7,3*8,3*9,3*10,3*11,3*12,3*13,3*14,3*15,3*16,5,7,10};

	unsigned nonzero_s = 0, nonzero_js = 0;
	unsigned total_s = 0, total_js = 0;

	sb = 0;
	for(unsigned band=0; band<4; ++band) {
		const L2BitAllocTableBand& bandinfo = g_L2BitAllocTables[bitalloc_table][band];
		const uint8 *bitalloc_row = g_L2BitAllocTableRows[bandinfo.table_row];

		for(unsigned i=0; i<bandinfo.subband_count; ++i) {
			bitalloc[sb][0] = bitalloc[sb][1] = bitalloc_row[bits.get(bandinfo.bits)];

			if (sb < bound) {
				bitalloc[sb][1] = bitalloc_row[bits.get(bandinfo.bits)];

				if (bitalloc[sb][0]) {
					++nonzero_s;
					total_s += bits_per_sample_triad[bitalloc[sb][0]];
				}

				if (bitalloc[sb][1]) {
					++nonzero_s;
					total_s += bits_per_sample_triad[bitalloc[sb][1]];
				}
			} else {
				if (bitalloc[sb][0]) {
					++nonzero_js;
					total_js += bits_per_sample_triad[bitalloc[sb][0]];
				}
			}

			++sb;
		}
	}
	sblimit = sb;

	// space check - scalefactor selectors and samples
	//
	// 2 bits per channel per subband with non-zero allocation

	unsigned scfsi_bits = 2*nonzero_js;
	
	if (stereo)
		scfsi_bits += 2*(nonzero_s + nonzero_js);

	if (!bits.chkavail(scfsi_bits))
		throw (int)ERR_INCOMPLETEFRAME;

#ifdef _DEBUG
	const unsigned expected_left_scfsi = bits.avail() - scfsi_bits;
#endif

	// read in scalefactor selectors

	unsigned scf_total = 0;

	static const uint8 bits_per_scfsi[4]={3*6, 2*6, 1*6, 2*6};

	if (stereo) {
		for(sb=0; sb<sblimit; ++sb) {
			if (bitalloc[sb][0])
				scf_total += bits_per_scfsi[scfsi[sb][0] = bits.get(2)];

			if (bitalloc[sb][1])
				scf_total += bits_per_scfsi[scfsi[sb][1] = bits.get(2)];
		}
	} else {
		for(sb=0; sb<sblimit; ++sb) {
			if (bitalloc[sb][0])
				scf_total += bits_per_scfsi[scfsi[sb][0] = bits.get(2)];
		}
	}

	// space check - scalefactors and samples
	//
	// 6 bits per scalefactor, plus 12 granules * 3 samples * total_sample_bits

	const unsigned scf_and_samples_bits = scf_total + 12*(total_s + total_js);
	
	if (!bits.chkavail(scf_and_samples_bits))
		throw (int)ERR_INCOMPLETEFRAME;

#ifdef _DEBUG
	VDASSERT(bits.avail() == expected_left_scfsi);

	const unsigned expected_left = bits.avail() - scf_and_samples_bits;
#endif

	// read in scale factors

	static const float sL2Dequant[18]={
		2.0f / 7.0f,
		2.0f / 15.0f,
		2.0f / 31.0f,
		2.0f / 63.0f,
		2.0f / 127.0f,
		2.0f / 255.0f,
		2.0f / 511.0f,
		2.0f / 1023.0f,
		2.0f / 2047.0f,
		2.0f / 4095.0f,
		2.0f / 8191.0f,
		2.0f / 16383.0f,
		2.0f / 32767.0f,
		2.0f / 65536.0f,
		2.0f / 3.0f,		// grouping - 3 x 3 steps
		2.0f / 5.0f,		// grouping - 3 x 5 steps
		2.0f / 9.0f,		// grouping - 3 x 9 steps
	};

	const unsigned nch = stereo ? 2 : 1;

	for(sb=0; sb<sblimit; ++sb) {
		for(unsigned ch=0; ch<nch; ++ch) {
			if (bitalloc[sb][ch]) {
				VDASSERT(bitalloc[sb][ch] >= 3);
				const float dq = sL2Dequant[bitalloc[sb][ch] - 3];
				float sf0 = dq*mL2Scalefactors[bits.get(6)];

				scalefac[0][sb][ch] = sf0;

				switch(scfsi[sb][ch]) {
				case 0:					// 3 distinct scale factors
					scalefac[1][sb][ch] = dq*mL2Scalefactors[bits.get(6)];
					scalefac[2][sb][ch] = dq*mL2Scalefactors[bits.get(6)];
					break;
				case 1:					// granules 0 and 1 share common scalefactor
					scalefac[1][sb][ch] = sf0;
					scalefac[2][sb][ch] = dq*mL2Scalefactors[bits.get(6)];
					break;
				case 3:					// granules 1 and 2 share common scalefactor
					scalefac[1][sb][ch] = scalefac[2][sb][ch] = dq*mL2Scalefactors[bits.get(6)];
					break;
				case 2:					// single scalefactor
					scalefac[1][sb][ch] = scalefac[2][sb][ch] = sf0;
					break;
				default:
					VDNEVERHERE;
				}
			}
		}
	}

	// dequantize subband samples and run through polyphase
	// (12*32 = 384 samples/frame)
	//
	// The layer I dequantization algorithm, according to 11172-3:
	// 1) Grab bits as two's complement value.
	// 2) Toggle MSB.
	// 3) Divide by 2^(N-1) so fraction is in [-1, 1).
	// 4) Compute y = (2^N / (2^N-1)) * (x + 2^(1-N)).
	//
	// Steps 1 and 2 essentially decode a biased value:
	//    x = (getbits(N) - 2^(N-1)) / 2^(N-1)
	//
	// We can absorb the additional bias from step 4:
	//    x' = (getbits(N) - 2^(N-1) + 1) / 2^(N-1)
	//
	// All ones as a sample is not valid.

	for(unsigned gr=0; gr<12; ++gr) {
		unsigned part = gr >> 2;
		float sample[3][2][32] = {0};
		unsigned nch = 2;

		for(sb=0; sb<sblimit; ++sb) {
			if (sb == bound)
				nch = 1;

			for(unsigned ch=0; ch<nch; ++ch) {
				if (unsigned n = bitalloc[sb][ch]) {
					int v0, v1, v2;

					if (n <= 16) {
						int bias = 1 - (1<<(n-1));

						v0 = (int)bits.get(n) + bias;
						v1 = (int)bits.get(n) + bias;
						v2 = (int)bits.get(n) + bias;

						VDASSERT(v0-bias != ((1<<n)-1));
						VDASSERT(v1-bias != ((1<<n)-1));
						VDASSERT(v2-bias != ((1<<n)-1));
					} else {
						int v;

						switch(n) {
						case 17:		// 3 x 3 (5 bits)
							v = (int)bits.get(5);
							VDASSERT(v < 27);
							v0 = mL2Ungroup3[v][0];
							v1 = mL2Ungroup3[v][1];
							v2 = mL2Ungroup3[v][2];
							break;
						case 18:		// 3 x 5 (7 bits)
							v = (int)bits.get(7);
							VDASSERT(v < 125);
							v0 = (v % 5) - 2;
							v1 = ((v / 5) % 5) - 2;
							v2 = ((v / 25) % 5) - 2;
							break;
						case 19:		// 3 x 9 (10 bits)
							v = (int)bits.get(10);
							VDASSERT(v < 729);
							v0 = (v % 9) - 4;
							v1 = ((v / 9) % 9) - 4;
							v2 = ((v / 81) % 9) - 4;
							break;
						default:
							VDNEVERHERE;
						}
					}

					sample[0][ch][sb] = v0 * scalefac[part][sb][ch];
					sample[1][ch][sb] = v1 * scalefac[part][sb][ch];
					sample[2][ch][sb] = v2 * scalefac[part][sb][ch];
					if (stereo && sb >= bound) {
						sample[0][1][sb] = v0 * scalefac[part][sb][1];
						sample[1][1][sb] = v1 * scalefac[part][sb][1];
						sample[2][1][sb] = v2 * scalefac[part][sb][1];
					}
				}
			}
		}

		for(unsigned s=0; s<3; ++s) {
			mpPolyphaseFilter->Generate(sample[s][0], stereo?sample[s][1]:NULL, mpSampleDst);
			mpSampleDst += stereo ? 64 : 32;
		}
	}

	mSamplesDecoded = stereo ? 2304 : 1152;

#ifdef _DEBUG
	VDASSERT(bits.avail() == expected_left);
#endif

	return true;
}

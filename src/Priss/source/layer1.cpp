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

#include "engine.h"
#include "bitreader.h"

// The format of an MPEG layer I frame:
//
// 1) bit allocations for separate subbands (4 bits per channel per subband)
// 2) bit allocations for joint subbands (4 bits per subband)
// 3) scale factors (6 bits per channel per subband that has bits allocated)
// 4) repeated 12 times:
//    a) samples for seperate subbands (n bits per channel per subband)
//    b) samples for joint subbands (n bits per subband)
//
// There are up to two channels and always 32 subbands.  Channels in the joint
// stereo range share the same samples but may have different scale factors.

bool VDMPEGAudioDecoder::DecodeLayerI() {
	unsigned	bitalloc[32][2] = {0};		// bit allocations
	float		scalefac[32][2];			// scale factors
	unsigned	sb;							// subband, channel
	unsigned	bound = 32;				// subband limit for stereo encoding
	bool		stereo = mMode != 3;

	if (mMode == 1)
		bound = mModeExtension * 4 + 4;

	if (!stereo)
		bound = 0;

	// The most critical case for layer I decoding is 32Kbps, 48KHz stereo.
	// That gives a frame size of 32 bytes, or 28 bytes of sound payload.

	VDMPEGAudioBitReader bits(mFrameBuffer, mFrameDataSize);

	// read in bit allocations
	//
	// Worst case is 2 channels x 32 subbands x 4 bits = 32 bytes.  So,
	// unfortunately, even this stage can die. :(

	if (!bits.chkavail(32*4 + bound*4))
		throw (int)ERR_INCOMPLETEFRAME;

	unsigned nonzero_s=0, total_s=0;

	for(sb=0; sb<bound; ++sb) {
		bitalloc[sb][0] = bits.get(4);
		bitalloc[sb][1] = bits.get(4);

		if (bitalloc[sb][0]) {
			++bitalloc[sb][0];
			++nonzero_s;
			total_s += bitalloc[sb][0];
		}

		if (bitalloc[sb][1]) {
			++bitalloc[sb][1];
			++nonzero_s;
			total_s += bitalloc[sb][1];
		}
	}

	unsigned nonzero_js = 0, total_js = 0;

	for(; sb<32; ++sb) {
		bitalloc[sb][0] = bits.get(4);

		if (bitalloc[sb][0]) {
			++bitalloc[sb][0];
			++nonzero_js;
			total_js += bitalloc[sb][0];
			bitalloc[sb][1] = bitalloc[sb][0];
		}
	}

	// Bit check.
	//
	// We need 6 bits per non-zero bitalloc for scalefactors, and N*12 bits for
	// samples.  For joint stereo bands, samples are shared but scalefactors are
	// not.

	unsigned scf_and_samples_bits = 6*nonzero_js + 12*total_js;

	if (stereo)
		scf_and_samples_bits += 6*(nonzero_s + nonzero_js) + 12*total_s;

	if (!bits.chkavail(scf_and_samples_bits))
		throw (int)ERR_INCOMPLETEFRAME;

#ifdef _DEBUG
	const unsigned expected_left = bits.avail() - scf_and_samples_bits;
#endif

	// read in scale factors (convert bit allocation indices to bits on the way)

	static const float sL1Dequant[14]={
		2.0f / 3.0f,
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
	};

	if (stereo) {
		for(sb=0; sb<32; ++sb) {
			if (bitalloc[sb][0])
				scalefac[sb][0] = mL2Scalefactors[bits.get(6)] * sL1Dequant[bitalloc[sb][0] - 2];

			if (bitalloc[sb][1])
				scalefac[sb][1] = mL2Scalefactors[bits.get(6)] * sL1Dequant[bitalloc[sb][1] - 2];
		}
	} else {
		for(sb=0; sb<32; ++sb) {
			if (bitalloc[sb][0])
				scalefac[sb][0] = mL2Scalefactors[bits.get(6)] * sL1Dequant[bitalloc[sb][0] - 2];
		}
	}

	// dequantize subband samples and run through polyphase
	// (12*32 = 384 samples/frame)
	//
	// The layer I dequantization algorithm, according to 11172-3:
	// 1) Grab bits as two's complement value.
	// 2) Toggle MSB.
	// 3) Divide by 2^N so fraction is in [-.5, .5).
	// 4) Compute y = (2^N / (2^N-1)) * (x + 2^(1-N)).
	//
	// Steps 1 and 2 essentially decode a biased value:
	//    x = (getbits(N) - 2^(N-1)) / 2^N
	//
	// We can absorb the additional bias from step 4:
	//    x' = (getbits(N) - 2^(N-1) + 2) / 2^N
	//
	// All ones as a sample is not valid.

	for(unsigned s=0; s<12; ++s) {
		float sample[2][32];

		for(sb=0; sb<bound; ++sb) {
			float x = 0.0f, y = 0.0f;

			if (unsigned b0 = bitalloc[sb][0])
				x = ((int)bits.get(b0) + 1 - (1<<(b0-1))) * scalefac[sb][0];

			if (unsigned b1 = bitalloc[sb][1])
				y = ((int)bits.get(b1) + 1 - (1<<(b1-1))) * scalefac[sb][1];

			sample[0][sb] = x;
			sample[1][sb] = y;
		}

		if (stereo) {
			for(; sb<32; ++sb) {
				float x = 0.0f, y = 0.0f;

				if (unsigned b0 = bitalloc[sb][0]) {
					float s = (float)((int)bits.get(b0) + 1 - (1<<(b0-1)));
					x = s * scalefac[sb][0];
					y = s * scalefac[sb][1];
				}

				sample[0][sb] = x;
				sample[1][sb] = y;
			}
		} else {
			for(; sb<32; ++sb) {
				float x = 0.0f;

				if (unsigned b0 = bitalloc[sb][0])
					x = ((int)bits.get(b0) + 1 - (1<<(b0-1))) * scalefac[sb][0];

				sample[0][sb] = sample[1][sb] = x;
			}
		}

		mpPolyphaseFilter->Generate(sample[0], stereo?sample[1]:NULL, mpSampleDst);

		mpSampleDst += stereo ? 64 : 32;
	}

	mSamplesDecoded = stereo ? 768 : 384;

#ifdef _DEBUG
	VDASSERT(bits.avail() == expected_left);
#endif

	return true;
}

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
#include <vd2/system/vdtypes.h>

#include "engine.h"

IVDMPEGAudioDecoder *VDCreateMPEGAudioDecoder() {
	return new VDMPEGAudioDecoder;
}

VDMPEGAudioDecoder::VDMPEGAudioDecoder()
	: mpSource(NULL)
	, mpPolyphaseFilter(NULL)
{
	Reset();
}

VDMPEGAudioDecoder::~VDMPEGAudioDecoder() {
	delete mpPolyphaseFilter;
}

void VDMPEGAudioDecoder::Init() {
	unsigned i,j;

	// layer II initialization

	for(i=0; i<64; ++i)
		mL2Scalefactors[i] = (float)(2.0 / pow(2.0, i/3.0));

	for(i=0; i<32; ++i) {
		mL2Ungroup3[i][0] = (i % 3) - 1;
		mL2Ungroup3[i][1] = ((i / 3) % 3) - 1;
		mL2Ungroup3[i][2] = ((i / 9) % 3) - 1;
	}

	// layer III initialization

	for(i=0; i<36; ++i)
		mL3Windows[0][i] = (float)sin((3.1415926535897932/36.0)*(i+0.5));

	for(i=0; i<18; ++i) {
		mL3Windows[1][i] = (float)sin((3.1415926535897932/36.0)*(i+0.5));
		mL3Windows[3][i+18] = (float)sin((3.1415926535897932/36.0)*(i+18+0.5));
	}

	for(i=0; i<6; ++i) {
		mL3Windows[1][i+18] = 1.0f;
		mL3Windows[1][i+24] = (float)sin((3.1415926535897932/12.0)*(i+6+0.5));
		mL3Windows[1][i+30] = 0.0f;

		mL3Windows[3][i] = 0.0f;
		mL3Windows[3][i+6] = (float)sin((3.1415926535897932/12.0)*(i+0.5));
		mL3Windows[3][i+12] = 1.0f;
	}

	for(i=0; i<12; ++i) {
		mL3Windows[2][i] = (float)sin((3.1415926535897932/12.0)*(i+0.5));
	}

	static const float coeff_idct_to_imdct[18]={		// 1/[2 cos (pi/72)(2i+1)]
		0.50047634258166f,
		0.50431448029008f,
		0.51213975715725f,
		0.52426456257041f,
		0.54119610014620f,
		0.56369097343317f,
		0.59284452371708f,
		0.63023620700513f,
		0.67817085245463f,
		0.74009361646113f,
		0.82133981585229f,
		0.93057949835179f,
		1.08284028510010f,
		1.30656296487638f,
		1.66275476171152f,
		2.31011315767265f,
		3.83064878777019f,
		11.46279281302667f,
	};

	for(j=0; j<4; ++j) {
		if (j==2)
			continue;
		for(i=0; i<9; ++i) {
			mL3Windows[j][i] *= coeff_idct_to_imdct[i+9];
			mL3Windows[j][i+9] *= coeff_idct_to_imdct[17-i];
			mL3Windows[j][i+18] *= -coeff_idct_to_imdct[8-i];
			mL3Windows[j][i+27] *= -coeff_idct_to_imdct[i];
		}
	}

	for(i=0; i<256; ++i) {
		float x = powf(fabsf(i - 128.0f), 4.0f/3.0f);

		if (i < 128)
			x = -x;

		mL3Pow43Tab[i] = x;
	}
}

void VDMPEGAudioDecoder::SetSource(IVDMPEGAudioBitsource *pSource) {
	mpSource = pSource;
}

void VDMPEGAudioDecoder::SetDestination(sint16 *psDest) {
	mpSampleDst = psDest;
}

uint32 VDMPEGAudioDecoder::GetSampleCount() {
	return mSamplesDecoded;
}

uint32 VDMPEGAudioDecoder::GetFrameDataSize() {
	return mFrameDataSize;
}

void VDMPEGAudioDecoder::GetStreamInfo(VDMPEGAudioStreamInfo *pasi) {
	*pasi = mHeader;
}

const char *VDMPEGAudioDecoder::GetErrorString(int err) {
	switch(err) {
	case ERR_NONE:				return "no error";
	case ERR_EOF:				return "end of file";
	case ERR_READ:				return "read error";
	case ERR_MPEG25:			return "cannot decode MPEG-2.5 streams";
	case ERR_FREEFORM:			return "cannot decode free-form streams";
	case ERR_SYNC:				return "sync error";
	case ERR_INTERNAL:			return "internal error";
	case ERR_INCOMPLETEFRAME:	return "incomplete frame";
	case ERR_INVALIDDATA:		return "invalid or out-of-spec data encountered";
	default:					return "unknown error code";
	}
}

void VDMPEGAudioDecoder::Reset() {
	if (!mpPolyphaseFilter || mpPolyphaseFilter->ShouldRecreate()) {
		delete mpPolyphaseFilter;
		mpPolyphaseFilter = VDMPEGAudioPolyphaseFilter::Create();
	}

	mpPolyphaseFilter->Reset();

	mL3BufferPos = 0;
	mL3BufferLevel = 0;

	memset(mL3OverlapBuffer, 0, sizeof mL3OverlapBuffer);
}

void VDMPEGAudioDecoder::ReadHeader() {
	// syncword			12 bits		0000F0FF
	// id				1 bit		00000800
	// layer			2 bits		00000600
	// protection		1 bit		00000100
	// bitrate			4 bits		00F00000
	// sampling rate	2 bits		000C0000
	// padding			1 bit		00020000
	// private bit		1 bit		00010000
	// mode				2 bits		C0000000
	// mode extension	2 bits		30000000
	// copyright		1 bit		08000000
	// original/copy	1 bit		04000000
	// emphasis			2 bits		03000000

	union {
		char	buf[4];
		uint32	v;
	} hdr;
	int bytes = 0;

	for(;;) {
		while(bytes < 4) {
			int r = mpSource->read(hdr.buf+bytes, 4-bytes);
			if (r<0)
				throw (int)ERR_READ;
			else if (!r)
				throw (int)ERR_EOF;
			bytes += r;
		}

		if ((hdr.v & 0xe0ff) == 0xe0ff)
			break;

		hdr.buf[0] = hdr.buf[1];
		hdr.buf[1] = hdr.buf[2];
		hdr.buf[2] = hdr.buf[3];
		--bytes;
	}

	// determine frame size

	static const int sBitrateTable[2][3][16]={
		{
			{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 },	// MPEG-1 layer I
			{ 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 },	// MPEG-1 layer II
			{ 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 },	// MPEG-1 layer III
		},
		{
			{ 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 },	// MPEG-2 layer I
			{ 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 },	// MPEG-2 layer II
			{ 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 },	// MPEG-2 layer III
		}
	};

	static const int sFrequencyTable[2][4]={{44100,48000,32000,0}, {22050,24000,16000,0}};

	bool is_mpeg2	= (hdr.v & 0x800) == 0;
	bool is_mpeg25	= (hdr.v & 0x1000) == 0;
	int layer		= 4 - ((hdr.v>>9)&3);
	int bitrate_idx	= (hdr.v>>20)&15;
	int freq_idx	= (hdr.v>>18)&3;
	int padding		= (hdr.v>>17)&1;
	int bitrate		= sBitrateTable[is_mpeg2][layer-1][bitrate_idx];
	int freq		= sFrequencyTable[is_mpeg2][freq_idx];

	if (is_mpeg25)
		freq >>= 1;

	mHeader.fStereo			= ((hdr.v>>30)&3) != 3;
	mHeader.lBitrate		= bitrate;
	mHeader.lSamplingFreq	= freq;
	mHeader.nLayer			= layer;
	mHeader.nMPEGVer		= (hdr.v & 0x800) != 0 ? 1 : 2;

	if (!bitrate_idx)
		throw (int)ERR_FREEFORM;

	if (!freq || !bitrate)
		throw (int)ERR_INVALIDDATA;

	if (layer == 1)
		mFrameDataSize = 4*(12000*bitrate/freq + padding);
	else {
		if (is_mpeg2 && layer == 3)
			mFrameDataSize = (72000*bitrate/freq + padding);
		else
			mFrameDataSize = (144000*bitrate/freq + padding);
	}

	// take off header size
	mFrameDataSize -= 4;

	// read CRC
	if (!(hdr.v&0x100)) {
		char crc[2];

		int r = mpSource->read(crc, 2);
		if (r < 0)
			throw (int)ERR_READ;
		else if (r < 2)
			throw (int)ERR_EOF;

		mFrameDataSize -= 2;
	}

	mBitrateIndex		= bitrate_idx;
	mSamplingRateIndex	= freq_idx;
	mMode			= (hdr.v>>30)&3;
	mModeExtension	= (hdr.v>>28)&3;
}

void VDMPEGAudioDecoder::PrereadFrame() {
	VDASSERT(mFrameDataSize < sizeof mFrameBuffer);
	int r = mpSource->read(mFrameBuffer, mFrameDataSize);

	if (r < (int)mFrameDataSize) {
		if (r < 0)
			throw (int)ERR_READ;
		else
			throw (int)ERR_INCOMPLETEFRAME;
	}

	if (mHeader.nLayer == 3)
		PrereadLayerIII();
	else
		mL3BufferLevel = 0;
}

bool VDMPEGAudioDecoder::DecodeFrame() {
	mSamplesDecoded = 0;

	PrereadFrame();

	switch(mHeader.nLayer) {
	case 1:
		return DecodeLayerI();
	case 2:
		return DecodeLayerII();
	case 3:
		return DecodeLayerIII();
	}

	return false;
}

void VDMPEGAudioDecoder::ConcealFrame() {
	mpPolyphaseFilter->Reset();
	memset(mL3OverlapBuffer, 0, sizeof mL3OverlapBuffer);
}

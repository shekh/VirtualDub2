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

#ifndef f_VD2_PRISS_POLYPHASE_H
#define f_VD2_PRISS_POLYPHASE_H

#include <vd2/system/vdtypes.h>

class VDMPEGAudioPolyphaseFilter {
public:
	void *operator new(size_t);
	void operator delete(void *);

	static VDMPEGAudioPolyphaseFilter *Create();

	VDMPEGAudioPolyphaseFilter();

	virtual bool ShouldRecreate() = 0;
	void Reset();
	void Generate(const float left[32], const float right[32], sint16 *dst);

protected:
	enum OptMode {
		kOptModeFPU,
		kOptModeSSE
	};

	static OptMode GetOptMode();
	virtual void DCTInputButterflies(float x[32], const float in[32]);
	virtual void DCT4x8(float *);
	virtual void Matrix(const float *, bool stereo, int ch);
	virtual void SynthesizeMono(sint16 *dst);
	virtual void SynthesizeStereo(sint16 *dst);

	__declspec(align(16)) union {
		float	mono[32][16];
		float	stereo[32][2][16];
	} mWindow;

	unsigned	mWindowPos;

	float		mFilter[17][32];
};

class VDMPEGAudioPolyphaseFilterFPU : public VDMPEGAudioPolyphaseFilter {
protected:
	bool ShouldRecreate() { return GetOptMode() != kOptModeFPU; }
};

class VDMPEGAudioPolyphaseFilterSSE : public VDMPEGAudioPolyphaseFilter {
protected:
	bool ShouldRecreate() { return GetOptMode() != kOptModeSSE; }

	void DCTInputButterflies(float x[32], const float in[32]);
	void DCT4x8(float *x);
	void SynthesizeStereo(sint16 *dst);
};

#endif

//	VirtualDub - Video processing and capture application
//	Application helper library
//	Copyright (C) 1998-2006 Avery Lee
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

#ifndef f_VD2_VDLIB_FFT_H
#define f_VD2_VDLIB_FFT_H

#include <vd2/system/vdtypes.h>

void VDMakePermuteTable(uint32 *dst0, unsigned bits);
void VDCreateRaisedCosineWindow(float *dst, int n);
void VDPermuteRevBitsComplex(float *v, unsigned bits, const uint32 *permuteTable);
void VDComputeComplexFFT_DIT(float *p, unsigned bits);
void VDComputeRealFFT(float *p, unsigned bits);
void VDComputeComplexFFT_DIF(float *p, unsigned bits);
void VDComputeRealIFFT(float *p, unsigned bits);
void VDComputeComplexFFT_Reference(float *out, float *in, unsigned bits, double sign = -1);

class VDRealFFT {
public:
	VDRealFFT();
	VDRealFFT(unsigned bits);
	~VDRealFFT();

	void Init(unsigned bits);
	void Shutdown();

	void ComputeRealFFT(float *p);
	void ComputeRealIFFT(float *p);

protected:
	unsigned mBits;
	uint32 *mpPermuteTable;
	float *mpWeightTable;
};

class VDRollingRealFFT {
public:
	VDRollingRealFFT();
	VDRollingRealFFT(unsigned bits);
	~VDRollingRealFFT();

	void Init(unsigned bits);
	void Shutdown();

	void Clear();
	void Advance(uint32 samples);
	void CopyIn8U(const uint8 *src, uint32 count, ptrdiff_t stride);
	void CopyIn16S(const sint16 *src, uint32 count, ptrdiff_t stride);

	void Transform();

	float GetPower(int bin) const;

protected:
	uint32	mPoints;
	uint32	mBufferLevel;
	float *mpWindow;
	float *mpSampleBuffer;
	float *mpTempArea;
	VDRealFFT mFFT;
};

#endif

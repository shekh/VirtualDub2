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

#include "stdafx.h"
#include <vd2/system/math.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/error.h>
#include <vd2/VDLib/fft.h>

#include "fft_radix2.inl"
#include "fft_radix4.inl"
#ifdef _M_IX86
	#include "fft_sse.inl"
#endif

inline uint32 VDRevBits32(uint32 x) {
	uint32 y;

	y = (x>>16) + (x<<16);
	x = ((y & 0xff00ff00) >> 8) + ((y & 0x00ff00ff) << 8);
	y = ((x & 0xf0f0f0f0) >> 4) + ((x & 0x0f0f0f0f) << 4);
	x = ((y & 0xcccccccc) >> 2) + ((y & 0x33333333) << 2);
	y = ((x & 0xaaaaaaaa) >> 1) + ((x & 0x55555555) << 1);

	return y;
}

inline uint32 VDRevBits(uint32 v, unsigned bits) {
	return VDRevBits32(v) >> (32 - bits);
}

void VDMakePermuteTable(uint32 *dst0, unsigned bits) {
	unsigned N = 1<<bits;
	uint32 *dst = dst0;

	for(unsigned i=0; i<N; ++i) {
		unsigned j = VDRevBits(i, bits);

		if (i < j) {
			dst[0] = i;
			dst[1] = j;
			dst += 2;
		}
	}

	VDASSERT((uint32)(dst - dst0) == N - (1 << ((bits + 1) >> 1)));
}

void VDPermuteRevBitsComplex(float *v, unsigned bits, const uint32 *permuteTable) {
	unsigned N = 1<<bits;
	unsigned count = N - (1 << ((bits + 1) >> 1));

	for(unsigned i=0; i<count; i += 2) {
		const unsigned x = permuteTable[0];
		const unsigned y = permuteTable[1];
		permuteTable += 2;

		const unsigned i2 = x+x;
		const unsigned j2 = y+y;
		float tmp;

		tmp = v[i2  ]; v[i2  ] = v[j2  ]; v[j2  ] = tmp;
		tmp = v[i2+1]; v[i2+1] = v[j2+1]; v[j2+1] = tmp;
	}
}

void VDCreateRaisedCosineWindow(float *dst, int n) {
	const double twopi_over_n = nsVDMath::krTwoPi / n;
	const double scalefac = 0.5 / n;

	for(int i=0; i<n; ++i)
		dst[i] = (float)(scalefac * (1.0 - cos(twopi_over_n * (i+0.5))));
}

/// Decimation-in-time FFT. Permute before calling.
void VDComputeComplexFFT_DIT(float *p, unsigned bits) {
	if (bits >= 2) {
		unsigned k = 3;

		VDComputeComplexFFT_DIT_radix4_iter0(p, bits);

		for(; k<bits; k += 2)
			VDComputeComplexFFT_DIT_radix4(p, bits, k);
	}

	if (bits & 1)
		VDComputeComplexFFT_DIT_radix2(p, bits, bits);
}

void VDComputeComplexFFT_DIT_table(float *p, unsigned bits, const float *table) {
	if (bits >= 2) {
#ifdef _M_IX86
		if (SSE_enabled) {
			VDComputeComplexFFT_DIT_radix4_iter0_SSE(p, bits);
			for(unsigned k = 3; k<bits; k += 2)
			{
				if (k < bits-1)
					VDComputeComplexFFT_DIT_radix4_table2x_SSE(p, bits, k, table, 6 << (bits - k - 1));
				else
					VDComputeComplexFFT_DIT_radix4_table_SSE(p, bits, k, table, 6 << (bits - k - 1));
			}
		} else
#endif
		{
			VDComputeComplexFFT_DIT_radix4_iter0(p, bits);
			for(unsigned k = 3; k<bits; k += 2)
				VDComputeComplexFFT_DIT_radix4_table(p, bits, k, table, 6 << (bits - k - 1));
		}
	}

	if (bits & 1)
		VDComputeComplexFFT_DIT_radix2(p, bits, bits);
}

void VDComputeRealFFT(float *p, unsigned bits) {
	// transform as if it were complex
	VDComputeComplexFFT_DIT(p, bits-1);

	// do even-odd decomposition and last radix-2 step
	const unsigned N = 1 << bits;		// number of elements in FT
	float *p0 = p + 2;
	float *p1 = p + N - 2;
	float angstep = nsVDMath::kfPi / (float)(1 << bits) * 2.0f;
	float scrot = cosf(angstep);
	float ssrot = sinf(angstep);
	float sc = scrot;
	float ss = ssrot;

	for(unsigned i=4; i<N; i += 4) {
		float r0 = p0[0];
		float i0 = p0[1];
		float r1 = p1[0];
		float i1 = p1[1];

		float r2 = (r0 + r1) * 0.5f;
		float i2 = (i0 - i1) * 0.5f;
		float r3 = (i0 + i1) * 0.5f;
		float i3 = (r0 - r1) * 0.5f;

		float r4 = r3*sc - i3*ss;	// rotate by +ang (== rot by 180-ang)
		float i4 = r3*ss + i3*sc;

		p0[0] = r2 + r4;
		p0[1] = i2 - i4;
		p1[0] = r2 - r4;
		p1[1] = -i2 - i4;

		float sc2 = sc*scrot - ss*ssrot;
		ss = sc*ssrot + ss*scrot;
		sc = sc2;

		p0 += 2;
		p1 -= 2;
	}

	p[(N>>1)+1] = -p[(N>>1)+1];

	// butterfly on 0 and N/2
	float rmin = p[0];
	float rmax = p[1];

	p[0] = rmin + rmax;
	p[1] = rmin - rmax;
}

void VDComputeComplexFFT_DIF(float *p, unsigned bits) {
	if (bits & 1)
		VDComputeComplexFFT_DIF_radix2(p, bits, bits);

	if (bits >= 2) {
		for(int k=(bits&~1)-1; k>=3; k -= 2)
			VDComputeComplexFFT_DIF_radix4(p, bits, (unsigned)k);

		VDComputeComplexFFT_DIF_radix4_iter0(p, bits);
	}
}

void VDComputeRealIFFT(float *p, unsigned bits) {
	const unsigned N = 1 << bits;		// number of elements in FT
	p[(N>>1)+1] = -p[(N>>1)+1];

	// do even-odd decomposition and last radix-2 step
	float *p0 = p + 2;
	float *p1 = p + N - 2;
	float angstep = -nsVDMath::kfPi / (float)(1 << bits) * 2.0f;
	float scrot = cosf(angstep);
	float ssrot = sinf(angstep);
	float sc = scrot;
	float ss = ssrot;
	for(unsigned i=4; i<N; i += 4) {
		float r0 = p0[0];			// F(k)
		float i0 = p0[1];
		float r1 = p1[0];			// F(N/2-k)
		float i1 = p1[1];

		float r2 = (r0 + r1)*0.5f;	// Fe(k) = .5 * (F(k) + F(N/2-k)*)
		float i2 = (i0 - i1)*0.5f;
		float r4 = (r0 - r1)*0.5f;
		float i4 = (-i1 - i0)*0.5f;

		float r3 = r4*sc - i4*ss;	// Fo(k) = .5 * (F(k) - F(N/2-k)*)*rot
		float i3 = r4*ss + i4*sc;

		// Z(k) = Fe(k) + jFo(k)
		p0[0] = r2 + i3;
		p0[1] = r3 + i2;
		p1[0] = r2 - i3;
		p1[1] = r3 - i2;

		float sc2 = sc*scrot - ss*ssrot;
		ss = sc*ssrot + ss*scrot;
		sc = sc2;

		p0 += 2;
		p1 -= 2;
	}

	// butterfly on 0 and N/2
	float rmin = p[0];
	float rmax = p[1];

	p[0] = (rmin + rmax);
	p[1] = (rmin - rmax);

	// transform as if it were complex
	VDComputeComplexFFT_DIF(p, bits-1);
}

void VDComputeComplexFFT_Reference(float *out, float *in, unsigned bits, double sign) {
	unsigned N = 1 << bits;
	double angstep = sign*2.0*nsVDMath::kfPi / (double)N;

	for(unsigned i=0; i<N; ++i) {
		double ang = 0;
		double angstep2 = angstep * i; 
		double sumr = 0;
		double sumi = 0;

		for(unsigned j=0; j<N; ++j) {
			double c = cos(ang);
			double s = sin(ang);
			double r = in[j*2+0];
			double i = in[j*2+1];

			sumr += c*r - s*i;
			sumi += c*i + s*r;

			ang += angstep2;
		}

		out[0] = (float)sumr;
		out[1] = (float)sumi;
		out += 2;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
	void sumdiff(float& x, float& y) {
		float t = x-y;
		x = x+y;
		y = t;
	}

	void rotate(float& x, float& y) {
		float a = x;
		float b = y;

		x = b;
		y = -a;
	}

	void rotatei(float& x, float& y) {
		float a = x;
		float b = y;

		x = -b;
		y = a;
	}

	void cmult(float& x, float& y, float r, float i) {
		float a = x;
		float b = y;

		x = a*r - b*i;
		y = a*i + b*r;
	}
}

VDRealFFT::VDRealFFT() 
	: mpPermuteTable(NULL)
	, mpWeightTable(NULL)
{
}

VDRealFFT::VDRealFFT(unsigned bits)
	: mpPermuteTable(NULL)
	, mpWeightTable(NULL)
{
	Init(bits);
}

VDRealFFT::~VDRealFFT() {
	Shutdown();
}

void VDRealFFT::Init(unsigned bits) {
	Shutdown();

	mBits = bits;

	uint32 N = 1 << bits;
	uint32 N2 = N >> 1;
	uint32 N4 = N >> 2;

	mpPermuteTable = new uint32[N2];
	mpWeightTable = new float[N*4];

	VDMakePermuteTable(mpPermuteTable, bits - 1);

	float *p = mpWeightTable;

	// compute weight table for real-to-complex conversion (N/4 points, 1/N spacing)
	float step = 6.283185307179586476925286766559f / (1 << bits);
	for(unsigned i=1; i<N4; ++i) {
		*p++ = cosf(step * i);
		*p++ = sinf(step * i);
	}

	// compute weights for radix-2/radix-4 FFT (initial: N/2 points, 1/2N spacing){
	unsigned size = 1 << (bits - 1);
	step *= 2.0f;

	for(unsigned i=0; i<size; ++i) {
		*p++ = cosf(step * i);
		*p++ = sinf(step * i);
		*p++ = cosf(step * i * 2);
		*p++ = sinf(step * i * 2);
		*p++ = cosf(step * i * 3);
		*p++ = sinf(step * i * 3);
	}
}

void VDRealFFT::Shutdown() {
	if (mpPermuteTable) {
		delete[] mpPermuteTable;
		mpPermuteTable = NULL;
	}

	if (mpWeightTable) {
		delete[] mpWeightTable;
		mpWeightTable = NULL;
	}
}

void VDRealFFT::ComputeRealFFT(float *p) {
	const unsigned N = 1 << mBits;		// number of elements in FT
	const float *w = mpWeightTable;

	// permute
	VDPermuteRevBitsComplex(p, mBits-1, mpPermuteTable);

	// transform as if it were complex
	VDComputeComplexFFT_DIT_table(p, mBits-1, w + (N>>1) - 2);

	// do even-odd decomposition and last radix-2 step
	float *p0 = p + 2;
	float *p1 = p + N - 2;

	for(unsigned i=4; i<N; i += 4) {
		float r0 = p0[0];
		float i0 = p0[1];
		float r1 = p1[0];
		float i1 = p1[1];

		float r2 = (r0 + r1) * 0.5f;
		float i2 = (i0 - i1) * 0.5f;
		float r3 = (i0 + i1) * 0.5f;
		float i3 = (r0 - r1) * 0.5f;

		float sc = *w++;
		float ss = *w++;
		float r4 = r3*sc - i3*ss;	// rotate by +ang (== rot by 180-ang)
		float i4 = r3*ss + i3*sc;

		p0[0] = r2 + r4;
		p0[1] = i2 - i4;
		p1[0] = r2 - r4;
		p1[1] = -i2 - i4;

		p0 += 2;
		p1 -= 2;
	}

	p[(N>>1)+1] = -p[(N>>1)+1];

	// butterfly on 0 and N/2
	float rmin = p[0];
	float rmax = p[1];

	p[0] = rmin + rmax;
	p[1] = rmin - rmax;
}

void VDRealFFT::ComputeRealIFFT(float *p) {
	const unsigned N = 1 << mBits;		// number of elements in FT
	const float *w = mpWeightTable;
	p[(N>>1)+1] = -p[(N>>1)+1];

	// do even-odd decomposition and last radix-2 step
	float *p0 = p + 2;
	float *p1 = p + N - 2;
	for(unsigned i=4; i<N; i += 4) {
		float r0 = p0[0];			// F(k)
		float i0 = p0[1];
		float r1 = p1[0];			// F(N/2-k)
		float i1 = p1[1];

		float r2 = (r0 + r1)*0.5f;	// Fe(k) = .5 * (F(k) + F(N/2-k)*)
		float i2 = (i0 - i1)*0.5f;
		float r4 = (r0 - r1)*0.5f;
		float i4 = (-i1 - i0)*0.5f;

		float sc = *w++;
		float ss = -*w++;
		float r3 = r4*sc - i4*ss;	// Fo(k) = .5 * (F(k) - F(N/2-k)*)*rot
		float i3 = r4*ss + i4*sc;

		// Z(k) = Fe(k) + jFo(k)
		p0[0] = r2 + i3;
		p0[1] = r3 + i2;
		p1[0] = r2 - i3;
		p1[1] = r3 - i2;

		p0 += 2;
		p1 -= 2;
	}

	// butterfly on 0 and N/2
	float rmin = p[0];
	float rmax = p[1];

	p[0] = (rmin + rmax)*0.5f;
	p[1] = (rmin - rmax)*0.5f;

	// transform as if it were complex
	VDComputeComplexFFT_DIF(p, mBits-1);

	// permute
	VDPermuteRevBitsComplex(p, mBits-1, mpPermuteTable);
}

///////////////////////////////////////////////////////////////////////////

VDRollingRealFFT::VDRollingRealFFT()
	: mPoints(0)
	, mBufferLevel(0)
	, mpWindow(NULL)
	, mpSampleBuffer(NULL)
	, mpTempArea(NULL)
{
}

VDRollingRealFFT::VDRollingRealFFT(unsigned bits)
	: mPoints(0)
	, mBufferLevel(0)
	, mpWindow(NULL)
	, mpSampleBuffer(NULL)
	, mpTempArea(NULL)
{
	Init(bits);
}

VDRollingRealFFT::~VDRollingRealFFT() {
	Shutdown();
}

void VDRollingRealFFT::Init(unsigned bits) {
	mPoints = 1 << bits;

	mpWindow = (float *)VDAlignedMalloc(mPoints*sizeof(float), 16);
	if (!mpWindow)
		throw MyMemoryError();

	const float step = 2*nsVDMath::kfPi / (sint32)mPoints;
	const float scalefac = 0.5f / mPoints;

	for(uint32 i=0; i<mPoints; ++i)
		mpWindow[i] = scalefac * (1.0f - cosf(step * ((sint32)i+0.5f)));

	mpSampleBuffer = (float *)VDAlignedMalloc(mPoints*sizeof(float), 16);
	if (!mpSampleBuffer)
		throw MyMemoryError();

	mpTempArea = (float *)VDAlignedMalloc(mPoints*sizeof(float), 16);
	if (!mpTempArea)
		throw MyMemoryError();

	mFFT.Init(bits);

	Clear();
}

void VDRollingRealFFT::Shutdown() {
	if (mpWindow) {
		VDAlignedFree(mpWindow);
		mpWindow = NULL;
	}

	if (mpTempArea) {
		VDAlignedFree(mpTempArea);
		mpTempArea = NULL;
	}

	if (mpSampleBuffer) {
		VDAlignedFree(mpSampleBuffer);
		mpSampleBuffer = NULL;
	}

	mFFT.Shutdown();
}

void VDRollingRealFFT::Clear() {
	memset(mpSampleBuffer, 0, sizeof(*mpSampleBuffer) * mPoints);
	mBufferLevel = mPoints;
}

void VDRollingRealFFT::Advance(uint32 samples) {
	if (samples >= mBufferLevel)
		mBufferLevel = 0;
	else {
		mBufferLevel -= samples;
		memmove(mpSampleBuffer, mpSampleBuffer + samples, mBufferLevel * sizeof(*mpSampleBuffer));
	}
}

void VDRollingRealFFT::CopyIn8U(const uint8 *src, uint32 count, ptrdiff_t stride) {
	if (count <= 0)
		return;

	if (count > mPoints) {
		src += (count-mPoints)*stride;
		count = mPoints;
	}

	if (count > mPoints - mBufferLevel)
		Advance(count - (mPoints - mBufferLevel));

	float *dst = mpSampleBuffer + mBufferLevel;
	mBufferLevel += count;
	do {
		*dst++ = ((sint32)*src - 0x80) * (1.0f / 128.0f);
		src += stride;
	} while(--count);
}

void VDRollingRealFFT::CopyIn16S(const sint16 *src, uint32 count, ptrdiff_t stride) {
	if (count <= 0)
		return;

	if (count > mPoints) {
		src = (const sint16 *)((const char *)src + (count-mPoints)*stride);
		count = mPoints;
	}

	if (count > mPoints - mBufferLevel)
		Advance(count - (mPoints - mBufferLevel));

	float *dst = mpSampleBuffer + mBufferLevel;
	mBufferLevel += count;
	do {
		*dst++ = *src * (1.0f / 32768.0f);
		src = (const signed short *)((const char *)src + stride);
	} while(--count);
}

void VDRollingRealFFT::CopyInF(const float *src, uint32 count, ptrdiff_t stride) {
	if (count <= 0)
		return;

	if (count > mPoints) {
		src = (const float *)((const char *)src + (count-mPoints)*stride);
		count = mPoints;
	}

	if (count > mPoints - mBufferLevel)
		Advance(count - (mPoints - mBufferLevel));

	float *dst = mpSampleBuffer + mBufferLevel;
	mBufferLevel += count;
	do {
		*dst++ = *src;
		src = (const float *)((const char *)src + stride);
	} while(--count);
}

void VDRollingRealFFT::CopyInZ(uint32 count) {
	if (count <= 0)
		return;

	if (count > mPoints) {
		count = mPoints;
	}

	if (count > mPoints - mBufferLevel)
		Advance(count - (mPoints - mBufferLevel));

	float *dst = mpSampleBuffer + mBufferLevel;
	mBufferLevel += count;
	do {
		*dst++ = 0;
	} while(--count);
}

void VDRollingRealFFT::Transform() {
	for(uint32 i=0; i<mPoints; ++i)
		mpTempArea[i] = mpSampleBuffer[i] * mpWindow[i];

	mFFT.ComputeRealFFT(mpTempArea);
}

float VDRollingRealFFT::GetPower(int bin) const {
	VDASSERT((unsigned)bin < mPoints);
	bin += bin;

	float re = mpTempArea[bin];
	float im = mpTempArea[bin+1];
	return re*re + im*im;
}


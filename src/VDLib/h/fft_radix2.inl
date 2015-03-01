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

void VDComputeComplexFFT_DIT_radix2(float *p, unsigned bits, unsigned subbits) {
	const unsigned N = 1 << bits;		// size of whole FT
	const unsigned Np = N+N;			// number of elements in FT
	const unsigned M = 1 << subbits;	// size of sub-FT, or half the number of elements
	const unsigned Mp = M+M;			// number of elements in sub-FT

	float angstep = nsVDMath::kfPi / (float)(1 << (subbits-1));
	float scrot	= cosf(angstep);	// shift rotation cosine
	float ssrot	= -sinf(angstep);	// shift rotation sine
	float sc	= 1.f;				// shift cosine
	float ss	= 0.f;				// shift sine

	for(unsigned j=0; j<M; j+=2) {
		float *p0 = p+j;			// 'even'
		float *p1 = p0 + M;			// 'odd'

		// perform one step across all sub-FTs
		for(unsigned i=0; i<Np; i+=Mp) {
			float r0 = p0[0];
			float i0 = p0[1];
			float r1 = p1[0];
			float i1 = p1[1];

			// shift odd elements
			float r1s = r1*sc - i1*ss;
			float i1s = r1*ss + i1*sc;

			p0[0] = r0+r1s;
			p0[1] = i0+i1s;
			p1[0] = r0-r1s;
			p1[1] = i0-i1s;

			p0 += Mp;
			p1 += Mp;
		}

		// rotate shift factor
		float stmp = sc*scrot - ss*ssrot;
		ss = sc*ssrot + ss*scrot;
		sc = stmp;
	}
}

void VDComputeComplexFFT_DIF_radix2(float *p, unsigned bits, unsigned subbits) {
	const unsigned N = 1 << bits;		// size of whole FT
	const unsigned Np = N+N;			// number of elements in FT
	const unsigned M = 1 << subbits;	// size of sub-FT, or half the number of elements
	const unsigned Mp = M+M;			// number of elements in sub-FT

	float angstep = nsVDMath::kfPi / (float)(1 << (subbits-1));
	float scrot	= cosf(angstep);	// shift rotation cosine
	float ssrot	= sinf(angstep);	// shift rotation sine
	float sc	= 1.f;				// shift cosine
	float ss	= 0.f;				// shift sine

	for(unsigned j=0; j<M; j+=2) {
		float *p0 = p+j;			// 'even'
		float *p1 = p0 + M;			// 'odd'

		// perform one step across all sub-FTs
		for(unsigned i=0; i<Np; i+=Mp) {
			float r0 = p0[0];
			float i0 = p0[1];
			float r1 = p1[0];
			float i1 = p1[1];

			// shift odd elements
			p0[0] = r0+r1;
			p0[1] = i0+i1;

			float r2 = r0-r1;
			float i2 = i0-i1;
			p1[0] = r2*sc - i2*ss;
			p1[1] = r2*ss + i2*sc;

			p0 += Mp;
			p1 += Mp;
		}

		// rotate shift factor
		float stmp = sc*scrot - ss*ssrot;
		ss = sc*ssrot + ss*scrot;
		sc = stmp;
	}
}

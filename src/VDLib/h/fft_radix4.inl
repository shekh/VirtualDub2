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

void VDComputeComplexFFT_DIT_radix4_iter0(float *p, unsigned bits) {
	const unsigned N = 1 << bits;		// size of whole FT
	const unsigned Np = N+N;			// number of elements in FT

	// perform one step across all sub-FTs
	for(unsigned i=0; i<Np; i+=8) {
		float r0 = p[0];
		float i0 = p[1];
		float r1 = p[4];	// swap middle two to undo intervening bitrev layer swap
		float i1 = p[5];
		float r2 = p[2];
		float i2 = p[3];
		float r3 = p[6];
		float i3 = p[7];

		// first layer of butterflies
		float r0b = r0 + r2;
		float i0b = i0 + i2;
		float r2b = r0 - r2;
		float i2b = i0 - i2;
		float r1b = r1 + r3;
		float i1b = i1 + i3;
		float r3b = r1 - r3;
		float i3b = i1 - i3;

		// second layer of butterflies, imaginary rotation
		// (r,i) rot -j = (i,-r)
		p[0] = r0b + r1b;
		p[1] = i0b + i1b;
		p[2] = r2b + i3b;
		p[3] = i2b - r3b;
		p[4] = r0b - r1b;
		p[5] = i0b - i1b;
		p[6] = r2b - i3b;
		p[7] = i2b + r3b;

		p += 8;
	}
}

void VDComputeComplexFFT_DIT_radix4(float *p, unsigned bits, unsigned subbits) {
	const unsigned N = 1 << bits;		// size of whole FT
	const unsigned Np = N+N;			// number of elements in FT
	const unsigned Mh = 1 << subbits;	// half-size of sub-FT, or quarter the number of elements
	const unsigned Mp = Mh<<2;			// number of elements in sub-FT

	float angstep = nsVDMath::kfPi / (float)(1 << subbits);
	float scrot = sinf(angstep*0.5f); scrot = scrot*scrot*2.0f;
	float ssrot = sinf(-angstep);
	float sc1	= 1.f;				// shift cosine
	float ss1	= 0.f;				// shift sine

	for(unsigned j=0; j<Mh; j+=2) {
		float *p0 = p+j;			// phase 0/4
		float *p1 = p0 + Mh;		// phase 1/4
		float *p2 = p1 + Mh;		// phase 2/4
		float *p3 = p2 + Mh;		// phase 3/4

		// compute double and triple shifts
		float sc2 = sc1*sc1 - ss1*ss1;
		float ss2 = sc1*ss1*2;
		float sc3 = sc2*sc1 - ss2*ss1;
		float ss3 = sc2*ss1 + ss2*sc1;

		// perform one step across all sub-FTs
		for(unsigned i=0; i<Np; i+=Mp) {
			float r0 = p0[0];
			float i0 = p0[1];
			float r1 = p2[0];	// swap middle two to undo intervening bitrev layer swap
			float i1 = p2[1];
			float r2 = p1[0];
			float i2 = p1[1];
			float r3 = p3[0];
			float i3 = p3[1];

			// do shifts
			float r1s = r1*sc1 - i1*ss1;
			float i1s = r1*ss1 + i1*sc1;
			float r2s = r2*sc2 - i2*ss2;
			float i2s = r2*ss2 + i2*sc2;
			float r3s = r3*sc3 - i3*ss3;
			float i3s = r3*ss3 + i3*sc3;

			// first layer of butterflies
			float r0b = r0 +r2s;
			float i0b = i0 +i2s;
			float r2b = r0 -r2s;
			float i2b = i0 -i2s;
			float r1b = r1s+r3s;
			float i1b = i1s+i3s;
			float r3b = r1s-r3s;
			float i3b = i1s-i3s;

			// second layer of butterflies, imaginary rotation
			// (r,i) rot -j = (i,-r)
			p0[0] = r0b + r1b;
			p0[1] = i0b + i1b;
			p1[0] = r2b + i3b;
			p1[1] = i2b - r3b;
			p2[0] = r0b - r1b;
			p2[1] = i0b - i1b;
			p3[0] = r2b - i3b;
			p3[1] = i2b + r3b;

			p0 += Mp;
			p1 += Mp;
			p2 += Mp;
			p3 += Mp;
		}

		// rotate shift factor
		float stmp = sc1 - (scrot*sc1 + ssrot*ss1);
		ss1 -= (scrot*ss1 - ssrot*sc1);
		sc1 = stmp;
	}
}

void VDComputeComplexFFT_DIT_radix4_table(float *p, unsigned bits, unsigned subbits, const float *table, int table_step) {
	const unsigned N = 1 << bits;		// size of whole FT
	const unsigned Np = N+N;			// number of elements in FT
	const unsigned Mh = 1 << subbits;	// half-size of sub-FT, or quarter the number of elements
	const unsigned Mp = Mh<<2;			// number of elements in sub-FT

	for(unsigned j=0; j<Mh; j+=2) {
		float *p0 = p+j;			// phase 0/4
		float *p1 = p0 + Mh;		// phase 1/4
		float *p2 = p1 + Mh;		// phase 2/4
		float *p3 = p2 + Mh;		// phase 3/4

		// compute double and triple shifts
		float sc1 = table[0];
		float ss1 = -table[1];
		float sc2 = table[2];
		float ss2 = -table[3];
		float sc3 = table[4];
		float ss3 = -table[5];
		table += table_step;

		// perform one step across all sub-FTs
		for(unsigned i=0; i<Np; i+=Mp) {
			float r0 = p0[0];
			float i0 = p0[1];
			float r1 = p2[0];	// swap middle two to undo intervening bitrev layer swap
			float i1 = p2[1];
			float r2 = p1[0];
			float i2 = p1[1];
			float r3 = p3[0];
			float i3 = p3[1];

			// do shifts
			float r1s = r1*sc1 - i1*ss1;
			float i1s = r1*ss1 + i1*sc1;
			float r2s = r2*sc2 - i2*ss2;
			float i2s = r2*ss2 + i2*sc2;
			float r3s = r3*sc3 - i3*ss3;
			float i3s = r3*ss3 + i3*sc3;

			// first layer of butterflies
			float r0b = r0 +r2s;
			float i0b = i0 +i2s;
			float r2b = r0 -r2s;
			float i2b = i0 -i2s;
			float r1b = r1s+r3s;
			float i1b = i1s+i3s;
			float r3b = r1s-r3s;
			float i3b = i1s-i3s;

			// second layer of butterflies, imaginary rotation
			// (r,i) rot -j = (i,-r)
			p0[0] = r0b + r1b;
			p0[1] = i0b + i1b;
			p1[0] = r2b + i3b;
			p1[1] = i2b - r3b;
			p2[0] = r0b - r1b;
			p2[1] = i0b - i1b;
			p3[0] = r2b - i3b;
			p3[1] = i2b + r3b;

			p0 += Mp;
			p1 += Mp;
			p2 += Mp;
			p3 += Mp;
		}
	}
}

void VDComputeComplexFFT_DIF_radix4_iter0(float *p, unsigned bits) {
	const unsigned N = 1 << bits;		// size of whole FT
	const unsigned Np = N+N;			// number of elements in FT

	// perform one step across all sub-FTs
	for(unsigned i=0; i<Np; i+=8) {
		float r0 = p[0];
		float i0 = p[1];
		float r1 = p[2];
		float i1 = p[3];
		float r2 = p[4];
		float i2 = p[5];
		float r3 = p[6];
		float i3 = p[7];

		// do first layer of butterflies
		float r0a = r0 + r2;
		float i0a = i0 + i2;
		float r1a = r1 + r3;
		float i1a = i1 + i3;
		float r2a = r0 - r2;
		float i2a = i0 - i2;
		float r3a = i3 - i1;	// rotate by i*sign
		float i3a = r1 - r3;

		// do second layer of butterflies
		float r0b = r0a + r1a;
		float i0b = i0a + i1a;
		float r2b = r0a - r1a;
		float i2b = i0a - i1a;
		float r1b = r2a + r3a;
		float i1b = i2a + i3a;
		float r3b = r2a - r3a;
		float i3b = i2a - i3a;

		// do shifts
		p[0] = r0b;
		p[1] = i0b;
		p[4] = r1b;	// swap middle two to undo intervening bitrev swap
		p[5] = i1b;
		p[2] = r2b;
		p[3] = i2b;
		p[6] = r3b;
		p[7] = i3b;

		p += 8;
	}
}

void VDComputeComplexFFT_DIF_radix4(float *p, unsigned bits, unsigned subbits) {
	const unsigned N = 1 << bits;		// size of whole FT
	const unsigned Np = N+N;			// number of elements in FT
	const unsigned Mh = 1 << subbits;	// half-size of sub-FT, or quarter the number of elements
	const unsigned Mp = Mh<<2;			// number of elements in sub-FT

	float angstep = nsVDMath::kfPi / (float)(2 << (subbits-1));
	float scrot = sinf(angstep*0.5f); scrot = scrot*scrot*2.0f;
	float ssrot = sinf(angstep);
	float sc1	= 1.f;				// shift cosine
	float ss1	= 0.f;				// shift sine

	for(unsigned j=0; j<Mh; j+=2) {
		float *p0 = p+j;			// phase 0/4
		float *p1 = p0 + Mh;		// phase 1/4
		float *p2 = p1 + Mh;		// phase 2/4
		float *p3 = p2 + Mh;		// phase 3/4

		// compute double and triple shifts
		float sc2 = sc1*sc1 - ss1*ss1;
		float ss2 = sc1*ss1*2;
		float sc3 = sc2*sc1 - ss2*ss1;
		float ss3 = sc2*ss1 + ss2*sc1;

		// perform one step across all sub-FTs
		for(unsigned i=0; i<Np; i+=Mp) {
			float r0 = p0[0];
			float i0 = p0[1];
			float r1 = p1[0];
			float i1 = p1[1];
			float r2 = p2[0];
			float i2 = p2[1];
			float r3 = p3[0];
			float i3 = p3[1];

			// do first layer of butterflies
			float r0a = r0 + r2;
			float i0a = i0 + i2;
			float r1a = r1 + r3;
			float i1a = i1 + i3;
			float r2a = r0 - r2;
			float i2a = i0 - i2;
			float r3a = i3 - i1;	// rotate by i*sign
			float i3a = r1 - r3;

			// do second layer of butterflies
			float r0b = r0a + r1a;
			float i0b = i0a + i1a;
			float r2b = r0a - r1a;
			float i2b = i0a - i1a;
			float r1b = r2a + r3a;
			float i1b = i2a + i3a;
			float r3b = r2a - r3a;
			float i3b = i2a - i3a;

			// do shifts
			p0[0] = r0b;
			p0[1] = i0b;
			p2[0] = r1b*sc1 - i1b*ss1;	// swap middle two to undo intervening bitrev swap
			p2[1] = r1b*ss1 + i1b*sc1;
			p1[0] = r2b*sc2 - i2b*ss2;
			p1[1] = r2b*ss2 + i2b*sc2;
			p3[0] = r3b*sc3 - i3b*ss3;
			p3[1] = r3b*ss3 + i3b*sc3;

			p0 += Mp;
			p1 += Mp;
			p2 += Mp;
			p3 += Mp;
		}

		// rotate shift factor
		float stmp = sc1 - (scrot*sc1 + ssrot*ss1);
		ss1 -= (scrot*ss1 - ssrot*sc1);
		sc1 = stmp;
	}
}

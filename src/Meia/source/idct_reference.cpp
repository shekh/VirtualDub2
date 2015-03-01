#include <string.h>

#include <vd2/Meia/MPEGIDCT.h>
#include "tables.h"

using namespace nsVDMPEGTables;

template<int stride, bool round, class DT, class ST>
static inline void reference_idct_1D(DT *dst, const ST *src) {
	static const double cosvals[32]={
		0.5*1,
		0.5*0.98078528040323043,
		0.5*0.92387953251128674,
		0.5*0.83146961230254524,
		0.5*0.70710678118654757,
		0.5*0.55557023301960229,
		0.5*0.38268343236508984,
		0.5*0.19509032201612833,
		0.5*6.1230317691118863e-017,
		0.5*-0.19509032201612819,
		0.5*-0.38268343236508973,
		0.5*-0.55557023301960196,
		0.5*-0.70710678118654746,
		0.5*-0.83146961230254535,
		0.5*-0.92387953251128674,
		0.5*-0.98078528040323043,
		0.5*-1,
		0.5*-0.98078528040323043,
		0.5*-0.92387953251128685,
		0.5*-0.83146961230254546,
		0.5*-0.70710678118654768,
		0.5*-0.55557023301960218,
		0.5*-0.38268343236509034,
		0.5*-0.19509032201612866,
		0.5*-1.8369095307335659e-016,
		0.5*0.1950903220161283,
		0.5*0.38268343236509,
		0.5*0.55557023301960184,
		0.5*0.70710678118654735,
		0.5*0.83146961230254524,
		0.5*0.92387953251128652,
	};

	static const double coeffs4[4]={
		4.0 * cosvals[1],
		4.0 * cosvals[3],
		4.0 * cosvals[5],
		4.0 * cosvals[7],
	};

	static const double coeffs2[2]={
		4.0 * cosvals[2],
		4.0 * cosvals[6],
	};

	double t[8];

	for(int i=0; i<8; ++i)
		t[i] = src[i*stride];

	double s[8], u[8];

	s[0] = t[0];
	s[1] = t[4];
	s[2] = t[2];
	s[3] = t[6];
	s[4] = t[1];
	s[5] = t[5];
	s[6] = t[3];
	s[7] = t[7];

	s[5] -= s[7];
	s[6] -= s[5];
	s[4] -= s[6];
	s[4] *= 0.70710678118654752440084436210485;
	s[2] -= s[3];
	s[2] *= 0.70710678118654752440084436210485;
	s[6] -= s[7];
	s[6] *= 0.70710678118654752440084436210485;

	// begin 2x2's

	u[0] = (s[0]+s[1]) * (0.70710678118654752440084436210485 * 0.5);
	u[1] = (s[0]-s[1]) * (0.70710678118654752440084436210485 * 0.5);
	u[2] = (s[2]+s[3]) * (0.70710678118654752440084436210485 * 0.5);
	u[3] = (s[2]-s[3]) * (0.70710678118654752440084436210485 * 0.5);
	u[4] = (s[4]+s[5]) * (0.70710678118654752440084436210485 * 0.5);
	u[5] = (s[4]-s[5]) * (0.70710678118654752440084436210485 * 0.5);
	u[6] = (s[6]+s[7]) * (0.70710678118654752440084436210485 * 0.5);
	u[7] = (s[6]-s[7]) * (0.70710678118654752440084436210485 * 0.5);

	// end 2x2's

	u[2] *= coeffs2[0];
	u[3] *= coeffs2[1];
	u[6] *= coeffs2[0];
	u[7] *= coeffs2[1];

	t[0] = (u[0]+u[2]);
	t[1] = (u[1]+u[3]);
	t[3] = (u[0]-u[2]);
	t[2] = (u[1]-u[3]);
	t[4] = (u[4]+u[6]);
	t[5] = (u[5]+u[7]);
	t[7] = (u[4]-u[6]);
	t[6] = (u[5]-u[7]);

	// end 4x4's

	t[4] *= coeffs4[0];
	t[5] *= coeffs4[1];
	t[6] *= coeffs4[2];
	t[7] *= coeffs4[3];

	s[0] = (t[0]+t[4]);
	s[1] = (t[1]+t[5]);
	s[2] = (t[2]+t[6]);
	s[3] = (t[3]+t[7]);
	s[4] = (t[3]-t[7]);
	s[5] = (t[2]-t[6]);
	s[6] = (t[1]-t[5]);
	s[7] = (t[0]-t[4]);

	static const float magic_value = (float)((1<<23) + (1<<22));

	if (round) {
		for(int i=0; i<8; ++i) {
			union {
				float f;
				int i;
			} converter;

			converter.f = (float)(s[i] + magic_value);

//			dst[i*stride] = (int)floor(0.5 + s[i]);
			dst[i*stride] = (signed short)converter.i;
		}
	} else {
		for(int i=0; i<8; ++i)
			dst[i*stride] = (DT)s[i];
	}
}

template<int stride, bool round, class DT, class ST>
static inline void reference_idct_1D_4x2(DT *dst, const ST *src) {
	static const double coeffs2[2]={
		4.0 * 0.5*0.92387953251128674,
		4.0 * 0.5*0.38268343236508984,
	};

	double t[8];

	for(int i=0; i<8; ++i)
		t[i] = src[i*stride];

	double s[8], u[8];

	s[0] = t[0];
	s[1] = t[4];
	s[2] = t[2];
	s[3] = t[6];
	s[4] = t[1];
	s[5] = t[5];
	s[6] = t[3];
	s[7] = t[7];

	s[2] -= s[3];
	s[2] *= 0.70710678118654752440084436210485;
	s[6] -= s[7];
	s[6] *= 0.70710678118654752440084436210485;

	// begin 2x2's

	u[0] = (s[0]+s[1]) * (0.70710678118654752440084436210485 * 0.5);
	u[1] = (s[0]-s[1]) * (0.70710678118654752440084436210485 * 0.5);
	u[2] = (s[2]+s[3]) * (0.70710678118654752440084436210485 * 0.5);
	u[3] = (s[2]-s[3]) * (0.70710678118654752440084436210485 * 0.5);
	u[4] = (s[4]+s[5]) * (0.70710678118654752440084436210485 * 0.5);
	u[5] = (s[4]-s[5]) * (0.70710678118654752440084436210485 * 0.5);
	u[6] = (s[6]+s[7]) * (0.70710678118654752440084436210485 * 0.5);
	u[7] = (s[6]-s[7]) * (0.70710678118654752440084436210485 * 0.5);

	// end 2x2's

	u[2] *= coeffs2[0];
	u[3] *= coeffs2[1];
	u[6] *= coeffs2[0];
	u[7] *= coeffs2[1];

	t[0] = (u[0]+u[2]);
	t[1] = (u[1]+u[3]);
	t[3] = (u[0]-u[2]);
	t[2] = (u[1]-u[3]);
	t[4] = (u[4]+u[6]);
	t[5] = (u[5]+u[7]);
	t[7] = (u[4]-u[6]);
	t[6] = (u[5]-u[7]);

	// end 4x4's

	s[0] = (t[0]+t[4]);
	s[1] = (t[0]-t[4]);
	s[2] = (t[1]+t[5]);
	s[3] = (t[1]-t[5]);
	s[4] = (t[2]+t[6]);
	s[5] = (t[2]-t[6]);
	s[6] = (t[3]+t[7]);
	s[7] = (t[3]-t[7]);

	static const float magic_value = (float)((1<<23) + (1<<22));

	if (round) {
		for(int i=0; i<8; ++i) {
			union {
				float f;
				int i;
			} converter;

			converter.f = (float)(s[i] + magic_value);

//			dst[i*stride] = (int)floor(0.5 + s[i]);
			dst[i*stride] = (signed short)converter.i;
		}
	} else {
		for(int i=0; i<8; ++i)
			dst[i*stride] = (DT)s[i];
	}
}

static void reference_idct_2D(short *dct_coeff) {
	double tmp[64];
	int i;

#ifndef _M_AMD64
	__asm emms
#endif

	for(i=0; i<8; ++i)
		reference_idct_1D<1, false, double, short>(tmp + 8*i, dct_coeff + 8*i);

	for(i=0; i<8; ++i)
		reference_idct_1D<8, true, short, double>(dct_coeff + i, tmp + i);
}

static void reference_idct_2D_4x2(short *dct_coeff) {
	double tmp[64];
	int i;

#ifndef _M_AMD64
	__asm emms
#endif

	for(i=0; i<8; ++i)
		reference_idct_1D<1, false, double, short>(tmp + 8*i, dct_coeff + 8*i);

	for(i=0; i<8; ++i)
		reference_idct_1D_4x2<8, true, short, double>(dct_coeff + i, tmp + i);
}

static void reference_idct_intra(unsigned char *dst, int pitch, const short *src, int last_pos) {
	short tmp[64];

	memcpy(tmp, src, sizeof tmp);

	reference_idct_2D(tmp);

	for(int j=0; j<8; ++j) {
		for(int i=0; i<8; ++i) {
			int v = tmp[j*8+i];

			dst[i] = clip_table[v + 288];
		}
		dst += pitch;
	}
}

static void reference_idct_nonintra(unsigned char *dst, int pitch, const short *src, int last_pos) {
	short tmp[64];

	memcpy(tmp, src, sizeof tmp);

	reference_idct_2D(tmp);

	for(int j=0; j<8; ++j) {
		for(int i=0; i<8; ++i) {
			int v = tmp[j*8+i] + dst[i];

			dst[i] = clip_table[v+288];
		}
		dst += pitch;
	}
}

static void reference_idct_intra4x2(unsigned char *dst, int pitch, const short *src, int last_pos) {
	short tmp[64];

	memcpy(tmp, src, sizeof tmp);

	reference_idct_2D_4x2(tmp);

	for(int j=0; j<8; ++j) {
		for(int i=0; i<8; ++i) {
			int v = tmp[j*8+i];

			dst[i] = clip_table[v+288];
		}
		dst += pitch;
	}
}

static void reference_idct_test(short *src, int last_pos) {
	reference_idct_2D(src);
}

const struct VDMPEGIDCTSet g_VDMPEGIDCT_reference = {
	(tVDMPEGIDCT)reference_idct_intra,
	(tVDMPEGIDCT)reference_idct_nonintra,
	(tVDMPEGIDCTTest)reference_idct_test,
	NULL,
	NULL,
	(tVDMPEGIDCT)reference_idct_intra4x2,
};

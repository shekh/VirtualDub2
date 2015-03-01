#include <vd2/Meia/MPEGPredict.h>

///////////////////////////////////////////////////////////////////////////

namespace nsVDMPEGPredictReference {
	static void predict_Y_normal(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<16; ++j)
			for(int i=0; i<16; ++i)
				dst[j*pitch+i] = src[j*pitch+i];
	}

	static void predict_Y_halfpelX(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<16; ++j)
			for(int i=0; i<16; ++i)
				dst[j*pitch+i] = ((unsigned)src[j*pitch+i] + (unsigned)src[j*pitch+i+1] + 1)>>1;
	}

	static void predict_Y_halfpelY(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<16; ++j)
			for(int i=0; i<16; ++i)
				dst[j*pitch+i] = ((unsigned)src[j*pitch+i] + (unsigned)src[(j+1)*pitch+i] + 1)>>1;
	}

	static void predict_Y_quadpel(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<16; ++j)
			for(int i=0; i<16; ++i)
				dst[j*pitch+i] = ((unsigned)src[j*pitch+i] + (unsigned)src[j*pitch+i+1] +
							(unsigned)src[(j+1)*pitch+i] + (unsigned)src[(j+1)*pitch+i+1] + 2)>>2;
	}

	static void predict_C_normal(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<8; ++j)
			for(int i=0; i<8; ++i)
				dst[j*pitch+i] = src[j*pitch+i];
	}

	static void predict_C_halfpelX(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<8; ++j)
			for(int i=0; i<8; ++i)
				dst[j*pitch+i] = ((unsigned)src[j*pitch+i] + (unsigned)src[j*pitch+i+1] + 1)>>1;
	}

	static void predict_C_halfpelY(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<8; ++j)
			for(int i=0; i<8; ++i)
				dst[j*pitch+i] = ((unsigned)src[j*pitch+i] + (unsigned)src[(j+1)*pitch+i] + 1)>>1;
	}

	static void predict_C_quadpel(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<8; ++j)
			for(int i=0; i<8; ++i)
				dst[j*pitch+i] = ((unsigned)src[j*pitch+i] + (unsigned)src[j*pitch+i+1] +
							(unsigned)src[(j+1)*pitch+i] + (unsigned)src[(j+1)*pitch+i+1] + 2)>>2;
	}

	static void add_Y_normal(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<16; ++j)
			for(int i=0; i<16; ++i)
				dst[j*pitch+i] = (dst[j*pitch+i] + src[j*pitch+i] + 1)>>1;
	}

	static void add_Y_halfpelX(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<16; ++j)
			for(int i=0; i<16; ++i)
				dst[j*pitch+i] = (dst[j*pitch+i] + (((unsigned)src[j*pitch+i] + (unsigned)src[j*pitch+i+1] + 1)>>1) + 1)>>1;
	}

	static void add_Y_halfpelY(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<16; ++j)
			for(int i=0; i<16; ++i)
				dst[j*pitch+i] = (dst[j*pitch+i] + (((unsigned)src[j*pitch+i] + (unsigned)src[(j+1)*pitch+i] + 1)>>1) + 1)>>1;
	}

	static void add_Y_quadpel(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<16; ++j)
			for(int i=0; i<16; ++i)
				dst[j*pitch+i] = (dst[j*pitch+i] + (((unsigned)src[j*pitch+i] + (unsigned)src[j*pitch+i+1] +
							(unsigned)src[(j+1)*pitch+i] + (unsigned)src[(j+1)*pitch+i+1] + 2)>>2) + 1)>>1;
	}

	static void add_C_normal(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<8; ++j)
			for(int i=0; i<8; ++i)
				dst[j*pitch+i] = (dst[j*pitch+i] + src[j*pitch+i] + 1)>>1;
	}

	static void add_C_halfpelX(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<8; ++j)
			for(int i=0; i<8; ++i)
				dst[j*pitch+i] = (dst[j*pitch+i] + (((unsigned)src[j*pitch+i] + (unsigned)src[j*pitch+i+1] + 1)>>1) + 1)>>1;
	}

	static void add_C_halfpelY(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<8; ++j)
			for(int i=0; i<8; ++i)
				dst[j*pitch+i] = (dst[j*pitch+i] + (((unsigned)src[j*pitch+i] + (unsigned)src[(j+1)*pitch+i] + 1)>>1) + 1)>>1;
	}

	static void add_C_quadpel(unsigned char *dst, unsigned char *src, ptrdiff_t pitch) {
		for(int j=0; j<8; ++j)
			for(int i=0; i<8; ++i)
				dst[j*pitch+i] = (dst[j*pitch+i] + (((unsigned)src[j*pitch+i] + (unsigned)src[j*pitch+i+1] +
							(unsigned)src[(j+1)*pitch+i] + (unsigned)src[(j+1)*pitch+i+1] + 2)>>2) + 1)>>1;
	}
}
///////////////////////////////////////////////////////////////////////////

const struct VDMPEGPredictorSet g_VDMPEGPredict_reference = {
	nsVDMPEGPredictReference::predict_Y_normal,
	nsVDMPEGPredictReference::predict_Y_halfpelX,
	nsVDMPEGPredictReference::predict_Y_halfpelY,
	nsVDMPEGPredictReference::predict_Y_quadpel,
	nsVDMPEGPredictReference::predict_C_normal,
	nsVDMPEGPredictReference::predict_C_halfpelX,
	nsVDMPEGPredictReference::predict_C_halfpelY,
	nsVDMPEGPredictReference::predict_C_quadpel,
	nsVDMPEGPredictReference::add_Y_normal,
	nsVDMPEGPredictReference::add_Y_halfpelX,
	nsVDMPEGPredictReference::add_Y_halfpelY,
	nsVDMPEGPredictReference::add_Y_quadpel,
	nsVDMPEGPredictReference::add_C_normal,
	nsVDMPEGPredictReference::add_C_halfpelX,
	nsVDMPEGPredictReference::add_C_halfpelY,
	nsVDMPEGPredictReference::add_C_quadpel,
};


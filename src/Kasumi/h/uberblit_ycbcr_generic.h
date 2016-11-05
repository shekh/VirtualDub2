#ifndef f_VD2_KASUMI_UBERBLIT_YCBCR_GENERIC_H
#define f_VD2_KASUMI_UBERBLIT_YCBCR_GENERIC_H

#include "uberblit_ycbcr.h"

struct VDPixmapGenYCbCrBasis {
	float mKr;
	float mKb;
	float mToRGB[2][3];
};

extern const VDPixmapGenYCbCrBasis g_VDPixmapGenYCbCrBasis_601;
extern const VDPixmapGenYCbCrBasis g_VDPixmapGenYCbCrBasis_709;

////////////////////////////////////////////////////////////////////////////

class VDPixmapGenYCbCrToRGB32Generic : public VDPixmapGenYCbCrToRGB32Base {
public:
	VDPixmapGenYCbCrToRGB32Generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB);

	uint32 GetType(uint32 output) const;

protected:
	virtual void Compute(void *dst0, sint32 y);

	sint32 mCoY;
	sint32 mCoRCr;
	sint32 mCoGCr;
	sint32 mCoGCb;
	sint32 mCoBCb;
	sint32 mBiasR;
	sint32 mBiasG;
	sint32 mBiasB;
};

////////////////////////////////////////////////////////////////////////////

class VDPixmapGenYCbCrToRGB64Generic : public VDPixmapGenYCbCrToRGB64Base {
public:
	VDPixmapGenYCbCrToRGB64Generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB);

	uint32 GetType(uint32 output) const;

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrcY->TransformPixmapInfo(src,buf);
		mpSrcCr->TransformPixmapInfo(src,buf);
		mpSrcCb->TransformPixmapInfo(src,buf);
		dst.copy_frame(buf);
		dst.ref_r = 0xFFFF;
		dst.ref_g = 0xFFFF;
		dst.ref_b = 0xFFFF;
		dst.ref_a = 0xFFFF;
		ref_r = buf.ref_r;
	}

	void Start() {
		VDPixmapGenYCbCrToRGB64Base::Start();
		UpdateParams();
	}

protected:
	void UpdateParams();
	virtual void Compute(void *dst0, sint32 y);

	VDPixmapGenYCbCrBasis basis;
	bool studioRGB;
	int ref_r;

	float mCoY;
	float mCoRCr;
	float mCoGCr;
	float mCoGCb;
	float mCoBCb;
	float mBiasR;
	float mBiasG;
	float mBiasB;
};

////////////////////////////////////////////////////////////////////////////

class VDPixmapGenYCbCrToRGB32FGeneric : public VDPixmapGenYCbCrToRGB32FBase {
public:
	VDPixmapGenYCbCrToRGB32FGeneric(const VDPixmapGenYCbCrBasis& basis, bool studioRGB);

	uint32 GetType(uint32 output) const;

protected:
	void Compute(void *dst0, sint32 y);

	float mCoY;
	float mCoRCr;
	float mCoGCr;
	float mCoGCb;
	float mCoBCb;
	float mBiasR;
	float mBiasG;
	float mBiasB;
};

////////////////////////////////////////////////////////////////////////////

class VDPixmapGenRGB32ToYCbCrGeneric : public VDPixmapGenRGB32ToYCbCrBase {
public:
	VDPixmapGenRGB32ToYCbCrGeneric(const VDPixmapGenYCbCrBasis& basis, bool studioRGB, uint32 colorSpace);

	uint32 GetType(uint32 output) const;

protected:
	void Compute(void *dst0, sint32 y);

	sint32 mCoYR;
	sint32 mCoYG;
	sint32 mCoYB;
	sint32 mCoCbR;
	sint32 mCoCbG;
	sint32 mCoCbB;
	sint32 mCoCrR;
	sint32 mCoCrG;
	sint32 mCoCrB;
	sint32 mCoYA;
	sint32 mCoCbA;
	sint32 mCoCrA;

	const uint32 mColorSpace;
};

////////////////////////////////////////////////////////////////////////////

class VDPixmapGenRGB32FToYCbCrGeneric : public VDPixmapGenRGB32FToYCbCrBase {
public:
	VDPixmapGenRGB32FToYCbCrGeneric(const VDPixmapGenYCbCrBasis& basis, bool studioRGB, uint32 colorSpace);

	uint32 GetType(uint32 output) const;

protected:
	void Compute(void *dst0, sint32 y);

 	float mCoYR;
	float mCoYG;
	float mCoYB;
	float mCoYA;
	float mCoCbR;
	float mCoCbG;
	float mCoCbB;
	float mCoCbA;
	float mCoCrR;
	float mCoCrG;
	float mCoCrB;
	float mCoCrA;

	const uint32 mColorSpace;
};

////////////////////////////////////////////////////////////////////////////

class VDPixmapGenYCbCrToYCbCrGeneric : public VDPixmapGenYCbCrToRGBBase {
public:
	VDPixmapGenYCbCrToYCbCrGeneric(const VDPixmapGenYCbCrBasis& dstBasis, bool dstLimitedRange, const VDPixmapGenYCbCrBasis& srcBasis, bool srcLimitedRange, uint32 colorSpace);
	 
	void Start();
	const void *GetRow(sint32 y, uint32 index);
	uint32 GetType(uint32 output) const;

protected:
	void Compute(void *dst0, sint32 ypos);

	sint32 mCoYY;
	sint32 mCoYCb;
	sint32 mCoYCr;
	sint32 mCoYA;
	sint32 mCoCbCb;
	sint32 mCoCbCr;
	sint32 mCoCbA;
	sint32 mCoCrCb;
	sint32 mCoCrCr;
	sint32 mCoCrA;

	const uint32 mColorSpace;
};

////////////////////////////////////////////////////////////////////////////

class VDPixmapGenYCbCrToYCbCrGeneric_32F : public VDPixmapGenYCbCrToRGBBase {
public:
	VDPixmapGenYCbCrToYCbCrGeneric_32F(const VDPixmapGenYCbCrBasis& dstBasis, bool dstLimitedRange, const VDPixmapGenYCbCrBasis& srcBasis, bool srcLimitedRange, uint32 colorSpace);

	void Start();
	const void *GetRow(sint32 y, uint32 index);
	uint32 GetType(uint32 output) const;

protected:
	void Compute(void *dst0, sint32 ypos);

	float mCoYY;
	float mCoYCb;
	float mCoYCr;
	float mCoYA;
	float mCoCbCb;
	float mCoCbCr;
	float mCoCbA;
	float mCoCrCb;
	float mCoCrCr;	
	float mCoCrA;	

	const uint32 mColorSpace;
};

#endif

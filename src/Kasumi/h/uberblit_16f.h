#ifndef f_VD2_KASUMI_UBERBLIT_16F_H
#define f_VD2_KASUMI_UBERBLIT_16F_H

#include <vd2/system/cpuaccel.h>
#include "uberblit_base.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32F -> 16F
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_32F_To_16F : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start();

	uint32 GetType(uint32 output) const;

protected:
	void Compute(void *dst0, sint32 y);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	16F -> 32F
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_16F_To_32F : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start();

	uint32 GetType(uint32 output) const;

protected:
	void Compute(void *dst0, sint32 y);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32F -> 16
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_32F_To_16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrc->TransformPixmapInfo(src,unused);
		dst.ref_r = 0xFFFF;
		m = float(dst.ref_r);
	}

	void Start();

	uint32 GetType(uint32 output) const;

protected:
	float m;

	void Compute(void *dst0, sint32 y);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	16 -> 32F
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_16_To_32F : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		ref = buf.ref_r;
		m = float(1.0/buf.ref_r);
	}

	void Start();

	uint32 GetType(uint32 output) const;

protected:
	int ref;
	float m;

	void Compute(void *dst0, sint32 y);
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_Y16_Normalize : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	VDPixmapGen_Y16_Normalize(){ max_value = 0xFFFF; }

	uint32 max_value;

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		mpSrc->TransformPixmapInfo(src,dst);
		if (dst.ref_r==max_value) {
			do_normalize = false;
		} else {
			do_normalize = true;
			ref = dst.ref_r;
			m = max_value*0x10000/ref;
			dst.ref_r = max_value;
			scale_down = true;
			if (m>=0x10000) scale_down = false;
		}
	}

	void Start() {
		StartWindow(mWidth * 2);
	}

protected:
	int ref;
	uint32 m;
	bool do_normalize;
	bool scale_down;

	void Compute(void *dst0, sint32 y);
	void ComputeNormalize(void *dst0, sint32 y);
};

class ExtraGen_YUV_Normalize : public IVDPixmapExtraGen {
public:
	uint32 max_value;

	ExtraGen_YUV_Normalize(){ max_value=0xFFFF; }
	virtual void Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst);
};

bool inline VDPixmap_YUV_IsNormalized(const FilterModPixmapInfo& info, uint32 max_value=0xFFFF) {
	if (info.ref_r!=max_value)
		return false;
	return true;
}

void VDPixmap_YUV_Normalize(VDPixmap& dst, const VDPixmap& src, uint32 max_value=0xFFFF);
void VDPixmap_bitmap_to_YUV422_Planar16(VDPixmap& dst, const VDPixmap& src, int variant);

#endif

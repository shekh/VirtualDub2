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

	virtual const char* dump_name(){ return "32F_To_16F"; }

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

	virtual const char* dump_name(){ return "16F_To_32F"; }

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

	virtual const char* dump_name(){ return "32F_To_16"; }

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

	virtual const char* dump_name(){ return "16_To_32F"; }

protected:
	int ref;
	float m;

	void Compute(void *dst0, sint32 y);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	16 -> 8
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_8_To_16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		dst.copy_frame(buf);
		dst.ref_r = 0xFFFF;
	}

	void Start() {
		StartWindow(mWidth * 2);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_16_LE;
	}

	virtual const char* dump_name(){ return "8_To_16"; }

protected:

	void Compute(void *dst0, sint32 y);
};

class VDPixmapGen_16_To_8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		dst.copy_frame(buf);
		ref = buf.ref_r;
		m = 0xFF0000/buf.ref_r;
	}

	void Start();

	uint32 GetType(uint32 output) const;

	virtual const char* dump_name(){ return "16_To_8"; }

protected:
	int ref;
	uint32 m;

	void Compute(void *dst0, sint32 y);
};

class VDPixmapGen_A8_To_A16 : public VDPixmapGen_8_To_16 {
public:
	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		dst.copy_alpha(buf);
		dst.ref_a = 0xFFFF;
	}
};

class VDPixmapGen_A16_To_A8 : public VDPixmapGen_16_To_8 {
public:
	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		dst.copy_alpha(buf);
		ref = buf.ref_a;
		m = 0xFF0000/buf.ref_a;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_Y16_Normalize : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	VDPixmapGen_Y16_Normalize(){ max_value = 0xFFFF; mask = 0xFFFF; }

	uint32 max_value;
	uint16 mask;

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		mpSrc->TransformPixmapInfo(src,dst);
		if (dst.ref_r==max_value) {
			do_normalize = false;
			ref = dst.ref_r;
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
		int type = mpSrc->GetType(0);
		bpp = 2;
		if ((type & kVDPixType_Mask)==kVDPixType_16x2_LE) bpp = 4;
		if ((type & kVDPixType_Mask)==kVDPixType_16x4_LE) bpp = 8;
		if ((type & kVDPixType_Mask)==kVDPixType_YU64) bpp = 4;
		StartWindow(mWidth * bpp);
	}

	virtual const char* dump_name(){ return "Y16_Normalize"; }

protected:
	int bpp;
	int ref;
	uint32 m;
	bool do_normalize;
	bool scale_down;

	void Compute(void *dst0, sint32 y);
	void ComputeNormalize(void *dst0, sint32 y);
	void ComputeMask(void *dst0, sint32 y);
};

class VDPixmapGen_A16_Normalize : public VDPixmapGen_Y16_Normalize {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		mpSrc->TransformPixmapInfo(src,dst);

		a_mask = 0;
		if(dst.alpha_type==FilterModPixmapInfo::kAlphaInvalid){
			a_mask = max_value;
			do_normalize = false;
			ref = 0;
		} else {
			if (dst.ref_a==max_value) {
				do_normalize = false;
				ref = dst.ref_a;
			} else {
				do_normalize = true;
				ref = dst.ref_a;
				m = max_value*0x10000/ref;
				dst.ref_a = max_value;
				scale_down = true;
				if (m>=0x10000) scale_down = false;
			}
		}
	}

	virtual const char* dump_name(){ return "A16_Normalize"; }

protected:
	uint32 a_mask;

	void Compute(void *dst0, sint32 y);
	void ComputeWipeAlpha(void *dst0, sint32 y);
};

class VDPixmapGen_A8_Normalize : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		mpSrc->TransformPixmapInfo(src,dst);

		a_mask = 0;
		if(dst.alpha_type==FilterModPixmapInfo::kAlphaInvalid)
			a_mask = 255;
	}

	void Start() {
		StartWindow(mWidth);
	}

	virtual const char* dump_name(){ return "A8_Normalize"; }

protected:
	uint32 a_mask;

	void Compute(void *dst0, sint32 y);
	void ComputeWipeAlpha(void *dst0, sint32 y);
};

class ExtraGen_YUV_Normalize : public IVDPixmapExtraGen {
public:
	uint32 max_value;
	uint16 mask;

	ExtraGen_YUV_Normalize(){ max_value=0xFFFF; mask=0xFFFF; }
	virtual void Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst);
};

class ExtraGen_A8_Normalize : public IVDPixmapExtraGen {
public:
	virtual void Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst);
};

bool inline VDPixmap_YUV_IsNormalized(const FilterModPixmapInfo& info, uint32 max_value=0xFFFF) {
	if (info.ref_r!=max_value)
		return false;
	return true;
}

void VDPixmap_YUV_Normalize(VDPixmap& dst, const VDPixmap& src, uint32 max_value=0xFFFF);

#endif

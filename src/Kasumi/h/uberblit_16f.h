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

	VDPixmapGen_32F_To_16(bool chroma){ isChroma = chroma; }

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrc->TransformPixmapInfo(src,unused);
		if (dst.colorRangeMode==vd2::kColorRangeMode_Full)
			dst.ref_r = 0xFFFF;
		else
			dst.ref_r = 0xFF00;
		m = float(dst.ref_r);
		bias = 0;
		if (isChroma) bias = 0x8000 - 128.0f / 255.0f * m;
	}

	void Start();

	uint32 GetType(uint32 output) const;

	virtual const char* dump_name(){ return "32F_To_16"; }

protected:
	bool isChroma;
	float m;
	float bias;

	void Compute(void *dst0, sint32 y);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	16 -> 32F
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_16_To_32F : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	VDPixmapGen_16_To_32F(bool chroma){ isChroma = chroma; }

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		ref = buf.ref_r;
		m = float(1.0/buf.ref_r);
		bias = 0;
		int mref = vd2::chroma_neutral(ref);
		if (isChroma) bias = -mref*m + 128.0f / 255.0f;
	}

	void Start();

	uint32 GetType(uint32 output) const;

	virtual const char* dump_name(){ return "16_To_32F"; }

protected:
	bool isChroma;
	int ref;
	float m;
	float bias;

	void Compute(void *dst0, sint32 y);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	16 -> 8
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_8_To_16 : public VDPixmapGenWindowBasedOneSourceAlign8to16 {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		dst.copy_frame(buf);
		dst.ref_r = 0xFF00;
		invalid = false;
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_16_LE;
	}

	virtual const char* dump_name(){ return "8_To_16"; }

protected:
	bool invalid;

	int ComputeSpan(uint16* dst, const uint8* src, int n);
};

class VDPixmapGen_A8_To_A16 : public VDPixmapGen_8_To_16 {
public:
	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		dst.copy_alpha(buf);
		dst.ref_a = 0xFFFF;
		invalid = !dst.alpha_type;
	}

	int ComputeSpan(uint16* dst, const uint8* src, int n);
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_16_To_8 : public VDPixmapGenWindowBasedOneSourceAlign16to8 {
public:
	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst);
	void Start();

	uint32 GetType(uint32 output) const;

	virtual const char* dump_name(){ return "16_To_8"; }

protected:
	int ref;
	uint32 m;
	bool invalid;

	int ComputeSpan(uint8* dst, const uint16* src, int n);
};

class VDPixmapGen_A16_To_A8 : public VDPixmapGen_16_To_8 {
public:
	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst);
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_Y16_Normalize : public VDPixmapGenWindowBasedOneSourceAlign16 {
public:

	VDPixmapGen_Y16_Normalize(bool chroma=false){ max_value = 0xFFFF; mask = 0xFFFF; isChroma = chroma; }

	uint32 max_value;
	uint16 mask;

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst);

	virtual const char* dump_name(){ return "Y16_Normalize"; }

protected:
	int ref;
	uint32 m;
	int bias;
	bool isChroma;
	bool do_normalize;

	int ComputeSpan(uint16* dst, const uint16* src, int n);
	void ComputeNormalize(uint16* dst, const uint16* src, int n);
	void ComputeNormalizeBias(uint16* dst, const uint16* src, int n);
	void ComputeMask(uint16* dst, const uint16* src, int n);
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
				bias = 0;
			}
		}
	}

	virtual const char* dump_name(){ return "A16_Normalize"; }

protected:
	uint32 a_mask;

	virtual int ComputeSpan(uint16* dst, const uint16* src, int n);
	void ComputeWipeAlpha(uint16* dst, int n);
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
	uint32 max_a_value;
	uint16 mask;

	ExtraGen_YUV_Normalize(){ max_value=0xFFFF; max_a_value=0xFFFF; mask=0xFFFF; }
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

//void VDPixmap_YUV_Normalize(VDPixmap& dst, const VDPixmap& src, uint32 max_value=0xFFFF);

#endif

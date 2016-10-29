#ifndef f_VD2_KASUMI_UBERBLIT_RGB64_H
#define f_VD2_KASUMI_UBERBLIT_RGB64_H

#include <vd2/system/cpuaccel.h>
#include <vd2/plugin/vdplugin.h>
#include "uberblit_base.h"

bool inline VDPixmap_X16R16G16B16_IsNormalized(const FilterModPixmapInfo& info, uint32 max_value=0xFFFF) {
	if (info.ref_r!=max_value || info.ref_g!=max_value || info.ref_b!=max_value)
		return false;
	if (info.alpha_type!=FilterModPixmapInfo::kAlphaInvalid && info.ref_a!=max_value)
		return false;
	return true;
}

void VDPixmap_X16R16G16B16_Normalize(VDPixmap& dst, const VDPixmap& src, uint32 max_value=0xFFFF);
void VDPixmap_X16R16G16B16_to_b64a(VDPixmap& dst, const VDPixmap& src);
void VDPixmap_b64a_to_X16R16G16B16(VDPixmap& dst, const VDPixmap& src);
void VDPixmap_bitmap_to_X16R16G16B16(VDPixmap& dst, const VDPixmap& src, int variant);

class VDPixmapGen_X8R8G8B8_To_X16R16G16B16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		dst.copy_frame(buf);
		dst.copy_alpha(buf);
		dst.ref_r = 0xFF00;
		dst.ref_g = 0xFF00;
		dst.ref_b = 0xFF00;
		dst.ref_a = 0xFF00;
	}

	void Start() {
		StartWindow(mWidth * 8);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_16x4_LE;
	}

protected:
	void Compute(void *dst0, sint32 y);
};

class VDPixmapGen_X16R16G16B16_To_X32B32G32R32F : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		ref_r = buf.ref_r;
		ref_g = buf.ref_g;
		ref_b = buf.ref_b;
		ref_a = buf.ref_a;
		mr = float(1.0/buf.ref_r);
		mg = float(1.0/buf.ref_g);
		mb = float(1.0/buf.ref_b);
		ma = float(1.0/buf.ref_a);
	}

	void Start() {
		StartWindow(mWidth * 16);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_32Fx4_LE;
	}

protected:
	int ref_r;
	int ref_g;
	int ref_b;
	int ref_a;
	float mr;
	float mg;
	float mb;
	float ma;
	
	void Compute(void *dst0, sint32 y);
};

class VDPixmapGen_X32B32G32R32F_To_X16R16G16B16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrc->TransformPixmapInfo(src,unused);
		dst.ref_r = 0xFFFF;
		dst.ref_g = 0xFFFF;
		dst.ref_b = 0xFFFF;
		dst.ref_a = 0xFFFF;
		mr = float(dst.ref_r);
		mg = float(dst.ref_g);
		mb = float(dst.ref_b);
		ma = float(dst.ref_a);
	}

	void Start() {
		StartWindow(mWidth * 8);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_16x4_LE;
	}

protected:
	float mr;
	float mg;
	float mb;
	float ma;
	
	void Compute(void *dst0, sint32 y);
};

class VDPixmapGen_X16R16G16B16_To_X8R8G8B8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		if(buf.alpha_type==FilterModPixmapInfo::kAlphaInvalid) buf.ref_a = 0xFFFF;
		dst.copy_frame(buf);
		dst.copy_alpha(buf);
		ref_r = buf.ref_r;
		ref_g = buf.ref_g;
		ref_b = buf.ref_b;
		ref_a = buf.ref_a;
		mr = 0xFF0000/buf.ref_r;
		mg = 0xFF0000/buf.ref_g;
		mb = 0xFF0000/buf.ref_b;
		ma = 0xFF0000/buf.ref_a;
	}

	void Start() {
		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

protected:
	int ref_r;
	int ref_g;
	int ref_b;
	int ref_a;
	uint32 mr;
	uint32 mg;
	uint32 mb;
	uint32 ma;

	void Compute(void *dst0, sint32 y);
};

class VDPixmapGen_X16R16G16B16_Normalize : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	VDPixmapGen_X16R16G16B16_Normalize(){ max_value=0xFFFF; }

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		mpSrc->TransformPixmapInfo(src,dst);
		if (VDPixmap_X16R16G16B16_IsNormalized(dst,max_value)) {
			do_normalize = false;
		} else {
			do_normalize = true;
			ref_r = dst.ref_r;
			ref_g = dst.ref_g;
			ref_b = dst.ref_b;
			ref_a = dst.ref_a;
			mr = max_value*0x10000/ref_r;
			mg = max_value*0x10000/ref_g;
			mb = max_value*0x10000/ref_b;
			ma = max_value*0x10000/ref_a;
			dst.ref_r = max_value;
			dst.ref_g = max_value;
			dst.ref_b = max_value;
			dst.ref_a = max_value;
			scale_down = true;
			if (mr>0x10000 || mg>0x10000 || mb>0x10000 || ma>0x10000) scale_down = false;
		}

		a_mask = 0;
		if(dst.alpha_type==FilterModPixmapInfo::kAlphaInvalid)
			a_mask = max_value;
	}

	void Start() {
		StartWindow(mWidth * 8);
	}

	uint32 max_value;

protected:
	int ref_r,ref_g,ref_b,ref_a;
	uint32 mr,mg,mb,ma;
	uint32 a_mask;
	bool do_normalize;
	bool scale_down;

	void Compute(void *dst0, sint32 y);
	void ComputeAll(void *dst0, sint32 y);
	void ComputeWipeAlpha(void *dst0, sint32 y);
};

class VDPixmapGen_X8R8G8B8_Normalize : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		mpSrc->TransformPixmapInfo(src,dst);

		a_mask = 0;
		if(dst.alpha_type==FilterModPixmapInfo::kAlphaInvalid)
			a_mask = 0xFF000000;
	}

	void Start() {
		StartWindow(mWidth * 4);
	}

protected:
	uint32 a_mask;

	void Compute(void *dst0, sint32 y);
	void ComputeWipeAlpha(void *dst0, sint32 y);
};

#endif

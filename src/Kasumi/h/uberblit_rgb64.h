#ifndef f_VD2_KASUMI_UBERBLIT_RGB64_H
#define f_VD2_KASUMI_UBERBLIT_RGB64_H

#include <vd2/system/cpuaccel.h>
#include "uberblit_base.h"

class VDPixmapGen_X8R8G8B8_To_X16R16G16B16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrc->TransformPixmapInfo(src,unused);
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
		mr = float(1.0/buf.ref_r);
		mg = float(1.0/buf.ref_g);
		mb = float(1.0/buf.ref_b);
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
	float mr;
	float mg;
	float mb;
	
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
		mr = float(dst.ref_r);
		mg = float(dst.ref_g);
		mb = float(dst.ref_b);
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
	
	void Compute(void *dst0, sint32 y);
};

class VDPixmapGen_X16R16G16B16_To_X8R8G8B8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,buf);
		ref_r = buf.ref_r;
		ref_g = buf.ref_g;
		ref_b = buf.ref_b;
		if(buf.ref_r==0xFFFF && buf.ref_g==0xFFFF && buf.ref_b==0xFFFF){
			unorm_mode = false;
		} else {
			unorm_mode = true;
			mr = 0xFF0000/buf.ref_r;
			mg = 0xFF0000/buf.ref_g;
			mb = 0xFF0000/buf.ref_b;
		}
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
	uint32 mr;
	uint32 mg;
	uint32 mb;
	bool unorm_mode;

	void Compute(void *dst0, sint32 y);
};

#endif

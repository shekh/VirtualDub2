#ifndef f_VD2_KASUMI_UBERBLIT_V210_H
#define f_VD2_KASUMI_UBERBLIT_V210_H

#include <vd2/system/cpuaccel.h>
#include "uberblit_base.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32F -> V210
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_32F_To_V210 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcR, uint32 srcindexR, IVDPixmapGen *srcG, uint32 srcindexG, IVDPixmapGen *srcB, uint32 srcindexB) {
		mpSrcR = srcR;
		mSrcIndexR = srcindexR;
		mpSrcG = srcG;
		mSrcIndexG = srcindexG;
		mpSrcB = srcB;
		mSrcIndexB = srcindexB;
		mWidth = srcG->GetWidth(srcindexG);
		mHeight = srcG->GetHeight(srcindexG);

		srcR->AddWindowRequest(0, 0);
		srcG->AddWindowRequest(0, 0);
		srcB->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrcR->Start();
		mpSrcG->Start();
		mpSrcB->Start();

		int qw = (mWidth + 47) / 48;
		StartWindow(qw * 128);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrcR->TransformPixmapInfo(src,unused);
		mpSrcG->TransformPixmapInfo(src,unused);
		mpSrcB->TransformPixmapInfo(src,unused);
	}

	uint32 GetType(uint32 output) const;

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrcR;
		if(index==1) return mpSrcG;
		if(index==2) return mpSrcB;
		return 0; 
	}

	virtual const char* dump_name(){ return "32F_To_V210"; }

protected:
	void Compute(void *dst0, sint32 y);

	IVDPixmapGen *mpSrcR;
	uint32 mSrcIndexR;
	IVDPixmapGen *mpSrcG;
	uint32 mSrcIndexG;
	IVDPixmapGen *mpSrcB;
	uint32 mSrcIndexB;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	V210 -> 32F
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_V210_To_32F : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start();
	const void *GetRow(sint32 y, uint32 index);

	sint32 GetWidth(int index) const;
	uint32 GetType(uint32 output) const;

	virtual const char* dump_name(){ return "V210_To_32F"; }

protected:
	void Compute(void *dst0, sint32 y);
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	V210 -> P16
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_V210_To_P16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start();
	const void *GetRow(sint32 y, uint32 index);

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrc->TransformPixmapInfo(src,unused);
		dst.colorSpaceMode = src.colorSpaceMode;
		dst.colorRangeMode = src.colorRangeMode;
		dst.ref_r = 0x3FF;
		dst.ref_g = 0;
		dst.ref_b = 0;
		dst.ref_a = 0;
	}

	sint32 GetWidth(int index) const;
	uint32 GetType(uint32 output) const;

	virtual const char* dump_name(){ return "V210_To_P16"; }

protected:
	void Compute(void *dst0, sint32 y);
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32F -> V410
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_32F_To_V410 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcR, uint32 srcindexR, IVDPixmapGen *srcG, uint32 srcindexG, IVDPixmapGen *srcB, uint32 srcindexB) {
		mpSrcR = srcR;
		mSrcIndexR = srcindexR;
		mpSrcG = srcG;
		mSrcIndexG = srcindexG;
		mpSrcB = srcB;
		mSrcIndexB = srcindexB;
		mWidth = srcG->GetWidth(srcindexG);
		mHeight = srcG->GetHeight(srcindexG);

		srcR->AddWindowRequest(0, 0);
		srcG->AddWindowRequest(0, 0);
		srcB->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrcR->Start();
		mpSrcG->Start();
		mpSrcB->Start();

		StartWindow(mWidth * 4);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrcR->TransformPixmapInfo(src,unused);
		mpSrcG->TransformPixmapInfo(src,unused);
		mpSrcB->TransformPixmapInfo(src,unused);
	}

	uint32 GetType(uint32 output) const;

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrcR;
		if(index==1) return mpSrcG;
		if(index==2) return mpSrcB;
		return 0; 
	}

	virtual const char* dump_name(){ return "32F_To_V410"; }

protected:
	void Compute(void *dst0, sint32 y);

	IVDPixmapGen *mpSrcR;
	uint32 mSrcIndexR;
	IVDPixmapGen *mpSrcG;
	uint32 mSrcIndexG;
	IVDPixmapGen *mpSrcB;
	uint32 mSrcIndexB;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	V410 -> 32F
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_V410_To_32F : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start();
	const void *GetRow(sint32 y, uint32 index);

	uint32 GetType(uint32 output) const;

	virtual const char* dump_name(){ return "V410_To_32F"; }

protected:
	void Compute(void *dst0, sint32 y);
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	V410 -> P16
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_V410_To_P16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start();
	const void *GetRow(sint32 y, uint32 index);

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrc->TransformPixmapInfo(src,unused);
		dst.colorSpaceMode = src.colorSpaceMode;
		dst.colorRangeMode = src.colorRangeMode;
		dst.ref_r = 0x3FF;
		dst.ref_g = 0;
		dst.ref_b = 0;
		dst.ref_a = 0;
	}

	uint32 GetType(uint32 output) const;

	virtual const char* dump_name(){ return "V410_To_P16"; }

protected:
	void Compute(void *dst0, sint32 y);
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32F -> Y410
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_32F_To_Y410 : public VDPixmapGen_32F_To_V410 {
public:
	uint32 GetType(uint32 output) const;
	virtual const char* dump_name(){ return "32F_To_Y410"; }

protected:
	void Compute(void *dst0, sint32 y);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Y410 -> 32F
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_Y410_To_32F : public VDPixmapGen_V410_To_32F {
public:
	virtual const char* dump_name(){ return "Y410_To_32F"; }
protected:
	void Compute(void *dst0, sint32 y);
};


///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Y410 -> P16
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_Y410_To_P16 : public VDPixmapGen_V410_To_P16 {
public:
	virtual const char* dump_name(){ return "Y410_To_P16"; }
protected:
	void Compute(void *dst0, sint32 y);
};

#endif	// f_VD2_KASUMI_UBERBLIT_V210_H

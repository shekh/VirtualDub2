//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2008 Avery Lee
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

#ifndef f_VD2_KASUMI_UBERBLIT_SWIZZLE_H
#define f_VD2_KASUMI_UBERBLIT_SWIZZLE_H

#include <vd2/system/cpuaccel.h>
#include "uberblit_base.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	generic converters
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_Swap8In16 : public VDPixmapGenWindowBasedOneSource {
public:
	void Init(IVDPixmapGen *gen, int srcIndex, uint32 w, uint32 h, uint32 bpr);
	void Start();

	uint32 GetType(uint32 index) const;

	virtual const char* dump_name(){ return "Swap8In16"; }

protected:
	void Compute(void *dst0, sint32 y);

	uint32 mRowLength;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32-bit deinterleavers
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_8In16 : public VDPixmapGenWindowBasedOneSource {
public:
	void Init(IVDPixmapGen *gen, int srcIndex, int offset, uint32 w, uint32 h) {
		InitSource(gen, srcIndex);
		mOffset = offset;
		SetOutputSize(w, h);
		gen->AddWindowRequest(0, 0);
	}

	void Start() {
		StartWindow(mWidth);
	}

	uint32 GetType(uint32 index) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8;
	}

	virtual const char* dump_name(){ return "8In16"; }

protected:
	void Compute(void *dst0, sint32 y) {
		const uint8 *srcp = (const uint8 *)mpSrc->GetRow(y, mSrcIndex) + mOffset;
		uint8 *dst = (uint8 *)dst0;
		sint32 w = mWidth;
		for(sint32 x=0; x<w; ++x) {
			*dst++ = *srcp;
			srcp += 2;
		}
	}

	int mOffset;
};

class VDPixmapGen_8In32 : public VDPixmapGenWindowBasedOneSource {
public:
	void Init(IVDPixmapGen *gen, int srcIndex, int offset, uint32 w, uint32 h) {
		InitSource(gen, srcIndex);
		mOffset = offset;
		SetOutputSize(w, h);
		gen->AddWindowRequest(0, 0);
	}

	void Start() {
		StartWindow(mWidth);
	}

	uint32 GetType(uint32 index) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8;
	}

	virtual const char* dump_name(){ return "8In32"; }

protected:
	void Compute(void *dst0, sint32 y) {
		const uint8 *srcp = (const uint8 *)mpSrc->GetRow(y, mSrcIndex) + mOffset;
		uint8 *dst = (uint8 *)dst0;
		sint32 w = mWidth;
		for(sint32 x=0; x<w; ++x) {
			*dst++ = *srcp;
			srcp += 4;
		}
	}

	int mOffset;
};

class VDPixmapGen_16In32 : public VDPixmapGenWindowBasedOneSource {
public:
	void Init(IVDPixmapGen *gen, int srcIndex, int offset, uint32 w, uint32 h) {
		InitSource(gen, srcIndex);
		mOffset = offset;
		SetOutputSize(w, h);
		gen->AddWindowRequest(0, 0);
	}

	void Start() {
		StartWindow(mWidth*2);
	}

	uint32 GetType(uint32 index) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_16_LE;
	}

	virtual const char* dump_name(){ return "16In32"; }

protected:
	void Compute(void *dst0, sint32 y);

	int mOffset;
};

class VDPixmapGen_16In64 : public VDPixmapGenWindowBasedOneSource {
public:
	void Init(IVDPixmapGen *gen, int srcIndex, int offset, uint32 w, uint32 h) {
		InitSource(gen, srcIndex);
		mOffset = offset;
		SetOutputSize(w, h);
		gen->AddWindowRequest(0, 0);
	}

	void Start() {
		StartWindow(mWidth*2);
	}

	uint32 GetType(uint32 index) const {
		return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_16_LE;
	}

	virtual const char* dump_name(){ return "16In64"; }

protected:
	void Compute(void *dst0, sint32 y);

	int mOffset;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	16-bit interleavers
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_B8x2_To_B8R8 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcCb, uint32 srcindexCb, IVDPixmapGen *srcCr, uint32 srcindexCr);
	void Start();
	uint32 GetType(uint32 output) const;

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrcCb->TransformPixmapInfo(src,unused);
		mpSrcCr->TransformPixmapInfo(src,unused);
	}

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrcCb;
		if(index==1) return mpSrcCr;
		return 0; 
	}

	virtual const char* dump_name(){ return "B8x2_To_B8R8"; }

protected:
	void Compute(void *dst0, sint32 y);

	IVDPixmapGen *mpSrcCb;
	uint32 mSrcIndexCb;
	IVDPixmapGen *mpSrcCr;
	uint32 mSrcIndexCr;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	32-bit interleavers
//
///////////////////////////////////////////////////////////////////////////////////////////////////

class VDPixmapGen_B8x3_To_G8B8_G8R8 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcCr, uint32 srcindexCr, IVDPixmapGen *srcY, uint32 srcindexY, IVDPixmapGen *srcCb, uint32 srcindexCb) {
		mpSrcY = srcY;
		mSrcIndexY = srcindexY;
		mpSrcCb = srcCb;
		mSrcIndexCb = srcindexCb;
		mpSrcCr = srcCr;
		mSrcIndexCr = srcindexCr;
		mWidth = srcY->GetWidth(srcindexY);
		mHeight = srcY->GetHeight(srcindexY);

		srcY->AddWindowRequest(0, 0);
		srcCb->AddWindowRequest(0, 0);
		srcCr->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrcY->Start();
		mpSrcCb->Start();
		mpSrcCr->Start();

		StartWindow(((mWidth + 1) & ~1) * 2);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrcY->TransformPixmapInfo(src,unused);
		mpSrcCb->TransformPixmapInfo(src,unused);
		mpSrcCr->TransformPixmapInfo(src,unused);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrcY->GetType(mSrcIndexY) & ~kVDPixType_Mask) | kVDPixType_B8G8_R8G8;
	}

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrcY;
		if(index==1) return mpSrcCb;
		if(index==2) return mpSrcCr;
		return 0; 
	}

	virtual const char* dump_name(){ return "B8x3_To_G8B8_G8R8"; }

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 *VDRESTRICT dst = (uint8 *)dst0;
		const uint8 *VDRESTRICT srcY = (const uint8 *)mpSrcY->GetRow(y, mSrcIndexY);
		const uint8 *VDRESTRICT srcCb = (const uint8 *)mpSrcCb->GetRow(y, mSrcIndexCb);
		const uint8 *VDRESTRICT srcCr = (const uint8 *)mpSrcCr->GetRow(y, mSrcIndexCr);

		sint32 w = mWidth >> 1;
		for(sint32 x=0; x<w; ++x) {
			uint8 y1 = srcY[0];
			uint8 cb = srcCb[0];
			uint8 y2 = srcY[1];
			uint8 cr = srcCr[0];

			dst[0] = y1;
			dst[1] = cb;
			dst[2] = y2;
			dst[3] = cr;

			srcY += 2;
			++srcCb;
			++srcCr;
			dst += 4;
		}

		if (mWidth & 1) {
			uint8 y1 = srcY[0];
			uint8 cb = srcCb[0];
			uint8 cr = srcCr[0];

			dst[0] = y1;
			dst[1] = cb;
			dst[2] = y1;
			dst[3] = cr;
		}
	}

	IVDPixmapGen *mpSrcY;
	uint32 mSrcIndexY;
	IVDPixmapGen *mpSrcCb;
	uint32 mSrcIndexCb;
	IVDPixmapGen *mpSrcCr;
	uint32 mSrcIndexCr;
};

class VDPixmapGen_B8x3_To_B8G8_R8G8 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcCr, uint32 srcindexCr, IVDPixmapGen *srcY, uint32 srcindexY, IVDPixmapGen *srcCb, uint32 srcindexCb) {
		mpSrcY = srcY;
		mSrcIndexY = srcindexY;
		mpSrcCb = srcCb;
		mSrcIndexCb = srcindexCb;
		mpSrcCr = srcCr;
		mSrcIndexCr = srcindexCr;
		mWidth = srcY->GetWidth(srcindexY);
		mHeight = srcY->GetHeight(srcindexY);

		srcY->AddWindowRequest(0, 0);
		srcCb->AddWindowRequest(0, 0);
		srcCr->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrcY->Start();
		mpSrcCb->Start();
		mpSrcCr->Start();

		StartWindow(((mWidth + 1) & ~1) * 2);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrcY->TransformPixmapInfo(src,unused);
		mpSrcCb->TransformPixmapInfo(src,unused);
		mpSrcCr->TransformPixmapInfo(src,unused);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrcY->GetType(mSrcIndexY) & ~kVDPixType_Mask) | kVDPixType_G8B8_G8R8;
	}

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrcY;
		if(index==1) return mpSrcCb;
		if(index==2) return mpSrcCr;
		return 0; 
	}

	virtual const char* dump_name(){ return "B8x3_To_B8G8_R8G8"; }

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 * VDRESTRICT dst = (uint8 *)dst0;
		const uint8 *VDRESTRICT srcY = (const uint8 *)mpSrcY->GetRow(y, mSrcIndexY);
		const uint8 *VDRESTRICT srcCb = (const uint8 *)mpSrcCb->GetRow(y, mSrcIndexCb);
		const uint8 *VDRESTRICT srcCr = (const uint8 *)mpSrcCr->GetRow(y, mSrcIndexCr);

		sint32 w2 = mWidth >> 1;
		for(sint32 x=0; x<w2; ++x) {
			uint8 cb = srcCb[0];
			uint8 y1 = srcY[0];
			uint8 cr = srcCr[0];
			uint8 y2 = srcY[1];

			dst[0] = cb;
			dst[1] = y1;
			dst[2] = cr;
			dst[3] = y2;
			dst += 4;
			srcY += 2;
			++srcCb;
			++srcCr;
		}

		if (mWidth & 1) {
			uint8 cb = srcCb[0];
			uint8 y1 = srcY[0];
			uint8 cr = srcCr[0];

			dst[0] = cb;
			dst[1] = y1;
			dst[2] = cr;
			dst[3] = y1;
		}
	}

	IVDPixmapGen *mpSrcY;
	uint32 mSrcIndexY;
	IVDPixmapGen *mpSrcCb;
	uint32 mSrcIndexCb;
	IVDPixmapGen *mpSrcCr;
	uint32 mSrcIndexCr;
};

class VDPixmapGen_B8x3_To_X8R8G8B8 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcCr, uint32 srcindexCr, IVDPixmapGen *srcY, uint32 srcindexY, IVDPixmapGen *srcCb, uint32 srcindexCb) {
		mpSrcY = srcY;
		mSrcIndexY = srcindexY;
		mpSrcCb = srcCb;
		mSrcIndexCb = srcindexCb;
		mpSrcCr = srcCr;
		mSrcIndexCr = srcindexCr;
		mWidth = srcY->GetWidth(srcindexY);
		mHeight = srcY->GetHeight(srcindexY);

		srcY->AddWindowRequest(0, 0);
		srcCb->AddWindowRequest(0, 0);
		srcCr->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrcY->Start();
		mpSrcCb->Start();
		mpSrcCr->Start();

		StartWindow(mWidth * 4);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrcY->TransformPixmapInfo(src,unused);
		mpSrcCb->TransformPixmapInfo(src,unused);
		mpSrcCr->TransformPixmapInfo(src,unused);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrcY->GetType(mSrcIndexY) & ~kVDPixType_Mask) | kVDPixType_8888;
	}

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrcY;
		if(index==1) return mpSrcCb;
		if(index==2) return mpSrcCr;
		return 0; 
	}

	virtual const char* dump_name(){ return "B8x3_To_X8R8G8B8"; }

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 *dst = (uint8 *)dst0;
		const uint8 *srcY = (const uint8 *)mpSrcY->GetRow(y, mSrcIndexY);
		const uint8 *srcCb = (const uint8 *)mpSrcCb->GetRow(y, mSrcIndexCb);
		const uint8 *srcCr = (const uint8 *)mpSrcCr->GetRow(y, mSrcIndexCr);

		for(sint32 x=0; x<mWidth; ++x) {
			uint8 y = *srcY++;
			uint8 cb = *srcCb++;
			uint8 cr = *srcCr++;

			dst[0] = cb;
			dst[1] = y;
			dst[2] = cr;
			dst[3] = 255;
			dst += 4;
		}
	}

	IVDPixmapGen *mpSrcY;
	uint32 mSrcIndexY;
	IVDPixmapGen *mpSrcCb;
	uint32 mSrcIndexCb;
	IVDPixmapGen *mpSrcCr;
	uint32 mSrcIndexCr;
};

class VDPixmapGen_B16x3_To_Y416 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcCr, uint32 srcindexCr, IVDPixmapGen *srcY, uint32 srcindexY, IVDPixmapGen *srcCb, uint32 srcindexCb) {
		mpSrcY = srcY;
		mSrcIndexY = srcindexY;
		mpSrcCb = srcCb;
		mSrcIndexCb = srcindexCb;
		mpSrcCr = srcCr;
		mSrcIndexCr = srcindexCr;
		mWidth = srcY->GetWidth(srcindexY);
		mHeight = srcY->GetHeight(srcindexY);

		srcY->AddWindowRequest(0, 0);
		srcCb->AddWindowRequest(0, 0);
		srcCr->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrcY->Start();
		mpSrcCb->Start();
		mpSrcCr->Start();

		StartWindow(mWidth * 8);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrcY->TransformPixmapInfo(src,dst);
		mpSrcCb->TransformPixmapInfo(src,unused);
		mpSrcCr->TransformPixmapInfo(src,unused);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrcY->GetType(mSrcIndexY) & ~kVDPixType_Mask) | kVDPixType_16x4_LE;
	}

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrcY;
		if(index==1) return mpSrcCb;
		if(index==2) return mpSrcCr;
		return 0; 
	}

	virtual const char* dump_name(){ return "B16x3_To_Y416"; }

protected:
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint16 *srcY = (const uint16 *)mpSrcY->GetRow(y, mSrcIndexY);
		const uint16 *srcCb = (const uint16 *)mpSrcCb->GetRow(y, mSrcIndexCb);
		const uint16 *srcCr = (const uint16 *)mpSrcCr->GetRow(y, mSrcIndexCr);

		for(sint32 x=0; x<mWidth; ++x) {
			uint16 y = *srcY++;
			uint16 cb = *srcCb++;
			uint16 cr = *srcCr++;

			dst[0] = cb;
			dst[1] = y;
			dst[2] = cr;
			dst[3] = 0;
			dst += 4;
		}
	}

	IVDPixmapGen *mpSrcY;
	uint32 mSrcIndexY;
	IVDPixmapGen *mpSrcCb;
	uint32 mSrcIndexCb;
	IVDPixmapGen *mpSrcCr;
	uint32 mSrcIndexCr;
};

class VDPixmapGen_X8R8G8B8_To_A8R8G8B8 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *src, uint32 srcindex, IVDPixmapGen *srcA, uint32 srcindexA) {
		mpSrc = src;
		mSrcIndex = srcindex;
		mpSrcA = srcA;
		mSrcIndexA = srcindexA;
		mWidth = src->GetWidth(srcindex);
		mHeight = src->GetHeight(srcindex);

		src->AddWindowRequest(0, 0);
		srcA->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrc->Start();
		mpSrcA->Start();

		StartWindow(mWidth * 4);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,dst);
		mpSrcA->TransformPixmapInfo(src,buf);
		dst.copy_alpha(src);
	}

	uint32 GetType(uint32 output) const {
		return mpSrc->GetType(mSrcIndex);
	}

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrc;
		if(index==1) return mpSrcA;
		return 0; 
	}

	virtual const char* dump_name(){ return "X8R8G8B8_To_A8R8G8B8"; }

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 *dst = (uint8 *)dst0;
		const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
		const uint8 *srcA = (const uint8 *)mpSrcA->GetRow(y, mSrcIndexA);

		for(sint32 x=0; x<mWidth; ++x) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = srcA[0];
			src += 4;
			srcA++;
			dst += 4;
		}
	}

	IVDPixmapGen *mpSrc;
	uint32 mSrcIndex;
	IVDPixmapGen *mpSrcA;
	uint32 mSrcIndexA;
};

class VDPixmapGen_X16R16G16B16_To_A16R16G16B16 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *src, uint32 srcindex, IVDPixmapGen *srcA, uint32 srcindexA) {
		mpSrc = src;
		mSrcIndex = srcindex;
		mpSrcA = srcA;
		mSrcIndexA = srcindexA;
		mWidth = src->GetWidth(srcindex);
		mHeight = src->GetHeight(srcindex);

		src->AddWindowRequest(0, 0);
		srcA->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrc->Start();
		mpSrcA->Start();

		StartWindow(mWidth * 8);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo buf;
		mpSrc->TransformPixmapInfo(src,dst);
		mpSrcA->TransformPixmapInfo(src,buf);
		dst.copy_alpha(src);
		dst.ref_a = buf.ref_a;
	}

	uint32 GetType(uint32 output) const {
		return mpSrc->GetType(mSrcIndex);
	}

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrc;
		if(index==1) return mpSrcA;
		return 0; 
	}

	virtual const char* dump_name(){ return "X16R16G16B16_To_A16R16G16B16"; }

protected:
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
		const uint16 *srcA = (const uint16 *)mpSrcA->GetRow(y, mSrcIndexA);

		for(sint32 x=0; x<mWidth; ++x) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = srcA[0];
			src += 4;
			srcA++;
			dst += 4;
		}
	}

	IVDPixmapGen *mpSrc;
	uint32 mSrcIndex;
	IVDPixmapGen *mpSrcA;
	uint32 mSrcIndexA;
};

class VDPixmapGen_B16x2_To_B16R16 : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcCb, uint32 srcindexCb, IVDPixmapGen *srcCr, uint32 srcindexCr) {
		mpSrcCb = srcCb;
		mSrcIndexCb = srcindexCb;
		mpSrcCr = srcCr;
		mSrcIndexCr = srcindexCr;
		mWidth = srcCb->GetWidth(srcindexCb);
		mHeight = srcCb->GetHeight(srcindexCb);

		srcCb->AddWindowRequest(0, 0);
		srcCr->AddWindowRequest(0, 0);
	}

	void Start() {
		mpSrcCb->Start();
		mpSrcCr->Start();

		StartWindow(mWidth * 4);
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		FilterModPixmapInfo unused;
		mpSrcCb->TransformPixmapInfo(src,dst);
		mpSrcCr->TransformPixmapInfo(src,unused);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrcCb->GetType(mSrcIndexCb) & ~kVDPixType_Mask) | kVDPixType_16x2_LE;
	}

	virtual IVDPixmapGen* dump_src(int index){
		if(index==0) return mpSrcCb;
		if(index==1) return mpSrcCr;
		return 0; 
	}

	virtual const char* dump_name(){ return "B16x2_To_B16R16"; }

protected:
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint16 *srcCb = (const uint16 *)mpSrcCb->GetRow(y, mSrcIndexCb);
		const uint16 *srcCr = (const uint16 *)mpSrcCr->GetRow(y, mSrcIndexCr);

		for(sint32 x=0; x<mWidth; ++x) {
			uint16 cb = *srcCb++;
			uint16 cr = *srcCr++;

			dst[0] = cb;
			dst[1] = cr;
			dst += 2;
		}
	}

	IVDPixmapGen *mpSrcCb;
	uint32 mSrcIndexCb;
	IVDPixmapGen *mpSrcCr;
	uint32 mSrcIndexCr;
};

#endif

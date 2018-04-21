#ifndef f_VD2_KASUMI_UBERBLIT_BASE_H
#define f_VD2_KASUMI_UBERBLIT_BASE_H

#include <vd2/system/vdstl.h>
#include "uberblit.h"

class VDPixmapGenWindowBased : public IVDPixmapGen {
public:
	VDPixmapGenWindowBased()
		: mWindowMinDY(0xffff)
		, mWindowMaxDY(-0xffff) {}

	void SetOutputSize(sint32 w, sint32 h) {
		mWidth = w;
		mHeight = h;
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
	}

	void AddWindowRequest(int minDY, int maxDY) {
		if (mWindowMinDY > minDY)
			mWindowMinDY = minDY;
		if (mWindowMaxDY < maxDY)
			mWindowMaxDY = maxDY;
	}

	void StartWindow(uint32 rowbytes, int outputCount = 1) {
		VDASSERT(mWindowMaxDY >= mWindowMinDY);
		mWindowSize = mWindowMaxDY + 1 - mWindowMinDY;

		mWindowPitch = (rowbytes + 15) & ~15;
		mWindowBuffer.resize(mWindowPitch * mWindowSize * outputCount + 15);
		mWindow.resize(mWindowSize * 2);

		size_t buf_base = size_t(mWindowBuffer.data() + 15) & ~15; 
		for(sint32 i=0; i<mWindowSize; ++i)
			mWindow[i] = mWindow[i + mWindowSize] = (uint8*)buf_base + (mWindowPitch * outputCount * i);

		mWindowIndex = 0;
		mWindowLastY = -0x3FFFFFFF;
	}

	sint32 GetWidth(int) const { return mWidth; }
	sint32 GetHeight(int) const { return mHeight; }

	bool IsStateful() const {
		return true;
	}

	const void *GetRow(sint32 y, uint32 index) {
		sint32 tostep = y - mWindowLastY;
		VDASSERT(y >= mWindowLastY - (sint32)mWindowSize + 1);

		if (tostep >= mWindowSize) {
			mWindowLastY = y - 1;
			tostep = 1;
		}

		while(tostep-- > 0) {
			++mWindowLastY;
			Compute(mWindow[mWindowIndex], mWindowLastY);
			if (++mWindowIndex >= mWindowSize)
				mWindowIndex = 0;
		}

		return mWindow[y + mWindowSize - 1 - mWindowLastY + mWindowIndex];
	}

	void ProcessRow(void *dst, sint32 y) {
		Compute(dst, y);
	}

protected:
	virtual void Compute(void *dst0, sint32 y) = 0;

	vdfastvector<uint8> mWindowBuffer;
	vdfastvector<uint8 *> mWindow;
	sint32 mWindowPitch;
	sint32 mWindowIndex;
	sint32 mWindowMinDY;
	sint32 mWindowMaxDY;
	sint32 mWindowSize;
	sint32 mWindowLastY;
	sint32 mWidth;
	sint32 mHeight;
};

class VDPixmapGenWindowBasedOneSource : public VDPixmapGenWindowBased {
public:
	bool alpha;
	VDPixmapGenWindowBasedOneSource(){ alpha = false; }

	void InitSource(IVDPixmapGen *src, uint32 srcindex) {
		mpSrc = src;
		mSrcIndex = srcindex;
		mSrcWidth = src->GetWidth(srcindex);
		mSrcHeight = src->GetHeight(srcindex);
		mWidth = mSrcWidth;
		mHeight = mSrcHeight;
	}

	void TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
		if (alpha) {
			FilterModPixmapInfo buf;
			mpSrc->TransformPixmapInfo(src,buf);
			dst.copy_alpha(buf);
			dst.ref_a = buf.ref_a;
		} else {
			mpSrc->TransformPixmapInfo(src,dst);
		}
	}

	void AddWindowRequest(int minDY, int maxDY) {
		VDPixmapGenWindowBased::AddWindowRequest(minDY, maxDY);
		mpSrc->AddWindowRequest(minDY, maxDY);
	}

	void StartWindow(uint32 rowbytes, int outputCount = 1) {
		mpSrc->Start();

		VDPixmapGenWindowBased::StartWindow(rowbytes, outputCount);
	}

	uint32 GetType(uint32 output) const {
		return mpSrc->GetType(mSrcIndex);
	}

	virtual IVDPixmapGen* dump_src(int index){ if(index==0) return mpSrc; return 0; }

protected:
	virtual void Compute(void *dst0, sint32 y) = 0;

	IVDPixmapGen *mpSrc;
	uint32 mSrcIndex;
	sint32 mSrcWidth;
	sint32 mSrcHeight;
};

class VDPixmapGenWindowBasedOneSourceSimple : public VDPixmapGenWindowBasedOneSource {
public:
	void Init(IVDPixmapGen *src, uint32 srcindex) {
		InitSource(src, srcindex);

		src->AddWindowRequest(0, 0);
	}
};

class VDPixmapGenWindowBasedOneSourceAlign16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
  int bpp;

	void Start() {
		int type = mpSrc->GetType(0);
		bpp = 2;
		if ((type & kVDPixType_Mask)==kVDPixType_16x2_LE) bpp = 4;
		if ((type & kVDPixType_Mask)==kVDPixType_16x4_LE) bpp = 8;
		if ((type & kVDPixType_Mask)==kVDPixType_YU64) bpp = 4;
		StartWindow(mWidth * bpp);
	}
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
		int w = mWidth*bpp/2;
		int w0 = ComputeSpan(dst,src,w);
		if (w0<w) {
			src += w0;
			dst += w0;
			w -= w0;
			uint16 src1[8];
			uint16 dst1[8];
			memcpy(src1,src,w*2);
			ComputeSpan(dst1,src1,8);
			memcpy(dst,dst1,w*2);
		}
	}
	virtual int ComputeSpan(uint16* dst, const uint16* src, int n) = 0;
};

class VDPixmapGenWindowBasedOneSourceAlign8to16 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth * 2);
	}
	void Compute(void *dst0, sint32 y) {
		uint16 *dst = (uint16 *)dst0;
		const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
		int w = mWidth;
		int w0 = ComputeSpan(dst,src,w);
		if (w0<w) {
			src += w0;
			dst += w0;
			w -= w0;
			uint8 src1[16];
			uint16 dst1[16];
			memcpy(src1,src,w);
			ComputeSpan(dst1,src1,16);
			memcpy(dst,dst1,w*2);
		}
	}
	virtual int ComputeSpan(uint16* dst, const uint8* src, int n) = 0;
};

class VDPixmapGenWindowBasedOneSourceAlign16to8 : public VDPixmapGenWindowBasedOneSourceSimple {
public:
	void Start() {
		StartWindow(mWidth);
	}
	void Compute(void *dst0, sint32 y) {
		uint8 *dst = (uint8 *)dst0;
		const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
		int w = mWidth;
		int w0 = ComputeSpan(dst,src,w);
		if (w0<w) {
			src += w0;
			dst += w0;
			w -= w0;
			uint16 src1[16];
			uint8 dst1[16];
			memcpy(src1,src,w*2);
			ComputeSpan(dst1,src1,16);
			memcpy(dst,dst1,w);
		}
	}
	virtual int ComputeSpan(uint8* dst, const uint16* src, int n) = 0;
};

class VDPixmapUberBlitterGenerator;

class IVDPixmapExtraGen {
public:
	virtual void Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst) = 0;
};

#endif

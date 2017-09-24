#include "stdafx.h"
#include <vd2/system/vdstl.h>
#include <vd2/system/memory.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include "capfilter.h"
#include "filters.h"
#include "FilterFrameManualSource.h"

static const char g_szCannotFilter[]="Cannot use video filtering: ";

bool VDPreferencesGetFilterAccelEnabled();

///////////////////////////////////////////////////////////////////////////
//
//	filters
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilter : public vdlist<VDCaptureFilter>::node {
public:
	virtual bool Run(VDPixmap& px) = 0;
	virtual bool ProcessOut(VDPixmap& px) { return false; }
	virtual void Shutdown() {}
};

///////////////////////////////////////////////////////////////////////////
//
//	filter: crop
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilterCrop : public VDCaptureFilter {
public:
	void Init(VDPixmapLayout& layout, uint32 x1, uint32 y1, uint32 x2, uint32 y2);
	bool Run(VDPixmap& px);

protected:
	VDPixmapLayout mLayout;
};

void VDCaptureFilterCrop::Init(VDPixmapLayout& layout, uint32 x1, uint32 y1, uint32 x2, uint32 y2) {
	const VDPixmapFormatInfo& format = VDPixmapGetInfo(layout.format);

	x1 -= x1 % (format.qw << format.auxwbits);
	y1 = (y1 >> (format.qhbits + format.auxhbits)) << (format.qhbits + format.auxhbits);
	x2 -= x2 % (format.qw << format.auxwbits);
	y2 = (y2 >> (format.qhbits + format.auxhbits)) << (format.qhbits + format.auxhbits);

	mLayout = VDPixmapLayoutOffset(layout, x1, y1);
	mLayout.w -= (x1+x2);
	mLayout.h -= (y1+y2);

	mLayout.data -= layout.data;
	mLayout.data2 -= layout.data;
	mLayout.data3 -= layout.data;

	layout = mLayout;
}

bool VDCaptureFilterCrop::Run(VDPixmap& px) {
	px = VDPixmapFromLayout(mLayout, px.data);
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	filter: noise reduction
//
///////////////////////////////////////////////////////////////////////////

#ifndef _M_AMD64
namespace {
	void __declspec(naked) dodnrMMX(uint32 *dst, uint32 *src, vdpixsize w, vdpixsize h, vdpixoffset dstmodulo, vdpixoffset srcmodulo, __int64 thresh1, __int64 thresh2) {
	static const __int64 bythree = 0x5555555555555555i64;
	static const __int64 round2 = 0x0002000200020002i64;
	static const __int64 three = 0x0003000300030003i64;

		__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx

			mov		edi,[esp+4+16]
			mov		esi,[esp+8+16]
			mov		edx,[esp+12+16]
			mov		ecx,[esp+16+16]
			mov		ebx,[esp+20+16]
			mov		eax,[esp+24+16]
			movq	mm6,[esp+36+16]
			movq	mm5,[esp+28+16]

	yloop:
			mov		ebp,edx
	xloop:
			movd	mm0,[esi]		;previous
			pxor	mm7,mm7

			movd	mm1,[edi]		;current
			punpcklbw	mm0,mm7

			punpcklbw	mm1,mm7
			movq	mm2,mm0

			movq	mm4,mm1
			movq	mm3,mm1

			movq	mm7,mm0
			paddw	mm4,mm4

			pmullw	mm0,three
			psubusb	mm2,mm1

			paddw	mm4,mm7
			psubusb	mm3,mm7

			pmulhw	mm4,bythree
			por		mm2,mm3

			movq	mm3,mm2
			paddw	mm0,mm1

			paddw	mm0,round2
			pcmpgtw	mm2,mm5			;set if diff > thresh1

			pcmpgtw	mm3,mm6			;set if diff > thresh2
			psrlw	mm0,2


			;	mm2		mm3		meaning						mm1		mm0		mm4
			;	FALSE	FALSE	diff <= thresh1				off		on		off
			;	FALSE	TRUE	impossible
			;	TRUE	FALSE	thresh1 < diff <= thresh2	off		off		on
			;	TRUE	TRUE	diff > thresh2				on		off		off

			pand	mm1,mm3			;keep pixels exceeding threshold2
			pand	mm4,mm2			;	average pixels <= threshold2...
			pandn	mm2,mm0			;replace pixels below threshold1
			pandn	mm3,mm4			;	but >= threshold1...
			por		mm1,mm2
			add		esi,4
			por		mm1,mm3
			add		edi,4
			packuswb	mm1,mm1
			dec		ebp

			movd	[esi-4],mm1		;store to both
			movd	[edi-4],mm1
			jne		xloop

			add		esi,eax
			add		edi,ebx
			dec		ecx
			jne		yloop

			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			emms
			ret
		}
	}
}
#else
	void dodnrMMX(uint32 *dst, uint32 *src, vdpixsize w, vdpixsize h, vdpixoffset dstmodulo, vdpixoffset srcmodulo, __int64 thresh1, __int64 thresh2) {
#pragma vdpragma_TODO("need AMD64 implementation of NR filter")
	}

#endif

class VDCaptureFilterNoiseReduction : public VDCaptureFilter {
public:
	void SetThreshold(int threshold);
	void Init(VDPixmapLayout& layout);
	bool Run(VDPixmap& px);

protected:
	vdblock<uint32, vdaligned_alloc<uint32> > mBuffer;

	sint64	mThresh1;
	sint64	mThresh2;
};

void VDCaptureFilterNoiseReduction::SetThreshold(int threshold) {
	mThresh1 = 0x0001000100010001i64*((threshold>>1)+1);
	mThresh2 = 0x0001000100010001i64*(threshold);

	if (!threshold)
		mThresh1 = mThresh2;
}

void VDCaptureFilterNoiseReduction::Init(VDPixmapLayout& layout) {
	const VDPixmapFormatInfo& format = VDPixmapGetInfo(layout.format);

	uint32 rowdwords = ((((layout.w + format.qw - 1) / format.qw) * format.qsize) >> 2);
	uint32 h = -(-layout.h >> format.qhbits);

	mBuffer.resize(rowdwords * h);
}


bool VDCaptureFilterNoiseReduction::Run(VDPixmap& px) {
	unsigned w;

	switch(px.format) {
		case nsVDPixmap::kPixFormat_RGB888:
			w = (px.w*3+3)>>2;
			dodnrMMX((uint32 *)px.data, mBuffer.data(), w, px.h, px.pitch - (w<<2), 0, mThresh1, mThresh2);
			break;
		case nsVDPixmap::kPixFormat_XRGB8888:
			w = px.w;
			dodnrMMX((uint32 *)px.data, mBuffer.data(), px.w, px.h, px.pitch - (px.w<<2), 0, mThresh1, mThresh2);
			break;
		case nsVDPixmap::kPixFormat_YUV422_UYVY:
		case nsVDPixmap::kPixFormat_YUV422_YUYV:
			w = (px.w*2+3)>>2;
			dodnrMMX((uint32 *)px.data, mBuffer.data(), w, px.h, px.pitch - (w<<2), 0, mThresh1, mThresh2);
			break;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	filter: luma squish
//
///////////////////////////////////////////////////////////////////////////

#ifndef _M_AMD64
namespace {
	// Squish 0...255 range to 16...235.
	//
	// The bIsUYVY value MUST be either 0 (false) or 1 (true) ....

	static void __declspec(naked) __cdecl lumasquish_MMX(void *dst, ptrdiff_t pitch, long w2, long h, int mode) {
		static const __int64 scalers[12] = {
			// YUY2					UYVY					Packed
			0x40003ba240003ba2i64,	0x3ba240003ba24000i64,	0x3ba23ba23ba23ba2i64,	// black only (0->16, 235->235)
			0x40003aa540003aa5i64,	0x3aa540003aa54000i64,	0x3aa53aa53aa53aa5i64,	// white only (16->16, 255->235)
			0x400036f7400036f7i64,	0x36f7400036f74000i64,	0x36f736f736f736f7i64,	// black & white
		};
		static const __int64 biases[12] = {
			// YUY2					UYVY					Packed
			0x0000004700000047i64,	0x0047000000470000i64,	0x0047004700470047i64,	// black only (16.5 / ratio)
			0x0000000800000008i64,	0x0000000800000008i64,	0x0008000800080008i64,	// white only (16.5 / ratio - 16*prescale)
			0x0000004d0000004di64,	0x004d0000004d0000i64,	0x004d004d004d004di64,	// black & white
		};

		__asm {
			push		ebp
			push		edi
			push		esi
			push		ebx

			mov			eax,[esp+20+16]

			movq		mm6,qword ptr [scalers+eax*8]
			movq		mm5,qword ptr [biases+eax*8]

			mov			ecx,[esp+12+16]
			mov			esi,[esp+4+16]
			mov			ebx,[esp+16+16]
			mov			eax,[esp+8+16]
			mov			edx,ecx
			shl			edx,2
			sub			eax,edx

	yloop:
			mov			edx,ecx
			test		esi,4
			jz			xloop_aligned_start

			movd		mm0,[esi]
			pxor		mm7,mm7
			punpcklbw	mm0,mm7
			add			esi,4
			psllw		mm0,2
			dec			edx
			paddw		mm0,mm5
			pmulhw		mm0,mm6
			packuswb	mm0,mm0
			movd		[esi-4],mm0
			jz			xloop_done

	xloop_aligned_start:
			sub			edx,3
			jbe			xloop_done
	xloop_aligned:
			movq		mm0,[esi]
			pxor		mm7,mm7

			movq		mm2,[esi+8]
			movq		mm1,mm0

			punpcklbw	mm0,mm7
			movq		mm3,mm2

			psllw		mm0,2
			add			esi,16

			paddw		mm0,mm5
			punpckhbw	mm1,mm7

			psllw		mm1,2
			pmulhw		mm0,mm6

			paddw		mm1,mm5
			punpcklbw	mm2,mm7

			pmulhw		mm1,mm6
			psllw		mm2,2

			punpckhbw	mm3,mm7
			paddw		mm2,mm5

			psllw		mm3,2
			pmulhw		mm2,mm6

			paddw		mm3,mm5
			packuswb	mm0,mm1

			pmulhw		mm3,mm6
			sub			edx,4

			movq		[esi-16],mm0

			packuswb	mm2,mm3

			movq		[esi-8],mm2
			ja			xloop_aligned

			add			edx,3
			jz			xloop_done

	xloop_tail:
			movd		mm0,[esi]
			pxor		mm7,mm7
			punpcklbw	mm0,mm7
			add			esi,4
			psllw		mm0,2
			dec			edx
			paddw		mm0,mm5
			pmulhw		mm0,mm6
			packuswb	mm0,mm0
			movd		[esi-4],mm0
			jne			xloop_tail

	xloop_done:
			add			esi,eax

			dec			ebx
			jne			yloop

			pop			ebx
			pop			esi
			pop			edi
			pop			ebp
			emms
			ret
		}
	}
}
#else
namespace {
	void lumasquish_MMX(void *dst, ptrdiff_t pitch, long w2, long h, int bIsUYVY) {
#pragma vdpragma_TODO("need AMD64 implementation of lumasquish filter")
	}
}
#endif

class VDCaptureFilterLumaSquish : public VDCaptureFilter {
public:
	VDCaptureFilterLumaSquish();

	void SetBounds(bool black, bool white);
	void Init(VDPixmapLayout& layout);
	bool Run(VDPixmap& px);

protected:
	int mBaseMode;
	int	mMode;
};

VDCaptureFilterLumaSquish::VDCaptureFilterLumaSquish()
	: mBaseMode(-1)
	, mMode(-1)
{
}

void VDCaptureFilterLumaSquish::SetBounds(bool black, bool white) {
	mMode = -1;

	if (!black && !white)
		return;

	mMode = -3;

	if (black)
		mMode += 3;

	if (white)
		mMode += 6;
}

void VDCaptureFilterLumaSquish::Init(VDPixmapLayout& layout) {
	mBaseMode = -1;

	switch(layout.format) {
	case nsVDPixmap::kPixFormat_YUV422_YUYV:
		mBaseMode = 0;
		break;
	case nsVDPixmap::kPixFormat_YUV422_UYVY:
		mBaseMode = 1;
		break;
	case nsVDPixmap::kPixFormat_YUV444_Planar:
	case nsVDPixmap::kPixFormat_YUV422_Planar:
	case nsVDPixmap::kPixFormat_YUV420_Planar:
	case nsVDPixmap::kPixFormat_YUV411_Planar:
	case nsVDPixmap::kPixFormat_YUV410_Planar:
		mBaseMode = 2;
		break;
	default:
		return;
	}
}

bool VDCaptureFilterLumaSquish::Run(VDPixmap& px) {
	if ((mMode|mBaseMode) >= 0) {
		if (px.format == nsVDPixmap::kPixFormat_YUV422_YUYV || px.format == nsVDPixmap::kPixFormat_YUV422_UYVY)
			lumasquish_MMX(px.data, px.pitch, (px.w+1)>>1, px.h, mMode + mBaseMode);
		else
			lumasquish_MMX(px.data, px.pitch, (px.w+3)>>2, px.h, mMode + mBaseMode);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	filter: swap fields
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilterSwapFields : public VDCaptureFilter {
public:
	void Init(VDPixmapLayout& layout);
	bool Run(VDPixmap& px);

	static void SwapPlaneRows(void *p, ptrdiff_t pitch, unsigned w, unsigned h);

protected:
	uint32 mBytesPerRow;
};

void VDCaptureFilterSwapFields::Init(VDPixmapLayout& layout) {
	const VDPixmapFormatInfo& format = VDPixmapGetInfo(layout.format);

	mBytesPerRow = ((layout.w + format.qw - 1) / format.qw) * format.qsize;
}

bool VDCaptureFilterSwapFields::Run(VDPixmap& px) {
	const VDPixmapFormatInfo& format = VDPixmapGetInfo(px.format);

	// swap plane 0 rows
	if (!format.qhbits)
		SwapPlaneRows(px.data, px.pitch, mBytesPerRow, px.h);

	// swap plane 1 and 2 rows
	if (format.auxbufs && !format.auxhbits) {
		unsigned auxw = -(-px.w >> format.auxwbits);

		SwapPlaneRows(px.data2, px.pitch2, auxw, px.h);

		if (format.auxbufs >= 2)
			SwapPlaneRows(px.data3, px.pitch3, auxw, px.h);
	}

	return true;
}

void VDCaptureFilterSwapFields::SwapPlaneRows(void *p1, ptrdiff_t pitch, unsigned w, unsigned h) {
	const ptrdiff_t pitch2x = pitch + pitch;
	void *p2 = vdptroffset(p1, pitch);

	for(vdpixsize y=0; y<h-1; y+=2) {
		VDSwapMemory(p1, p2, w);
		vdptrstep(p1, pitch2x);
		vdptrstep(p2, pitch2x);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	filter: vert squash
//
///////////////////////////////////////////////////////////////////////////

extern "C" long resize_table_col_by2linear_MMX(Pixel *out, Pixel **in_table, vdpixsize w);
extern "C" long resize_table_col_by2cubic_MMX(Pixel *out, Pixel **in_table, vdpixsize w);

class VDCaptureFilterVertSquash : public VDCaptureFilter {
public:
	void SetMode(IVDCaptureFilterSystem::FilterMode mode);
	void Init(VDPixmapLayout& layout);
	bool Run(VDPixmap& px);
	void Shutdown();

protected:
	vdblock<uint32, vdaligned_alloc<uint32> >	mRowBuffers;
	uint32	mDwordsPerRow;
	IVDCaptureFilterSystem::FilterMode	mMode;
};

void VDCaptureFilterVertSquash::SetMode(IVDCaptureFilterSystem::FilterMode mode) {
	mMode = mode;
}

void VDCaptureFilterVertSquash::Init(VDPixmapLayout& layout) {
	const VDPixmapFormatInfo& info = VDPixmapGetInfo(layout.format);

	mDwordsPerRow = (((layout.w + info.qw - 1) / info.qw) * info.qsize) >> 2;

	if (mMode == IVDCaptureFilterSystem::kFilterCubic)
		mRowBuffers.resize(mDwordsPerRow * 3);

	layout.h >>= 1;
}

bool VDCaptureFilterVertSquash::Run(VDPixmap& px) {
#ifndef _M_AMD64
	Pixel *srclimit = vdptroffset((Pixel *)px.data, px.pitch * (px.h - 1));
	Pixel *dst = (Pixel *)px.data;
	uint32 y = px.h >> 1;
	Pixel *src[8];

	switch(mMode) {
	case IVDCaptureFilterSystem::kFilterCubic:
		memcpy(mRowBuffers.data() + mDwordsPerRow*0, (char *)px.data + px.pitch*0, mDwordsPerRow*4);
		memcpy(mRowBuffers.data() + mDwordsPerRow*1, (char *)px.data + px.pitch*1, mDwordsPerRow*4);
		memcpy(mRowBuffers.data() + mDwordsPerRow*2, (char *)px.data + px.pitch*2, mDwordsPerRow*4);

		src[0] = (Pixel *)mRowBuffers.data();
		src[1] = src[0] + mDwordsPerRow*1;
		src[2] = src[1] + mDwordsPerRow*2;

		src[3] = vdptroffset(dst, 3*px.pitch);
		src[4] = vdptroffset(src[3], px.pitch);
		src[5] = vdptroffset(src[4], px.pitch);
		src[6] = vdptroffset(src[5], px.pitch);
		src[7] = vdptroffset(src[6], px.pitch);

		while(y--) {
			resize_table_col_by2cubic_MMX((Pixel *)dst, (Pixel **)src, mDwordsPerRow);

			vdptrstep(dst, px.pitch);
			src[0] = src[2];
			src[1] = src[3];
			src[2] = src[4];
			src[3] = src[5];
			src[4] = src[6];
			src[5] = src[7];
			vdptrstep(src[6], px.pitch*2);
			vdptrstep(src[7], px.pitch*2);

			if (px.pitch < 0) {
				if (src[6] <= srclimit)
					src[6] = srclimit;
				if (src[7] <= srclimit)
					src[7] = srclimit;
			} else {
				if (src[6] >= srclimit)
					src[6] = srclimit;
				if (src[7] >= srclimit)
					src[7] = srclimit;
			}
		}
		__asm emms
		break;

	case IVDCaptureFilterSystem::kFilterLinear:
		src[1] = (Pixel *)px.data;
		src[2] = src[1];
		src[3] = vdptroffset(src[2], px.pitch);

		while(y--) {
			resize_table_col_by2linear_MMX((Pixel *)dst, (Pixel **)src, mDwordsPerRow);

			vdptrstep(dst, px.pitch);
			src[1] = src[3];
			vdptrstep(src[2], px.pitch*2);
			vdptrstep(src[3], px.pitch*2);

			if (px.pitch < 0) {
				if (src[2] <= srclimit)
					src[2] = srclimit;
				if (src[3] <= srclimit)
					src[3] = srclimit;
			} else {
				if (src[2] >= srclimit)
					src[2] = srclimit;
				if (src[3] >= srclimit)
					src[3] = srclimit;
			}
		}
		__asm emms
		break;

	}
#else
#pragma vdpragma_TODO("need AMD64 blah blah blah")
#endif

	px.h >>= 1;
	return true;
}

void VDCaptureFilterVertSquash::Shutdown() {
	mRowBuffers.clear();
}


///////////////////////////////////////////////////////////////////////////
//
//	filter: ChainAdapter
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilterChainFrameSource : public VDFilterFrameManualSource {
	VDCaptureFilterChainFrameSource(const VDCaptureFilterChainFrameSource&);
	VDCaptureFilterChainFrameSource& operator=(const VDCaptureFilterChainFrameSource&);
public:
	VDCaptureFilterChainFrameSource();
	~VDCaptureFilterChainFrameSource();

	void Init(uint32 frameCount, const VDPixmapLayout& layout);
	void PreallocateFrames();
	void Shutdown();

	void Push(const VDPixmap& px);
	bool Run();
	
protected:
	VDPosition	mWindowFrameStart;
	uint32		mWindowFrameCount;
	uint32		mWindowStartIndex;
	uint32		mWindowSize;

	typedef vdfastvector<VDFilterFrameBuffer *> FrameQueue;
	FrameQueue mFrameQueue;

	vdautoptr<IVDPixmapBlitter> mpBlitter;
};

VDCaptureFilterChainFrameSource::VDCaptureFilterChainFrameSource() {
}

VDCaptureFilterChainFrameSource::~VDCaptureFilterChainFrameSource() {
	Shutdown();
}

void VDCaptureFilterChainFrameSource::Init(uint32 frameCount, const VDPixmapLayout& layout) {
	VDASSERT(mFrameQueue.empty());

	mWindowFrameStart = 0;
	mWindowFrameCount = 0;
	mWindowStartIndex = 0;
	mWindowSize = frameCount;
	
	SetOutputLayout(layout);

	mFrameQueue.resize(frameCount, NULL);
}

void VDCaptureFilterChainFrameSource::PreallocateFrames() {
	for(uint32 i=0; i<mWindowSize; ++i)
		mAllocator.Allocate(&mFrameQueue[i]);
}

void VDCaptureFilterChainFrameSource::Shutdown() {
	while(!mFrameQueue.empty()) {
		VDFilterFrameBuffer *buf = mFrameQueue.back();
		if (buf)
			buf->Release();

		mFrameQueue.pop_back();
	}

	mpBlitter = NULL;
}

void VDCaptureFilterChainFrameSource::Push(const VDPixmap& px) {
	while(mWindowFrameCount >= mWindowSize) {
		++mWindowFrameStart;

		if (++mWindowStartIndex >= mWindowSize)
			mWindowStartIndex = 0;

		--mWindowFrameCount;
	}

	int index = (mWindowStartIndex + mWindowFrameCount) % mWindowSize;

	VDFilterFrameBuffer *buf = mFrameQueue[index];

	const VDPixmap pxdst(VDPixmapFromLayout(mLayout, buf->LockWrite()));

	if (!mpBlitter)
		mpBlitter = VDPixmapCreateBlitter(pxdst, px);

	mpBlitter->Blit(pxdst, px);
	buf->Unlock();
	++mWindowFrameCount;
}

bool VDCaptureFilterChainFrameSource::Run() {
	if (!mWindowFrameCount)
		return false;

	bool activity = false;

	VDPosition frame;
	vdrefptr<VDFilterFrameRequest> req;
	while(PeekNextRequestFrame(frame)) {
		uint32 frameWindowOffset = 0;

		if (frame < mWindowFrameStart)
			frameWindowOffset = 0;
		else {
			VDPosition frameOffset = frame - mWindowFrameStart;

			if (frameOffset >= mWindowFrameCount)
				return false;
			else
				frameWindowOffset = (uint32)frameOffset;
		}

		uint32 frameWindowIndex = frameWindowOffset + mWindowStartIndex;
		if (frameWindowIndex >= mWindowSize)
			frameWindowIndex -= mWindowSize;

		VDVERIFY(GetNextRequest(NULL, ~req));
		req->SetResultBuffer(mFrameQueue[frameWindowIndex]);
		req->MarkComplete(true);
		CompleteRequest(req, false);
		activity = true;
	}

	return activity;
}

class VDCaptureFilterChainAdapter : public VDCaptureFilter {
public:
	void SetFrameRate(const VDFraction& frameRate);
	VDFraction GetOutputFrameRate() const { return filters.GetOutputFrameRate(); }

	void Init(VDPixmapLayout& layout);
	bool Run(VDPixmap& px);
	bool ProcessOut(VDPixmap& px);
	void Shutdown();

protected:
	sint64		mFrame;
	double		mFrameToTimeMSFactor;
	VDFraction	mFrameRate;
	bool		mbFlushRequestNextCall;

	vdrefptr<VDCaptureFilterChainFrameSource>	mpFrameSource;
	vdrefptr<IVDFilterFrameClientRequest>	mpRequest;
};

void VDCaptureFilterChainAdapter::SetFrameRate(const VDFraction& frameRate) {
	mFrameRate = frameRate;
	mFrameToTimeMSFactor = mFrameRate.AsInverseDouble() * 1000.0;
}

void VDCaptureFilterChainAdapter::Init(VDPixmapLayout& layout) {
	filters.DeinitFilters();
	filters.prepareLinearChain(&g_filterChain, layout.w, layout.h, layout.format, mFrameRate, -1, VDFraction(0, 0));

	mpFrameSource = new VDCaptureFilterChainFrameSource;
	mpFrameSource->Init(3, filters.GetInputLayout());

	filters.SetVisualAccelDebugEnabled(false);
	filters.SetAccelEnabled(VDPreferencesGetFilterAccelEnabled());
	filters.SetAsyncThreadCount(-1);
	filters.initLinearChain(NULL, VDXFilterStateInfo::kStateRealTime, &g_filterChain, mpFrameSource, layout.w, layout.h, layout.format, layout.palette, mFrameRate, -1, VDFraction(0, 0));
	filters.ReadyFilters();

	mpFrameSource->PreallocateFrames();

	layout = filters.GetOutputLayout();

	mFrame = 0;
	mbFlushRequestNextCall = false;
}

bool VDCaptureFilterChainAdapter::Run(VDPixmap& px) {
	mpFrameSource->Push(px);
	return false;
}

bool VDCaptureFilterChainAdapter::ProcessOut(VDPixmap& px) {
	if (mbFlushRequestNextCall) {
		mbFlushRequestNextCall = false;

		if (mpRequest) {
			VDFilterFrameBuffer *buf = mpRequest->GetResultBuffer();
			if (buf)
				buf->Unlock();

			mpRequest = NULL;
		}
	}

	if (!mpRequest) {
		filters.RequestFrame(mFrame, 0, ~mpRequest);
		++mFrame;
	}

	while(!mpRequest->IsCompleted()) {
		if (mpFrameSource->Run())
			continue;

		switch(filters.Run(NULL, false)) {
			case FilterSystem::kRunResult_Idle:
				return false;
			case FilterSystem::kRunResult_Blocked:
				filters.Block();
				break;
		}
	}
	
	if (!mpRequest->IsSuccessful()) {
		VDFilterFrameRequestError *err = mpRequest->GetError();

		if (err)
			throw MyError("%s", err);
		else
			throw MyError("Unknown error occurred while running video filters.");
	}

	VDFilterFrameBuffer *buf = mpRequest->GetResultBuffer();
	px = VDPixmapFromLayout(filters.GetOutputLayout(), (void *)buf->LockRead());
	mbFlushRequestNextCall = true;
	return true;
}

void VDCaptureFilterChainAdapter::Shutdown() {
	if (mpRequest) {
		VDFilterFrameBuffer *buf = mpRequest->GetResultBuffer();
		if (buf)
			buf->Unlock();

		mpRequest = NULL;
	}

	filters.DeinitFilters();
	filters.DeallocateBuffers();
}

///////////////////////////////////////////////////////////////////////////
//
//	filter system
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilterSystem : public IVDCaptureFilterSystem {
public:
	VDCaptureFilterSystem();
	~VDCaptureFilterSystem();

	void SetCrop(uint32 x1, uint32 y1, uint32 x2, uint32 y2);
	void SetNoiseReduction(uint32 threshold);
	void SetLumaSquish(bool enableBlack, bool enableWhite);
	void SetFieldSwap(bool enable);
	void SetVertSquashMode(FilterMode mode);
	void SetChainEnable(bool enable, bool force24Bit);

	VDFraction GetOutputFrameRate();

	void Init(VDPixmapLayout& layout, const VDFraction& frameRate);
	void ProcessIn(const VDPixmap& px);
	bool ProcessOut(VDPixmap& px, void *&outputData, uint32& outputSize);
	void Shutdown();

protected:
	void RebuildFilterChain();

	VDPixmap	mLastFrameOutput;
	bool		mbLastFrameValid;

	ptrdiff_t	mBaseOffset;
	uint32		mOutputSize;
	VDFraction	mOutputFrameRate;

	uint32	mCropX1;
	uint32	mCropY1;
	uint32	mCropX2;
	uint32	mCropY2;
	uint32	mNoiseReductionThreshold;
	FilterMode	mVertSquashMode;
	bool	mbLumaSquishBlack;
	bool	mbLumaSquishWhite;
	bool	mbFieldSwap;
	bool	mbChainEnable;
	bool	mbForce24Bit;
	bool	mbCropEnable;
	bool	mbInitialized;

	VDPixmapLayout	mLinearLayout;
	VDPixmapLayout	mPreNRLayout;

	vdblock<char, vdaligned_alloc<char> > mLinearBuffer;

	typedef vdlist<VDCaptureFilter> tFilterChain;
	tFilterChain		mFilterChain;

	VDCaptureFilterCrop				mFilterCrop;
	VDCaptureFilterNoiseReduction	mFilterNoiseReduction;
	VDCaptureFilterLumaSquish		mFilterLumaSquish;
	VDCaptureFilterSwapFields		mFilterSwapFields;
	VDCaptureFilterVertSquash		mFilterVertSquash;
	VDCaptureFilterChainAdapter		mFilterChainAdapter;
};

IVDCaptureFilterSystem *VDCreateCaptureFilterSystem() {
	return new VDCaptureFilterSystem;
}

VDCaptureFilterSystem::VDCaptureFilterSystem()
	: mCropX1(0)
	, mCropY1(0)
	, mCropX2(0)
	, mCropY2(0)
	, mNoiseReductionThreshold(0)
	, mVertSquashMode(kFilterDisable)
	, mbLumaSquishBlack(false)
	, mbLumaSquishWhite(false)
	, mbFieldSwap(false)
	, mbChainEnable(false)
	, mbForce24Bit(false)
	, mbInitialized(false)
{
}

VDCaptureFilterSystem::~VDCaptureFilterSystem() {
	Shutdown();
}

void VDCaptureFilterSystem::SetCrop(uint32 x1, uint32 y1, uint32 x2, uint32 y2) {
	mCropX1 = x1;
	mCropY1 = y1;
	mCropX2 = x2;
	mCropY2 = y2;
}

void VDCaptureFilterSystem::SetNoiseReduction(uint32 threshold) {
	if (threshold == mNoiseReductionThreshold)
		return;

	// We allow this to be set on the fly, as the NR filter doesn't change the
	// bitmap layout. Note that we may have to tweak the filter chain a bit...
	bool rebuild = false;

	if (mbInitialized) {
		mFilterNoiseReduction.SetThreshold(threshold);

		// Check if we have to init or shut down the NR filter.
		if (threshold && !mNoiseReductionThreshold) {
			mFilterNoiseReduction.Init(mPreNRLayout);
			rebuild = true;
		} else if (!threshold && mNoiseReductionThreshold) {
			mFilterNoiseReduction.Shutdown();
			rebuild = true;
		}
	}

	mNoiseReductionThreshold = threshold;

	if (rebuild)
		RebuildFilterChain();
}

void VDCaptureFilterSystem::SetLumaSquish(bool enableBlack, bool enableWhite) {
	if (mbLumaSquishBlack == enableBlack && mbLumaSquishWhite == enableWhite)
		return;

	mbLumaSquishBlack = enableBlack;
	mbLumaSquishWhite = enableWhite;

	if (mbInitialized) {
		mFilterLumaSquish.SetBounds(enableBlack, enableWhite);
		mFilterLumaSquish.Init(mPreNRLayout);
		RebuildFilterChain();
	}
}

void VDCaptureFilterSystem::SetFieldSwap(bool enable) {
	if (mbFieldSwap == enable)
		return;

	mbFieldSwap = enable;

	if (mbInitialized)
		RebuildFilterChain();
}

void VDCaptureFilterSystem::SetVertSquashMode(FilterMode mode) {
	mVertSquashMode = mode;
}

void VDCaptureFilterSystem::SetChainEnable(bool enable, bool force24Bit) {
	mbChainEnable = enable;
	mbForce24Bit = force24Bit;
}

VDFraction VDCaptureFilterSystem::GetOutputFrameRate() {
	return mOutputFrameRate;
}

void VDCaptureFilterSystem::Init(VDPixmapLayout& pxl, const VDFraction& frameRate) {
	VDASSERT(!mbInitialized);
	mbInitialized = true;
	mbCropEnable = false;
	mbLastFrameValid = false;

	if (mCropX1|mCropY1|mCropX2|mCropY2) {
		mbCropEnable = true;
		mFilterCrop.Init(pxl, mCropX1, mCropY1, mCropX2, mCropY2);
	}

	mPreNRLayout = pxl;
	if (mNoiseReductionThreshold) {
		mFilterNoiseReduction.SetThreshold(mNoiseReductionThreshold);
		mFilterNoiseReduction.Init(pxl);
	}

	if (mbLumaSquishBlack || mbLumaSquishWhite) {
		mFilterLumaSquish.SetBounds(mbLumaSquishBlack, mbLumaSquishWhite);
		mFilterLumaSquish.Init(pxl);
	}

	if (mbFieldSwap) {
		mFilterSwapFields.Init(pxl);
	}

	if (mVertSquashMode) {
		mFilterVertSquash.SetMode(mVertSquashMode);
		mFilterVertSquash.Init(pxl);
	}

	mOutputFrameRate = frameRate;

	if (mbChainEnable) {
		mFilterChainAdapter.SetFrameRate(mOutputFrameRate);
		mFilterChainAdapter.Init(pxl);
		mOutputFrameRate = mFilterChainAdapter.GetOutputFrameRate();
	}

	RebuildFilterChain();

	// We need to linearize the bitmap if cropping or the filter chain is enabled.
	if (mbCropEnable || mbChainEnable) {
		int format = pxl.format;

		if (mbChainEnable && mbForce24Bit)
			format = nsVDPixmap::kPixFormat_RGB888;

		uint32 bytes = VDMakeBitmapCompatiblePixmapLayout(mLinearLayout, pxl.w, pxl.h, format, 0);

		mLinearBuffer.resize(bytes);

		pxl = mLinearLayout;
	}

	mOutputSize = VDPixmapLayoutGetMinSize(pxl);
	mBaseOffset = pxl.data;
}

void VDCaptureFilterSystem::ProcessIn(const VDPixmap& px0) {
	VDPROFILEBEGIN("V-FilterIn");
	tFilterChain::iterator it(mFilterChain.begin()), itEnd(mFilterChain.end());
	VDPixmap px(px0);

	mbLastFrameValid = false;

	for(; it!=itEnd; ++it) {
		VDCaptureFilter *pFilt = *it;

		VDAssertValidPixmap(px);
		if (!pFilt->Run(px)) {
			VDPROFILEEND();
			return;
		}
	}

	VDAssertValidPixmap(px);

	mLastFrameOutput = px;
	mbLastFrameValid = true;
	VDPROFILEEND();
}

bool VDCaptureFilterSystem::ProcessOut(VDPixmap& px, void*& outputData, uint32& outputSize) {
	if (!mbLastFrameValid) {
		if (!mFilterChain.empty()) {
			VDPROFILEBEGIN("V-FilterOut");
			mbLastFrameValid = mFilterChain.back()->ProcessOut(mLastFrameOutput);
			VDPROFILEEND();
		}

		if (!mbLastFrameValid)
			return false;
	}

	mbLastFrameValid = false;
	px = mLastFrameOutput;

	// Check if we have to linearize the bitmap.
	if (mbCropEnable || mbChainEnable) {
		VDPROFILEBEGIN("V-BlitFilterOut");
		VDPixmap pxLinear(VDPixmapFromLayout(mLinearLayout, mLinearBuffer.data()));
		VDPixmapBlt(pxLinear, px);
		px = pxLinear;
		VDPROFILEEND();
	}

	outputData = (char *)px.data - mBaseOffset;
	outputSize = mOutputSize;

	return true;
}

#if _MSC_VER < 1400
	// VC6 goofs up the clear() below when no-aliasing is set. Hmmmm.
	#pragma optimize("a", off)
#endif

void VDCaptureFilterSystem::Shutdown() {
	mbInitialized = false;

	tFilterChain::iterator it(mFilterChain.begin()), itEnd(mFilterChain.end());

	for(; it!=itEnd; ++it) {
		VDCaptureFilter *pFilt = *it;

		pFilt->Shutdown();
	}

	mFilterChain.clear();
}

void VDCaptureFilterSystem::RebuildFilterChain() {
	mFilterChain.clear();

	if (mbCropEnable)
		mFilterChain.push_back(&mFilterCrop);

	if (mNoiseReductionThreshold)
		mFilterChain.push_back(&mFilterNoiseReduction);

	if (mbLumaSquishBlack || mbLumaSquishWhite)
		mFilterChain.push_back(&mFilterLumaSquish);

	if (mbFieldSwap)
		mFilterChain.push_back(&mFilterSwapFields);

	if (mVertSquashMode)
		mFilterChain.push_back(&mFilterVertSquash);

	if (mbChainEnable)
		mFilterChain.push_back(&mFilterChainAdapter);
}

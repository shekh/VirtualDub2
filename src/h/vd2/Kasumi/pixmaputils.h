#ifndef f_VD2_KASUMI_PIXMAPUTILS_H
#define f_VD2_KASUMI_PIXMAPUTILS_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/Kasumi/pixmap.h>
#include <vd2/system/vdstring.h>
#include <bitset>

struct VDPixmapFormatInfo {
	const char *name;		// debugging name
	bool qchunky;			// quantums are chunky (not 1x1 pixels)
	int qw, qh;				// width, height of a quantum
	int	qwbits, qhbits;		// width and height of a quantum as shifts
	int qsize;				// size of a pixel in bytes
	int auxbufs;			// number of auxiliary buffers (0 for chunky formats, usually 2 for planar)
	int	auxwbits, auxhbits;	// subsampling factors for auxiliary buffers in shifts
	int auxsize;			// size of an aux sample in bytes
	int palsize;			// entries in palette
	int aux4size;			// size of an aux4 sample in bytes (Alpha plane of YUVA)
};

extern VDPixmapFormatInfo g_vdPixmapFormats[];

inline const VDPixmapFormatInfo& VDPixmapGetInfo(sint32 format) {
	VDASSERT((uint32)format < nsVDPixmap::kPixFormat_Max_Standard);
	return g_vdPixmapFormats[(uint32)format < nsVDPixmap::kPixFormat_Max_Standard ? format : 0];
}

#ifdef _DEBUG
	bool VDAssertValidPixmap(const VDPixmap& px);
	bool VDAssertValidPixmapInfo(const VDPixmap& px);
#else
	inline bool VDAssertValidPixmap(const VDPixmap& px) { return true; }
	inline bool VDAssertValidPixmapInfo(const VDPixmap& px) { return true; }
#endif

inline VDPixmap VDPixmapFromLayout(const VDPixmapLayout& layout, void *p) {
	VDPixmap px;

	px.data		= (char *)p + layout.data;
	px.data2	= (char *)p + layout.data2;
	px.data3	= (char *)p + layout.data3;
	px.data4	= (char *)p + layout.data4;
	px.format	= layout.format;
	px.info.colorRangeMode = layout.formatEx.colorRangeMode;
	px.info.colorSpaceMode = layout.formatEx.colorSpaceMode;
	px.w		= layout.w;
	px.h		= layout.h;
	px.palette	= layout.palette;
	px.pitch	= layout.pitch;
	px.pitch2	= layout.pitch2;
	px.pitch3	= layout.pitch3;
	px.pitch4	= layout.pitch4;

	return px;
}

inline VDPixmapLayout VDPixmapToLayoutFromBase(const VDPixmap& px, void *p) {
	VDPixmapLayout layout;
	layout.data		= (char *)px.data - (char *)p;
	layout.data2	= (char *)px.data2 - (char *)p;
	layout.data3	= (char *)px.data3 - (char *)p;
	layout.data4	= (char *)px.data4 - (char *)p;
	layout.format	= px.format;
	layout.formatEx = px;
	layout.w		= px.w;
	layout.h		= px.h;
	layout.palette	= px.palette;
	layout.pitch	= px.pitch;
	layout.pitch2	= px.pitch2;
	layout.pitch3	= px.pitch3;
	layout.pitch4	= px.pitch4;
	return layout;
}

inline VDPixmapLayout VDPixmapToLayout(const VDPixmap& px, void *&p) {
	VDPixmapLayout layout;
	p = px.data;
	layout.data		= 0;
	layout.data2	= (char *)px.data2 - (char *)px.data;
	layout.data3	= (char *)px.data3 - (char *)px.data;
	layout.data4	= (char *)px.data4 - (char *)px.data;
	layout.format	= px.format;
	layout.formatEx = px;
	layout.w		= px.w;
	layout.h		= px.h;
	layout.palette	= px.palette;
	layout.pitch	= px.pitch;
	layout.pitch2	= px.pitch2;
	layout.pitch3	= px.pitch3;
	layout.pitch4	= px.pitch4;
	return layout;
}

bool VDPixmapFormatHasAlpha(sint32 format);
bool VDPixmapFormatHasAlphaPlane(sint32 format);
bool VDPixmapFormatHasRGBPlane(sint32 format);
bool VDPixmapFormatHasYUV16(sint32 format);
bool VDPixmapFormatGray(sint32 format);
int VDPixmapFormatMatrixType(sint32 format);
int VDPixmapFormatDifference(VDPixmapFormatEx src, VDPixmapFormatEx dst);
int VDPixmapFormatGroup(int src);
VDPixmapFormatEx VDPixmapFormatNormalize(VDPixmapFormatEx format);
VDPixmapFormatEx VDPixmapFormatCombine(VDPixmapFormatEx format);
VDPixmapFormatEx VDPixmapFormatNormalizeOpt(VDPixmapFormatEx format);
VDPixmapFormatEx VDPixmapFormatCombineOpt(VDPixmapFormatEx format, VDPixmapFormatEx opt);
VDStringA VDPixmapFormatPrintSpec(VDPixmapFormatEx format);
VDStringA VDPixmapFormatPrintColor(VDPixmapFormatEx format);

struct MatchFilterFormat {
	std::bitset<nsVDPixmap::kPixFormat_Max_Standard> formatMask;
	VDPixmapFormatEx original;
	VDPixmapFormatEx format;
	int base;
	int base1;
	int* follow_list;
	int pos;
	int backup;
	int legacy;

	MatchFilterFormat(VDPixmapFormatEx originalFormat) {
		original = VDPixmapFormatNormalize(originalFormat);
		format = originalFormat;
		initMask();
		initBase();
		backup = 0;
		legacy = 1;
		while(!empty()) {
			if (formatMask.test(format)) break;
			next1();
		}
	}
	void initMask();
	void initBase();
	int next_base();
	void next1();
	void next() {
		while(!empty()) {
			next1();
			if (formatMask.test(format)) break;
		}
	}
	bool empty() {
		if (format && formatMask.any()) return false;
		return true;
	}
};

uint32 VDPixmapCreateLinearLayout(VDPixmapLayout& layout, VDPixmapFormatEx format, vdpixsize w, vdpixsize h, int alignment);

VDPixmap VDPixmapOffset(const VDPixmap& src, vdpixpos x, vdpixpos y);
VDPixmapLayout VDPixmapLayoutOffset(const VDPixmapLayout& src, vdpixpos x, vdpixpos y);

void VDPixmapFlipV(VDPixmap& layout);
void VDPixmapLayoutFlipV(VDPixmapLayout& layout);

uint32 VDPixmapLayoutGetMinSize(const VDPixmapLayout& layout);

VDPixmap VDPixmapExtractField(const VDPixmap& src, bool field2);

#ifndef VDPTRSTEP_DECLARED
	template<class T>
	inline void vdptrstep(T *&p, ptrdiff_t offset) {
		p = (T *)((char *)p + offset);
	}
#endif
#ifndef VDPTROFFSET_DECLARED
	template<class T>
	inline T *vdptroffset(T *p, ptrdiff_t offset) {
		return (T *)((char *)p + offset);
	}
#endif
#ifndef VDPTRDIFFABS_DECLARED
	inline ptrdiff_t vdptrdiffabs(ptrdiff_t x) {
		return x<0 ? -x : x;
	}
#endif


typedef void (*VDPixmapBlitterFn)(const VDPixmap& dst, const VDPixmap& src, vdpixsize w, vdpixsize h);
typedef VDPixmapBlitterFn (*tpVDPixBltTable)[nsVDPixmap::kPixFormat_Max_Standard];

tpVDPixBltTable VDGetPixBltTableReference();
tpVDPixBltTable VDGetPixBltTableX86Scalar();
tpVDPixBltTable VDGetPixBltTableX86MMX();



class VDPixmapBuffer : public VDPixmap {
public:
	VDPixmapBuffer() : mpBuffer(NULL), mLinearSize(0) { data = NULL; format = 0; }
	explicit VDPixmapBuffer(const VDPixmap& src);
	VDPixmapBuffer(const VDPixmapBuffer& src);
	VDPixmapBuffer(sint32 w, sint32 h, int format) : mpBuffer(NULL), mLinearSize(0) {
		init(w, h, format);
	}
	explicit VDPixmapBuffer(const VDPixmapLayout& layout);

	~VDPixmapBuffer();

	void clear() {
		if (mpBuffer)		// to reduce debug checks
			delete[] mpBuffer;
		mpBuffer = NULL;
		mLinearSize = 0;
		format = nsVDPixmap::kPixFormat_Null;
	}

#ifdef _DEBUG
	void *base() { return mpBuffer + (-(int)(uintptr)mpBuffer & 15) + 16; }
	const void *base() const { return mpBuffer + (-(int)(uintptr)mpBuffer & 15) + 16; }
	size_t size() const { return mLinearSize - 28; }

	void validate();
#else
	void *base() { return mpBuffer + (-(int)(uintptr)mpBuffer & 15); }
	const void *base() const { return mpBuffer + (-(int)(uintptr)mpBuffer & 15); }
	size_t size() const { return mLinearSize; }

	void validate() {}
#endif

	void init(sint32 w, sint32 h, int format);
	void init(const VDPixmapLayout&, uint32 additionalPadding = 0);

	void assign(const VDPixmap& src);

	void swap(VDPixmapBuffer&);

protected:
	char *mpBuffer;
	size_t	mLinearSize;
};


#endif

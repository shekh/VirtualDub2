#ifndef f_VD2_KASUMI_UBERBLIT_GEN_H
#define f_VD2_KASUMI_UBERBLIT_GEN_H

#include <vd2/system/vectors.h>
#include "uberblit.h"

class IVDPixmapGenSrc;
struct VDPixmapGenYCbCrBasis;
class VDPixmapGenWindowBasedOneSourceSimple;

class VDPixmapUberBlitterDirectCopy : public IVDPixmapBlitter {
public:
	VDPixmapUberBlitterDirectCopy();
	~VDPixmapUberBlitterDirectCopy();

	void Blit(const VDPixmap& dst, const VDPixmap& src);
	void Blit(const VDPixmap& dst, const vdrect32 *rDst, const VDPixmap& src);
};

class VDPixmapUberBlitter : public IVDPixmapBlitter {
public:
	VDPixmapUberBlitter();
	~VDPixmapUberBlitter();

	void Blit(const VDPixmap& dst, const VDPixmap& src);
	void Blit(const VDPixmap& dst, const vdrect32 *rDst, const VDPixmap& src);

protected:
	void Blit(const VDPixmap& dst, const vdrect32 *rDst, const FilterModPixmapInfo& src);
	void Blit3(const VDPixmap& dst, const vdrect32 *rDst, const FilterModPixmapInfo& src);
	void Blit3Split(const VDPixmap& dst, const vdrect32 *rDst, const FilterModPixmapInfo& src);
	void Blit3Separated(const VDPixmap& px, const vdrect32 *rDst, const FilterModPixmapInfo& src);
	void Blit2(const VDPixmap& dst, const vdrect32 *rDst, const FilterModPixmapInfo& src);
	void Blit2Separated(const VDPixmap& px, const vdrect32 *rDst, const FilterModPixmapInfo& src);
	void BlitAlpha(const VDPixmap& dst, const vdrect32 *rDst, const FilterModPixmapInfo& src);

	friend class VDPixmapUberBlitterGenerator;

	struct OutputEntry {
		IVDPixmapGen *mpSrc;
		int mSrcIndex;
	} mOutputs[4];

	struct SourceEntry {
		IVDPixmapGenSrc *mpSrc;
		IVDPixmapGen *mpGen;
		int mSrcIndex;
		int mSrcPlane;
		int mSrcX;
		int mSrcY;
	};

	typedef vdfastvector<IVDPixmapGen *> Generators;
	Generators mGenerators;

	typedef vdfastvector<SourceEntry> Sources;
	Sources mSources;

	bool mbIndependentChromaPlanes;
	bool mbIndependentPlanes;
	bool mbIndependentAlpha;
	bool mbRGB;
};

class VDPixmapUberBlitterGenerator {
public:
	bool mbRGB;

	struct StackEntry {
		IVDPixmapGen *mpSrc;
		uint32 mSrcIndex;

		StackEntry() {}
		StackEntry(IVDPixmapGen *src, uint32 index) : mpSrc(src), mSrcIndex(index) {}
	};

	VDPixmapUberBlitterGenerator();
	~VDPixmapUberBlitterGenerator();

	void swap(int index);
	void move_to_end(int index);
	void move_to(int index);
	void dup();
	void pop();
	void pop(StackEntry& se);
	void push(StackEntry& se);
	void swap(VDPixmapGenWindowBasedOneSourceSimple* extra, int srcIndex=0);

	void ldsrc(int srcIndex, int srcPlane, int x, int y, uint32 w, uint32 h, uint32 type, uint32 bpr);

	void ldconst(uint8 fill, uint32 bpr, uint32 w, uint32 h, uint32 type);
	void ldconst16c(uint32 bpr, uint32 w, uint32 h, uint32 type);
	void ldconstF(float fill, uint32 bpr, uint32 w, uint32 h, uint32 type);
	void dup_r16();

	void extract_8in16(int offset, uint32 w, uint32 h);
	void extract_8in32(int offset, uint32 w, uint32 h, bool alpha=false);
	void extract_16in32(int offset, uint32 w, uint32 h);
	void extract_16in64(int offset, uint32 w, uint32 h, bool alpha=false);
	void swap_8in16(uint32 w, uint32 h, uint32 bpr);
	void extract_32in128(int offset, uint32 w, uint32 h, bool alpha=false);

	void conv_Pal1_to_8888(int srcIndex);
	void conv_Pal2_to_8888(int srcIndex);
	void conv_Pal4_to_8888(int srcIndex);
	void conv_Pal8_to_8888(int srcIndex);

	void conv_555_to_8888();
	void conv_565_to_8888();
	void conv_888_to_8888();
	void conv_555_to_565();
	void conv_565_to_555();
	void conv_8888_to_X32F();
	void conv_8_to_32F();
	void conv_8_to_16();
	void conv_a8_to_a16();
	void conv_r8_to_r16();
	void conv_16F_to_32F();
	void conv_16_to_32F(bool chroma=false);
	void conv_a16_to_a32F();
	void conv_16_to_8();
	void conv_a16_to_a8();
	void conv_V210_to_32F();
	void conv_V210_to_P16();
	void conv_YU64_to_P16();
	void conv_P16_to_YU64();
	void conv_V410_to_32F();
	void conv_V410_to_P16();
	void conv_Y410_to_32F();
	void conv_Y410_to_P16();
	void conv_V308_to_P8();
	void conv_P8_to_V308();

	void conv_8888_to_555();
	void conv_8888_to_565();
	void conv_8888_to_888();
	void conv_32F_to_8();
	void conv_a32F_to_a8();
	void conv_X32F_to_8888();
	void conv_32F_to_16F();
	void conv_32F_to_16(bool chroma=false);
	void conv_32F_to_r16();
	void conv_32F_to_V210();
	void conv_32F_to_V410();
	void conv_32F_to_Y410();

	void convd_8888_to_555();
	void convd_8888_to_565();
	void convd_32F_to_8();
	void convd_X32F_to_8888();

	void interleave_B8G8_R8G8();
	void interleave_G8B8_G8R8();
	void interleave_X8R8G8B8(uint32 type);
	void interleave_RGB64(uint32 type);
	void interleave_Y416();
	void interleave_B8R8();
	void interleave_B16R16();
	void X8R8G8B8_to_A8R8G8B8();
	void X16R16G16B16_to_A16R16G16B16();

	void conv_X16_to_8888();
	void conv_8888_to_X16();
	void conv_X16_to_X32F();
	void conv_X32F_to_X16();
	void conv_X16_to_R210();
	void conv_X16_to_R10K();
	void conv_X16_to_B48R();
	void conv_R210_to_X16();
	void conv_R10K_to_X16();
	void conv_B48R_to_X16();

	void merge_fields(uint32 w, uint32 h, uint32 bpr);
	void split_fields(uint32 bpr);

	//void ycbcr601_to_rgb32();
	//void ycbcr709_to_rgb32();
	void rgb32_to_ycbcr601();
	void rgb32_to_ycbcr709();

	void ycbcr601_to_rgb32_32f();
	void ycbcr709_to_rgb32_32f();
	void rgb32_to_ycbcr601_32f();
	void rgb32_to_ycbcr709_32f();

	//void ycbcr601_to_ycbcr709();
	//void ycbcr709_to_ycbcr601();

	void ycbcr_to_rgb32_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB);
	void ycbcr_to_rgb64_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB);
	void ycbcr_to_rgb32f_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB);
	void rgb32_to_ycbcr_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB, uint32 colorSpace);
	void rgb32f_to_ycbcr_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB, uint32 colorSpace);
	void ycbcr_to_ycbcr_generic(const VDPixmapGenYCbCrBasis& basisDst, bool dstLimitedRange, const VDPixmapGenYCbCrBasis& basisSrc, bool srcLimitedRange, uint32 colorSpace);

	void pointh(float xoffset, float xfactor, uint32 w);
	void pointv(float yoffset, float yfactor, uint32 h);
	void linearh(float xoffset, float xfactor, uint32 w, bool interpOnly);
	void linearv(float yoffset, float yfactor, uint32 h, bool interpOnly);
	void linear(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h);
	void cubich(float xoffset, float xfactor, uint32 w, float splineFactor, bool interpOnly);
	void cubicv(float yoffset, float yfactor, uint32 h, float splineFactor, bool interpOnly);
	void cubic(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h, float splineFactor);
	void lanczos3h(float xoffset, float xfactor, uint32 w);
	void lanczos3v(float yoffset, float yfactor, uint32 h);
	void lanczos3(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h);

	IVDPixmapBlitter *create();
	VDString dump();
	VDString dump_gen(IVDPixmapGen* src);
	void debug_dump();

protected:
	void MarkDependency(IVDPixmapGen *dst, IVDPixmapGen *src);

	vdfastvector<StackEntry> mStack;

	typedef vdfastvector<IVDPixmapGen *> Generators;
	Generators mGenerators;

	struct Dependency {
		int mDstIdx;
		int mSrcIdx;
	};

	vdfastvector<Dependency> mDependencies;

	typedef VDPixmapUberBlitter::SourceEntry SourceEntry;
	vdfastvector<SourceEntry> mSources;
};

void VDPixmapGenerate(void *dst, ptrdiff_t pitch, sint32 bpr, sint32 height, IVDPixmapGen *gen, int genIndex);
IVDPixmapBlitter *VDCreatePixmapUberBlitterDirectCopy(const VDPixmap& dst, const VDPixmap& src);
IVDPixmapBlitter *VDCreatePixmapUberBlitterDirectCopy(const VDPixmapLayout& dst, const VDPixmapLayout& src);

#endif

#ifndef f_VD2_KASUMI_BLITTER_H
#define f_VD2_KASUMI_BLITTER_H

#include <vd2/system/vectors.h>
#include <vd2/kasumi/pixmap.h>

struct VDPixmap;
struct VDPixmapLayout;
class IVDPixmapGen;

class IVDPixmapBlitter {
public:
	virtual ~IVDPixmapBlitter() {}
	virtual void Blit(const VDPixmap& dst, const VDPixmap& src) = 0;
	virtual void Blit(const VDPixmap& dst, const vdrect32 *rDst, const VDPixmap& src) = 0;
};

IVDPixmapBlitter *VDPixmapCreateBlitter(const VDPixmap& dst, const VDPixmap& src, IVDPixmapGen* extraDst=0);
IVDPixmapBlitter *VDPixmapCreateBlitter(const VDPixmapLayout& dst, const VDPixmapLayout& src, IVDPixmapGen* extraDst=0, int src_swizzle=0);

class VDPixmapCachedBlitter {
	VDPixmapCachedBlitter(const VDPixmapCachedBlitter&);
	VDPixmapCachedBlitter& operator=(const VDPixmapCachedBlitter&);
public:
	VDPixmapCachedBlitter();
	~VDPixmapCachedBlitter();

	void Blit(const VDPixmap& dst, const VDPixmap& src);
	void Invalidate();

protected:
	sint32 mSrcWidth;
	sint32 mSrcHeight;
	VDPixmapFormatEx mSrcFormat;
	sint32 mDstWidth;
	sint32 mDstHeight;
	VDPixmapFormatEx mDstFormat;
	IVDPixmapBlitter *mpCachedBlitter;
};

#endif

#ifndef f_VD2_VDDISPLAY_RENDERER_H
#define f_VD2_VDDISPLAY_RENDERER_H

#include <vd2/Kasumi/pixmap.h>
#include <vd2/system/refcount.h>
#include <vd2/system/unknown.h>

class VDDisplayImageView;

class IVDDisplayRenderer {
public:
	virtual void SetColorRGB(uint32 color) = 0;
	virtual void FillRect(sint32 x, sint32 y, sint32 w, sint32 h) = 0;
	virtual void Blt(sint32 x, sint32 y, VDDisplayImageView& imageView) = 0;
};

class VDDisplayImageView {
public:
	VDDisplayImageView();
	~VDDisplayImageView();

	bool IsDynamic() const { return mbDynamic; }
	const VDPixmap& GetImage() const { return mPixmap; }
	void SetImage(const VDPixmap& px, bool dynamic);

	void SetCachedImage(IVDRefUnknown *p);
	IVDRefUnknown *GetCachedImage() const { return mpCachedImage; }

	uint32 GetUniquenessCounter() const { return mUniquenessCounter; }

	void Invalidate();

protected:
	vdrefptr<IVDRefUnknown> mpCachedImage;
	uint32 mUniquenessCounter;
	VDPixmap mPixmap;
	bool mbDynamic;
};

#endif

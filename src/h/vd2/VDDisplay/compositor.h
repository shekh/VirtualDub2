#ifndef f_VD2_VDDISPLAY_COMPOSITOR_H
#define f_VD2_VDDISPLAY_COMPOSITOR_H

#include <vd2/system/refcount.h>

class IVDDisplayRenderer;

class VDINTERFACE IVDDisplayCompositor : public IVDRefCount {
public:
	virtual void Composite(IVDDisplayRenderer& r) = 0;
};

#endif

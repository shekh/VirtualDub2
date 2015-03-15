#ifndef tiff_image_h
#define tiff_image_h

#include <vd2/system/vdtypes.h>

bool VDIsTiffHeader(const void *pv, uint32 len);

struct VDPixmap;

class VDINTERFACE IVDImageDecoderTIFF {
public:
	virtual ~IVDImageDecoderTIFF() {}

	virtual void Decode(const void *src, uint32 srclen) = 0;
	virtual void GetSize(int& w, int& h) = 0;
	virtual void GetImage(void *p, int pitch, int format) = 0;
};

class VDINTERFACE IVDImageEncoderTIFF {
public:
	virtual ~IVDImageEncoderTIFF() {}
	virtual void Encode(const VDPixmap& px, void *&p, uint32& len, bool lzw_compress, bool alpha) = 0;
};

IVDImageDecoderTIFF *VDCreateImageDecoderTIFF();
IVDImageEncoderTIFF *VDCreateImageEncoderTIFF();

#endif

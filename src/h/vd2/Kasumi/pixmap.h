#ifndef f_VD2_KASUMI_PIXMAP_H
#define f_VD2_KASUMI_PIXMAP_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/vdtypes.h>
#include <vd2/plugin/vdplugin.h>

namespace nsVDPixmap {
	enum VDPixmapFormat {
		kPixFormat_Null,
		kPixFormat_Pal1,
		kPixFormat_Pal2,
		kPixFormat_Pal4,
		kPixFormat_Pal8,
		kPixFormat_XRGB1555,
		kPixFormat_RGB565,
		kPixFormat_RGB888,
		kPixFormat_XRGB8888,
		kPixFormat_Y8,
		kPixFormat_YUV422_UYVY,
		kPixFormat_YUV422_YUYV,
		kPixFormat_YUV444_XVYU,				// The reason for the strange VYU ordering is to make it easier to convert to UYVY/YUY2.
		kPixFormat_YUV444_Planar,
		kPixFormat_YUV422_Planar,
		kPixFormat_YUV420_Planar,
		kPixFormat_YUV411_Planar,
		kPixFormat_YUV410_Planar,
		kPixFormat_YUV422_Planar_Centered,	// MPEG-1/MJPEG chroma alignment
		kPixFormat_YUV420_Planar_Centered,	// MPEG-1/MJPEG chroma alignment
		kPixFormat_YUV422_Planar_16F,
		kPixFormat_YUV422_V210,
		kPixFormat_YUV422_UYVY_709,			// Also known as HDYC.
		kPixFormat_YUV420_NV12,
		kPixFormat_Y8_FR,
		kPixFormat_YUV422_YUYV_709,
		kPixFormat_YUV444_Planar_709,
		kPixFormat_YUV422_Planar_709,
		kPixFormat_YUV420_Planar_709,
		kPixFormat_YUV411_Planar_709,
		kPixFormat_YUV410_Planar_709,
		kPixFormat_YUV422_UYVY_FR,
		kPixFormat_YUV422_YUYV_FR,
		kPixFormat_YUV444_Planar_FR,
		kPixFormat_YUV422_Planar_FR,
		kPixFormat_YUV420_Planar_FR,
		kPixFormat_YUV411_Planar_FR,
		kPixFormat_YUV410_Planar_FR,
		kPixFormat_YUV422_UYVY_709_FR,
		kPixFormat_YUV422_YUYV_709_FR,
		kPixFormat_YUV444_Planar_709_FR,
		kPixFormat_YUV422_Planar_709_FR,
		kPixFormat_YUV420_Planar_709_FR,
		kPixFormat_YUV411_Planar_709_FR,
		kPixFormat_YUV410_Planar_709_FR,
		kPixFormat_YUV420i_Planar,
		kPixFormat_YUV420i_Planar_FR,
		kPixFormat_YUV420i_Planar_709,
		kPixFormat_YUV420i_Planar_709_FR,
		kPixFormat_YUV420it_Planar,
		kPixFormat_YUV420it_Planar_FR,
		kPixFormat_YUV420it_Planar_709,
		kPixFormat_YUV420it_Planar_709_FR,
		kPixFormat_YUV420ib_Planar,
		kPixFormat_YUV420ib_Planar_FR,
		kPixFormat_YUV420ib_Planar_709,
		kPixFormat_YUV420ib_Planar_709_FR,
		kPixFormat_XRGB64,
		kPixFormat_YUV444_Planar16,
		kPixFormat_YUV422_Planar16,
		kPixFormat_YUV420_Planar16,
		kPixFormat_Y16,
		kPixFormat_YUVA444_Y416,
		kPixFormat_YUV444_V410,
		kPixFormat_YUV444_Y410,
		kPixFormat_R210,
		kPixFormat_R10K,
		kPixFormat_YUV444_V308,
		kPixFormat_YUV422_P210,
		kPixFormat_YUV420_P010,
		kPixFormat_YUV422_P216,
		kPixFormat_YUV420_P016,

		kPixFormat_YUV444_Alpha_Planar,
		kPixFormat_YUV422_Alpha_Planar,
		kPixFormat_YUV420_Alpha_Planar,
		kPixFormat_YUV444_Alpha_Planar16,
		kPixFormat_YUV422_Alpha_Planar16,
		kPixFormat_YUV420_Alpha_Planar16,
		kPixFormat_YUV422_YU64,
		kPixFormat_B64A,
		kPixFormat_Max_Standard
	};
}

typedef sint32		vdpixpos;
typedef sint32		vdpixsize;
typedef ptrdiff_t	vdpixoffset;

struct VDPixmap {
	void			*data;
	const uint32	*palette;
	vdpixsize		w;
	vdpixsize		h;
	vdpixoffset		pitch;
	sint32			format;

	// Auxiliary planes are always byte-per-pixel.

	void			*data2;		// Cb (U) for YCbCr
	vdpixoffset		pitch2;
	void			*data3;		// Cr (V) for YCbCr
	vdpixoffset		pitch3;
	void			*data4;		// Alpha
	vdpixoffset		pitch4;

	FilterModPixmapInfo info;

	void clear() {
		data = 0;
		palette = 0;
		w = 0;
		h = 0;
		pitch = 0;
		format = 0;
		data2 = 0;
		pitch2 = 0;
		data3 = 0;
		pitch3 = 0;
		data4 = 0;
		pitch4 = 0;
		info.clear();
	}

	static VDPixmap copy(const VDXPixmap& a);
};

struct VDPixmapFormatEx {
	sint32			format;
	vd2::ColorSpaceMode colorSpaceMode;
	vd2::ColorRangeMode colorRangeMode;

	operator int() const { return format; }

	VDPixmapFormatEx() {
		format = 0;
		colorSpaceMode = vd2::kColorSpaceMode_None;
		colorRangeMode = vd2::kColorRangeMode_None;
	}
	VDPixmapFormatEx(sint32 v) {
		format = v;
		colorSpaceMode = vd2::kColorSpaceMode_None;
		colorRangeMode = vd2::kColorRangeMode_None;
	}
	VDPixmapFormatEx(const VDPixmap& v) {
		format = v.format;
		colorSpaceMode = v.info.colorSpaceMode;
		colorRangeMode = v.info.colorRangeMode;
	}
	VDPixmapFormatEx(const VDPixmapFormatEx& v) {
		format = v.format;
		colorSpaceMode = v.colorSpaceMode;
		colorRangeMode = v.colorRangeMode;
	}
	bool fullEqual(const VDPixmapFormatEx& v) const {
		return (format == v.format &&
		colorSpaceMode == v.colorSpaceMode &&
		colorRangeMode == v.colorRangeMode);
	}
	bool defaultMode() const {
		return colorSpaceMode == vd2::kColorSpaceMode_None && colorRangeMode == vd2::kColorRangeMode_None;
	}
};

struct VDPixmapLayout {
	ptrdiff_t		data;
	const uint32	*palette;
	vdpixsize		w;
	vdpixsize		h;
	vdpixoffset		pitch;
	sint32			format;

	// Auxiliary planes are always byte-per-pixel.

	ptrdiff_t		data2;		// Cb (U) for YCbCr
	vdpixoffset		pitch2;
	ptrdiff_t		data3;		// Cr (V) for YCbCr
	vdpixoffset		pitch3;
	ptrdiff_t		data4;		// Alpha
	vdpixoffset		pitch4;

	VDPixmapFormatEx formatEx;
};

#endif

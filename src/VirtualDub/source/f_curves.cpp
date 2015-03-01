#include "stdafx.h"
#include <vd2/VDXFrame/VideoFilter.h>

namespace {
	void TransformB8(void *VDRESTRICT dst0, ptrdiff_t dstpitch, uint32 w, uint32 h, const uint8 * VDRESTRICT lookup) {
		uint8 * VDRESTRICT dst = (uint8 * VDRESTRICT)dst0;

		do {
			for(uint32 i=0; i<w; ++i)
				dst[i] = lookup[dst[i]];

			dst += dstpitch;
		} while(--h);
	}

	void TransformX8R8G8B8(void *VDRESTRICT dst0, ptrdiff_t dstpitch, uint32 w, uint32 h, const uint8 (* VDRESTRICT lookup)[256]) {
		uint8 * VDRESTRICT dst = (uint8 * VDRESTRICT)dst0;
		const uint8 *VDRESTRICT rtable = lookup[0];
		const uint8 *VDRESTRICT gtable = lookup[1];
		const uint8 *VDRESTRICT btable = lookup[2];

		ptrdiff_t dstmodulo = dstpitch - 4*w;
		do {
			for(uint32 i=0; i<w; ++i) {
				dst[0] = btable[dst[0]];
				dst[1] = gtable[dst[1]];
				dst[2] = rtable[dst[2]];
				dst += 4;
			}

			dst += dstmodulo;
		} while(--h);
	}

	void TransformY8U8Y8V8(void *VDRESTRICT dst0, ptrdiff_t dstpitch, uint32 w, uint32 h, const uint8 (* VDRESTRICT lookup)[256]) {
		uint8 * VDRESTRICT dst = (uint8 * VDRESTRICT)dst0;
		const uint8 *VDRESTRICT ytable = lookup[0];
		const uint8 *VDRESTRICT utable = lookup[1];
		const uint8 *VDRESTRICT vtable = lookup[2];

		w = (w + 1) >> 1;

		ptrdiff_t dstmodulo = dstpitch - 4*w;
		do {
			for(uint32 i=0; i<w; ++i) {
				dst[0] = ytable[dst[0]];
				dst[1] = utable[dst[1]];
				dst[2] = ytable[dst[2]];
				dst[3] = vtable[dst[3]];
				dst += 4;
			}

			dst += dstmodulo;
		} while(--h);
	}

	void TransformU8Y8V8Y8(void *VDRESTRICT dst0, ptrdiff_t dstpitch, uint32 w, uint32 h, const uint8 (* VDRESTRICT lookup)[256]) {
		uint8 * VDRESTRICT dst = (uint8 * VDRESTRICT)dst0;
		const uint8 *VDRESTRICT ytable = lookup[0];
		const uint8 *VDRESTRICT utable = lookup[1];
		const uint8 *VDRESTRICT vtable = lookup[2];

		w = (w + 1) >> 1;

		ptrdiff_t dstmodulo = dstpitch - 4*w;
		do {
			for(uint32 i=0; i<w; ++i) {
				dst[0] = utable[dst[0]];
				dst[1] = ytable[dst[1]];
				dst[2] = vtable[dst[2]];
				dst[3] = ytable[dst[3]];
				dst += 4;
			}

			dst += dstmodulo;
		} while(--h);
	}
}

struct VDVideoFilterCurvesConfig {
	struct Point {
		float x;
		float y;
	};

	typedef vdfastvector<Point> Curve;
	Curve mCurve[3];

	bool mbYCbCrMode;

	VDVideoFilterCurvesConfig()
		: mbYCbCrMode(false)
	{
		for(int i=0; i<3; ++i) {
			Point y[5] = {
				{ 0.0f, 0.0f },
				{ 0.2f, 1.0f },
				{ 0.5f, 0.5f },
				{ 0.7f, 0.0f },
				{ 1.0f, 1.0f }
			};

			mCurve[i].assign(y, y+5);
		}
	}
};

class VDVideoFilterCurves : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Run();

protected:
	uint32 mChromaWidth;
	uint32 mChromaHeight;

	typedef VDVideoFilterCurvesConfig Config;
	Config mConfig;

	uint8	mLookupTables[3][256];
};

uint32 VDVideoFilterCurves::GetParams() {
	const VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;
	const uint32 w = pxldst.w;
	const uint32 h = pxldst.h;
	bool ycbcrMode = true;

	mChromaWidth = w;
	mChromaHeight = h;

	switch(pxldst.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			ycbcrMode = false;
			break;

		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
			break;

		case nsVDXPixmap::kPixFormat_YUV422_Planar:
			mChromaWidth = (w + 1) >> 1;
			break;

		case nsVDXPixmap::kPixFormat_YUV411_Planar:
			mChromaWidth = (w + 3) >> 2;
			break;

		case nsVDXPixmap::kPixFormat_YUV420_Planar:
			mChromaWidth = (w + 1) >> 1;
			mChromaHeight = (h + 1) >> 1;
			break;

		case nsVDXPixmap::kPixFormat_YUV410_Planar:
			mChromaWidth = (w + 3) >> 2;
			mChromaHeight = (h + 3) >> 2;
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	if (mConfig.mbYCbCrMode != ycbcrMode)
		return FILTERPARAM_NOT_SUPPORTED;

	for(int i=0; i<3; ++i) {
		uint8 (&table)[256] = mLookupTables[i];
		const Config::Curve& curve = mConfig.mCurve[i];

		size_t n = curve.size();

		vdfastvector<float> a(n, 0);
		vdfastvector<float> b(n, 0);
		vdfastvector<float> c(n, 0);
		vdfastvector<float> r(n, 0);

		// compute initial a, b, c
		for(size_t j = 1; j < n-1; ++j) {
			a[j] = (curve[j].x - curve[j-1].x) / 6.0f;
			b[j] = (curve[j+1].x - curve[j-1].x) / 3.0f;
			c[j] = (curve[j+1].x - curve[j].x) / 6.0f;
			r[j] = (curve[j+1].y - curve[j].y) / (curve[j+1].x - curve[j].x)
					- (curve[j].y - curve[j-1].y) / (curve[j].x - curve[j-1].x);
		}

		// trim endpoints
		a[1] = 0;
		c[n-2] = 0;

		// forward eliminate: makes a[i] = 0
		for(size_t j = 1; j < n-2; ++j) {
			float f = a[j+1] / b[j];

			b[j+1] -= c[j] * f;
			r[j+1] -= r[j] * f;
		}

		// backwards eliminate: makes c[i] = 0
		for(size_t j = n-3; j >= 1; --j)
			r[j] -= r[j+1] * (c[j] / b[j+1]);

		// solve
		for(size_t j = n-2; j >= 1; --j)
			r[j] /= b[j];

		// begin interpolation
		int pos = 0;
		for(int j=0; j<256; ++j) {
			const float x = (float)j / 255.0f;

			while(x > curve[pos+1].x)
				++pos;

			// compute A-D
			const float x0 = curve[pos].x;
			const float x1 = curve[pos+1].x;
			const float y0 = curve[pos].y;
			const float y1 = curve[pos+1].y;
			const float dx = x1 - x0;

#if 0
			const float A = (x1 - x) / dx;
			const float B = (x - x0) / dx;
			const float C = (1.0f/6.0f) * (A*A*A - A) * dx * dx;
			const float D = (1.0f/6.0f) * (B*B*B - B) * dx * dx;

			float y = A*y0 + B*y1 + C*r[pos] + D*r[pos+1];
#else
			const float s = (1.0f/6.0f) * dx * dx;
			const float t = (x - x0) / dx;
			const float k0 = s*r[pos];
			const float k1 = s*r[pos+1];

			const float c0 = y0;
			const float c1 = y1 - y0 - k1 - 2*k0;
			const float c2 = 3*k0;
			const float c3 = k1 - k0;

			float y = c0 + t*(c1 + t*(c2 + t*c3));
#endif

			if (y < 0)
				y = 0;

			if (y > 1)
				y = 1;

			table[j] = (uint8)(255.0f * y + 0.5f);
		}
	}

	fa->dst.offset = fa->src.offset;
	return FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVideoFilterCurves::Run() {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const uint32 w = pxdst.w;
	const uint32 h = pxdst.h;

	switch(pxdst.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			TransformX8R8G8B8(pxdst.data, pxdst.pitch, w, h, mLookupTables);
			break;

		case nsVDXPixmap::kPixFormat_Y8:
			TransformB8(pxdst.data, pxdst.pitch, w, h, mLookupTables[0]);
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
			TransformB8(pxdst.data, pxdst.pitch, w, h, mLookupTables[0]);
			TransformB8(pxdst.data2, pxdst.pitch2, mChromaWidth, mChromaHeight, mLookupTables[1]);
			TransformB8(pxdst.data3, pxdst.pitch3, mChromaWidth, mChromaHeight, mLookupTables[2]);
			break;

		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
			TransformU8Y8V8Y8(pxdst.data, pxdst.pitch, w, h, mLookupTables);
			break;

		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
			TransformY8U8Y8V8(pxdst.data, pxdst.pitch, w, h, mLookupTables);
			break;
	}
}

extern const VDXFilterDefinition filterDef_curves = VDXVideoFilterDefinition<VDVideoFilterCurves>(
	NULL,
	"curves",
	"Applies arbitrary curves to video channels.");

#ifdef _MSC_VER
	#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed
#endif
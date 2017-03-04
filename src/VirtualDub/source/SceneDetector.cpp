//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include <vd2/system/error.h>
#include <vd2/Kasumi/pixel.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#include "SceneDetector.h"

SceneDetector::SceneDetector() {
	tile_w = 0;
	tile_h = 0;
}

void SceneDetector::Resize(uint32 width, uint32 height) {
	last_valid = false;
	first_diff = true;

	cut_threshold = 50 * tile_w * tile_h;
	fade_threshold = 4 * tile_w * tile_h;

	tile_w = (width + 7)/8;
	tile_h = (height + 7)/8;

	uint32 tileCount = tile_w * tile_h;

	mCurrentLummap.resize(tileCount);
	mPrevLummap.resize(tileCount);
}

SceneDetector::~SceneDetector() {
}

//////////////////////////////////////////////////////////////////////////

void SceneDetector::SetThresholds(int cut_threshold, int fade_threshold) {
	enabled = cut_threshold>0 || fade_threshold>0;
	this->cut_threshold		= (cut_threshold * tile_w * tile_h) >> 4;
	this->fade_threshold	= (fade_threshold * tile_w * tile_h)/16.0f;
}

bool SceneDetector::Enabled() {
	return enabled;
}

bool SceneDetector::Submit(const VDPixmap& src) {
	if (!Enabled())
		return false;

	if (!src.format)
		return false;

	if (!tile_w || !tile_h)
		Resize(src.w,src.h);

	if (src.w > tile_w*8 || src.h > tile_h*8)
		return false;

	long last_frame_diffs = 0;
	long lum_total = 0;
	double lum_sq_total = 0.0;
	long len = tile_w * tile_h;

	FlipBuffers();
	BitmapToLummap(mCurrentLummap.data(), src);

	if (!last_valid) {
		last_valid = true;
		return false;
	}

/////////////

	const uint32 *t1 = mCurrentLummap.data(), *t2 = mPrevLummap.data();

	do {
		uint32 c1 = *t1++;
		uint32 c2 = *t2++;

		last_frame_diffs +=(   54*abs((int)(c2>>16)-(int)(c1>>16))
							+ 183*abs((int)((c2>>8)&255) -(int)((c1>>8)&255))
							+  19*abs((int)(c2&255)-(int)(c1&255))) >> 8;

		long lum = ((c1>>16)*54 + ((c1>>8)&255)*183 + (c1&255)*19 + 128)>>8;

		lum_total += lum;
		lum_sq_total += (double)lum * (double)lum;
	} while(--len);

	const double tile_count = tile_w * tile_h;

//	_RPT3(0,"Last frame diffs=%ld, lum(linear)=%ld, lum(rms)=%f\n",last_frame_diffs,lum_total,sqrt(lum_sq_total));

	if (fade_threshold) {
		// Var(X)	= E(X^2) - E(X)^2 
		//			= sum(X^2)/N - sum(X)^2 / N^2
		// SD(X)	= sqrt(N * sum(X^2) - sum(X)^2)) / N

		bool is_fade = sqrt(lum_sq_total * tile_count - (double)lum_total * lum_total) < fade_threshold;

		if (first_diff) {
			last_fade_state = is_fade;
			first_diff = false;
		} else {
			// If we've encountered a new fade, return 'scene changed'

			if (!last_fade_state && is_fade) {
				last_fade_state = true;
				return true;
			}

			// Hit the end of an initial fade?

			if (last_fade_state && !is_fade)
				last_fade_state = false;
		}
	}

	// Cut/dissolve detection

	return cut_threshold ? last_frame_diffs > cut_threshold : false;
}

void SceneDetector::Reset() {
	last_valid = false;
}

//////////////////////////////////////////////////////////////////////////

namespace {
	uint32 scene_lumtile32(const void *src0, long w, long h, ptrdiff_t pitch) {
		w <<= 2;

		const char *src = (const char *)src0 + w;
		uint32 rb_total = 0;
		uint32 g_total = 0;

		w = -w;
		do {
			long x = w;

			do {
				const uint32 px = *(uint32 *)(src + x);

				rb_total += px & 0xff00ff;
				g_total += px & 0x00ff00;
			} while(x += 4);

			src += pitch;
		} while(--h);

		return (((rb_total + 0x00200020) & 0x3fc03fc0) + ((g_total + 0x00002000) & 0x003fc000)) >> 6;
	}

	uint32 scene_lumtile24(const void *src0, long w, long h, ptrdiff_t pitch) {
		const uint8 *src = (const uint8 *)src0;
		pitch -= 3*w;
		uint32 r_total = 0;
		uint32 g_total = 0;
		uint32 b_total = 0;
		do {
			long x = w;

			do {
				b_total += src[0];
				g_total += src[1];
				r_total += src[2];
				src += 3;
			} while(--x);

			src += pitch;
		} while(--h);

		r_total = (r_total + 0x20) >> 6;
		g_total = (g_total + 0x20) >> 6;
		b_total = (b_total + 0x20) >> 6;
		return (r_total << 16) + (g_total << 8) + b_total;
	}

	uint32 scene_lumtile16(const void *src0, long w, long h, ptrdiff_t pitch) {
		w += w;

		const char *src = (const char *)src0 + w;
		uint32 r_total = 0;
		uint32 g_total = 0;
		uint32 b_total = 0;

		w = -w;
		do {
			long x = w;

			do {
				const uint32 px = *(uint16 *)(src + x);

				r_total += px & 0x7c00;
				g_total += px & 0x03e0;
				b_total += px & 0x001f;
			} while(x += 2);

			src += pitch;
		} while(--h);

		r_total = (r_total + 0x1000) << 3;
		g_total = (g_total + 0x0080);
		b_total = (b_total + 0x0004) >> 3;
		return (r_total & 0xff0000) + (g_total & 0x00ff00) + (b_total & 0x0000ff);
	}

	uint32 scene_lumtile565(const void *src0, long w, long h, ptrdiff_t pitch) {
		w += w;

		const char *src = (const char *)src0 + w;
		uint32 r_total = 0;
		uint32 g_total = 0;
		uint32 b_total = 0;

		w = -w;
		do {
			long x = w;

			do {
				const uint32 px = *(uint16 *)(src + x);

				r_total += px & 0xf800;
				g_total += px & 0x07e0;
				b_total += px & 0x001f;
			} while(x += 2);

			src += pitch;
		} while(--h);

		r_total = (r_total + 0x2000) << 2;
		g_total = (g_total + 0x0100) >> 1;
		b_total = (b_total + 0x0004) >> 3;
		return (r_total & 0xff0000) + (g_total & 0x00ff00) + (b_total & 0x0000ff);
	}

	uint32 scene_lumtileUYVY(const void *src0, long w, long h, ptrdiff_t pitch) {
		const uint8 *src = (const uint8 *)src0;
		uint32 y_total = 0;
		uint32 cb_total = 0;
		uint32 cr_total = 0;

		sint32 y_bias = (w * h) * 0x10;
		sint32 c_bias = (w * h) * 0x40;		// 0x80 * (w*h)/2

		do {
			long x = w;

			const uint8 *src2 = src;
			do {
				y_total += src2[1] + src2[3];
				cb_total += src2[0];
				cr_total += src2[2];
				src2 += 4;
			} while(x -= 2);

			src += pitch;
		} while(--h);

		y_total = ((sint32)(y_total + 32 - y_bias) >> 6) + 0x10;
		cb_total = ((sint32)(cb_total + 16 - c_bias) >> 5) + 0x80;
		cr_total = ((sint32)(cr_total + 16 - c_bias) >> 5) + 0x80;

		return VDConvertYCbCrToRGB((uint8)y_total, (uint8)cb_total, (uint8)cr_total, false, false);
	}

	uint32 scene_lumtileYUY2(const void *src0, long w, long h, ptrdiff_t pitch) {
		const uint8 *src = (const uint8 *)src0;
		uint32 y_total = 0;
		uint32 cb_total = 0;
		uint32 cr_total = 0;

		sint32 y_bias = (w * h) * 0x10;
		sint32 c_bias = (w * h) * 0x40;		// 0x80 * (w*h)/2

		do {
			long x = w;

			const uint8 *src2 = src;
			do {
				y_total += src2[0] + src2[2];
				cb_total += src2[1];
				cr_total += src2[3];
				src2 += 4;
			} while(x -= 2);

			src += pitch;
		} while(--h);

		y_total = ((sint32)(y_total + 32 - y_bias) >> 6) + 0x10;
		cb_total = ((sint32)(cb_total + 16 - c_bias) >> 5) + 0x80;
		cr_total = ((sint32)(cr_total + 16 - c_bias) >> 5) + 0x80;

		return VDConvertYCbCrToRGB((uint8)y_total, (uint8)cb_total, (uint8)cr_total, false, false);
	}

	uint32 scene_lumtileY8(const void *src0, long w, long h, ptrdiff_t pitch) {
		const uint8 *src = (const uint8 *)src0;
		uint32 y_total = 0;
		sint32 y_bias = (w * h) * 0x10;

		do {
			long x = w;

			const uint8 *src2 = src;
			do {
				y_total += *src2++;
			} while(--x);

			src += pitch;
		} while(--h);

		y_total = ((sint32)(y_total + 32 - y_bias) >> 6) + 0x10;

		return VDConvertYCbCrToRGB((uint8)y_total, 0x80, 0x80, false, false);
	}

	uint32 scene_lumtileI8(const void *src0, long w, long h, ptrdiff_t pitch) {
		const uint8 *src = (const uint8 *)src0;
		uint32 y_total = 0;

		do {
			long x = w;

			const uint8 *src2 = src;
			do {
				y_total += *src2++;
			} while(--x);

			src += pitch;
		} while(--h);

		return ((sint32)(y_total + 0x20) >> 6) * 0x010101;
	}

	template<int kXShift, int kYShift>
	uint32 scene_lumtileYCbCrPlanar(const uint8 *y, ptrdiff_t ypitch, const uint8 *cb, ptrdiff_t cbpitch, const uint8 *cr, ptrdiff_t crpitch, uint32 w, uint32 h) {
		uint32 y_total = 0;
		uint32 cb_total = 0;
		uint32 cr_total = 0;

		sint32 y_bias = (w * h) * 0x10;

		uint32 w2 = w >> kXShift;
		uint32 h2 = h >> kYShift;
		sint32 c_bias = (w2 * h2) * 0x80;

		for(uint32 i=0; i<h; ++i) {
			switch(w) {
				case 8:		y_total += y[7];
				case 7:		y_total += y[6];
				case 6:		y_total += y[5];
				case 5:		y_total += y[4];
				case 4:		y_total += y[3];
				case 3:		y_total += y[2];
				case 2:		y_total += y[1];
				case 1:		y_total += y[0];
			}

			y += ypitch;
		}

		for(uint32 i=0; i<h2; ++i) {
			switch(w2) {
				case 8:		cb_total += cb[7];
							cr_total += cr[7];
				case 7:		cb_total += cb[6];
							cr_total += cr[6];
				case 6:		cb_total += cb[5];
							cr_total += cr[5];
				case 5:		cb_total += cb[4];
							cr_total += cr[4];
				case 4:		cb_total += cb[3];
							cr_total += cr[3];
				case 3:		cb_total += cb[2];
							cr_total += cr[2];
				case 2:		cb_total += cb[1];
							cr_total += cr[1];
				case 1:		cb_total += cb[0];
							cr_total += cr[0];
			}

			cb += cbpitch;
			cr += crpitch;
		}

		enum {
			kCShiftDown = 6 - (kXShift + kYShift),
			kCRound = 1 << (kCShiftDown - 1)
		};

		y_total = ((sint32)(y_total + 32 - y_bias) >> 6) + 0x10;
		cb_total = ((sint32)(cb_total + kCRound - c_bias) >> kCShiftDown) + 0x80;
		cr_total = ((sint32)(cr_total + kCRound - c_bias) >> kCShiftDown) + 0x80;

		return VDConvertYCbCrToRGB((uint8)y_total, (uint8)cb_total, (uint8)cr_total, false, false);
	}

	uint32 scene_lumtileNV12(const uint8 *y, ptrdiff_t ypitch, const uint8 *c, ptrdiff_t cpitch, uint32 w, uint32 h) {
		uint32 y_total = 0;
		uint32 cb_total = 0;
		uint32 cr_total = 0;

		sint32 y_bias = (w * h) * 0x10;

		uint32 w2 = w >> 1;
		uint32 h2 = h >> 1;
		sint32 c_bias = (w2 * h2) * 0x80;

		for(uint32 i=0; i<h; ++i) {
			switch(w) {
				case 8:		y_total += y[7];
				case 7:		y_total += y[6];
				case 6:		y_total += y[5];
				case 5:		y_total += y[4];
				case 4:		y_total += y[3];
				case 3:		y_total += y[2];
				case 2:		y_total += y[1];
				case 1:		y_total += y[0];
			}

			y += ypitch;
		}

		for(uint32 i=0; i<h2; ++i) {
			switch(w2) {
				case 8:		cr_total += c[15];
							cb_total += c[14];
				case 7:		cr_total += c[13];
							cb_total += c[12];
				case 6:		cr_total += c[11];
							cb_total += c[10];
				case 5:		cr_total += c[9];
							cb_total += c[8];
				case 4:		cr_total += c[7];
							cb_total += c[6];
				case 3:		cr_total += c[5];
							cb_total += c[4];
				case 2:		cr_total += c[3];
							cb_total += c[2];
				case 1:		cr_total += c[1];
							cb_total += c[0];
			}

			c += cpitch;
		}

		enum {
			kCShiftDown = 6 - 4,
			kCRound = 1 << (kCShiftDown - 1)
		};

		y_total = ((sint32)(y_total + 32 - y_bias) >> 6) + 0x10;
		cb_total = ((sint32)(cb_total + kCRound - c_bias) >> kCShiftDown) + 0x80;
		cr_total = ((sint32)(cr_total + kCRound - c_bias) >> kCShiftDown) + 0x80;

		return VDConvertYCbCrToRGB((uint8)y_total, (uint8)cb_total, (uint8)cr_total, false, false);
	}
}

#ifdef _M_IX86
	extern "C" uint32 __cdecl asm_scene_lumtile32(const void *src, long w, long h, long pitch);
	extern "C" uint32 __cdecl asm_scene_lumtile24(const void *src, long w, long h, long pitch);
	extern "C" uint32 __cdecl asm_scene_lumtile16(const void *src, long w, long h, long pitch);
#else
	#define asm_scene_lumtile32 scene_lumtile32
	#define asm_scene_lumtile24 scene_lumtile24
	#define asm_scene_lumtile16 scene_lumtile16
#endif

void SceneDetector::BitmapToLummap(uint32 *lummap, const VDPixmap& pxsrc) {
	int mh = 8;
	uint32 w, h;

	h = (pxsrc.h+7) >> 3;

	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(pxsrc.format);

	if (formatInfo.auxbufs == 0) {
		const char *src_row = (const char *)pxsrc.data;

		do {
			if (h<=1 && (pxsrc.h&7))
				mh = pxsrc.h&7;

			const char *src = src_row;
			src_row += pxsrc.pitch*8;

			w = pxsrc.w >> 3;

			switch(pxsrc.format) {
				case nsVDPixmap::kPixFormat_XRGB1555:
					do {
						*lummap++ = asm_scene_lumtile16(src, 8, mh, pxsrc.pitch);
						src += 16;
					} while(--w);

					if (pxsrc.w & 6) {
						*lummap++ = asm_scene_lumtile16(src, pxsrc.w&6, mh, pxsrc.pitch);
					}
					break;

				case nsVDPixmap::kPixFormat_RGB565:
					do {
						*lummap++ = scene_lumtile565(src, 8, mh, pxsrc.pitch);
						src += 16;
					} while(--w);

					if (pxsrc.w & 6) {
						*lummap++ = scene_lumtile565(src, pxsrc.w&6, mh, pxsrc.pitch);
					}
					break;

				case nsVDPixmap::kPixFormat_RGB888:
					do {
						*lummap++ = asm_scene_lumtile24(src, 8, mh, pxsrc.pitch);
						src += 24;
					} while(--w);

					if (pxsrc.w & 7) {
						*lummap++ = asm_scene_lumtile24(src, pxsrc.w&7, mh, pxsrc.pitch);
					}
					break;

				case nsVDPixmap::kPixFormat_XRGB8888:
					do {
						*lummap++ = asm_scene_lumtile32(src, 8, mh, pxsrc.pitch);
						src += 32;
					} while(--w);

					if (pxsrc.w & 7) {
						*lummap++ = asm_scene_lumtile32(src, pxsrc.w&7, mh, pxsrc.pitch);
					}
					break;

				case nsVDPixmap::kPixFormat_YUV422_UYVY:
				case nsVDPixmap::kPixFormat_YUV422_UYVY_709:
				case nsVDPixmap::kPixFormat_YUV422_UYVY_FR:
				case nsVDPixmap::kPixFormat_YUV422_UYVY_709_FR:
					do {
						*lummap++ = scene_lumtileUYVY(src, 8, mh, pxsrc.pitch);
						src += 16;
					} while(--w);

					if (pxsrc.w & 6) {
						*lummap++ = scene_lumtileUYVY(src, pxsrc.w&6, mh, pxsrc.pitch);
					}
					break;

				case nsVDPixmap::kPixFormat_YUV422_YUYV:
				case nsVDPixmap::kPixFormat_YUV422_YUYV_709:
				case nsVDPixmap::kPixFormat_YUV422_YUYV_FR:
				case nsVDPixmap::kPixFormat_YUV422_YUYV_709_FR:
					do {
						*lummap++ = scene_lumtileYUY2(src, 8, mh, pxsrc.pitch);
						src += 16;
					} while(--w);

					if (pxsrc.w & 6) {
						*lummap++ = scene_lumtileYUY2(src, pxsrc.w&6, mh, pxsrc.pitch);
					}
					break;

				case nsVDPixmap::kPixFormat_Y8:
					do {
						*lummap++ = scene_lumtileY8(src, 8, mh, pxsrc.pitch);
						src += 8;
					} while(--w);

					if (pxsrc.w & 7)
						*lummap++ = scene_lumtileY8(src, pxsrc.w&7, mh, pxsrc.pitch);
					break;

				case nsVDPixmap::kPixFormat_Y8_FR:
					do {
						*lummap++ = scene_lumtileI8(src, 8, mh, pxsrc.pitch);
						src += 8;
					} while(--w);

					if (pxsrc.w & 7)
						*lummap++ = scene_lumtileI8(src, pxsrc.w&7, mh, pxsrc.pitch);
					break;

				case nsVDPixmap::kPixFormat_XRGB64:
				case nsVDPixmap::kPixFormat_YUV444_Planar16:
				case nsVDPixmap::kPixFormat_YUV422_Planar16:
				case nsVDPixmap::kPixFormat_YUV420_Planar16:
				case nsVDPixmap::kPixFormat_Y16:
				case nsVDPixmap::kPixFormat_XYUV64:
					break;

				default:
					VDASSERTCT(nsVDPixmap::kPixFormat_Max_Standard == nsVDPixmap::kPixFormat_XYUV64 + 1);
					VDASSERT(false);
			}
		} while(--h);
	} else if (formatInfo.auxbufs == 1) {
		const uint8 *srcRowY = (const uint8 *)pxsrc.data;
		const uint8 *srcRowC = (const uint8 *)pxsrc.data2;
		const ptrdiff_t pitchY = pxsrc.pitch;
		const ptrdiff_t pitchC = pxsrc.pitch2;
		const ptrdiff_t pitchRowY = pitchY * 8;
		const ptrdiff_t pitchRowC = pitchC << (3 - formatInfo.auxhbits);

		do {
			if (h<=1 && (pxsrc.h&7))
				mh = pxsrc.h&7;

			const uint8 *srcY = srcRowY;
			const uint8 *srcC = srcRowC;

			srcRowY += pitchRowY;
			srcRowC += pitchRowC;

			w = pxsrc.w >> 3;

			switch(pxsrc.format) {
				case nsVDPixmap::kPixFormat_YUV420_NV12:
					do {
						*lummap++ = scene_lumtileNV12(srcY, pitchY, srcC, pitchC, 8, mh);
						srcY += 8;
						srcC += 8;
					} while(--w);

					if (pxsrc.w & 7)
						*lummap++ = scene_lumtileNV12(srcY, pitchY, srcC, pitchC, pxsrc.w & 7, mh);
					break;
			}
		} while(--h);
	} else {
		const uint8 *srcRowY = (const uint8 *)pxsrc.data;
		const uint8 *srcRowCb = (const uint8 *)pxsrc.data2;
		const uint8 *srcRowCr = (const uint8 *)pxsrc.data3;
		const ptrdiff_t pitchY = pxsrc.pitch;
		const ptrdiff_t pitchCb = pxsrc.pitch2;
		const ptrdiff_t pitchCr = pxsrc.pitch3;
		const ptrdiff_t pitchRowY = pitchY * 8;
		const ptrdiff_t pitchRowCb = pitchCr << (3 - formatInfo.auxhbits);
		const ptrdiff_t pitchRowCr = pitchCb << (3 - formatInfo.auxhbits);

		do {
			if (h<=1 && (pxsrc.h&7))
				mh = pxsrc.h&7;

			const uint8 *srcY = srcRowY;
			const uint8 *srcCb = srcRowCb;
			const uint8 *srcCr = srcRowCr;

			srcRowY += pitchRowY;
			srcRowCb += pitchRowCb;
			srcRowCr += pitchRowCr;

			w = pxsrc.w >> 3;

			switch(pxsrc.format) {
				case nsVDPixmap::kPixFormat_YUV444_Planar:
				case nsVDPixmap::kPixFormat_YUV444_Planar_709:
				case nsVDPixmap::kPixFormat_YUV444_Planar_FR:
				case nsVDPixmap::kPixFormat_YUV444_Planar_709_FR:
					do {
						*lummap++ = scene_lumtileYCbCrPlanar<0, 0>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, 8, mh);
						srcY += 8;
						srcCb += 8;
						srcCr += 8;
					} while(--w);

					if (pxsrc.w & 7)
						*lummap++ = scene_lumtileYCbCrPlanar<0, 0>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, pxsrc.w & 7, mh);
					break;

				case nsVDPixmap::kPixFormat_YUV422_Planar:
				case nsVDPixmap::kPixFormat_YUV422_Planar_709:
				case nsVDPixmap::kPixFormat_YUV422_Planar_FR:
				case nsVDPixmap::kPixFormat_YUV422_Planar_709_FR:
				case nsVDPixmap::kPixFormat_YUV422_Planar_Centered:
					do {
						*lummap++ = scene_lumtileYCbCrPlanar<1, 0>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, 8, mh);
						srcY += 8;
						srcCb += 4;
						srcCr += 4;
					} while(--w);

					if (pxsrc.w & 7)
						*lummap++ = scene_lumtileYCbCrPlanar<1, 0>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, pxsrc.w & 7, mh);
					break;

				case nsVDPixmap::kPixFormat_YUV411_Planar:
				case nsVDPixmap::kPixFormat_YUV411_Planar_709:
				case nsVDPixmap::kPixFormat_YUV411_Planar_FR:
				case nsVDPixmap::kPixFormat_YUV411_Planar_709_FR:
					do {
						*lummap++ = scene_lumtileYCbCrPlanar<2, 0>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, 8, mh);
						srcY += 8;
						srcCb += 2;
						srcCr += 2;
					} while(--w);

					if (pxsrc.w & 7)
						*lummap++ = scene_lumtileYCbCrPlanar<2, 0>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, pxsrc.w & 7, mh);
					break;

				case nsVDPixmap::kPixFormat_YUV420_Planar:
				case nsVDPixmap::kPixFormat_YUV420_Planar_709:
				case nsVDPixmap::kPixFormat_YUV420_Planar_FR:
				case nsVDPixmap::kPixFormat_YUV420_Planar_709_FR:
				case nsVDPixmap::kPixFormat_YUV420_Planar_Centered:
				case nsVDPixmap::kPixFormat_YUV420i_Planar:
				case nsVDPixmap::kPixFormat_YUV420i_Planar_709:
				case nsVDPixmap::kPixFormat_YUV420i_Planar_FR:
				case nsVDPixmap::kPixFormat_YUV420i_Planar_709_FR:
				case nsVDPixmap::kPixFormat_YUV420it_Planar:
				case nsVDPixmap::kPixFormat_YUV420it_Planar_709:
				case nsVDPixmap::kPixFormat_YUV420it_Planar_FR:
				case nsVDPixmap::kPixFormat_YUV420it_Planar_709_FR:
				case nsVDPixmap::kPixFormat_YUV420ib_Planar:
				case nsVDPixmap::kPixFormat_YUV420ib_Planar_709:
				case nsVDPixmap::kPixFormat_YUV420ib_Planar_FR:
				case nsVDPixmap::kPixFormat_YUV420ib_Planar_709_FR:
					do {
						*lummap++ = scene_lumtileYCbCrPlanar<1, 1>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, 8, mh);
						srcY += 8;
						srcCb += 4;
						srcCr += 4;
					} while(--w);

					if (pxsrc.w & 7)
						*lummap++ = scene_lumtileYCbCrPlanar<1, 1>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, pxsrc.w & 7, mh);
					break;

				case nsVDPixmap::kPixFormat_YUV410_Planar:
				case nsVDPixmap::kPixFormat_YUV410_Planar_709:
				case nsVDPixmap::kPixFormat_YUV410_Planar_FR:
				case nsVDPixmap::kPixFormat_YUV410_Planar_709_FR:
					do {
						*lummap++ = scene_lumtileYCbCrPlanar<2, 2>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, 8, mh);
						srcY += 8;
						srcCb += 2;
						srcCr += 2;
					} while(--w);

					if (pxsrc.w & 7)
						*lummap++ = scene_lumtileYCbCrPlanar<2, 2>(srcY, pitchY, srcCb, pitchCb, srcCr, pitchCr, pxsrc.w & 7, mh);
					break;

				case nsVDPixmap::kPixFormat_YUV422_Planar_16F:
				case nsVDPixmap::kPixFormat_YUV422_V210:
#pragma vdpragma_TODO("Must implement 16F and V210 formats in scene detector")
					break;
			}
		} while(--h);
	}
}

void SceneDetector::FlipBuffers() {
	mCurrentLummap.swap(mPrevLummap);
}

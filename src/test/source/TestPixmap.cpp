#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/system/math.h>
#include <vd2/system/memory.h>
#include <vd2/system/vectors.h>
#include <tchar.h>
#include "test.h"

DEFINE_TEST(Pixmap) {
	using namespace nsVDPixmap;

	// test pal1
	for(int format=kPixFormat_Pal1; format<=kPixFormat_Pal8; ++format) {

		_tprintf(_T("    Testing format %hs\n"), VDPixmapGetInfo(format).name);

		int testw = 2048 >> (format - kPixFormat_Pal1);
		int teststep = 8 >> (format - kPixFormat_Pal1);

		VDPixmapBuffer srcbuf(testw, 2, format);

		int palcount = 1 << (1 << (format - kPixFormat_Pal1));
		for(int k=0; k<palcount; ++k) {
			uint32 v = 0;

			if (k & 1)
				v |= 0x000000ff;
			if (k & 2)
				v |= 0x0000ff00;
			if (k & 4)
				v |= 0x00ff0000;
			if (k & 8)
				v |= 0xff000000;

			((uint32 *)srcbuf.palette)[k] = v;
		}

		for(int q=0; q<256; ++q)
			((uint8 *)srcbuf.data)[q] = ((uint8 *)srcbuf.data)[srcbuf.pitch + q] = (uint8)q;

		VDInvertMemory(vdptroffset(srcbuf.data, srcbuf.pitch), 256);

		VDPixmapBuffer intbuf[4];
		
		intbuf[0].init(testw, 2, kPixFormat_XRGB1555);
		intbuf[1].init(testw, 2, kPixFormat_RGB565);
		intbuf[2].init(testw, 2, kPixFormat_RGB888);
		intbuf[3].init(testw, 2, kPixFormat_XRGB8888);

		VDPixmapBuffer dstbuf(testw, 2, kPixFormat_RGB888);

		for(int x1=0; x1<testw; x1+=teststep) {
			int xlimit = std::min<int>(testw, x1+64);
			for(int x2=x1+8; x2<xlimit; x2+=teststep) {
				for(int i=0; i<4; ++i) {
					VDMemset8Rect(intbuf[i].data, intbuf[i].pitch, 0, intbuf[i].w * VDPixmapGetInfo(intbuf[i].format).qsize, intbuf[i].h);
					VDVERIFY(VDPixmapBlt(intbuf[i], x1, 0, srcbuf, x1, 0, x2-x1, 2));
				}

				for(int j=0; j<3; ++j) {
					VDMemset8Rect(dstbuf.data, dstbuf.pitch, 0, 3*dstbuf.w, dstbuf.h);
					VDVERIFY(VDPixmapBlt(dstbuf, intbuf[j]));

					VDVERIFY(!VDCompareRect(intbuf[2].data, intbuf[2].pitch, dstbuf.data, dstbuf.pitch, 3*dstbuf.w, dstbuf.h));
				}
			}
		}
	}

	static const uint32 kColors[11]={
		0xff000000,
		0xffffffff,
		0xff0000ff,
		0xff00ff00,
		0xff00ffff,
		0xffff0000,
		0xffff00ff,
		0xffffff00,
	};

	static const uint32 kColorsYCCFull[11]={
		0xff008080,
		0xffff8080,
		0xff800000,
		0xff800080,
		0xff8000ff,
		0xff808000,
		0xff808080,
		0xff8080ff,
		0xff80ff00,
		0xff80ff80,
		0xff80ffff,
	};

	static const uint32 kColorsYCCLimited[11]={
		0xff108080,
		0xffeb8080,
		0xff7e1010,
		0xff7e1080,
		0xff7ef0f0,
		0xff7e8010,
		0xff7e8080,
		0xff7e80f0,
		0xff7ef010,
		0xff7ef080,
		0xff7ef0f0,
	};

	uint8 checkvals[5][5][11][3];

	vdfloat3x3 ycc601_to_rgb;
	vdfloat3x3 ycc709_to_rgb;

	ycc601_to_rgb.m[0].set(1.0f, 1.0f, 1.0f);
	ycc601_to_rgb.m[1].set(0.0f, -0.3441363f, 1.772f);
	ycc601_to_rgb.m[2].set(1.402f, -0.7141363f, 0.0f);

	ycc709_to_rgb.m[0].set(1.0f, 1.0f, 1.0f);
	ycc709_to_rgb.m[1].set(0.0f, -0.1873243f, 1.8556f);
	ycc709_to_rgb.m[2].set(1.5748f, -0.4681243f, 0.0f);

	vdfloat3x3 rgb_to_ycc601(~ycc601_to_rgb);
	vdfloat3x3 rgb_to_ycc709(~ycc709_to_rgb);

	const vdfloat3c lrscale(219.0f / 255.0f, 224.0f / 255.0f, 224.0f / 255.0f);
	const vdfloat3c lrbias(16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);
	const vdfloat3c frbias(0, 128.0f / 255.0f, 128.0f / 255.0f);

	const vdfloat3c inlrbias(16.0f/255.0f, 128.0f/255.0f, 128.0f/255.0f);
	const vdfloat3c inlrscale(255.0f/219.0f, 255.0f/224.0f, 255.0f/224.0f);
	const vdfloat3c infrbias(0.0f, 128.0f/255.0f, 128.0f/255.0f);
	const vdfloat3c infrscale(1.0f, 1.0f, 1.0f);

	for(int srcmode = 0; srcmode < 5; ++srcmode) {
		vdfloat3 rgb[11];

		const uint32 *testcolors;
		switch(srcmode) {
			case 0:
				testcolors = kColors;
				break;

			case 1:
			case 2:
				testcolors = kColorsYCCLimited;
				break;

			case 3:
			case 4:
				testcolors = kColorsYCCFull;
				break;
		}

		for(int i = 0; i < 11; ++i) {
			rgb[i].x = (float)(testcolors[i] & 0xff0000) / (float)0xff0000;
			rgb[i].y = (float)(testcolors[i] & 0x00ff00) / (float)0x00ff00;
			rgb[i].z = (float)(testcolors[i] & 0x0000ff) / (float)0x0000ff;

			switch(srcmode) {
				case 1:
					rgb[i] = ((rgb[i] - inlrbias) * inlrscale) * ycc601_to_rgb;
					break;

				case 2:				
					rgb[i] = ((rgb[i] - inlrbias) * inlrscale) * ycc709_to_rgb;
					break;
			
				case 3:				
					rgb[i] = ((rgb[i] - infrbias) * infrscale) * ycc601_to_rgb;
					break;
			
				case 4:				
					rgb[i] = ((rgb[i] - infrbias) * infrscale) * ycc709_to_rgb;
					break;
			}
		}

		for(int dstmode = 0; dstmode < 5; ++dstmode) {
			for(int i = 0; i < 11; ++i) {
				vdfloat3 out(rgb[i]);

				switch(dstmode) {
					case 0:
						break;

					case 1:
						out = out * rgb_to_ycc601 * lrscale + lrbias;
						break;

					case 2:
						out = out * rgb_to_ycc709 * lrscale + lrbias;
						break;

					case 3:
						out = out * rgb_to_ycc601 + frbias;
						break;

					case 4:
						out = out * rgb_to_ycc709 + frbias;
						break;
				}

				checkvals[srcmode][dstmode][i][0] = VDClampedRoundFixedToUint8Fast(out.x);
				checkvals[srcmode][dstmode][i][1] = VDClampedRoundFixedToUint8Fast(out.y);
				checkvals[srcmode][dstmode][i][2] = VDClampedRoundFixedToUint8Fast(out.z);
			}
		}
	}

	// test primary color conversion
	VDPixmapBuffer srcarray[11];
	for(int size=7; size<=9; ++size) {
		for(int srcformat = nsVDPixmap::kPixFormat_XRGB1555; srcformat < nsVDPixmap::kPixFormat_Max_Standard; ++srcformat) {
			if (srcformat == kPixFormat_YUV444_XVYU)
				continue;

			int srccheckidx = 0;

			switch(srcformat) {
				// RGB formats
				case kPixFormat_XRGB1555:
				case kPixFormat_RGB565:
				case kPixFormat_RGB888:
				case kPixFormat_XRGB8888:
					for(int i=0; i<8; ++i) {
						VDPixmapBuffer& src = srcarray[i];
						src.init(size, size, kPixFormat_XRGB8888);

						VDMemset32Rect(src.data, src.pitch, kColors[i], size, size);
					}
					srccheckidx = 0;
					break;

				// YCbCr (Rec.601 limited range)
				case kPixFormat_Y8:
				case kPixFormat_YUV422_UYVY:
				case kPixFormat_YUV422_YUYV:
				case kPixFormat_YUV444_Planar:
				case kPixFormat_YUV422_Planar:
				case kPixFormat_YUV420_Planar:
				case kPixFormat_YUV411_Planar:
				case kPixFormat_YUV410_Planar:
				case kPixFormat_YUV422_Planar_Centered:
				case kPixFormat_YUV420_Planar_Centered:
				case kPixFormat_YUV422_Planar_16F:
				case kPixFormat_YUV422_V210:
				case kPixFormat_YUV420i_Planar:
				case kPixFormat_YUV420it_Planar:
				case kPixFormat_YUV420ib_Planar:
					for(int i=0; i<11; ++i) {
						VDPixmapBuffer& src = srcarray[i];
						src.init(size, size, kPixFormat_YUV444_Planar);

						VDMemset8Rect(src.data, src.pitch, (uint8)(kColorsYCCLimited[i] >> 16), size, size);
						VDMemset8Rect(src.data2, src.pitch2, (uint8)(kColorsYCCLimited[i] >> 8), size, size);
						VDMemset8Rect(src.data3, src.pitch3, (uint8)(kColorsYCCLimited[i] >> 0), size, size);
					}
					srccheckidx = 1;
					break;

				// YCbCr (Rec.709 limited range)
				case kPixFormat_YUV422_UYVY_709:
				case kPixFormat_YUV420_NV12:
				case kPixFormat_YUV422_YUYV_709:
				case kPixFormat_YUV444_Planar_709:
				case kPixFormat_YUV422_Planar_709:
				case kPixFormat_YUV420_Planar_709:
				case kPixFormat_YUV411_Planar_709:
				case kPixFormat_YUV410_Planar_709:
				case kPixFormat_YUV420i_Planar_709:
				case kPixFormat_YUV420it_Planar_709:
				case kPixFormat_YUV420ib_Planar_709:
					for(int i=0; i<11; ++i) {
						VDPixmapBuffer& src = srcarray[i];
						src.init(size, size, kPixFormat_YUV444_Planar_709);

						VDMemset8Rect(src.data, src.pitch, (uint8)(kColorsYCCLimited[i] >> 16), size, size);
						VDMemset8Rect(src.data2, src.pitch2, (uint8)(kColorsYCCLimited[i] >> 8), size, size);
						VDMemset8Rect(src.data3, src.pitch3, (uint8)(kColorsYCCLimited[i] >> 0), size, size);
					}
					srccheckidx = 2;
					break;

				// YCbCr (Rec.601 full range)
				case kPixFormat_YUV422_UYVY_FR:
				case kPixFormat_YUV422_YUYV_FR:
				case kPixFormat_YUV444_Planar_FR:
				case kPixFormat_YUV422_Planar_FR:
				case kPixFormat_YUV420_Planar_FR:
				case kPixFormat_YUV411_Planar_FR:
				case kPixFormat_YUV410_Planar_FR:
				case kPixFormat_YUV420i_Planar_FR:
				case kPixFormat_YUV420it_Planar_FR:
				case kPixFormat_YUV420ib_Planar_FR:
					for(int i=0; i<11; ++i) {
						VDPixmapBuffer& src = srcarray[i];
						src.init(size, size, kPixFormat_YUV444_Planar_FR);

						VDMemset8Rect(src.data, src.pitch, (uint8)(kColorsYCCFull[i] >> 16), size, size);
						VDMemset8Rect(src.data2, src.pitch2, (uint8)(kColorsYCCFull[i] >> 8), size, size);
						VDMemset8Rect(src.data3, src.pitch3, (uint8)(kColorsYCCFull[i] >> 0), size, size);
					}
					srccheckidx = 3;
					break;

				// YCbCr (Rec.709 limited range)
				case kPixFormat_YUV422_UYVY_709_FR:
				case kPixFormat_YUV422_YUYV_709_FR:
				case kPixFormat_YUV444_Planar_709_FR:
				case kPixFormat_YUV422_Planar_709_FR:
				case kPixFormat_YUV420_Planar_709_FR:
				case kPixFormat_YUV411_Planar_709_FR:
				case kPixFormat_YUV410_Planar_709_FR:
				case kPixFormat_YUV420i_Planar_709_FR:
				case kPixFormat_YUV420it_Planar_709_FR:
				case kPixFormat_YUV420ib_Planar_709_FR:
					for(int i=0; i<11; ++i) {
						VDPixmapBuffer& src = srcarray[i];
						src.init(size, size, kPixFormat_YUV444_Planar_709_FR);

						VDMemset8Rect(src.data, src.pitch, (uint8)(kColorsYCCFull[i] >> 16), size, size);
						VDMemset8Rect(src.data2, src.pitch2, (uint8)(kColorsYCCFull[i] >> 8), size, size);
						VDMemset8Rect(src.data3, src.pitch3, (uint8)(kColorsYCCFull[i] >> 0), size, size);
					}
					srccheckidx = 4;
					break;
			}

			VDPixmapBuffer inarray[11];
			
			for(int i=0; i<(srccheckidx ? 11 : 8); ++i) {
				inarray[i].init(size, size, srcformat);

				VDPixmapBlt(inarray[i], srcarray[i]);
			}

			_tprintf(_T("    Testing source format %hs (size=%d)\n"), VDPixmapGetInfo(srcformat).name, size);

			VDPixmapBuffer in(size, size, srcformat);

			for(int dstformat = nsVDPixmap::kPixFormat_XRGB1555; dstformat < nsVDPixmap::kPixFormat_Max_Standard; ++dstformat) {
				if (dstformat == kPixFormat_YUV444_XVYU)
					continue;

				int checkformat = kPixFormat_XRGB8888;
				int dstcheckidx = 0;
				switch(dstformat) {
					// RGB formats
					case kPixFormat_XRGB1555:
					case kPixFormat_RGB565:
					case kPixFormat_RGB888:
					case kPixFormat_XRGB8888:
						checkformat = kPixFormat_XRGB8888;
						dstcheckidx = 0;
						break;

					// YCbCr (Rec.601 limited range)
					case kPixFormat_Y8:
					case kPixFormat_YUV422_UYVY:
					case kPixFormat_YUV422_YUYV:
					case kPixFormat_YUV444_Planar:
					case kPixFormat_YUV422_Planar:
					case kPixFormat_YUV420_Planar:
					case kPixFormat_YUV411_Planar:
					case kPixFormat_YUV410_Planar:
					case kPixFormat_YUV422_Planar_Centered:
					case kPixFormat_YUV420_Planar_Centered:
					case kPixFormat_YUV422_Planar_16F:
					case kPixFormat_YUV422_V210:
					case kPixFormat_YUV420i_Planar:
					case kPixFormat_YUV420it_Planar:
					case kPixFormat_YUV420ib_Planar:
						checkformat = kPixFormat_YUV444_Planar;
						dstcheckidx = 1;
						break;

					// YCbCr (Rec.709 limited range)
					case kPixFormat_YUV422_UYVY_709:
					case kPixFormat_YUV420_NV12:
					case kPixFormat_YUV422_YUYV_709:
					case kPixFormat_YUV444_Planar_709:
					case kPixFormat_YUV422_Planar_709:
					case kPixFormat_YUV420_Planar_709:
					case kPixFormat_YUV411_Planar_709:
					case kPixFormat_YUV410_Planar_709:
					case kPixFormat_YUV420i_Planar_709:
					case kPixFormat_YUV420it_Planar_709:
					case kPixFormat_YUV420ib_Planar_709:
						checkformat = kPixFormat_YUV444_Planar_709;
						dstcheckidx = 2;
						break;

					// YCbCr (Rec.601 full range)
					case kPixFormat_Y8_FR:
					case kPixFormat_YUV422_UYVY_FR:
					case kPixFormat_YUV422_YUYV_FR:
					case kPixFormat_YUV444_Planar_FR:
					case kPixFormat_YUV422_Planar_FR:
					case kPixFormat_YUV420_Planar_FR:
					case kPixFormat_YUV411_Planar_FR:
					case kPixFormat_YUV410_Planar_FR:
					case kPixFormat_YUV420i_Planar_FR:
					case kPixFormat_YUV420it_Planar_FR:
					case kPixFormat_YUV420ib_Planar_FR:
						checkformat = kPixFormat_YUV444_Planar_FR;
						dstcheckidx = 3;
						break;

					// YCbCr (Rec.709 limited range)
					case kPixFormat_YUV422_UYVY_709_FR:
					case kPixFormat_YUV422_YUYV_709_FR:
					case kPixFormat_YUV444_Planar_709_FR:
					case kPixFormat_YUV422_Planar_709_FR:
					case kPixFormat_YUV420_Planar_709_FR:
					case kPixFormat_YUV411_Planar_709_FR:
					case kPixFormat_YUV410_Planar_709_FR:
					case kPixFormat_YUV420i_Planar_709_FR:
					case kPixFormat_YUV420it_Planar_709_FR:
					case kPixFormat_YUV420ib_Planar_709_FR:
						checkformat = kPixFormat_YUV444_Planar_709_FR;
						dstcheckidx = 4;
						break;
				}

				VDPixmapBuffer out(size, size, dstformat);
				VDPixmapBuffer output(size, size, checkformat);

				int maxtest = (srcformat == kPixFormat_Y8 ||
					srcformat == kPixFormat_Y8_FR ||
					dstformat == kPixFormat_Y8 ||
					dstformat == kPixFormat_Y8_FR
					) ? 2 : srccheckidx ? 11 : 8;

				for(int v=0; v<maxtest; ++v) {
					const VDPixmap& in = inarray[v];

					VDVERIFY(VDPixmapBlt(out, in));
					VDVERIFY(VDPixmapBlt(output, out));

					const uint8 *testvals = checkvals[srccheckidx][dstcheckidx][v];
					const int check_r = testvals[0];
					const int check_g = testvals[1];
					const int check_b = testvals[2];

					if (dstcheckidx == 0) {
						int rb_thresh = 1;
						int g_thresh = 1;

						switch(dstformat) {
							case kPixFormat_XRGB1555:
								rb_thresh = 8;
								g_thresh = 8;
								break;

							case kPixFormat_RGB565:
								rb_thresh = 8;
								g_thresh = 4;
								break;
						}

						for(int y=0; y<size; ++y) {
							uint32 *dp = (uint32 *)vdptroffset(output.data, output.pitch*y);

							for(int x=0; x<size; ++x) {
								const uint32 dpx = dp[x];
								int dr = (int)((dpx>>16)&0xff);
								int dg = (int)((dpx>> 8)&0xff);
								int db = (int)((dpx    )&0xff);

								const int re = check_r - dr;
								const int ge = check_g - dg;
								const int be = check_b - db;

								if (abs(re) > rb_thresh || abs(ge) > g_thresh || abs(be) > rb_thresh) {
									printf("        Failed: %s -> %s (color index %d)\n", VDPixmapGetInfo(srcformat).name, VDPixmapGetInfo(dstformat).name, v);
									printf("            (%d,%d) %02x,%02x,%02x != %02x,%02x,%02x\n", x, y, dr, dg, db, check_r, check_g, check_b);
									printf("            Input bytes  @(0,0): %02x %02x %02x %02x\n"
											, ((const uint8 *)in.data)[0]
											, ((const uint8 *)in.data)[1]
											, ((const uint8 *)in.data)[2]
											, ((const uint8 *)in.data)[3]);
									printf("            Output bytes @(0,0): %02x %02x %02x %02x\n"
											, ((const uint8 *)out.data)[0]
											, ((const uint8 *)out.data)[1]
											, ((const uint8 *)out.data)[2]
											, ((const uint8 *)out.data)[3]);

									VDASSERT(false);
									goto failed;
								}
							}
						}
					} else if (maxtest == 2) {
						for(int y=0; y<size; ++y) {
							uint8 *dp0 = (uint8 *)vdptroffset(output.data, output.pitch*y);

							for(int x=0; x<size; ++x) {
								int dr = (int)dp0[x];

								const int re = check_r - dr;

								if (abs(re) > 1) {
									printf("        Failed: %s -> %s (color index %d)\n", VDPixmapGetInfo(srcformat).name, VDPixmapGetInfo(dstformat).name, v);
									printf("            (%d,%d) %02x != %02x\n", x, y, dr, check_r);
									printf("            Input bytes  @(0,0): %02x %02x %02x %02x\n"
											, ((const uint8 *)in.data)[0]
											, ((const uint8 *)in.data)[1]
											, ((const uint8 *)in.data)[2]
											, ((const uint8 *)in.data)[3]);
									printf("            Output bytes @(0,0): %02x %02x %02x %02x\n"
											, ((const uint8 *)out.data)[0]
											, ((const uint8 *)out.data)[1]
											, ((const uint8 *)out.data)[2]
											, ((const uint8 *)out.data)[3]);

									VDASSERT(false);
									goto failed;
								}
							}
						}
					} else {
						for(int y=0; y<size; ++y) {
							uint8 *dp0 = (uint8 *)vdptroffset(output.data, output.pitch*y);
							uint8 *dp1 = (uint8 *)vdptroffset(output.data2, output.pitch2*y);
							uint8 *dp2 = (uint8 *)vdptroffset(output.data3, output.pitch3*y);

							for(int x=0; x<size; ++x) {
								int dr = (int)dp0[x];
								int dg = (int)dp1[x];
								int db = (int)dp2[x];

								const int re = check_r - dr;
								const int ge = check_g - dg;
								const int be = check_b - db;

								if (abs(re) > 1 || abs(ge) > 1 || abs(be) > 1) {
									printf("        Failed: %s -> %s (color index %d)\n", VDPixmapGetInfo(srcformat).name, VDPixmapGetInfo(dstformat).name, v);
									printf("            (%d,%d) %02x,%02x,%02x != %02x,%02x,%02x\n", x, y, dr, dg, db, check_r, check_g, check_b);
									printf("            Input bytes  @(0,0): %02x %02x %02x %02x\n"
											, ((const uint8 *)in.data)[0]
											, ((const uint8 *)in.data)[1]
											, ((const uint8 *)in.data)[2]
											, ((const uint8 *)in.data)[3]);
									printf("            Output bytes @(0,0): %02x %02x %02x %02x\n"
											, ((const uint8 *)out.data)[0]
											, ((const uint8 *)out.data)[1]
											, ((const uint8 *)out.data)[2]
											, ((const uint8 *)out.data)[3]);

									VDASSERT(false);
									goto failed;
								}
							}
						}
					}
	failed:;
				}
			}
		}
	}
	return 0;
}


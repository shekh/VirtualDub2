#include "test.h"
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixel.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "../../Kasumi/h/uberblit.h"
#include "../../Kasumi/h/uberblit_16f.h"
#include "../../Kasumi/h/uberblit_rgb64.h"
#include "../../Kasumi/h/uberblit_input.h"

namespace {
	bool can_sample(const VDPixmap& px) {
		switch (px.format) {
		case nsVDPixmap::kPixFormat_YUV422_YU64:
		case nsVDPixmap::kPixFormat_B64A:
			return false;
		}
		return true;
	}

	bool CheckBlit(const VDPixmap& dst, const VDPixmap& src, uint32 color) {
		uint32 w = std::min<uint32>(src.w, dst.w);
		uint32 h = std::min<uint32>(src.h, dst.h);

		for(uint32 y=0; y<h; ++y) {
			for(uint32 x=0; x<w; ++x) {
				uint32 p1 = VDPixmapSample(src, x, y);
				uint32 p2 = VDPixmapSample(dst, x, y);

				if (!can_sample(src)) p1 = color;
				if (!can_sample(dst)) p2 = color;

				int y1 = ((p1 & 0xff00ff)*0x130036 + (p1 & 0xff00)*0xb700) >> 16;
				int y2 = ((p2 & 0xff00ff)*0x130036 + (p2 & 0xff00)*0xb700) >> 16;

				if (abs(y1 - y2) > 512) {
					printf("Bitmap comparison failed at (%d, %d) with formats %s and %s: #%06x != #%06x (diff=%d)"
						, x
						, y
						, VDPixmapGetInfo(src.format).name
						, VDPixmapGetInfo(dst.format).name
						, p1 & 0xffffff
						, p2 & 0xffffff
						, abs(y1 - y2)
						);

					VDASSERT(false);
					return false;
				}
			}
		}

		return true;
	}
}

void test_normalize_bias() {
  int width = 4;
  uint16 data[4];
  uint16 data2[4];

  VDPixmapGenSrc src;
  src.Init(width,1,kVDPixType_16_LE,width*2);
  src.SetSource(data,width*2,0);

  {for(int x=0; x<8; x++){
    FilterModPixmapInfo info;
    info.ref_r = (256<<x)-1;
    data[0] = 0;
    data[1] = 128<<x;
    data[2] = (256<<x)-1;
    data[3] = 65535;

    {for(int y=0; y<8; y++){
      if(x==y) continue;

      FilterModPixmapInfo info2;

      VDPixmapGen_Y16_Normalize gen(true);
      gen.max_value = (256<<y)-1;
      gen.Init(&src,0);
      gen.TransformPixmapInfo(info,info2);
      gen.AddWindowRequest(1,1);
      gen.Start();

      gen.ProcessRow(data2,0);

      double f1 = (double(info.ref_r)-(128<<x))/info.ref_r*gen.max_value+(128<<y);
      double f0 = (double(0)-(128<<x))/info.ref_r*gen.max_value+(128<<y);
      
      int d1 = ((info.ref_r-(128<<x))*gen.max_value*2/info.ref_r+1)/2 + (128<<y);
      int d0 = ((int64(0)-(128<<x))*gen.max_value*2/info.ref_r-1)/2 + (128<<y);
      if(d0<0) d0=0;

      if(data2[1]!=128<<y) VDASSERT(false);
      if(data2[0]!=d0) VDASSERT(false);
      if(data2[2]!=d1) VDASSERT(false);
    }}
  }}
}

void test_normalize() {
  int width = 65536;
  /*
  uint16* data = (uint16*)malloc(width*2);
  uint16* data2 = (uint16*)malloc(width*2);

  {for(int i=0; i<width; i++) data[i] = i; }

  VDPixmapGenSrc src;
  src.Init(width,1,kVDPixType_16_LE,width*2);
  src.SetSource(data,width*2,0);

  FilterModPixmapInfo info;
  info.ref_r = 0x3FF;
  FilterModPixmapInfo info2;

  VDPixmapGen_Y16_Normalize gen(true);
  gen.max_value = 0xFF;
  gen.Init(&src,0);
  gen.TransformPixmapInfo(info,info2);
  gen.AddWindowRequest(1,1);
  gen.Start();

  gen.ProcessRow(data2,0);
  */
  /*
  uint8* data = (uint8*)malloc(width);
  uint16* data2 = (uint16*)malloc(width*2);

  {for(int i=0; i<width; i++) data[i] = i; }

  VDPixmapGenSrc src;
  src.Init(width,1,kVDPixType_8,width);
  src.SetSource(data,width,0);

  FilterModPixmapInfo info;
  FilterModPixmapInfo info2;
  VDPixmapGen_8_To_16 gen;
  gen.Init(&src,0);
  gen.TransformPixmapInfo(info,info2);
  gen.AddWindowRequest(1,1);
  gen.Start();

  gen.ProcessRow(data2,0);
  */

  uint16* data = (uint16*)malloc(width*2);
  uint16* data2 = (uint16*)malloc(width*2);

  {for(int i=0; i<width; i++) data[i] = i; }

  VDPixmapGenSrc src;
  src.Init(width/4,1,kVDPixType_16x4_LE,width*2);
  src.SetSource(data,width*2,0);

  FilterModPixmapInfo info;
  info.ref_r = 0xFF;
  info.ref_g = 0xFF;
  info.ref_b = 0xFF;
  FilterModPixmapInfo info2;

  VDPixmapGen_X16R16G16B16_Normalize gen;
  gen.max_r = 0xFFFF;
  gen.Init(&src,0);
  gen.TransformPixmapInfo(info,info2);
  gen.AddWindowRequest(1,1);
  gen.Start();

  gen.ProcessRow(data2,0);

  free(data);
  free(data2);
}

DEFINE_TEST(Uberblit) {
	using namespace nsVDPixmap;

  test_normalize_bias();
  //test_normalize();

	// test primary color conversion

	static const uint32 kColors[]={
		0xff000000,
		0xffffffff,

		0xff0000ff,
		0xff00ff00,
		0xff00ffff,
		0xffff0000,
		0xffff00ff,
		0xffffff00,
		0xffF49BC1,
		0xffED008C,
	};

	const int kColorCount = sizeof(kColors)/sizeof(kColors[0]);
	const int size = 8;

	VDPixmapBuffer src[kColorCount];
	for(int i=0; i<kColorCount; ++i) {
		src[i].init(size, size, kPixFormat_XRGB8888);
		src[i].info.alpha_type = FilterModPixmapInfo::kAlphaMask;
		VDMemset32Rect(src[i].data, src[i].pitch, kColors[i], size, size);
	}

	VDPixmapBuffer output(size, size, kPixFormat_XRGB8888);

	const VDPixmapFormatInfo& fiSrc = VDPixmapGetInfo(src[0].format);
	const VDPixmapFormatInfo& fiOutput = VDPixmapGetInfo(output.format);

	int src_start = nsVDPixmap::kPixFormat_XRGB1555;
	int src_end = nsVDPixmap::kPixFormat_Max_Standard-1;
	int dst_start = nsVDPixmap::kPixFormat_XRGB1555;
	int dst_end = nsVDPixmap::kPixFormat_Max_Standard-1;
	IVDPixmapExtraGen* extraDst = 0;
	#if 0
	ExtraGen_YUV_Normalize* normalize = new ExtraGen_YUV_Normalize;
	normalize->max_value = 1020;
	extraDst = normalize;
	#endif

	#if 0
	src_start = src_end = nsVDPixmap::kPixFormat_YUV444_Planar16;
	dst_start = dst_end = nsVDPixmap::kPixFormat_XRGB64;
	#endif

	for(int srcformat = src_start; srcformat <= src_end; ++srcformat) {
		VDPixmapFormat srcformat2 = (VDPixmapFormat)srcformat;

		if (srcformat == kPixFormat_YUV444_XVYU)
			continue;

		VDPixmapBuffer in[kColorCount];
		vdautoptr<IVDPixmapBlitter> blit1;
				const VDPixmapFormatInfo& fiIn = VDPixmapGetInfo(in[0].format);

		const int maxsrctest = (srcformat == kPixFormat_Y8 || srcformat == kPixFormat_Y8_FR || srcformat == kPixFormat_Y16) ? 2 : 8;

		for(int v=0; v<maxsrctest; ++v) {
			in[v].init(size, size, srcformat);

			if (!v)
				blit1 = VDPixmapCreateBlitter(in[0], src[0]);

			blit1->Blit(in[v], src[v]);
			if (!CheckBlit(in[v], src[v], kColors[v])) goto failed2;
			in[v].validate();
		}

		for(int dstformat = dst_start; dstformat <= dst_end; ++dstformat) {
			VDPixmapFormat dstformat2 = (VDPixmapFormat)dstformat;

			if (dstformat == kPixFormat_YUV444_XVYU)
				continue;


			VDPixmapBuffer out(size, size, dstformat);

			int maxtest = (srcformat == kPixFormat_Y8 ||
				srcformat == kPixFormat_Y8_FR ||
				srcformat == kPixFormat_Y16 ||
				dstformat == kPixFormat_Y8 ||
				dstformat == kPixFormat_Y8_FR ||
				dstformat == kPixFormat_Y16
				) ? 2 : 8;

			// convert src to dst
			vdautoptr<IVDPixmapBlitter> blit2(VDPixmapCreateBlitter(out, in[0], extraDst));

			// convert dst to rgb32
			vdautoptr<IVDPixmapBlitter> blit3(VDPixmapCreateBlitter(output, out));

			const VDPixmapFormatInfo& fiOut = VDPixmapGetInfo(out.format);

			for(int v=0; v<maxtest; ++v) {
				blit2->Blit(out, in[v]);
				if (!CheckBlit(out, in[v], kColors[v])) goto failed;
				out.validate();
				blit3->Blit(output, out);
				if (!CheckBlit(output, out, kColors[v])) goto failed;
				output.validate();

				// white and black must be exact
				if (v < 2) {
					for(int y=0; y<size; ++y) {
						const uint32 *sp = (const uint32 *)vdptroffset(src[v].data, src[v].pitch*y);
						uint32 *dp = (uint32 *)vdptroffset(output.data, output.pitch*y);

						for(int x=0; x<size; ++x) {
							const uint32 spx = sp[x];
							const uint32 dpx = dp[x];

							if ((spx ^ dpx) & 0xffffff) {
								printf("        Failed: %s -> %s\n", VDPixmapGetInfo(srcformat).name, VDPixmapGetInfo(dstformat).name);
								printf("            (%d,%d) %08lx != %08lx\n", x, y, spx, dpx);
								VDASSERT(false);
								goto failed;
							}
						}
					}
				} else {
					for(int y=0; y<size; ++y) {
						const uint32 *sp = (const uint32 *)vdptroffset(src[v].data, src[v].pitch*y);
						uint32 *dp = (uint32 *)vdptroffset(output.data, output.pitch*y);

						for(int x=0; x<size; ++x) {
							const uint32 spx = sp[x];
							const uint32 dpx = dp[x];

							const int re = (int)((spx>>16)&0xff) - (int)((dpx>>16)&0xff);
							const int ge = (int)((spx>> 8)&0xff) - (int)((dpx>> 8)&0xff);
							const int be = (int)((spx    )&0xff) - (int)((dpx    )&0xff);

							if (abs(re) > 3 || abs(ge) > 3 || abs(be) > 3) {
								printf("        Failed: %s -> %s\n", VDPixmapGetInfo(srcformat).name, VDPixmapGetInfo(dstformat).name);
								printf("            (%d,%d) %08lx != %08lx\n", x, y, spx, dpx);
								VDASSERT(false);
								goto failed;
							}
						}
					}
				}
failed:;
			}
		}
failed2:;
	}

	delete extraDst;
	return 0;
}

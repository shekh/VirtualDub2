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
		case nsVDPixmap::kPixFormat_B48R:
			return false;
		}
		return true;
	}

	bool CheckBlit(const VDPixmap& dst, const VDPixmap& src, uint32 color) {
		uint32 w = std::min<uint32>(src.w, dst.w);
		uint32 h = std::min<uint32>(src.h, dst.h);

		int error = 3;
		if (color>>24==0x40) {
			// not primary color
			error = 8;
		}
		if ((color>>24==0) || (color>>24==0xFF)) {
			// white and black must be exact
			error = 0;
		}

		for(uint32 y=0; y<h; ++y) {
			for(uint32 x=0; x<w; ++x) {
				uint32 p1 = VDPixmapSample(src, x, y);
				uint32 p2 = VDPixmapSample(dst, x, y);

				if (!can_sample(src)) p1 = color;
				if (!can_sample(dst)) p2 = color;

				int b1 = p1 & 0xFF;
				int b2 = p2 & 0xFF;
				int g1 = (p1>>8) & 0xFF;
				int g2 = (p2>>8) & 0xFF;
				int r1 = (p1>>16) & 0xFF;
				int r2 = (p2>>16) & 0xFF;
				int a1 = (p1>>24) & 0xFF;
				int a2 = (p2>>24) & 0xFF;

				int db = abs(b1-b2);
				int dg = abs(g1-g2);
				int dr = abs(r1-r2);
				int da = abs(a1-a2);
				if (!src.info.alpha_type) da = 0;
				if (!dst.info.alpha_type) da = 0;
				int d = db;
				if (dg>d) d = dg;
				if (dr>d) d = dr;
				if (da>d) d = da;

				if (d > error) {
					printf("Bitmap comparison failed at (%d, %d) with formats %s and %s: #%06x != #%06x (diff=%d)"
						, x
						, y
						, VDPixmapGetInfo(src.format).name
						, VDPixmapGetInfo(dst.format).name
						, p1
						, p2
						, d
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

  {for(int x=0; x<=8; x++){
    FilterModPixmapInfo info;
    info.ref_r = (256<<x)-1;
    data[0] = 0;
    data[1] = 128<<x;
    data[2] = (256<<x)-1;
    data[3] = 65535;

    {for(int y=0; y<=8; y++){
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

void test_normalize_scale() {
  int width = 4;
  uint16 data[4];
  uint16 data2[4];

  VDPixmapGenSrc src;
  src.Init(width,1,kVDPixType_16_LE,width*2);
  src.SetSource(data,width*2,0);

  {for(int x=0; x<=8; x++){
    FilterModPixmapInfo info;
    info.ref_r = 255*(1<<x);
    data[0] = 0;
    data[1] = 128<<x;
    data[2] = 240<<x;
    data[3] = 65535;

    {for(int y=0; y<=8; y++){
      if(x==y) continue;

      FilterModPixmapInfo info2;

      VDPixmapGen_Y16_Normalize gen(true);
      gen.max_value = 255*(1<<y);
      gen.Init(&src,0);
      gen.TransformPixmapInfo(info,info2);
      gen.AddWindowRequest(1,1);
      gen.Start();

      gen.ProcessRow(data2,0);

      int d3 = (256<<y)-1;
      if(y>x) d3 = ((256<<x)-1)*(1<<(y-x));

      if(data2[0]!=0) VDASSERT(false);
      if(data2[1]!=128<<y) VDASSERT(false);
      if(data2[2]!=240<<y) VDASSERT(false);
      if(data2[3]!=d3) VDASSERT(false);
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
  test_normalize_scale();
  //test_normalize();

	// test primary color conversion

	static const uint32 kColors[]={
		0x40102030,
		0x40F49BC1,
		0x40ED008C,

		0x110000ff,
		0x2200ff00,
		0x3300ffff,
		0x44ff0000,
		0x55ff00ff,
		0x66ffff00,

		0xff000000,
		0x00ffffff,
	};

	const int kColorCount = sizeof(kColors)/sizeof(kColors[0]);
	const int size = 8;

	VDPixmapBuffer ref[kColorCount];
	for(int i=0; i<kColorCount; ++i) {
		ref[i].init(size, size, kPixFormat_XRGB8888);
		ref[i].info.alpha_type = FilterModPixmapInfo::kAlphaMask;
		VDMemset32Rect(ref[i].data, ref[i].pitch, kColors[i], size, size);
	}

	VDPixmapBuffer chk(size, size, kPixFormat_XRGB8888);

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
		if (srcformat == kPixFormat_YUV444_XVYU) continue;

		VDPixmapBuffer src[kColorCount];
		src[0].init(size, size, srcformat);
		vdautoptr<IVDPixmapBlitter> blit1(VDPixmapCreateBlitter(src[0], ref[0]));

		const int maxsrctest = VDPixmapFormatGray(srcformat) ? 2 : kColorCount;

		for(int v=kColorCount-maxsrctest; v<kColorCount; ++v) {
			src[v].init(size, size, srcformat);
			blit1->Blit(src[v], ref[v]);
			if (!CheckBlit(src[v], ref[v], kColors[v])) goto failed2;
			src[v].validate();
		}

		for(int dstformat = dst_start; dstformat <= dst_end; ++dstformat) {
			VDPixmapFormat dstformat2 = (VDPixmapFormat)dstformat;
			if (dstformat == kPixFormat_YUV444_XVYU) continue;
			int maxtest = (VDPixmapFormatGray(srcformat) || VDPixmapFormatGray(dstformat)) ? 2 : kColorCount;

			// convert src to dst
			VDPixmapBuffer dst(size, size, dstformat);
			vdautoptr<IVDPixmapBlitter> blit2(VDPixmapCreateBlitter(dst, src[0], extraDst));

			// convert dst to rgb32
			vdautoptr<IVDPixmapBlitter> blit3(VDPixmapCreateBlitter(chk, dst));

			for(int v=kColorCount-maxtest; v<kColorCount; ++v) {
				blit2->Blit(dst, src[v]);
				if (!CheckBlit(dst, src[v], kColors[v])) goto failed;
				dst.validate();
				blit3->Blit(chk, dst);
				if (!CheckBlit(chk, dst, kColors[v])) goto failed;
				chk.validate();
				if (!CheckBlit(chk, ref[v], kColors[v])) goto failed;
failed:;
			}
		}
failed2:;
	}

	delete extraDst;
	return 0;
}

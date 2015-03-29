#include <vd2/libav_tiff/tiff_image.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/system/error.h>

extern "C" {
#include "tiff_common.h"

int tiff_init(AVCodecContext *avctx);
int tiff_end(AVCodecContext *avctx);
int tiff_decode_frame(AVCodecContext *avctx, void *data, int *got_frame, AVPacket *avpkt);
int tiff_encode_frame(AVCodecContext *avctx, AVPacket *pkt, const AVFrame *pict, int *got_packet);
int tiff_encode_init(AVCodecContext *avctx, int compr);
int tiff_encode_close(AVCodecContext *avctx);
};

bool VDIsTiffHeader(const void *pv, uint32 len) {
  GetByteContext gb;
  bytestream2_init(&gb, (const uint8_t*)pv, len);
  int off,le;
  int ret = ff_tdecode_header(&gb, &le, &off);
	return ret==0;
}

class VDImageDecoderTIFF : public IVDImageDecoderTIFF {
public:
  VDImageDecoderTIFF()
  {
    frame=0; 
  }

  ~VDImageDecoderTIFF()
  {
    av_frame_free(&frame);
  }

	void Decode(const void *src, uint32 srclen);
	void GetSize(int& w, int& h);
	void GetImage(void *p, int pitch, int format);

protected:
  AVFrame* frame;
};

class VDImageEncoderTIFF : public IVDImageEncoderTIFF {
	void Encode(const VDPixmap& px, void *&p, uint32& len, bool lzw_compress, bool alpha);
};

IVDImageDecoderTIFF *VDCreateImageDecoderTIFF() {
	return new VDImageDecoderTIFF;
}

IVDImageEncoderTIFF *VDCreateImageEncoderTIFF() {
	return new VDImageEncoderTIFF;
}

void VDImageDecoderTIFF::GetSize(int& w, int& h)
{
  w = frame->width;
  h = frame->height;
}

void VDImageDecoderTIFF::Decode(const void *src, uint32 srclen)
{
  AVPacket pkt;
  pkt.data = (uint8_t*)src;
  pkt.size = srclen;
  AVCodecContext ctx = {0};
  frame = av_frame_alloc();
  tiff_init(&ctx);
  int got_frame;
  tiff_decode_frame(&ctx,frame,&got_frame,&pkt);
  tiff_end(&ctx);

  if(!got_frame)
		throw MyError("Cannot read TIFF file: Invalid data.");

  switch(ctx.pix_fmt){
  case AV_PIX_FMT_RGBA64BE:
  case AV_PIX_FMT_RGBA64LE:
  case AV_PIX_FMT_RGB48BE:
  case AV_PIX_FMT_RGB48LE:
  case AV_PIX_FMT_RGB24:
  case AV_PIX_FMT_RGBA:
    break;
  default:
		throw MyError("Cannot read TIFF file: The format is unsupported.");
  }
}

void VDImageDecoderTIFF::GetImage(void *p, int pitch, int format)
{
  if(frame->pix_fmt==AV_PIX_FMT_RGBA64BE){
    frame->pix_fmt = AV_PIX_FMT_RGBA64LE;
    {for(int y=0; y<frame->height; y++){
      uint8_t* s = frame->data[0] + frame->linesize[0]*y;

      {for(int x=0; x<frame->width*4; x++){
        int a = s[0];
        int b = s[1];
        s[0] = b;
        s[1] = a;

        s+=2;
      }}
    }}
  }

  if(frame->pix_fmt==AV_PIX_FMT_RGB48BE){
    frame->pix_fmt = AV_PIX_FMT_RGB48LE;
    {for(int y=0; y<frame->height; y++){
      uint8_t* s = frame->data[0] + frame->linesize[0]*y;

      {for(int x=0; x<frame->width*3; x++){
        int a = s[0];
        int b = s[1];
        s[0] = b;
        s[1] = a;

        s+=2;
      }}
    }}
  }

  {for(int y=0; y<frame->height; y++){
    uint8_t* d = (uint8_t*)p + pitch*y;
    uint8_t* s1 = (uint8_t*)(frame->data[0] + frame->linesize[0]*y);
    uint16_t* s2 = (uint16_t*)(frame->data[0] + frame->linesize[0]*y);

    if(frame->pix_fmt==AV_PIX_FMT_RGB48LE) {for(int x=0; x<frame->width; x++){
      d[0] = s2[2] >> 8;
      d[1] = s2[1] >> 8;
      d[2] = s2[0] >> 8;
      d[3] = 255;

      s2+=3;
      d+=3;
    }}

    if(frame->pix_fmt==AV_PIX_FMT_RGBA64LE) {for(int x=0; x<frame->width; x++){
      d[0] = s2[2] >> 8;
      d[1] = s2[1] >> 8;
      d[2] = s2[0] >> 8;
      d[3] = s2[3] >> 8;

      s2+=4;
      d+=4;
    }}

    if(frame->pix_fmt==AV_PIX_FMT_RGB24) {for(int x=0; x<frame->width; x++){
      d[0] = s1[2];
      d[1] = s1[1];
      d[2] = s1[0];
      d[3] = 255;

      s1+=3;
      d+=4;
    }}

    if(frame->pix_fmt==AV_PIX_FMT_RGBA) {for(int x=0; x<frame->width; x++){
      d[0] = s1[2];
      d[1] = s1[1];
      d[2] = s1[0];
      d[3] = s1[3];

      s1+=4;
      d+=4;
    }}
  }}

  av_frame_free(&frame);
}

void VDImageEncoderTIFF::Encode(const VDPixmap& px, void *&p, uint32& len, bool lzw_compress, bool alpha)
{
  int temp_format;
  switch(px.format){
  case nsVDPixmap::kPixFormat_XRGB8888:
    temp_format = alpha ? nsVDPixmap::kPixFormat_XRGB8888:nsVDPixmap::kPixFormat_RGB888;
    break;
  case nsVDPixmap::kPixFormat_RGB888:
    temp_format = px.format;
    break;
  case nsVDPixmap::kPixFormat_XRGB64:
    temp_format = px.format;
    break;
  default:
    temp_format = nsVDPixmap::kPixFormat_RGB888;
  }
  
  VDPixmapBuffer buf;
  buf.init(px.w,px.h,temp_format);
	VDPixmapBlt(buf, px);

  {for(int y=0; y<px.h; y++){
    uint8_t* s = (uint8_t*)buf.data + buf.pitch*y;
    uint16_t* s2 = (uint16_t*)(size_t(buf.data) + buf.pitch*y);
    uint16_t* d2 = s2;

    if(buf.format==nsVDPixmap::kPixFormat_RGB888) {for(int x=0; x<px.w; x++){
      int a = s[0];
      int b = s[2];
      s[0] = b;
      s[2] = a;

      s+=3;
    }}

    if(buf.format==nsVDPixmap::kPixFormat_XRGB8888) {for(int x=0; x<px.w; x++){
      int a = s[0];
      int b = s[2];
      s[0] = b;
      s[2] = a;

      s+=4;
    }}

    if(buf.format==nsVDPixmap::kPixFormat_XRGB64 && alpha) {for(int x=0; x<px.w; x++){
      int a = s2[0];
      int b = s2[2];
      s2[0] = b;
      s2[2] = a;

      s2+=4;
    }}

    if(buf.format==nsVDPixmap::kPixFormat_XRGB64 && !alpha) {for(int x=0; x<px.w; x++){
      int a = s2[0];
      int b = s2[2];
      int c = s2[1];
      d2[0] = b;
      d2[1] = c;
      d2[2] = a;

      s2+=4;
      d2+=3;
    }}
  }}

  AVFrame frame = {0};
  frame.width = px.w;
  frame.height = px.h;
  frame.data[0] = (uint8_t*)buf.data;
  frame.linesize[0] = buf.pitch;

  switch(buf.format){
  case nsVDPixmap::kPixFormat_RGB888:
    frame.pix_fmt = AV_PIX_FMT_RGB24;
    break;
  case nsVDPixmap::kPixFormat_XRGB8888:
    frame.pix_fmt = AV_PIX_FMT_RGBA;
    break;
  case nsVDPixmap::kPixFormat_XRGB64:
    frame.pix_fmt = alpha ? AV_PIX_FMT_RGBA64LE : AV_PIX_FMT_RGB48LE;
    break;
  }

  AVCodecContext ctx = {0};
  ctx.pix_fmt = frame.pix_fmt;
  ctx.width = frame.width;
  ctx.height = frame.height;
  tiff_encode_init(&ctx, lzw_compress ? TIFF_LZW:TIFF_RAW);
  AVPacket pkt = {0};
  int got_packet = 0;
  tiff_encode_frame(&ctx,&pkt,&frame,&got_packet);
  tiff_encode_close(&ctx);

  p = pkt.data;
  len = pkt.size;
}

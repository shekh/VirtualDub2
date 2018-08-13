#ifndef AVCODEC_STUB_H
#define AVCODEC_STUB_H

#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#pragma warning(disable:4018)
#pragma warning(disable:4244)

#define av_always_inline
#define av_cold
#define av_unused

typedef struct AVDictionary {
  int unused;
} AVDictionary;

enum {
  AV_PKT_FLAG_KEY = 1,
};

typedef struct AVPacket {
  uint8_t* data;
  size_t size;
  int flags;
} AVPacket;

static void* av_malloc(size_t n){ return malloc(n); }
static void* av_mallocz(size_t n){ void* r = malloc(n); memset(r,0,n); return r; }
static void* av_mallocz_array(size_t count, size_t s){ return av_mallocz(count*s); }
static void* av_malloc_array(size_t count, size_t s){ return av_malloc(count*s); }
static void av_free(void* p){ free(p); }
static void av_freep(void** p){ free(*p); *p=0; }
static char* av_strdup(const char* p){ 
  size_t n = strlen(p); 
  char* r = (char*)malloc(n+1); 
  memcpy(r,p,n+1); 
  return r; 
}

#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

#undef ENOMEM
#undef ENOSYS
#undef EINVAL
#define AVERROR_UNKNOWN -1
#define AVERROR_INVALIDDATA -1
#define ENOMEM -2
#define ENOSYS -3
#define AVERROR_PATCHWELCOME -4
#define EINVAL -5
#define AVERROR(x) x
#define AV_LOG_WARNING 0
#define AV_LOG_ERROR 1

#define snprintf _snprintf

#define FFMAX(a,b) (a>b ? a:b)
#define FFMIN(a,b) (a<b ? a:b)
#define FFSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)

#define FF_INPUT_BUFFER_PADDING_SIZE 32
#define FF_MIN_BUFFER_SIZE 16384

static int ff_fast_malloc(void *ptr, unsigned int *size, size_t min_size, int zero_realloc)
{
  void **p = (void**)ptr;
  if (min_size < *size)
    return 0;
  min_size = FFMAX(17 * min_size / 16 + 32, min_size);
  av_free(*p);
  *p = zero_realloc ? av_mallocz(min_size) : av_malloc(min_size);
  if (!*p)
    min_size = 0;
  *size = (unsigned int)min_size;
  return 1;
}

static void av_fast_padded_malloc(void *ptr, unsigned int *size, size_t min_size)
{
  uint8_t **p = (uint8_t **)ptr;
  if (min_size > SIZE_MAX - FF_INPUT_BUFFER_PADDING_SIZE) {
    av_freep((void**)p);
    *size = 0;
    return;
  }
  if (!ff_fast_malloc(p, size, min_size + FF_INPUT_BUFFER_PADDING_SIZE, 1))
    memset(*p + min_size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
}

static void av_fast_padded_mallocz(void *ptr, unsigned int *size, size_t min_size)
{
  uint8_t **p = (uint8_t **)ptr;
  if (min_size > SIZE_MAX - FF_INPUT_BUFFER_PADDING_SIZE) {
    av_freep((void**)p);
    *size = 0;
    return;
  }
  if (!ff_fast_malloc(p, size, min_size + FF_INPUT_BUFFER_PADDING_SIZE, 1))
    memset(*p, 0, min_size + FF_INPUT_BUFFER_PADDING_SIZE);
}

enum {
AV_PIX_FMT_RGB48LE = 1,
AV_PIX_FMT_RGB48BE,
AV_PIX_FMT_RGBA64LE,
AV_PIX_FMT_RGBA64BE,
AV_PIX_FMT_GBRP16LE,
AV_PIX_FMT_GBRP16BE,
AV_PIX_FMT_GBRAP16LE,
AV_PIX_FMT_GBRAP16BE,
AV_PIX_FMT_PAL8,
AV_PIX_FMT_RGBA,
AV_PIX_FMT_RGB24,
AV_PIX_FMT_GBRP,
AV_PIX_FMT_GBRAP,
AV_PIX_FMT_GRAY16LE,
AV_PIX_FMT_GRAY16BE,
AV_PIX_FMT_MONOBLACK,
AV_PIX_FMT_GRAY8,
};

typedef struct AVCodecContext {
  int pix_fmt;
  int width;
  int height;
  void* priv_data;
  struct AVFrame* coded_frame;
} AVCodecContext;

static int ff_set_dimensions(AVCodecContext* s, int width, int height)
{
  s->width = width;
  s->height = height;
  return 0;
}

typedef struct AVFrame {
  int pix_fmt;
  int width;
  int height;
  int linesize[4];
  uint8_t* data[4];
  uint8_t* buffer;
} AVFrame;

typedef struct ThreadFrame {
  AVFrame *f;
  AVCodecContext *owner;
  // progress->data is an array of 2 ints holding progress for top/bottom
  // fields
  //AVBufferRef *progress;
} ThreadFrame;

static AVFrame *av_frame_alloc(void)
{
  AVFrame *frame = (AVFrame*)av_mallocz(sizeof(*frame));

  if (!frame)
    return NULL;

  return frame;
}

static void av_frame_free(AVFrame **frame)
{
  if (!frame || !*frame)
    return;

  av_free((*frame)->buffer);
  av_freep((void**)frame);
}

static int ff_thread_get_buffer(AVCodecContext* s, ThreadFrame* frame, int x)
{
  int bpp = 0;
  size_t linesize;
  uint8_t* buffer;
  if(s->pix_fmt==AV_PIX_FMT_RGB48LE) bpp=6;
  if(s->pix_fmt==AV_PIX_FMT_RGB48BE) bpp=6;
  if(s->pix_fmt==AV_PIX_FMT_RGBA64LE) bpp=8;
  if(s->pix_fmt==AV_PIX_FMT_RGBA64BE) bpp=8;
  if(s->pix_fmt==AV_PIX_FMT_RGBA) bpp=4;
  if(s->pix_fmt==AV_PIX_FMT_RGB24) bpp=3;
  if(s->pix_fmt==AV_PIX_FMT_GRAY16LE) bpp=2;
  if(s->pix_fmt==AV_PIX_FMT_GRAY16BE) bpp=2;
  if(s->pix_fmt==AV_PIX_FMT_GRAY8) bpp=1;
  if(s->pix_fmt==AV_PIX_FMT_PAL8) bpp=1;

  frame->f->width = s->width;
  frame->f->height = s->height;
  frame->f->pix_fmt = s->pix_fmt;

  linesize = (bpp*s->width + 15) & ~15;
  buffer = (uint8_t*)av_malloc(linesize*s->height+15);
  frame->f->buffer = buffer;
  frame->f->linesize[0] = (int)linesize;
  frame->f->linesize[1] = 0;
  frame->f->linesize[2] = 0;
  frame->f->linesize[3] = 0;
  frame->f->data[0] = (uint8_t*)((size_t)(buffer+15) & ~15);
  frame->f->data[1] = 0;
  frame->f->data[2] = 0;
  frame->f->data[3] = 0;
  return 0;
}

static int ff_alloc_packet2(AVCodecContext* s, AVPacket* pkt, size_t size)
{
  pkt->size = size;
  pkt->data = (uint8_t*)av_malloc(size);
  return 0;
}

static int av_clip(int a, int amin, int amax)
{
  if      (a < amin) return amin;
  else if (a > amax) return amax;
  else               return a;
}

extern const uint8_t ff_reverse[256];

static void av_log(AVCodecContext* ctx, int err, const char* msg, ...){}
#define av_assert0(x)
#define av_assert2(x)

#include "bytestream.h"

#endif

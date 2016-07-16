//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
//
//	Animated PNG support by Max Stepin
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"

#include <vd2/Riza/bitmap.h>
#include "AVIOutputAPNG.h"
#include "APNG.h"

class AVIVideoAPNGOutputStream : public AVIOutputStream 
{
public:
	AVIVideoAPNGOutputStream(FILE *pFile);
	~AVIVideoAPNGOutputStream();

	void init(int frameCount, int loopCount, int alpha, int grayscale, int rate, int scale, FILE * pFile);
	void finalize();
	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer);
	void partialWriteEnd();

private:
	FILE			* mpFile; 
	int				mFrameCount;
	int				mLoopCount;
	int				mAlpha;
	int				mGrayscale;
	int				mRate;
	int				mScale;
	int				mCurFrame;
	uint32			w0, h0, x0, y0;
	uint8			bop0;

	VDPixmapLayout	mSrcLayout;
	VDPixmapBuffer	mPreviousFrame;
	VDPixmapBuffer	mPrePreviousFrame;
	VDPixmapBuffer	mConvertBuffer;
	png_structp		png_ptr;
	uint8			** row_pointers_preprev;
	uint8			** row_pointers_prev;
	uint8			** row_pointers_cur;

	uint32 get_rect1(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph);
	uint32 get_rect2(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph, uint8 *pBop);
	uint32 get_rect2(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph, uint8 *pBop, uint32 x0, uint32 y0, uint32 w0, uint32 h0);
	uint32 get_rect3(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph);
	uint32 get_rect4(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph, uint8 *pBop);
	uint32 get_rect4(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph, uint8 *pBop, uint32 x0, uint32 y0, uint32 w0, uint32 h0);
	void prepare_buffer(uint8 **pp1, uint8 **pp2, uint32 x_offset, uint32 y_offset, uint32 width, uint32 height, uint8 dispose_op);
	void write_chunk(const char * name, uint8 * data, size_t length);
	void write_frame_head(uint32 x_offset, uint32 y_offset, uint32 width, uint32 height, uint8 dispose_op, uint8 blend_op);
	void write_IDATs(uint8 * data, size_t length);
	void write_row(uint32 j, uint8 * row);
};

AVIVideoAPNGOutputStream::AVIVideoAPNGOutputStream(FILE *pFile)
	: mpFile(pFile)
	, png_ptr(NULL)
	, mCurFrame(0)
{
}

AVIVideoAPNGOutputStream::~AVIVideoAPNGOutputStream() 
{
}

void AVIVideoAPNGOutputStream::init(int frameCount, int loopCount, int alpha, int grayscale, int rate, int scale, FILE * pFile) 
{
	mFrameCount = frameCount;
	mLoopCount = loopCount;
	mAlpha = alpha;
	mGrayscale = grayscale;
	mRate  = rate;
	mScale = scale;
	mpFile = pFile;
	unsigned char * ptr;
	uint32 j;

	const VDAVIBitmapInfoHeader *bih = (const VDAVIBitmapInfoHeader *)getFormat();
	int variant;

	int format = VDBitmapFormatToPixmapFormat(*bih, variant);

	if (!format)
		throw MyError("The current output format is not an uncompressed format that can be converted to an animated PNG.");

	VDMakeBitmapCompatiblePixmapLayout(mSrcLayout, bih->biWidth, bih->biHeight, format, variant);
	mConvertBuffer.init(bih->biWidth, bih->biHeight, nsVDPixmap::kPixFormat_XRGB8888);
	mPreviousFrame.init(bih->biWidth, bih->biHeight, nsVDPixmap::kPixFormat_XRGB8888);
	mPrePreviousFrame.init(bih->biWidth, bih->biHeight, nsVDPixmap::kPixFormat_XRGB8888);

	png_ptr = (png_structp)malloc(sizeof(png_struct));

	if (png_ptr != NULL)
	{
		png_ptr->width = bih->biWidth;
		png_ptr->height = bih->biHeight;
		png_ptr->bit_depth = 8;
		png_ptr->color_type = PNG_COLOR_TYPE_GRAY;
		png_ptr->bpp = 1;

		if (mAlpha)
		{
			png_ptr->color_type |= PNG_COLOR_MASK_ALPHA;
			png_ptr->bpp++;
		}

		if (!mGrayscale)
		{
			png_ptr->color_type |= PNG_COLOR_MASK_COLOR;
			png_ptr->bpp+=2;
		}

		png_ptr->rowbytes = png_ptr->bpp * bih->biWidth;
		png_ptr->idat_size = png_ptr->height * (png_ptr->rowbytes + 1);
		png_ptr->next_seq_num = 0;
		png_ptr->num_frames_written = 0;

		png_ptr->zstream1.data_type = Z_BINARY;
		png_ptr->zstream1.zalloc = Z_NULL;
		png_ptr->zstream1.zfree = Z_NULL;
		png_ptr->zstream1.opaque = Z_NULL;

		png_ptr->zstream2.data_type = Z_BINARY;
		png_ptr->zstream2.zalloc = Z_NULL;
		png_ptr->zstream2.zfree = Z_NULL;
		png_ptr->zstream2.opaque = Z_NULL;

		deflateInit2(&png_ptr->zstream1, Z_BEST_COMPRESSION, 8, 15, 8, Z_DEFAULT_STRATEGY);
		deflateInit2(&png_ptr->zstream2, Z_BEST_COMPRESSION, 8, 15, 8, Z_FILTERED);

		png_ptr->zbuf_size = png_ptr->idat_size + ((png_ptr->idat_size + 7) >> 3) + ((png_ptr->idat_size + 63) >> 6) + 11;

		png_ptr->zbuf1 = (uint8 *)malloc(png_ptr->zbuf_size);
		png_ptr->zstream1.next_out = png_ptr->zbuf1;
		png_ptr->zstream1.avail_out = (uInt)png_ptr->zbuf_size;

		png_ptr->zbuf2 = (uint8 *)malloc(png_ptr->zbuf_size);
		png_ptr->zstream2.next_out = png_ptr->zbuf2;
		png_ptr->zstream2.avail_out = (uInt)png_ptr->zbuf_size;

		row_pointers_preprev = (uint8 **)malloc(sizeof(uint8 *) * bih->biHeight);
		row_pointers_prev = (uint8 **)malloc(sizeof(uint8 *) * bih->biHeight);
		row_pointers_cur = (uint8 **)malloc(sizeof(uint8 *) * bih->biHeight);

		ptr = (uint8 *)mPreviousFrame.data;
		for (j=0; j<bih->biHeight; j++)
		{
			row_pointers_prev[j] = ptr;
			ptr += mPreviousFrame.pitch;
		}
		ptr = (uint8 *)mPrePreviousFrame.data;
		for (j=0; j<bih->biHeight; j++)
		{
			row_pointers_preprev[j] = ptr;
			ptr += mPrePreviousFrame.pitch;
		}

		png_ptr->prev_row = (uint8 *)malloc(png_ptr->rowbytes + 1);
		memset(png_ptr->prev_row, 0, png_ptr->rowbytes + 1);

		png_ptr->row_buf = (uint8 *)malloc(png_ptr->rowbytes + 1);
		png_ptr->row_buf[0] = PNG_FILTER_VALUE_NONE;

		png_ptr->sub_row = (uint8 *)malloc(png_ptr->rowbytes + 1);
		png_ptr->sub_row[0] = PNG_FILTER_VALUE_SUB;

		png_ptr->up_row = (uint8 *)malloc(png_ptr->rowbytes + 1);
		png_ptr->up_row[0] = PNG_FILTER_VALUE_UP;

		png_ptr->avg_row = (uint8 *)malloc(png_ptr->rowbytes + 1);
		png_ptr->avg_row[0] = PNG_FILTER_VALUE_AVG;

		png_ptr->paeth_row = (uint8 *)malloc(png_ptr->rowbytes + 1);
		png_ptr->paeth_row[0] = PNG_FILTER_VALUE_PAETH;

		uint8 png_sign[8] = {137, 80, 78, 71, 13, 10, 26, 10};

		if (mpFile != NULL)
		{
			fwrite(png_sign, 1, 8, mpFile);

			struct IHDR {
				uint32	mWidth;
				uint32	mHeight;
				uint8	mDepth;
				uint8	mColorType;
				uint8	mCompression;
				uint8	mFilterMethod;
				uint8	mInterlaceMethod;
			} ihdr;

			ihdr.mWidth				= VDSwizzleU32(png_ptr->width);
			ihdr.mHeight			= VDSwizzleU32(png_ptr->height);
			ihdr.mDepth				= png_ptr->bit_depth;
			ihdr.mColorType			= png_ptr->color_type;
			ihdr.mCompression		= PNG_COMPRESSION_TYPE_BASE;
			ihdr.mFilterMethod		= PNG_FILTER_TYPE_BASE;
			ihdr.mInterlaceMethod	= PNG_INTERLACE_NONE;
			write_chunk("IHDR", (uint8 *)(&ihdr), 13);

			struct acTL {
				uint32	mFrameCount;
				uint32	mLoopCount;
			} actl;

			actl.mFrameCount  = VDSwizzleU32(mFrameCount);
			actl.mLoopCount   = VDSwizzleU32(mLoopCount);
			write_chunk("acTL", (uint8 *)(&actl), 8);
		}
	}
}

void AVIVideoAPNGOutputStream::finalize() 
{
	uint8 png_Software[19] = { 83, 111, 102, 116, 119, 97, 114, 101, '\0', 86, 105, 114, 116, 117, 97, 108, 68, 117, 98};

	if (png_ptr != NULL)
	{
		if (mpFile != NULL)
		{
			write_chunk("tEXt", png_Software, 19);
			write_chunk("IEND", 0, 0);
		}

		deflateEnd(&png_ptr->zstream1);
		deflateEnd(&png_ptr->zstream2);

		if (png_ptr->zbuf1 != NULL)
			free(png_ptr->zbuf1);

		if (png_ptr->zbuf2 != NULL)
			free(png_ptr->zbuf2);

		if (row_pointers_preprev != NULL)
			free(row_pointers_preprev);

		if (row_pointers_prev != NULL)
			free(row_pointers_prev);

		if (row_pointers_cur != NULL)
			free(row_pointers_cur);

		if (png_ptr->row_buf != NULL)
			free(png_ptr->row_buf);

		if (png_ptr->prev_row != NULL)
			free(png_ptr->prev_row);

		if (png_ptr->sub_row != NULL)
			free(png_ptr->sub_row);

		if (png_ptr->up_row != NULL)
			free(png_ptr->up_row);

		if (png_ptr->avg_row != NULL)
			free(png_ptr->avg_row);

		if (png_ptr->paeth_row != NULL)
			free(png_ptr->paeth_row);

		memset(png_ptr, 0, sizeof(png_struct));

		free(png_ptr);
		png_ptr = NULL;
	}
}

void AVIVideoAPNGOutputStream::write_frame_head(uint32 x0, uint32 y0, uint32 w0, uint32 h0, uint8 dispose_op, uint8 blend_op)
{
	png_ptr->width = w0;
	png_ptr->height = h0;
	png_ptr->rowbytes = png_ptr->bpp * w0;
	png_ptr->idat_size = png_ptr->height * (png_ptr->rowbytes + 1);

	memset(png_ptr->prev_row, 0, png_ptr->rowbytes + 1);

	struct fcTL {
		uint32	mSeq;
		uint32	mWidth;
		uint32	mHeight;
		uint32	mXOffset;
		uint32	mYOffset;
		uint16	mDelayNum;
		uint16	mDelayDen;
		uint8	mDisposeOp;
		uint8	mBlendOp;
	} fctl;

	fctl.mSeq       = VDSwizzleU32(png_ptr->next_seq_num++);
	fctl.mWidth     = VDSwizzleU32(w0);
	fctl.mHeight    = VDSwizzleU32(h0);
	fctl.mXOffset   = VDSwizzleU32(x0);
	fctl.mYOffset   = VDSwizzleU32(y0);
	fctl.mDelayNum  = VDSwizzleU16((uint16)mScale);
	fctl.mDelayDen  = VDSwizzleU16((uint16)mRate);
	fctl.mDisposeOp = dispose_op;
	fctl.mBlendOp   = blend_op;

	write_chunk("fcTL", (uint8 *)(&fctl), 26);

	png_ptr->zstream1.avail_out = (uInt)png_ptr->zbuf_size;
	png_ptr->zstream1.next_out = png_ptr->zbuf1;

	png_ptr->zstream2.avail_out = (uInt)png_ptr->zbuf_size;
	png_ptr->zstream2.next_out = png_ptr->zbuf2;
}

void AVIVideoAPNGOutputStream::write_chunk(const char * name, uint8 * data, size_t length)
{
	uint32 crc = crc32(0, Z_NULL, 0);
	uint32 len = VDSwizzleU32(length);

	fwrite(&len, 1, 4, mpFile);
	fwrite(name, 1, 4, mpFile);
	crc = crc32(crc, (const Bytef *)name, 4);

	if (memcmp(name, "fdAT", 4) == 0)
	{
		uint32 seq = VDSwizzleU32(png_ptr->next_seq_num++);
		fwrite(&seq, 1, 4, mpFile);
		crc = crc32(crc, (const Bytef *)(&seq), 4);
		length -= 4;
	}

	if (data != NULL && length > 0)
	{
		fwrite(data, 1, length, mpFile);
		crc = crc32(crc, data, (uInt)length);
	}

	uint32 crc2 = VDSwizzleU32(crc);
	fwrite(&crc2, 1, 4, mpFile);
}

void AVIVideoAPNGOutputStream::write_IDATs(uint8 * data, size_t length)
{
	unsigned int z_cmf = data[0];
	if ((z_cmf & 0x0f) == 8 && (z_cmf & 0xf0) <= 0x70)
	{
		if (length >= 2)
		{
			unsigned int z_cinfo = z_cmf >> 4;
			unsigned int half_z_window_size = 1 << (z_cinfo + 7);
			while (png_ptr->idat_size <= half_z_window_size && half_z_window_size >= 256)
			{
				z_cinfo--;
				half_z_window_size >>= 1;
			}
			z_cmf = (z_cmf & 0x0f) | (z_cinfo << 4);
			if (data[0] != (uint8)z_cmf)
			{
				data[0] = (uint8)z_cmf;
				data[1] &= 0xe0;
				data[1] += (uint8)(0x1f - ((z_cmf << 8) + data[1]) % 0x1f);
			}
		}
	}

	while (length > 0)
	{
		size_t ds = length;
		if (ds > PNG_ZBUF_SIZE)
			ds = PNG_ZBUF_SIZE;

		if (png_ptr->num_frames_written == 0)
			write_chunk("IDAT", data, ds);
		else
			write_chunk("fdAT", data, ds+4);

		data += ds;
		length -= ds;
	}

	png_ptr->num_frames_written++;
}

void AVIVideoAPNGOutputStream::write_row(uint32 j, uint8 * row)
{
	uint8 * prev_row    = png_ptr->prev_row;
	uint8 * best_row    = png_ptr->row_buf;
	uint8 * row_buf     = best_row;
	uint32  mins        = ((uint32)(-1)) >> 1;
	uint32  i;
	int v;

	memcpy(png_ptr->row_buf+1, row, png_ptr->rowbytes);

	uint8 * rp; 
	uint8 * dp;
	uint8 * lp;
	uint8 * pp;
	uint8 * cp;
	uint32  sum = 0;

	for (i=0, rp=row_buf+1; i<png_ptr->rowbytes; i++, rp++)
	{
		v = *rp;
		sum += (v < 128) ? v : 256 - v;
	}
	mins = sum;

	sum = 0;
	for (i=0, rp=row_buf+1, dp=png_ptr->sub_row+1; i<png_ptr->bpp; i++, rp++, dp++)
	{
		v = *dp = *rp;
		sum += (v < 128) ? v : 256 - v;
	}
	for (lp=row_buf+1; i<png_ptr->rowbytes; i++, rp++, lp++, dp++)
	{
		v = *dp = (uint8)(((int)*rp - (int)*lp) & 0xff);
		sum += (v < 128) ? v : 256 - v;

		if (sum > mins)
			break;
	}
	if (sum < mins)
	{
		mins = sum;
		best_row = png_ptr->sub_row;
	}

	if (j > 0)
	{
		sum = 0;
		for (i=0, rp=row_buf+1, dp=png_ptr->up_row+1, pp=prev_row+1; i<png_ptr->rowbytes; i++)
		{
			v = *dp++ = (uint8)(((int)*rp++ - (int)*pp++) & 0xff);
			sum += (v < 128) ? v : 256 - v;

			if (sum > mins)
				break;
		}
		if (sum < mins)
		{
			mins = sum;
			best_row = png_ptr->up_row;
		}

		sum = 0;
		for (i=0, rp=row_buf+1, dp=png_ptr->avg_row+1, pp=prev_row+1; i<png_ptr->bpp; i++)
		{
			v = *dp++ = (uint8)(((int)*rp++ - ((int)*pp++ / 2)) & 0xff);
			sum += (v < 128) ? v : 256 - v;
		}
		for (lp=row_buf+1; i<png_ptr->rowbytes; i++)
		{
			v = *dp++ = (uint8)(((int)*rp++ - (((int)*pp++ + (int)*lp++) / 2)) & 0xff);
			sum += (v < 128) ? v : 256 - v;

			if (sum > mins)
				break;
		}
		if (sum < mins)
		{ 
			mins = sum;
			best_row = png_ptr->avg_row;
		}

		sum = 0;
		for (i=0, rp=row_buf+1, dp=png_ptr->paeth_row+1, pp=prev_row+1; i<png_ptr->bpp; i++)
		{
			v = *dp++ = (uint8)(((int)*rp++ - (int)*pp++) & 0xff);
			sum += (v < 128) ? v : 256 - v;
		}
		for (lp = row_buf+1, cp = prev_row+1; i<png_ptr->rowbytes; i++)
		{
			int a, b, c, pa, pb, pc, p;

			b = *pp++;
			c = *cp++;
			a = *lp++;

			p = b - c;
			pc = a - c;

			pa = abs(p);
			pb = abs(pc);
			pc = abs(p + pc);

			p = (pa <= pb && pa <=pc) ? a : (pb <= pc) ? b : c;
			v = *dp++ = (uint8)(((int)*rp++ - p) & 0xff);
			sum += (v < 128) ? v : 256 - v;

			if (sum > mins)
				break;
		}
		if (sum < mins)
		{
			best_row = png_ptr->paeth_row;
		}
	}

	png_ptr->zstream1.next_in = png_ptr->row_buf;
	png_ptr->zstream1.avail_in = (uInt)png_ptr->rowbytes + 1;
	deflate(&png_ptr->zstream1, Z_NO_FLUSH);

	png_ptr->zstream2.next_in = best_row;
	png_ptr->zstream2.avail_in = (uInt)png_ptr->rowbytes + 1;
	deflate(&png_ptr->zstream2, Z_NO_FLUSH);

	if (png_ptr->prev_row != NULL)
	{
		uint8 * tptr = png_ptr->prev_row;
		png_ptr->prev_row = png_ptr->row_buf;
		png_ptr->row_buf = tptr;
	}
}

uint32 AVIVideoAPNGOutputStream::get_rect1(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph)
{
	uint32 i, j;
	uint32 x_min = w-1;
	uint32 y_min = h-1;
	uint32 x_max = 0;
	uint32 y_max = 0;
	uint32 diffnum = 0;
	uint32 src_num = 0;

	for (j=0; j<h; j++)
	{
		uint8 * sp = pp1[j];
		uint8 * dp = pp2[j];

		for (i=0; i<w; i++)
		{
			if (*(sp++) != *(dp++))
			{
				diffnum++;
				if (i<x_min) x_min = i;
				if (i>x_max) x_max = i;
				if (j<y_min) y_min = j;
				if (j>y_max) y_max = j;
			}
		}
	}

	if (diffnum == 0)
	{
		*px = *py = 0;
		*pw = *ph = 1; 
		return 1;
	}

	*px = x_min;
	*py = y_min;
	*pw = x_max-x_min+1;
	*ph = y_max-y_min+1;

	for (j=y_min; j<=y_max; j++)
	{
		uint8 * dp = pp2[j];

		for (i=x_min; i<=x_max; i++)
		{
			if (*(dp+i))
				src_num++;
		}
	}

	return src_num;
}

uint32 AVIVideoAPNGOutputStream::get_rect2(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph, uint8 *pBop)
{
	uint32 i, j;
	uint16 s, d;
	uint32 x_min = w-1;
	uint32 y_min = h-1;
	uint32 x_max = 0;
	uint32 y_max = 0;
	uint32 diffnum = 0;
	uint32 ovr_num = 0;
	uint32 src_num = 0;
	uint32 over_is_possible = 1;

	for (j=0; j<h; j++)
	{
		uint16 * sp = (uint16 *)pp1[j];
		uint16 * dp = (uint16 *)pp2[j];

		for (i=0; i<w; i++)
		{
			s = (*sp++);
			d = (*dp++);

			if (s != d)
			{
				diffnum++;
				if ((d & 0xFF00) == 0xFF00) 
					ovr_num++; 
				else
					over_is_possible = 0;

				if (i<x_min) x_min = i;
				if (i>x_max) x_max = i;
				if (j<y_min) y_min = j;
				if (j>y_max) y_max = j;
			}
		}
	}

	if (diffnum == 0)
	{
		*px = *py = 0;
		*pw = *ph = 1; 
		*pBop = PNG_BLEND_OP_SOURCE;
		return 1;
	}

	*px = x_min;
	*py = y_min;
	*pw = x_max-x_min+1;
	*ph = y_max-y_min+1;

	for (j=y_min; j<=y_max; j++)
	{
		uint16 * dp = (uint16*)(pp2[j]);

		for (i=x_min; i<=x_max; i++)
		{
			if (*(dp+i) != 0) 
				src_num++;
		}
	}

	if ((over_is_possible) && (ovr_num < src_num))
	{
		*pBop = PNG_BLEND_OP_OVER;
		return ovr_num;
	}

	*pBop = PNG_BLEND_OP_SOURCE;
	return src_num;
}

uint32 AVIVideoAPNGOutputStream::get_rect2(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph, uint8 *pBop, uint32 x0, uint32 y0, uint32 w0, uint32 h0)
{
	uint32 i, j;
	uint16 s, d;
	uint32 x_min = w-1;
	uint32 y_min = h-1;
	uint32 x_max = 0;
	uint32 y_max = 0;
	uint32 diffnum = 0;
	uint32 ovr_num = 0;
	uint32 src_num = 0;
	uint32 over_is_possible = 1;

	for (j=0; j<h; j++)
	{
		uint16 * sp = (uint16 *)pp1[j];
		uint16 * dp = (uint16 *)pp2[j];

		for (i=0; i<w; i++)
		{
			d = (*dp++);
			if ((j>=y0) && (j<y0+h0) && (i>=x0) && (i<x0+w0))
				s = 0;
			else
				s = *sp;

			if (s != d)
			{
				diffnum++;
				if ((d & 0xFF00) == 0xFF00) 
					ovr_num++; 
				else
					over_is_possible = 0;

				if (i<x_min) x_min = i;
				if (i>x_max) x_max = i;
				if (j<y_min) y_min = j;
				if (j>y_max) y_max = j;
			}
		}
	}

	if (diffnum == 0)
	{
		*px = *py = 0;
		*pw = *ph = 1; 
		*pBop = PNG_BLEND_OP_SOURCE;
		return 1;
	}

	*px = x_min;
	*py = y_min;
	*pw = x_max-x_min+1;
	*ph = y_max-y_min+1;

	for (j=y_min; j<=y_max; j++)
	{
		uint16 * dp = (uint16*)(pp2[j]);

		for (i=x_min; i<=x_max; i++)
		{
			if (*(dp+i) != 0) 
				src_num++;
		}
	}

	if ((over_is_possible) && (ovr_num < src_num))
	{
		*pBop = PNG_BLEND_OP_OVER;
		return ovr_num;
	}

	*pBop = PNG_BLEND_OP_SOURCE;
	return src_num;
}

uint32 AVIVideoAPNGOutputStream::get_rect3(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph)
{
	uint32 i, j;
	uint32 x_min = w-1;
	uint32 y_min = h-1;
	uint32 x_max = 0;
	uint32 y_max = 0;
	uint32 diffnum = 0;
	uint32 src_num = 0;

	for (j=0; j<h; j++)
	{
		uint8 * sp = pp1[j];
		uint8 * dp = pp2[j];

		for (i=0; i<w; i++)
		{
			uint32 diff = 0;
			if (*(sp++) != *(dp++)) diff = 1;
			if (*(sp++) != *(dp++)) diff = 1;
			if (*(sp++) != *(dp++)) diff = 1;

			if (diff)
			{
				diffnum++;
				if (i<x_min) x_min = i;
				if (i>x_max) x_max = i;
				if (j<y_min) y_min = j;
				if (j>y_max) y_max = j;
			}
		}
	}

	if (diffnum == 0)
	{
		*px = *py = 0;
		*pw = *ph = 1; 
		return 1;
	}

	*px = x_min;
	*py = y_min;
	*pw = x_max-x_min+1;
	*ph = y_max-y_min+1;

	for (j=y_min; j<=y_max; j++)
	{
		uint8 * dp = pp2[j];

		for (i=x_min; i<=x_max; i++)
		{
			if ((*(dp+i*3) != 0) || (*(dp+i*3+1) != 0) || (*(dp+i*3+2) != 0))
				src_num++;
		}
	}

	return src_num;
}

uint32 AVIVideoAPNGOutputStream::get_rect4(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph, uint8 *pBop)
{
	uint32 i, j, s, d;
	uint32 x_min = w-1;
	uint32 y_min = h-1;
	uint32 x_max = 0;
	uint32 y_max = 0;
	uint32 diffnum = 0;
	uint32 ovr_num = 0;
	uint32 src_num = 0;
	uint32 over_is_possible = 1;

	for (j=0; j<h; j++)
	{
		uint32 * sp = (uint32*)(pp1[j]);
		uint32 * dp = (uint32*)(pp2[j]);

		for (i=0; i<w; i++)
		{
			s = (*sp++);
			d = (*dp++);

			if (s != d)
			{
				diffnum++;
				if ((d & 0xFF000000) == 0xFF000000) 
					ovr_num++; 
				else
					over_is_possible = 0;

				if (i<x_min) x_min = i;
				if (i>x_max) x_max = i;
				if (j<y_min) y_min = j;
				if (j>y_max) y_max = j;
			}
		}
	}

	if (diffnum == 0)
	{
		*px = *py = 0;
		*pw = *ph = 1; 
		*pBop = PNG_BLEND_OP_SOURCE;
		return 1;
	}

	*px = x_min;
	*py = y_min;
	*pw = x_max-x_min+1;
	*ph = y_max-y_min+1;

	for (j=y_min; j<=y_max; j++)
	{
		uint32 * dp = (uint32*)(pp2[j]);

		for (i=x_min; i<=x_max; i++)
		{
			if (*(dp+i) != 0) 
				src_num++;
		}
	}

	if ((over_is_possible) && (ovr_num < src_num))
	{
		*pBop = PNG_BLEND_OP_OVER;
		return ovr_num;
	}

	*pBop = PNG_BLEND_OP_SOURCE;
	return src_num;
}

uint32 AVIVideoAPNGOutputStream::get_rect4(uint32 w, uint32 h, uint8 **pp1, uint8 **pp2, uint32 *px, uint32 *py, uint32 *pw, uint32 *ph, uint8 *pBop, uint32 x0, uint32 y0, uint32 w0, uint32 h0)
{
	uint32 i, j, s, d;
	uint32 x_min = w-1;
	uint32 y_min = h-1;
	uint32 x_max = 0;
	uint32 y_max = 0;
	uint32 diffnum = 0;
	uint32 ovr_num = 0;
	uint32 src_num = 0;
	uint32 over_is_possible = 1;

	for (j=0; j<h; j++)
	{
		uint32 * sp = (uint32*)(pp1[j]);
		uint32 * dp = (uint32*)(pp2[j]);

		for (i=0; i<w; i++)
		{
			d = *dp;
			if ((j>=y0) && (j<y0+h0) && (i>=x0) && (i<x0+w0))
				s = 0;
			else
				s = *sp;

			if (s != d)
			{
				diffnum++;
				if ((d & 0xFF000000) == 0xFF000000) 
					ovr_num++; 
				else
					over_is_possible = 0;

				if (i<x_min) x_min = i;
				if (i>x_max) x_max = i;
				if (j<y_min) y_min = j;
				if (j>y_max) y_max = j;
			}
			sp++;
			dp++;
		}
	}

	if (diffnum == 0)
	{
		*px = *py = 0;
		*pw = *ph = 1; 
		*pBop = PNG_BLEND_OP_SOURCE;
		return 1;
	}

	*px = x_min;
	*py = y_min;
	*pw = x_max-x_min+1;
	*ph = y_max-y_min+1;

	for (j=y_min; j<=y_max; j++)
	{
		uint32 * dp = (uint32*)(pp2[j]);

		for (i=x_min; i<=x_max; i++)
		{
			if (*(dp+i) != 0) 
				src_num++;
		}
	}

	if ((over_is_possible) && (ovr_num < src_num))
	{
		*pBop = PNG_BLEND_OP_OVER;
		return ovr_num;
	}

	*pBop = PNG_BLEND_OP_SOURCE;
	return src_num;
}

void AVIVideoAPNGOutputStream::prepare_buffer(uint8 **pp1, uint8 **pp2, uint32 x0, uint32 y0, uint32 w0, uint32 h0, uint8 dispose_op)
{
	uint32 i, j;
	if (png_ptr->bpp == 4)
	{
		if (dispose_op==PNG_DISPOSE_OP_NONE)
		{
			for (j=y0; j<y0+h0; j++)
			{
				uint32 * sp = (uint32*)(pp1[j]);
				uint32 * dp = (uint32*)(pp2[j]);

				for (i=x0; i<x0+w0; i++)
				{
					if (*(sp+i) == *(dp+i)) 
						*(dp+i) = 0;
					else
						*(sp+i) = *(dp+i);
				}
			}
		}
		else
		if (dispose_op==PNG_DISPOSE_OP_PREVIOUS)
		{
			for (j=y0; j<y0+h0; j++)
			{
				uint32 * sp = (uint32*)(pp1[j]);
				uint32 * dp = (uint32*)(pp2[j]);

				for (i=x0; i<x0+w0; i++)
				{
					if (*(sp+i) == *(dp+i)) 
						*(dp+i) = 0;
				}
			}
		}
		else
		{
			for (j=y0; j<y0+h0; j++)
			{
				uint32 * sp = (uint32*)(pp1[j]);
				uint32 * dp = (uint32*)(pp2[j]);

				for (i=x0; i<x0+w0; i++)
				{
					if (*(sp+i) == *(dp+i)) 
						*(dp+i) = 0;
					*(sp+i) = 0;
				}
			}
		}
	}
	else
	{
		if (dispose_op==PNG_DISPOSE_OP_NONE)
		{
			for (j=y0; j<y0+h0; j++)
			{
				uint16 * sp = (uint16*)(pp1[j]);
				uint16 * dp = (uint16*)(pp2[j]);

				for (i=x0; i<x0+w0; i++)
				{
					if (*(sp+i) == *(dp+i)) 
						*(dp+i) = 0;
					else
						*(sp+i) = *(dp+i);
				}
			}
		}
		else
		if (dispose_op==PNG_DISPOSE_OP_PREVIOUS)
		{
			for (j=y0; j<y0+h0; j++)
			{
				uint16 * sp = (uint16*)(pp1[j]);
				uint16 * dp = (uint16*)(pp2[j]);

				for (i=x0; i<x0+w0; i++)
				{
					if (*(sp+i) == *(dp+i)) 
						*(dp+i) = 0;
				}
			}
		}
		else
		{
			for (j=y0; j<y0+h0; j++)
			{
				uint16 * sp = (uint16*)(pp1[j]);
				uint16 * dp = (uint16*)(pp2[j]);

				for (i=x0; i<x0+w0; i++)
				{
					if (*(sp+i) == *(dp+i)) 
						*(dp+i) = 0;
					*(sp+i) = 0;
				}
			}
		}
	}
}

void AVIVideoAPNGOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) 
{
	uint32 i, j;
	uint32 area_none, area_prev, area_back;
	uint32 w1, h1, x1, y1;
	uint32 w2, h2, x2, y2;
	uint8 bop1, bop2;
	uint8 dop = PNG_DISPOSE_OP_NONE;

	if (!cbBuffer || !mpFile || !png_ptr) 
	{
		++mCurFrame;
		return;
	}

	VDPixmap pxsrc(VDPixmapFromLayout(mSrcLayout, (void *)pBuffer));
	VDPixmapBlt(mConvertBuffer, pxsrc);
	pxsrc = mConvertBuffer;

	uint8 * curp = (uint8 *)pxsrc.data;

	for (j=0; j<pxsrc.h; j++)
	{
		uint8   r, g, b, a;
		uint8 * sp = curp;
		uint8 * dp = curp; 
		row_pointers_cur[j] = curp;

		if (png_ptr->color_type & PNG_COLOR_MASK_COLOR)
		{
			if (png_ptr->color_type & PNG_COLOR_MASK_ALPHA)
			{
				for (i=0; i<pxsrc.w; i++)
				{
					b = *sp++;
					g = *sp++;
					r = *sp++;
					a = *sp++;
					if (a == 0) { r = g = b = 0; }
					*dp++ = r;
					*dp++ = g;
					*dp++ = b;
					*dp++ = a;
				}
			}
			else
			{
				for (i=0; i<pxsrc.w; i++)
				{
					b = *sp++;
					g = *sp++;
					r = *sp++;
					a = *sp++;
					*dp++ = r;
					*dp++ = g;
					*dp++ = b;
				}
			}
		}
		else
		{
			if (png_ptr->color_type & PNG_COLOR_MASK_ALPHA)
			{
				for (i=0; i<pxsrc.w; i++)
				{
					b = *sp++;
					g = *sp++;
					r = *sp++;
					a = *sp++;
					*dp++ = (uint8)((19595*r + 38470*g + 7471*b)>>16);
					*dp++ = a;
				}
			}
			else
			{
				for (i=0; i<pxsrc.w; i++)
				{
					b = *sp++;
					g = *sp++;
					r = *sp++;
					a = *sp++;
					*dp++ = (uint8)((19595*r + 38470*g + 7471*b)>>16);
				}
			}
		}
		curp += pxsrc.pitch;
	}

	if (mCurFrame == 0)
	{
		VDPixmapBlt(mPreviousFrame, pxsrc);
		VDPixmapBlt(mPrePreviousFrame, pxsrc);

		w0 = pxsrc.w;
		h0 = pxsrc.h;
		x0 = 0;
		y0 = 0;
		bop0 = PNG_BLEND_OP_SOURCE;
	}
	else
	{
		switch (png_ptr->bpp)
		{
			case 1:
			{
				area_none = get_rect1(pxsrc.w, pxsrc.h, row_pointers_prev, row_pointers_cur, &x1, &y1, &w1, &h1);
				area_prev = get_rect1(pxsrc.w, pxsrc.h, row_pointers_preprev, row_pointers_cur, &x2, &y2, &w2, &h2);
				if (area_prev < area_none)
				{
					area_none = area_prev;
					x1 = x2; y1 = y2;
					w1 = w2; h1 = h2;
					dop = PNG_DISPOSE_OP_PREVIOUS;
				}
				bop1 = PNG_BLEND_OP_SOURCE;
				break;
			}
			case 2:
			{
				area_none = get_rect2(pxsrc.w, pxsrc.h, row_pointers_prev, row_pointers_cur, &x1, &y1, &w1, &h1, &bop1);
				area_prev = get_rect2(pxsrc.w, pxsrc.h, row_pointers_preprev, row_pointers_cur, &x2, &y2, &w2, &h2, &bop2);
				if (area_prev < area_none)
				{
					area_none = area_prev;
					x1 = x2; y1 = y2;
					w1 = w2; h1 = h2;
					bop1 = bop2;
					dop = PNG_DISPOSE_OP_PREVIOUS;
				}
				area_back = get_rect2(pxsrc.w, pxsrc.h, row_pointers_prev, row_pointers_cur, &x2, &y2, &w2, &h2, &bop2, x0, y0, w0, h0);
				if (area_back < area_none)
				{
					area_none = area_back;
					x1 = x2; y1 = y2;
					w1 = w2; h1 = h2;
					bop1 = bop2;
					dop = PNG_DISPOSE_OP_BACKGROUND;
				}
				break;
			}
			case 3:
			{
				area_none = get_rect3(pxsrc.w, pxsrc.h, row_pointers_prev, row_pointers_cur, &x1, &y1, &w1, &h1);
				area_prev = get_rect3(pxsrc.w, pxsrc.h, row_pointers_preprev, row_pointers_cur, &x2, &y2, &w2, &h2);
				if (area_prev < area_none)
				{
					area_none = area_prev;
					x1 = x2; y1 = y2;
					w1 = w2; h1 = h2;
					dop = PNG_DISPOSE_OP_PREVIOUS;
				}
				bop1 = PNG_BLEND_OP_SOURCE;
				break;
			}
			case 4:
			default:
			{
				area_none = get_rect4(pxsrc.w, pxsrc.h, row_pointers_prev, row_pointers_cur, &x1, &y1, &w1, &h1, &bop1);
				area_prev = get_rect4(pxsrc.w, pxsrc.h, row_pointers_preprev, row_pointers_cur, &x2, &y2, &w2, &h2, &bop2);
				if (area_prev < area_none)
				{
					area_none = area_prev;
					x1 = x2; y1 = y2;
					w1 = w2; h1 = h2;
					bop1 = bop2;
					dop = PNG_DISPOSE_OP_PREVIOUS;
				}
				area_back = get_rect4(pxsrc.w, pxsrc.h, row_pointers_prev, row_pointers_cur, &x2, &y2, &w2, &h2, &bop2, x0, y0, w0, h0);
				if (area_back < area_none)
				{
					area_none = area_back;
					x1 = x2; y1 = y2;
					w1 = w2; h1 = h2;
					bop1 = bop2;
					dop = PNG_DISPOSE_OP_BACKGROUND;
				}
				break;
			}
		}

		write_frame_head(x0, y0, w0, h0, dop, bop0);

		if (bop0 == PNG_BLEND_OP_OVER)
			prepare_buffer(row_pointers_preprev, row_pointers_prev, x0, y0, w0, h0, dop);
		else
		{
			if (dop != PNG_DISPOSE_OP_PREVIOUS)
			{
				VDPixmapBlt(mPrePreviousFrame, mPreviousFrame);
				if (dop == PNG_DISPOSE_OP_BACKGROUND)
					VDMemset32Rect((uint8 *)mPrePreviousFrame.data+y0*mPrePreviousFrame.pitch+x0*4, mPrePreviousFrame.pitch, 0, w0, h0);
			}
		}

		for (int j=0; j<h0; j++)
			write_row(j, row_pointers_prev[y0+j] + x0*png_ptr->bpp);

		deflate(&png_ptr->zstream1, Z_FINISH);
		deflate(&png_ptr->zstream2, Z_FINISH);

		if (png_ptr->zstream1.total_out <= png_ptr->zstream2.total_out)
			write_IDATs(png_ptr->zbuf1, png_ptr->zstream1.total_out);
		else
			write_IDATs(png_ptr->zbuf2, png_ptr->zstream2.total_out);

		deflateReset(&png_ptr->zstream1);
		png_ptr->zstream1.data_type = Z_BINARY;
		deflateReset(&png_ptr->zstream2);
		png_ptr->zstream2.data_type = Z_BINARY;

		w0 = w1;
		h0 = h1;
		x0 = x1;
		y0 = y1;
		bop0 = bop1;

		VDPixmapBlt(mPreviousFrame, pxsrc);
	}

	if (mCurFrame == mFrameCount-1)
	{
		write_frame_head(x0, y0, w0, h0, PNG_DISPOSE_OP_NONE, bop0);

		if (bop0 == PNG_BLEND_OP_OVER)
			prepare_buffer(row_pointers_preprev, row_pointers_prev, x0, y0, w0, h0, PNG_DISPOSE_OP_NONE);

		for (int j=0; j<h0; j++)
			write_row(j, row_pointers_prev[y0+j] + x0*png_ptr->bpp);

		deflate(&png_ptr->zstream1, Z_FINISH);
		deflate(&png_ptr->zstream2, Z_FINISH);

		if (png_ptr->zstream1.total_out <= png_ptr->zstream2.total_out)
			write_IDATs(png_ptr->zbuf1, png_ptr->zstream1.total_out);
		else
			write_IDATs(png_ptr->zbuf2, png_ptr->zstream2.total_out);

		deflateReset(&png_ptr->zstream1);
		deflateReset(&png_ptr->zstream2);
	}

	++mCurFrame;
}

void AVIVideoAPNGOutputStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) 
{
	throw MyError("Partial write operations are not supported for video streams.");
}

void AVIVideoAPNGOutputStream::partialWrite(const void *pBuffer, uint32 cbBuffer) 
{
}

void AVIVideoAPNGOutputStream::partialWriteEnd() 
{
}

//////////////////////////////////

class AVIOutputAPNG : public AVIOutput, public IVDAVIOutputAPNG 
{
public:
	AVIOutputAPNG();

	AVIOutput *AsAVIOutput() { return this; }
	void SetFramesCount(int count) { mFramesCount = count; }
	void SetLoopCount(int count) { mLoopCount = count; }
	void SetAlpha(int count) { mAlpha = count; }
	void SetGrayscale(int count) { mGrayscale = count; }
	void SetRate(int rate) { mRate = rate; }
	void SetScale(int scale) { mScale = scale;}

	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *createAudioStream();

	bool init(const wchar_t *szFile);
	void finalize();

protected:
	FILE * mpFile; 
	int mFramesCount;
	int mLoopCount;
	int mAlpha;
	int mGrayscale;
	int mRate;
	int mScale;
};

IVDAVIOutputAPNG *VDCreateAVIOutputAPNG() { return new AVIOutputAPNG; }

AVIOutputAPNG::AVIOutputAPNG()
	: mpFile(NULL)
	, mFramesCount(0)
	, mLoopCount(0)
	, mAlpha(0)
	, mGrayscale(0)
	, mRate(0)
	, mScale(0)
{
}

AVIOutput *VDGetAVIOutputAPNG() 
{
	return new AVIOutputAPNG;
}

IVDMediaOutputStream *AVIOutputAPNG::createVideoStream() 
{
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoAPNGOutputStream(mpFile)))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputAPNG::createAudioStream() 
{
	return NULL;
}

bool AVIOutputAPNG::init(const wchar_t *szFile) 
{
	if ((mpFile = _wfopen(szFile, L"wb")) == 0)
	{
		throw MyError("Cannot create \"%ls\"", szFile);
		return false;
	}

	if (!videoOut)
		return false;

	static_cast<AVIVideoAPNGOutputStream *>(videoOut)->init(mFramesCount, mLoopCount, mAlpha, mGrayscale, mRate, mScale, mpFile);

	return true;
}

void AVIOutputAPNG::finalize() 
{
	if (videoOut)
		static_cast<AVIVideoAPNGOutputStream *>(videoOut)->finalize();
	if (mpFile != 0) 
		fclose(mpFile);
}

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

#include <windows.h>

#include "VBitmap.h"
#include "Histogram.h"
#include <vd2/system/error.h>

const char histo_log_table[]={
8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
8,8,8,8,8,8,8,8,7,7,7,7,7,7,7,7,
7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
7,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
6,6,6,6,6,6,6,6,6,6,6,6,5,5,5,5,
5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
5,5,5,5,5,5,5,5,5,5,5,4,4,4,4,4,
4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,
3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,
2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

const RGBQUAD Histogram::fore_colors[]={
	{ 0xff, 0x00, 0x00 },
	{ 0x00, 0xff, 0x00 },
	{ 0x00, 0x00, 0xff },
	{ 0xff, 0xff, 0xff },
};

Histogram::Histogram(HDC hDC, int max_height) {
	struct {
		BITMAPINFO bmi;
		RGBQUAD bmiColor1;
	} bmp;

	hBitmapGraph = NULL;
	hDCGraph = NULL;

	bmp.bmi.bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	bmp.bmi.bmiHeader.biWidth			= 256;
	bmp.bmi.bmiHeader.biHeight			= max_height;
	bmp.bmi.bmiHeader.biPlanes			= 1;
	bmp.bmi.bmiHeader.biBitCount		= 1;
	bmp.bmi.bmiHeader.biCompression		= BI_RGB;
	bmp.bmi.bmiHeader.biSizeImage		= max_height * 32;
	bmp.bmi.bmiHeader.biXPelsPerMeter	= 80;
	bmp.bmi.bmiHeader.biYPelsPerMeter	= 72;
	bmp.bmi.bmiHeader.biClrUsed			= 2;
	bmp.bmi.bmiHeader.biClrImportant	= 2;
	bmp.bmi.bmiColors[0].rgbBlue = 0;
	bmp.bmi.bmiColors[0].rgbGreen = 0;
	bmp.bmi.bmiColors[0].rgbRed = 0;
	bmp.bmiColor1.rgbBlue = 0;
	bmp.bmiColor1.rgbGreen = 0;
	bmp.bmiColor1.rgbRed = 0xff;

	if (!(hDCGraph = CreateCompatibleDC(hDC)))
		throw MyError("Histogram: Couldn't create display context");

	if (!(hBitmapGraph = CreateDIBSection(hDC, &bmp.bmi, DIB_RGB_COLORS, (LPVOID *)&lpGraphBits, NULL, 0)))
		throw MyError("Histogram: Couldn't allocate DIB section");

	SelectObject(hDCGraph, hBitmapGraph);

	this->max_height = max_height;

	SetMode(MODE_GRAY);
}

Histogram::~Histogram() {
	if (hBitmapGraph) DeleteObject(hBitmapGraph);
	if (hDCGraph) DeleteDC(hDCGraph);
}

void Histogram::Zero() {
	memset(histo, 0, sizeof histo);
	total_pixels = 0;
}

void Histogram::Process(const VBitmap *vbmp) {
#ifndef _M_IX86
	Pixel c, *src = (Pixel *)vbmp->data;
	long w, h;

	if (!vbmp->w || !vbmp->h) return;

	h = vbmp->h; 
	do {
		w = vbmp->w;
		do {
			c = *src++;
			++histo[(54*((c>>16)&255) + 183*((c>>8)&255) + 19*(c&255))>>8];
		} while(--w);

		src = (Pixel *)((char *)src + vbmp->modulo);
	} while(--h);
#else

	if (histo_mode < MODE_GRAY)
		asm_histogram_color_run((char *)vbmp->data + histo_mode, vbmp->w, vbmp->h, vbmp->pitch, histo);
	else
		asm_histogram_gray_run(vbmp->data, vbmp->w, vbmp->h, vbmp->pitch, histo);

#endif

	total_pixels += vbmp->w * vbmp->h;
}

void Histogram::Process24(const VBitmap *vbmp) {
#ifndef _M_IX86
	Pixel c, *src = (Pixel *)vbmp->data;
	long w, h;

	if (!vbmp->w || !vbmp->h) return;

	h = vbmp->h; 
	do {
		w = vbmp->w;
		do {
			c = *src++;
			++histo[(54*((c>>16)&255) + 183*((c>>8)&255) + 19*(c&255))>>8];
		} while(--w);

		src = (Pixel *)((char *)src + vbmp->modulo);
	} while(--h);
#else

	if (histo_mode < MODE_GRAY)
		asm_histogram_color24_run((char *)vbmp->data + histo_mode, vbmp->w, vbmp->h, vbmp->pitch, histo);
	else
		asm_histogram_gray24_run(vbmp->data, vbmp->w, vbmp->h, vbmp->pitch, histo);

#endif

	total_pixels += vbmp->w * vbmp->h;
}

void Histogram::Process16(const VBitmap *vbmp) {
	static const long pixmasks[3]={
		0x001f,
		0x03e0,
		0x7c00,
	};

#ifdef _M_IX86
	if (histo_mode < MODE_GRAY)
		asm_histogram16_run(vbmp->data, vbmp->w, vbmp->h, vbmp->pitch, histo, pixmasks[histo_mode]);
	else
		asm_histogram16_run(vbmp->data, vbmp->w, vbmp->h, vbmp->pitch, histo, 0x7fff);
#else
#pragma vdpragma_TODO("fixme")
#endif

	total_pixels += vbmp->w * vbmp->h;
}

void Histogram::ComputeHeights(long *heights, long graph_height) {
#if 0
	long h;
	double c = log(pow(2, 1.0/8.0));

	for(int i=0; i<256; i++) {
		if (!histo[i])
			heights[i] = 0;
		else {
			h = graph_height + log((double)histo[i]/total_pixels) / c;

			if (h<0)
				heights[i] = 1;
			else
				heights[i] = h;
		}

//			heights[i] = (histo[i]*graph_height + total_pixels - 1) / total_pixels;
	}
#else
	long h;
	int s;

	for(int i=0; i<256; i++) {
		if (!histo[i])
			heights[i] = 0;
		else {
			if (histo[i] == total_pixels)
				heights[i] = graph_height;
			else {
				h = MulDiv(histo[i], 0x10000000L, total_pixels);

				if (!h)
					heights[i] = 1;
				else {
					s = -1;

					while(h<0x10000000) {
						h<<=1;
						++s;
					}

					h = graph_height - 8*s - histo_log_table[(h>>20) & 255];
					if (h<1) h=1;

					heights[i] = h;
				}
			}

		}

//			heights[i] = (histo[i]*graph_height + total_pixels - 1) / total_pixels;
	}
#endif
}

void Histogram::Draw(HDC hDC, LPRECT lpr) {
	long heights[256];

#if 1
	int i,j;

	ComputeHeights(heights, lpr->bottom - lpr->top);

	GdiFlush();

	memset(lpGraphBits, 0, 32*(lpr->bottom - lpr->top));

	for(i=0; i<256; i++) {
		char *lp = lpGraphBits + (i>>3);
		char mask = (char)(0x80 >> (i&7));

		j = heights[i];
		if(j) do {
			*lp |= mask;
			lp += 32;
		} while(--j);
	}

	BitBlt(hDC, lpr->left,lpr->top, 256, lpr->bottom-lpr->top, hDCGraph, 0, 0, SRCCOPY);
#else
	long frac_accum, frac_inc;
	long width = lpr->right - lpr->left;
	long height = lpr->bottom - lpr->top;
	HPEN hPenOld, hPenPositive, hPenNegative;
	int i;

	frac_inc = width<256 ? 0xFFFFFF/(width-1) : (0x1000000+width/2)/width;

	ComputeHeights(heights, height);

	hPenPositive = CreatePen(PS_SOLID, 0, RGB(255,0,0));
	hPenNegative = CreatePen(PS_SOLID, 0, RGB(0,0,0));

	if (hPenPositive && hPenNegative) {
		hPenOld = SelectObject(hDC, hPenNegative);

		frac_accum = 0;

		for(i=0; i<width; i++) {
			if (heights[frac_accum>>16]<height) {
				MoveToEx(hDC, lpr->left+i, lpr->top, NULL);
				LineTo(hDC, lpr->left+i, lpr->top+(height-heights[frac_accum>>16]));
			}

			frac_accum += frac_inc;
		}

		SelectObject(hDC, hPenPositive);

		frac_accum = 0;

		for(i=0; i<width; i++) {
			if (heights[frac_accum>>16]) {
				MoveToEx(hDC, lpr->left+i, lpr->top+(height-heights[frac_accum>>16]), NULL);
				LineTo(hDC, lpr->left+i, lpr->bottom);
			}

			frac_accum += frac_inc;
		}

		SelectObject(hDC, hPenOld);
	}

	if (hPenPositive) DeleteObject(hPenPositive);
	if (hPenNegative) DeleteObject(hPenNegative);
#endif
}

void Histogram::SetMode(int new_mode) {
	if (new_mode == MODE_NEXT) {
		if (++histo_mode >= NUM_MODES) histo_mode = 0;
	} else
		histo_mode = new_mode;

	SetDIBColorTable(hDCGraph, 1, 1, &fore_colors[histo_mode]);

}

int Histogram::GetMode() {
	return histo_mode;
}

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

#ifndef f_MJPEGDECODER_H
#define f_MJPEGDECODER_H

#include "VirtualDub.h"

class IMJPEGDecoder {
public:
	virtual ~IMJPEGDecoder() {};

	virtual void decodeFrameRGB15(uint32 *output, uint8 *input, int len)=0;
	virtual void decodeFrameRGB32(uint32 *output, uint8 *input, int len)=0;
	virtual void decodeFrameUYVY(uint32 *output, uint8 *input, int len)=0;
	virtual void decodeFrameYUY2(uint32 *output, uint8 *input, int len)=0;
};

IMJPEGDecoder *CreateMJPEGDecoder(int w, int h);

#endif

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

#ifndef f_CAPCLIP_H
#define f_CAPCLIP_H

#include <windows.h>
#include <vfw.h>

#include "VBitmap.h"

class CaptureFrameSource {
private:
	BITMAPINFOHEADER	bmihDecomp;
	BITMAPINFOHEADER	*bmihSrc;
	HIC					hic;
	VBitmap				vbAnalyze;
	void *				pFrameBuffer;
	bool				fDecompressionOk;

	void _destruct();

public:
	CaptureFrameSource(HWND);
	~CaptureFrameSource();

	bool CheckFrameSize(int w, int h);
	const VBitmap *Decompress(VIDEOHDR *pvhdr);
	const VBitmap *getFrameBuffer() {
		return &vbAnalyze;
	}
};

#endif

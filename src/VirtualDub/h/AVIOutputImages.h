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

#ifndef f_AVIOUTPUT_IMAGES_H
#define f_AVIOUTPUT_IMAGES_H

#include <vd2/system/VDString.h>
#include "AVIOutput.h"

class VideoSource;

class AVIOutputImages : public AVIOutput {
protected:
	VDStringW mPrefix;
	VDStringW mSuffix;
	int mDigits;
	int mFormat;
	int mQuality;

public:
	enum {
		kFormatBMP,
		kFormatTGA,
		kFormatJPEG,
		kFormatPNG,
		kFormatTGAUncompressed,
		kFormatTIFF_LZW,
		kFormatTIFF_RAW,
		kFormatCount
	};

	AVIOutputImages(const wchar_t *pszPrefix, const wchar_t *pszSuffix, int iDigits, int format, int q);
	~AVIOutputImages();

	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *createAudioStream();

	bool init(const wchar_t *szFile);
	void finalize();

	static void WriteSingleImage(const wchar_t *name, int format, int q, VDPixmap* px);
};

#endif

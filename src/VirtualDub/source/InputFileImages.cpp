//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#include <vd2/system/filesys.h>
#include <vd2/libav_tiff/tiff_image.h>
#include "ProgressDialog.h"
#include "VideoSourceImages.h"
#include "InputFileImages.h"
#include "image.h"

extern const char g_szError[];
extern HWND g_hWnd;

VDInputFileImages::VDInputFileImages(uint32 flags) {
	single_file_mode = (flags & IVDInputDriver::kOF_SingleFile)!=0;
}

VDInputFileImages::~VDInputFileImages() {
}

void VDInputFileImages::Init(const wchar_t *szFile) {
	// Attempt to discern path format.
	//
	// First, find the start of the filename.  Then skip
	// backwards until the first period is found, then to the
	// beginning of the first number.

	mBaseName = szFile;
	const wchar_t *pszBaseFormat = mBaseName.c_str();

	const wchar_t *pszFileBase = VDFileSplitPath(pszBaseFormat);
	const wchar_t *s = pszFileBase;

	mLastDigitPos = -1;

	while(*s)
		++s;

	while(s > pszFileBase && s[-1] != L'.')
		--s;

	while(s > pszFileBase) {
		--s;

		if (iswdigit(*s)) {
			mLastDigitPos = s - pszBaseFormat;
			break;
		}
	}

	mFrames = 1;

	// Make sure the first file exists.
	vdfastvector<wchar_t> namebuf;
	if (!VDDoesPathExist(ComputeFilename(namebuf, 0)))
		throw MyError("File \"%ls\" does not exist.", namebuf.data());

	// Stat as many files as we can until we get an error.
	if (mLastDigitPos >= 0 && !single_file_mode) {
		vdfastvector<wchar_t> namebuf;

		ProgressDialog pd(g_hWnd, "Image import filter", "Scanning for images", 0x3FFFFFFF, true);

		pd.setValueFormat("Scanning frame %lu");

		while(VDDoesPathExist(ComputeFilename(namebuf, mFrames))) {
			++mFrames;
			pd.advance((long)mFrames);
		}
	}

	// make sure the first frame is valid
	vdrefptr<IVDVideoSource> vs;
	GetVideoSource(0, ~vs);
}

void VDInputFileImages::setAutomated(bool fAuto) {
}

bool VDInputFileImages::GetVideoSource(int index, IVDVideoSource **ppSrc) {
	if (index)
		return false;

	*ppSrc = VDCreateVideoSourceImages(this);
	if (!*ppSrc)
		return false;
	(*ppSrc)->AddRef();
	return true;
}

bool VDInputFileImages::GetAudioSource(int index, AudioSource **ppSrc) {
	return false;
}

const wchar_t *VDInputFileImages::ComputeFilename(vdfastvector<wchar_t>& pathBuf, VDPosition pos) {
	const wchar_t *fn = mBaseName.c_str();

	if (mLastDigitPos < 0)
		return fn;

	pathBuf.assign(fn, fn + mBaseName.size() + 1);

	char buf[32];

	sprintf(buf, "%I64d", pos);

	int srcidx = strlen(buf) - 1;
	int dstidx = mLastDigitPos;
	int v = 0;

	do {
		if (srcidx >= 0)
			v += buf[srcidx--] - '0';

		if (dstidx < 0 || (unsigned)(pathBuf[dstidx] - '0') >= 10) {
			pathBuf.insert(pathBuf.begin() + (dstidx + 1), '0');
			++dstidx;
		}

		wchar_t& c = pathBuf[dstidx--];

		int result = v + (c - L'0');
		v = 0;

		if (result >= 10) {
			result -= 10;
			v = 1;
		}

		c = (wchar_t)(L'0' + result);
	} while(v || srcidx >= 0);

	return pathBuf.data();
}

///////////////////////////////////////////////////////////////////////////

class VDInputDriverImages : public vdrefcounted<IVDInputDriver> {
public:
	const wchar_t *GetSignatureName() { return L"Image sequence input driver (internal)"; }

	int GetDefaultPriority() {
		return -1;
	}

	uint32 GetFlags() { return kF_Video; }

	const wchar_t *GetFilenamePattern() {
		return L"Image sequence (*.png,*.bmp,*.tga,*.jpg,*.jpeg,*.tif,*.tiff,*.iff)\0*.png;*.bmp;*.tga;*.jpg;*.jpeg;*.tif;*.tiff;*.iff\0";
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		size_t l = wcslen(pszFilename);

		if (l>4 && !_wcsicmp(pszFilename + l - 4, L".tga"))
			return true;

		return false;
	}

	DetectionConfidence DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		if (nHeaderSize >= 32) {
			const uint8 *buf = (const uint8 *)pHeader;

			const uint8 kPNGSignature[8]={137,80,78,71,13,10,26,10};

			// Check for PNG
			if (!memcmp(buf, kPNGSignature, 8))
				return kDC_High;

			// Check for BMP
			if (buf[0] == 'B' && buf[1] == 'M')
				return kDC_High;

			// Check for MayaIFF (FOR4....CIMG)
			if (VDIsMayaIFFHeader(pHeader, nHeaderSize))
				return kDC_High;

			if (VDIsTiffHeader(pHeader, nHeaderSize))
				return kDC_High;

			if (buf[0] == 0xFF && buf[1] == 0xD8) {
				
				if (buf[2] == 0xFF && buf[3] == 0xE0) {		// x'FF' SOI x'FF' APP0
					// Hmm... might be a JPEG image.  Check for JFIF tag.

					if (buf[6] == 'J' && buf[7] == 'F' && buf[8] == 'I' && buf[9] == 'F')
						return kDC_High;		// Looks like JPEG to me.
				}

				// Nope, see if it's an Exif file instead (used by digital cameras).

				if (buf[2] == 0xFF && buf[3] == 0xE1) {		// x'FF' SOI x'FF' APP1
					if (buf[6] == 'E' && buf[7] == 'x' && buf[8] == 'i' && buf[9] == 'f')
						return kDC_High;		// Looks like JPEG to me.
				}

				// Look for a bare JPEG (start of second marker and x'FF' EOI at the end
				const uint8 *footer = (const uint8 *)pFooter;

				if (buf[2] == 0xFF && nFooterSize >= 2 && footer[nFooterSize - 2] == 0xFF && footer[nFooterSize - 1] == 0xD9)
					return kDC_High;
			}
		}

		if (nFooterSize > 18) {
			if (!memcmp((const uint8 *)pFooter + nFooterSize - 18, "TRUEVISION-XFILE.", 18))
				return kDC_High;
		}

		return kDC_None;
	}

	DetectionConfidence DetectBySignature2(VDXMediaInfo& info, const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		wcscpy(info.format_name, L"Image");
		return DetectBySignature(pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize);
	}

	InputFile *CreateInputFile(uint32 flags) {
		return new VDInputFileImages(flags);
	}
};

extern IVDInputDriver *VDCreateInputDriverImages() { return new VDInputDriverImages; }

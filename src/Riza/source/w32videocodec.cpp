//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#include <windows.h>
#include <vfw.h>
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/protscope.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDString.h>
#include <vd2/system/vdstdc.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/Riza/videocodec.h>

IVDVideoCodecBugTrap *g_pVDVideoCodecBugTrap;

class VDVideoDecompressorVCM : public IVDVideoDecompressor {
public:
	VDVideoDecompressorVCM();
	~VDVideoDecompressorVCM();

	void Init(const void *srcFormat, uint32 srcFormatSize, HIC hic);

	bool QueryTargetFormat(int format);
	bool QueryTargetFormat(const void *format);
	bool SetTargetFormat(int format);
	bool SetTargetFormat(const void *format);
	int GetTargetFormat() { return mFormat; }
	int GetTargetFormatVariant() { return mFormatVariant; }
	const uint32 *GetTargetFormatPalette() { return mFormatPalette; }
	void Start();
	void Stop();
	void DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll);
	const void *GetRawCodecHandlePtr();
	const wchar_t *GetName();
	bool GetAlpha(){ return mbUseAlpha; }

protected:
	HIC			mhic;
	int			mBestFormat;
	int			mFormat;
	int			mFormatVariant;
	bool		mbActive;
	bool		mbUseEx;
	bool		mbUseAlpha;
	VDStringW	mName;
	VDStringW	mDriverName;
	vdstructex<VDAVIBitmapInfoHeader>	mSrcFormat;
	vdstructex<VDAVIBitmapInfoHeader>	mDstFormat;

	uint32		mFormatPalette[256];
};

IVDVideoDecompressor *VDCreateVideoDecompressorVCM(const void *srcFormat, uint32 srcFormatSize, const void *pHIC) {
	vdautoptr<VDVideoDecompressorVCM> p(new VDVideoDecompressorVCM);

	p->Init(srcFormat, srcFormatSize, *(const HIC *)pHIC);
	return p.release();
}

VDVideoDecompressorVCM::VDVideoDecompressorVCM()
	: mhic(NULL)
	, mbActive(false)
	, mbUseAlpha(false)
	, mBestFormat(0)
	, mFormat(0)
	, mFormatVariant(0)
{
}

VDVideoDecompressorVCM::~VDVideoDecompressorVCM() {
	Stop();

	if (mhic) {
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		ICClose(mhic);
	}
}

void VDVideoDecompressorVCM::Init(const void *srcFormat, uint32 srcFormatSize, HIC hic) {
	VDASSERT(!mhic);

	mhic = hic;

	const VDAVIBitmapInfoHeader *bih = (const VDAVIBitmapInfoHeader *)srcFormat;

	mSrcFormat.assign(bih, srcFormatSize);

	ICINFO info = {sizeof(ICINFO)};
	DWORD rv;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		rv = ICGetInfo(mhic, &info, sizeof info);
	}

	if (rv >= sizeof info) {
		mName = info.szDescription;
		const wchar_t *pName = info.szDescription;
		mDriverName = VDswprintf(L"Video codec \"%ls\"", 1, &pName);
	}

	// utvideo alpha sources
	if (bih->biCompression==VDMAKEFOURCC('U', 'L', 'R', 'A')) mbUseAlpha = true;
	if (bih->biCompression==VDMAKEFOURCC('U', 'Q', 'R', 'A')) mbUseAlpha = true;

	// magicyuv alpha sources
	if (bih->biCompression==VDMAKEFOURCC('M', '8', 'R', 'A')) mbUseAlpha = true;
	if (bih->biCompression==VDMAKEFOURCC('M', '8', 'Y', 'A')) mbUseAlpha = true;
	if (bih->biCompression==VDMAKEFOURCC('M', '0', 'R', 'A')) mbUseAlpha = true;
	if (bih->biCompression==VDMAKEFOURCC('M', '2', 'R', 'A')) mbUseAlpha = true;
}

bool VDVideoDecompressorVCM::QueryTargetFormat(int format) {
	vdstructex<VDAVIBitmapInfoHeader> bmformat;
	const int variants = VDGetPixmapToBitmapVariants(format);

	for(int variant=1; variant<=variants; ++variant) {
		if (VDMakeBitmapFormatFromPixmapFormat(bmformat, mSrcFormat, format, variant) && QueryTargetFormat(bmformat.data()))
			return true;
	}

	return false;
}

bool VDVideoDecompressorVCM::QueryTargetFormat(const void *format) {
	const BITMAPINFO *pSrcFormat = (const BITMAPINFO *)mSrcFormat.data();
	DWORD retval;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		retval = ICDecompressQuery(mhic, pSrcFormat, (BITMAPINFO *)format);
	}

	return retval == ICERR_OK;
}

bool VDVideoDecompressorVCM::SetTargetFormat(int format) {
	using namespace nsVDPixmap;

	mBestFormat = 0;
	const BITMAPINFO *pSrcFormat = (const BITMAPINFO *)mSrcFormat.data();
	VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
	DWORD size = ICDecompressGetFormat(mhic, pSrcFormat, 0);
	if (size) {
		vdstructex<VDAVIBitmapInfoHeader> bm;
		bm.resize(size);
		DWORD retval = ICDecompressGetFormat(mhic, pSrcFormat, bm.data());
		if (retval==ICERR_OK) {
			int variant;
			mBestFormat = VDBitmapFormatToPixmapFormat(*bm.data(),variant);
		}
	}

	if (!format) {
		switch (mBestFormat) {
		case kPixFormat_YUV422_V210:
		case kPixFormat_YUV422_Planar16:
			if (SetTargetFormat(kPixFormat_YUV422_Planar16)) return true;
			if (SetTargetFormat(kPixFormat_YUV422_V210)) return true;
			break;
		}

		if (mBestFormat && SetTargetFormat(mBestFormat)) return true;

		if (SetTargetFormat(kPixFormat_YUV422_Planar16)) return true;
		if (SetTargetFormat(kPixFormat_YUV422_V210)) return true;
		if (SetTargetFormat(kPixFormat_XYUV64)) return true;
		if (SetTargetFormat(kPixFormat_XRGB64)) return true;

		if (SetTargetFormat(kPixFormat_RGB888)) return true;
		if (SetTargetFormat(kPixFormat_XRGB8888)) return true;
		if (SetTargetFormat(kPixFormat_XRGB1555)) return true;

		if (SetTargetFormat(kPixFormat_YUV422_YUYV)) return true;
		if (SetTargetFormat(kPixFormat_YUV422_UYVY)) return true;
		if (SetTargetFormat(kPixFormat_YUV420_Planar)) return true;

		if (SetTargetFormat(kPixFormat_Pal8)) return true;

		return false;
	}

	vdstructex<VDAVIBitmapInfoHeader> bmformat;
	const int variants = VDGetPixmapToBitmapVariants(format);

	for(int variant=1; variant<=variants; ++variant) {
		if (VDMakeBitmapFormatFromPixmapFormat(bmformat, mSrcFormat, format, variant) && SetTargetFormat(bmformat.data())) {
			mFormat = format;
			mFormatVariant = variant;
			return true;
		}
	}

	return false;
}

bool VDVideoDecompressorVCM::SetTargetFormat(const void *format) {
	BITMAPINFO *pSrcFormat = (BITMAPINFO *)mSrcFormat.data();
	BITMAPINFO *pDstFormat = (BITMAPINFO *)format;
	DWORD retval;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		retval = ICDecompressQuery(mhic, pSrcFormat, pDstFormat);
	}

	if (retval == ICERR_OK) {
		if (mbActive)
			Stop();

		if (pDstFormat->bmiHeader.biCompression == BI_RGB && pDstFormat->bmiHeader.biBitCount <= 8) {
			uint32 colors = 1 << pDstFormat->bmiHeader.biBitCount;

			if (pDstFormat->bmiHeader.biClrUsed && pDstFormat->bmiHeader.biClrUsed < colors)
				colors = pDstFormat->bmiHeader.biClrUsed;

			if (colors > 256)
				colors = 256;

			uint32 palOffset = pDstFormat->bmiHeader.biSize;
			memcpy(mFormatPalette, (const char *)format + palOffset, sizeof(uint32)*colors);
		}

		mDstFormat.assign((const VDAVIBitmapInfoHeader *)format, VDGetSizeOfBitmapHeaderW32((const BITMAPINFOHEADER *)format));
		mFormat = 0;
		mFormatVariant = 0;
		return true;
	}

	return false;
}

void VDVideoDecompressorVCM::Start() {
	if (mDstFormat.empty())
		throw MyError("Cannot find compatible target format for video decompression.");

	if (!mbActive) {
		BITMAPINFO *pSrcFormat = (BITMAPINFO *)mSrcFormat.data();
		BITMAPINFO *pDstFormat = (BITMAPINFO *)mDstFormat.data();
		DWORD retval;

		mbUseEx = false;
		{
			VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
			retval = ICDecompressBegin(mhic, pSrcFormat, pDstFormat);

			if (retval != ICERR_OK) {
				BITMAPINFOHEADER *bihSrc = (BITMAPINFOHEADER *)pSrcFormat;
				BITMAPINFOHEADER *bihDst = (BITMAPINFOHEADER *)pDstFormat;
				if (ICERR_OK == ICDecompressExBegin(mhic, 0, bihSrc, NULL, 0, 0, bihSrc->biWidth, abs(bihSrc->biHeight), bihDst, NULL, 0, 0, bihDst->biWidth, abs(bihDst->biHeight))) {
					mbUseEx = true;
					retval = ICERR_OK;
				}
			}
		}

		if (retval != ICERR_OK)
			throw MyICError("VideoSourceAVI", retval);

		mbActive = true;
	}
}

void VDVideoDecompressorVCM::Stop() {
	if (mbActive) {
		mbActive = false;

		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		if (mbUseEx)
			ICDecompressExEnd(mhic);
		else
			ICDecompressEnd(mhic);
	}
}

void VDVideoDecompressorVCM::DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) {
	if (!mbActive)
		Start();

	VDAVIBitmapInfoHeader *pSrcFormat = mSrcFormat.data();
	VDAVIBitmapInfoHeader *pDstFormat = mDstFormat.data();

	DWORD dwFlags = 0;

	if (!keyframe)
		dwFlags |= ICDECOMPRESS_NOTKEYFRAME;

	if (preroll)
		dwFlags |= ICDECOMPRESS_PREROLL;

	DWORD dwOldSize = pSrcFormat->biSizeImage;
	pSrcFormat->biSizeImage = srcSize;
	DWORD retval;
	
	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		if (mbUseEx) {
			BITMAPINFOHEADER *bihSrc = (BITMAPINFOHEADER *)pSrcFormat;
			BITMAPINFOHEADER *bihDst = (BITMAPINFOHEADER *)pDstFormat;
			retval = ICDecompressEx(mhic, dwFlags, bihSrc, (LPVOID)src, 0, 0, bihSrc->biWidth, abs(bihSrc->biHeight), bihDst, dst, 0, 0, bihDst->biWidth, abs(bihDst->biHeight));
		} else
			retval = ICDecompress(mhic, dwFlags, (BITMAPINFOHEADER *)pSrcFormat, (LPVOID)src, (BITMAPINFOHEADER *)pDstFormat, dst);
	}

	pSrcFormat->biSizeImage = dwOldSize;

	// We will get ICERR_DONTDRAW if we set preroll.
	if (retval < 0)
		throw MyICError(retval, "%%s (Error code: %d)", (int)retval);
}

const void *VDVideoDecompressorVCM::GetRawCodecHandlePtr() {
	return &mhic;
}

const wchar_t *VDVideoDecompressorVCM::GetName() {
	return mName.c_str();
}

///////////////////////////////////////////////////////////////////////////

namespace {
	static bool CheckMPEG4Codec(HIC hic, bool isV3) {
		char frame[0x380];
		BITMAPINFOHEADER bih;

		// Form a completely black frame if it's V3.

		bih.biSize			= 40;
		bih.biWidth			= 320;
		bih.biHeight		= 240;
		bih.biPlanes		= 1;
		bih.biBitCount		= 24;
		bih.biCompression	= '24PM';
		bih.biSizeImage		= 0;
		bih.biXPelsPerMeter	= 0;
		bih.biYPelsPerMeter	= 0;
		bih.biClrUsed		= 0;
		bih.biClrImportant	= 0;

		if (isV3) {
			int i;

			frame[0] = (char)0x3f;
			frame[1] = (char)0x71;
			frame[2] = (char)0x1b;
			frame[3] = (char)0x7c;

			for(i=4; i<0x179; i+=5) {
				frame[i+0] = (char)0x2f;
				frame[i+1] = (char)0x0b;
				frame[i+2] = (char)0xc2;
				frame[i+3] = (char)0xf0;
				frame[i+4] = (char)0xbc;
			}

			frame[0x179] = (char)0xf0;
			frame[0x17a] = (char)0xb8;
			frame[0x17b] = (char)0x01;

			bih.biCompression	= '34PM';
			bih.biSizeImage		= 0x17c;
		}

		// Attempt to decompress.

		HANDLE h;

		{
			VDSilentExternalCodeBracket bracket;
			h = ICImageDecompress(hic, 0, (BITMAPINFO *)&bih, frame, NULL);
		}

		if (h) {
			GlobalFree(h);
			return true;
		} else {
			return false;
		}
	}

	DWORD VDSafeICDecompressQueryW32(HIC hic, LPBITMAPINFOHEADER lpbiIn, uint32 cbIn, LPBITMAPINFOHEADER lpbiOut, uint32 cbOut, const wchar_t *codecDesc) {
		vdstructex<BITMAPINFOHEADER> bihIn, bihOut;

		if (lpbiIn)
			bihIn.assign(lpbiIn, cbIn);

		if (lpbiOut)
			bihOut.assign(lpbiIn, cbOut);

		// AngelPotion overwrites its input format with biCompression='MP43' and doesn't
		// restore it, which leads to video codec lookup errors.  So what we do here is
		// make a copy of the format in nice, safe memory and feed that in instead.

		// We used to write protect the format here, but apparently some versions of Windows
		// have certain functions accepting (BITMAPINFOHEADER *) that actually call
		// IsBadWritePtr() to verify the incoming pointer, even though those functions
		// don't actually need to write to the format.  It would be nice if someone learned
		// what 'const' was for.  AngelPotion doesn't crash because it has a try/catch
		// handler wrapped around its code.

		DWORD result;
		{
			VDExternalCodeBracket bracket(codecDesc, __FILE__, __LINE__);
			result = ICDecompressQuery(hic, lpbiIn ? bihIn.data() : NULL, lpbiOut ? bihOut.data() : NULL);
		}

		// check for unwanted modification
		if ((lpbiIn && memcmp(bihIn.data(), lpbiIn, cbIn)) || (lpbiOut && memcmp(bihOut.data(), lpbiOut, cbOut))) {
			ICINFO info = {sizeof(ICINFO)};
			{
				VDExternalCodeBracket bracket(codecDesc, __FILE__, __LINE__);
				ICGetInfo(hic, &info, sizeof info);
			}

			if (g_pVDVideoCodecBugTrap)
				g_pVDVideoCodecBugTrap->OnCodecRenamingDetected(info.szDescription);
		}

		// if the result passed, check whether we have a bad MS MPEG-4 V2/V3 codec

		if (result == ICERR_OK) {
			// check for bad MPEG-4 V2/V3 codec

			if (lpbiIn->biCompression == '24PM') {
				if (!CheckMPEG4Codec(hic, false))
					return ICERR_UNSUPPORTED;
			} else if (lpbiIn->biCompression == '34PM') {
				if (!CheckMPEG4Codec(hic, true))
					return ICERR_UNSUPPORTED;
			}
		}

		return result;
	}

	HIC VDSafeICOpenW32(DWORD fccType, DWORD fccHandler, UINT wMode) {
		HIC hic;
		
		vdprotected1("attempting to open video codec with FOURCC '%.4s'", const char *, (const char *)&fccHandler) {
			wchar_t buf[64];
			vdswprintf(buf, sizeof buf / sizeof buf[0], L"A video codec with FOURCC '%.4S'", (const char *)&fccHandler);
			VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);
			hic = ICOpen(fccType, fccHandler, wMode);
		}

		return hic;
	}

	HIC VDSafeICLocateDecompressW32(DWORD fccType, DWORD fccHandler, LPBITMAPINFOHEADER lpbiIn, uint32 cbIn, LPBITMAPINFOHEADER lpbiOut, uint32 cbOut) {
		ICINFO info={0};

		for(DWORD id=0; ICInfo(fccType, id, &info); ++id) {
			info.dwSize = sizeof(ICINFO);	// I don't think this is necessary, but just in case....

			HIC hic = VDSafeICOpenW32(fccType, info.fccHandler, ICMODE_DECOMPRESS);

			if (!hic)
				continue;

			wchar_t buf[64];
			vdswprintf(buf, 64, L"A video codec with FOURCC '%.4S'", (const char *)&fccHandler);

			vdprotected1("querying video codec with FOURCC \"%.4s\"", const char *, (const char *)&info.fccHandler) {
				DWORD result = VDSafeICDecompressQueryW32(hic, lpbiIn, cbIn, lpbiOut, cbOut, buf);

				if (result == ICERR_OK) {
					// Check for a codec that doesn't actually support what it says it does.
					// We ask the codec whether it can do a specific conversion that it can't
					// possibly support.  If it does it, then we call BS and ignore the codec.
					// The Grand Tech Camera Codec and Panasonic DV codecs are known to do this.
					//
					// (general idea from Raymond Chen's blog)

					BITMAPINFOHEADER testSrc = {		// note: can't be static const since IsBadWritePtr() will get called on it
						sizeof(BITMAPINFOHEADER),
						320,
						240,
						1,
						24,
						0x2E532E42,
						320*240*3,
						0,
						0,
						0,
						0
					};

					DWORD res;
					{
						VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);
						res = ICDecompressQuery(hic, &testSrc, NULL);
					}

					if (ICERR_OK == res) {		// Don't need to wrap this, as it's OK if testSrc gets modified.
						ICINFO info = {sizeof(ICINFO)};

						{
							VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);
							ICGetInfo(hic, &info, sizeof info);
						}

						if (g_pVDVideoCodecBugTrap)
							g_pVDVideoCodecBugTrap->OnAcceptedBS(info.szDescription);

						// Okay, let's give the codec a chance to redeem itself. Reformat the input format into
						// a plain 24-bit RGB image, and ask it what the compressed format is. If it produces
						// a FOURCC that matches, allow it to handle the format. This should allow at least
						// the codec's primary format to work. Otherwise, drop it on the ground.
						
						if (lpbiIn) {
							BITMAPINFOHEADER unpackedSrc={
								sizeof(BITMAPINFOHEADER),
								lpbiIn ? lpbiIn->biWidth : 320,
								lpbiIn ? lpbiIn->biHeight : 240,
								1,
								24,
								BI_RGB,
								0,
								0,
								0,
								0,
								0
							};

							unpackedSrc.biSizeImage = ((unpackedSrc.biWidth*3+3)&~3)*abs(unpackedSrc.biHeight);

							LONG size = ICCompressGetFormatSize(hic, &unpackedSrc);

							if (size >= sizeof(BITMAPINFOHEADER)) {
								vdstructex<BITMAPINFOHEADER> tmp;

								tmp.resize(size);
								if (ICERR_OK == ICCompressGetFormat(hic, &unpackedSrc, tmp.data()) && tmp->biCompression == lpbiIn->biCompression)
									return hic;
							}
						}
					} else {
						return hic;
					}
				}

				ICClose(hic);
			}
		}

		return NULL;
	}
}

IVDVideoDecompressor *VDFindVideoDecompressor(uint32 preferredHandler, const void *srcFormat, uint32 srcFormatSize) {
	vdstructex<BITMAPINFOHEADER> bmih((const BITMAPINFOHEADER *)srcFormat, srcFormatSize);
	HIC hicDecomp = NULL;

	vdprotected2("attempting codec negotiation: fccHandler=0x%08x, biCompression=0x%08x", unsigned, preferredHandler, unsigned, bmih->biCompression) {
		VDExternalCodeBracket bracket(L"A video codec", __FILE__, __LINE__);

		// Try the handler specified in the file first.  In some cases, it'll
		// be wrong or missing. (VideoMatrix, among other programs, sets fccHandler=0.

		if (preferredHandler)
			hicDecomp = VDSafeICOpenW32(ICTYPE_VIDEO, preferredHandler, ICMODE_DECOMPRESS);

		wchar_t buf[64];
		vdswprintf(buf, 64, L"A video codec with FOURCC '%.4S'", (const char *)&preferredHandler);

		if (!hicDecomp || ICERR_OK!=VDSafeICDecompressQueryW32(hicDecomp, &*bmih, bmih.size(), NULL, 0, buf)) {
			if (hicDecomp)
				ICClose(hicDecomp);

			// Pick a handler based on the biCompression field instead. We should imitate the
			// mappings that ICLocate() does -- namely, BI_RGB and BI_RLE8 map to MRLE, and
			// CRAM maps to MSVC (apparently an outdated name for Microsoft Video 1).

			DWORD fcc = bmih->biCompression;

			if (fcc == BI_RGB || fcc == BI_RLE8)
				fcc = 'ELRM';
			else if (fcc == 'MARC')
				fcc = 'CVSM';

			if (fcc >= 0x10000)		// if we couldn't map a numerical value like BI_BITFIELDS, don't open a random codec
				hicDecomp = VDSafeICOpenW32(ICTYPE_VIDEO, fcc, ICMODE_DECOMPRESS);

			if (!hicDecomp || ICERR_OK!=VDSafeICDecompressQueryW32(hicDecomp, &*bmih, bmih.size(), NULL, 0, buf)) {
				if (hicDecomp) {
					ICClose(hicDecomp);
					hicDecomp = NULL;
				}

				// Failed. Check if it is an MPEG-4 V3 clone; if so, cycle through the known clones
				// in order.

				static const uint32 kMPEG4V3Clones[]={
					'34PM',
					'3VID',
					'4VID',
					'5VID',
					'14PA'
				};

				enum { kMPEG4V3CloneCount = sizeof kMPEG4V3Clones / sizeof kMPEG4V3Clones[0] };

				for(int i=0; i<kMPEG4V3CloneCount; ++i) {
					if (bmih->biCompression == kMPEG4V3Clones[i]) {
						// clone
						for(int j=0; j<kMPEG4V3CloneCount; ++j) {
							if (i == j)
								continue;

							bmih->biCompression = kMPEG4V3Clones[j];
							hicDecomp = VDSafeICLocateDecompressW32(ICTYPE_VIDEO, NULL, &*bmih, bmih.size(), NULL, 0);
							if (hicDecomp)
								break;
							bmih->biCompression = fcc;
						}

						break;
					}
				}

				// Okay, search all installed codecs.
				if (!hicDecomp)
					hicDecomp = VDSafeICLocateDecompressW32(ICTYPE_VIDEO, NULL, &*bmih, bmih.size(), NULL, 0);
			}
		}
	}

	if (!hicDecomp)
		return NULL;

	// All good!

	return VDCreateVideoDecompressorVCM(&*bmih, bmih.size(), &hicDecomp);
}

void VDSetVideoCodecBugTrap(IVDVideoCodecBugTrap *p) {
	g_pVDVideoCodecBugTrap = p;
}

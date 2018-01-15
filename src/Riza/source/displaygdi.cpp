#include <windows.h>
#include <vd2/system/binary.h>
#include <vd2/system/vectors.h>
#include <vd2/system/VDString.h>
#include <vd2/Kasumi/blitter.h>
#include <vd2/system/profile.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/compositor.h>
#include <vd2/VDDisplay/displaydrv.h>
#include <vd2/VDDisplay/renderer.h>

#define VDDEBUG_DISP (void)sizeof printf
//#define VDDEBUG_DISP VDDEBUG

void VDDitherImage(VDPixmap& dst, const VDPixmap& src, const uint8 *pLogPal);

///////////////////////////////////////////////////////////////////////////////
class VDDisplayCachedImageGDI : public vdrefcounted<IVDRefUnknown>, public vdlist_node {
	VDDisplayCachedImageGDI(const VDDisplayCachedImageGDI&);
	VDDisplayCachedImageGDI& operator=(const VDDisplayCachedImageGDI&);
public:
	enum { kTypeID = 'cimI' };

	VDDisplayCachedImageGDI();
	~VDDisplayCachedImageGDI();

	void *AsInterface(uint32 iid);

	bool Init(void *owner, const VDDisplayImageView& imageView);
	void Shutdown();

	void Update(const VDDisplayImageView& imageView);

public:
	void *mpOwner;
	HDC		mhdc;
	HBITMAP	mhbm;
	HGDIOBJ	mhbmOld;
	sint32	mWidth;
	sint32	mHeight;
	uint32	mUniquenessCounter;
};

VDDisplayCachedImageGDI::VDDisplayCachedImageGDI()
	: mpOwner(NULL)
	, mhdc(NULL)
	, mhbm(NULL)
	, mhbmOld(NULL)
	, mWidth(0)
	, mHeight(0)
{
	mListNodePrev = NULL;
	mListNodeNext = NULL;
}

VDDisplayCachedImageGDI::~VDDisplayCachedImageGDI() {
	if (mListNodePrev)
		vdlist_base::unlink(*this);
}

void *VDDisplayCachedImageGDI::AsInterface(uint32 iid) {
	if (iid == kTypeID)
		return this;

	return NULL;
}

bool VDDisplayCachedImageGDI::Init(void *owner, const VDDisplayImageView& imageView) {
	const VDPixmap& px = imageView.GetImage();
	int w = px.w;
	int h = px.h;

	HDC hdc = GetDC(NULL);
	if (hdc) {
		mhdc = CreateCompatibleDC(NULL);
		mhbm = CreateCompatibleBitmap(hdc, w, h);
	}

	if (!mhdc || !mhbm) {
		Shutdown();
		return false;
	}

	mhbmOld = SelectObject(mhdc, mhbm);

	mWidth = px.w;
	mHeight = px.h;
	mpOwner = owner;

	Update(imageView);
	return true;
}

void VDDisplayCachedImageGDI::Shutdown() {
	if (mhdc) {
		if (mhbmOld) {
			SelectObject(mhdc, mhbmOld);
			mhbmOld = NULL;
		}

		DeleteDC(mhdc);
	}

	if (mhbm) {
		DeleteObject(mhbm);
		mhbm = NULL;
	}

	mpOwner = NULL;
}

void VDDisplayCachedImageGDI::Update(const VDDisplayImageView& imageView) {
	mUniquenessCounter = imageView.GetUniquenessCounter();

	if (mhbm) {
		const VDPixmap& px = imageView.GetImage();

		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, mWidth, mHeight, 4);
		VDPixmapLayoutFlipV(layout);

		VDPixmapBuffer buf;
		buf.init(layout);

		VDPixmapBlt(buf, px);

		BITMAPINFO bi = {};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = mWidth;
		bi.bmiHeader.biHeight = mHeight;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biSizeImage = 0;
		bi.bmiHeader.biCompression = BI_RGB;

		VDVERIFY(::SetDIBits(mhdc, mhbm, 0, mHeight, buf.base(), &bi, DIB_RGB_COLORS));
	}
}

///////////////////////////////////////////////////////////////////////////////
class VDDisplayRendererGDI : public IVDDisplayRenderer {
public:
	VDDisplayRendererGDI();
	~VDDisplayRendererGDI();

	void Init();
	void Shutdown();

	bool Begin(HDC hdc);
	void End();

	void SetColorRGB(uint32 color);
	void FillRect(sint32 x, sint32 y, sint32 w, sint32 h);
	void Blt(sint32 x, sint32 y, VDDisplayImageView& imageView);

protected:
	VDDisplayCachedImageGDI *GetCachedImage(VDDisplayImageView& imageView);

	HDC		mhdc;
	int		mSavedDC;
	uint32	mColor;
	uint32	mPenColor;
	HPEN	mhPen;
	uint32	mBrushColor;
	HBRUSH	mhBrush;

	vdlist<VDDisplayCachedImageGDI> mCachedImages;
};

VDDisplayRendererGDI::VDDisplayRendererGDI()
	: mhdc(NULL)
	, mSavedDC(0)
	, mPenColor(0)
	, mhPen((HPEN)::GetStockObject(BLACK_PEN))
	, mBrushColor(0)
	, mhBrush((HBRUSH)::GetStockObject(BLACK_BRUSH))
{
}

VDDisplayRendererGDI::~VDDisplayRendererGDI() {
}

void VDDisplayRendererGDI::Init() {
}

void VDDisplayRendererGDI::Shutdown() {
	while(!mCachedImages.empty()) {
		VDDisplayCachedImageGDI *img = mCachedImages.front();
		mCachedImages.pop_front();

		img->mListNodePrev = NULL;
		img->mListNodeNext = NULL;

		img->Shutdown();
	}
}

bool VDDisplayRendererGDI::Begin(HDC hdc) {
	mhdc = hdc;

	mSavedDC = SaveDC(mhdc);
	if (!mSavedDC)
		return false;

	mhPen = (HPEN)::GetStockObject(BLACK_PEN);
	mhBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
	mBrushColor = 0;

	return true;
}

void VDDisplayRendererGDI::End() {
	if (mSavedDC) {
		RestoreDC(mhdc, mSavedDC);
		mSavedDC = 0;
	}

	if (mhPen) {
		DeleteObject(mhPen);
		mhPen = NULL;
	}

	if (mhBrush) {
		DeleteObject(mhBrush);
		mhBrush = NULL;
	}
}

void VDDisplayRendererGDI::SetColorRGB(uint32 color) {
	mColor = color;
}

void VDDisplayRendererGDI::FillRect(sint32 x, sint32 y, sint32 w, sint32 h) {
	if (w <= 0 || h <= 0)
		return;

	if (mBrushColor != mColor) {
		mBrushColor = mColor;

		HBRUSH hNewBrush = CreateSolidBrush(VDSwizzleU32(mColor) >> 8);
		if (hNewBrush) {
			DeleteObject(mhBrush);
			mhBrush = hNewBrush;
		}
	}

	RECT r = { x, y, x + w, y + h };
	::FillRect(mhdc, &r, mhBrush);
}

void VDDisplayRendererGDI::Blt(sint32 x, sint32 y, VDDisplayImageView& imageView) {
	VDDisplayCachedImageGDI *cachedImage = GetCachedImage(imageView);

	if (!cachedImage)
		return;

	BitBlt(mhdc, x, y, cachedImage->mWidth, cachedImage->mHeight, cachedImage->mhdc, 0, 0, SRCCOPY);
}

VDDisplayCachedImageGDI *VDDisplayRendererGDI::GetCachedImage(VDDisplayImageView& imageView) {
	VDDisplayCachedImageGDI *cachedImage = vdpoly_cast<VDDisplayCachedImageGDI *>(imageView.GetCachedImage());

	if (cachedImage && cachedImage->mpOwner != this)
		cachedImage = NULL;

	if (!cachedImage) {
		cachedImage = new_nothrow VDDisplayCachedImageGDI;

		if (!cachedImage)
			return NULL;
		
		cachedImage->AddRef();
		if (!cachedImage->Init(this, imageView)) {
			cachedImage->Release();
			return NULL;
		}

		imageView.SetCachedImage(cachedImage);
		mCachedImages.push_back(cachedImage);

		cachedImage->Release();
	} else {
		uint32 c = imageView.GetUniquenessCounter();

		if (cachedImage->mUniquenessCounter != c)
			cachedImage->Update(imageView);
	}

	return cachedImage;
}

///////////////////////////////////////////////////////////////////////////////
class VDVideoDisplayMinidriverGDI : public VDVideoDisplayMinidriver {
public:
	VDVideoDisplayMinidriverGDI();
	~VDVideoDisplayMinidriverGDI();

	bool Init(HWND hwnd, HMONITOR hmonitor, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid() { return mbValid; }
	void SetDestRect(const vdrect32 *r, uint32 color);

	bool Update(UpdateMode);
	void Refresh(UpdateMode);
	bool Paint(HDC hdc, const RECT& rClient, UpdateMode mode);
	bool SetSubrect(const vdrect32 *r);
	void SetLogicalPalette(const uint8 *pLogicalPalette) { mpLogicalPalette = pLogicalPalette; }

protected:
	void InternalRefresh(HDC hdc, const RECT& rClient, UpdateMode mode);
	static int GetScreenIntermediatePixmapFormat(HDC);

	void InitCompositionBuffer();
	void ShutdownCompositionBuffer();

	HWND		mhwnd;
	HDC			mhdc;
	HBITMAP		mhbm;
	HGDIOBJ		mhbmOld;
	void *		mpBitmapBits;
	ptrdiff_t	mPitch;
	HPALETTE	mpal;
	const uint8 *mpLogicalPalette;
	bool		mbPaletted;
	bool		mbValid;
	bool		mbUseSubrect;

	uint32		mCompBufferWidth;
	uint32		mCompBufferHeight;
	HBITMAP		mhbmCompBuffer;
	HGDIOBJ		mhbmCompBufferOld;
	HDC			mhdcCompBuffer;

	bool		mbConvertToScreenFormat;
	int			mScreenFormat;

	vdrect32	mSubrect;

	uint8		mIdentTab[256];

	VDVideoDisplaySourceInfo	mSource;

	VDDisplayRendererGDI mRenderer;

	VDPixmapCachedBlitter mCachedBlitter;
};

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverGDI() {
	return new VDVideoDisplayMinidriverGDI;
}

VDVideoDisplayMinidriverGDI::VDVideoDisplayMinidriverGDI()
	: mhwnd(0)
	, mhdc(0)
	, mhbm(0)
	, mpal(0)
	, mpLogicalPalette(NULL)
	, mbValid(false)
	, mbUseSubrect(false)
	, mCompBufferWidth(0)
	, mCompBufferHeight(0)
	, mhbmCompBuffer(NULL)
	, mhbmCompBufferOld(NULL)
	, mhdcCompBuffer(NULL)
{
	memset(&mSource, 0, sizeof mSource);
}

VDVideoDisplayMinidriverGDI::~VDVideoDisplayMinidriverGDI() {
}

bool VDVideoDisplayMinidriverGDI::Init(HWND hwnd, HMONITOR hmonitor, const VDVideoDisplaySourceInfo& info) {
	mCachedBlitter.Invalidate();

	if (info.pixmap.format>nsVDPixmap::kPixFormat_Max_Standard) return false;

	switch(info.pixmap.format) {
	case nsVDPixmap::kPixFormat_Pal8:
	case nsVDPixmap::kPixFormat_XRGB1555:
	case nsVDPixmap::kPixFormat_RGB565:
	case nsVDPixmap::kPixFormat_RGB888:
	case nsVDPixmap::kPixFormat_XRGB8888:
		break;

	case nsVDPixmap::kPixFormat_YUV444_XVYU:
	//case nsVDPixmap::kPixFormat_YUV422_Planar_16F:
	//case nsVDPixmap::kPixFormat_YUV422_Planar_Centered:
	//case nsVDPixmap::kPixFormat_YUV420_Planar_Centered:
		return false;

	default:
		if (!info.bAllowConversion)	return false;
	}
	
	mhwnd	= hwnd;
	mSource	= info;
	mbConvertToScreenFormat = false;

	if (HDC hdc = GetDC(mhwnd)) {
		mScreenFormat = GetScreenIntermediatePixmapFormat(hdc);

		mhdc = CreateCompatibleDC(hdc);

		if (mhdc) {
			bool bPaletted = 0 != (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE);

			mbPaletted = bPaletted;

			if (bPaletted) {
				struct {
					BITMAPINFOHEADER hdr;
					RGBQUAD pal[256];
				} bih;

				bih.hdr.biSize			= sizeof(BITMAPINFOHEADER);
				bih.hdr.biWidth			= mSource.pixmap.w;
				bih.hdr.biHeight		= mSource.pixmap.h;
				bih.hdr.biPlanes		= 1;
				bih.hdr.biCompression	= BI_RGB;
				bih.hdr.biBitCount		= 8;

				mPitch = ((mSource.pixmap.w + 3) & ~3);
				bih.hdr.biSizeImage		= mPitch * mSource.pixmap.h;
				bih.hdr.biClrUsed		= 216;
				bih.hdr.biClrImportant	= 216;

				for(int i=0; i<216; ++i) {
					bih.pal[i].rgbRed	= (BYTE)((i / 36) * 51);
					bih.pal[i].rgbGreen	= (BYTE)(((i%36) / 6) * 51);
					bih.pal[i].rgbBlue	= (BYTE)((i%6) * 51);
					bih.pal[i].rgbReserved = 0;
				}

				for(int j=0; j<256; ++j)
					mIdentTab[j] = (uint8)j;

				mhbm = CreateDIBSection(hdc, (const BITMAPINFO *)&bih, DIB_RGB_COLORS, &mpBitmapBits, mSource.pSharedObject, mSource.sharedOffset);
			} else if (mSource.pixmap.format == nsVDPixmap::kPixFormat_Pal8) {
				struct {
					BITMAPINFOHEADER hdr;
					RGBQUAD pal[256];
				} bih;

				bih.hdr.biSize			= sizeof(BITMAPINFOHEADER);
				bih.hdr.biWidth			= mSource.pixmap.w;
				bih.hdr.biHeight		= mSource.pixmap.h;
				bih.hdr.biPlanes		= 1;
				bih.hdr.biCompression	= BI_RGB;
				bih.hdr.biBitCount		= 8;

				mPitch = ((mSource.pixmap.w + 3) & ~3);
				bih.hdr.biSizeImage		= mPitch * mSource.pixmap.h;
				bih.hdr.biClrUsed		= 256;
				bih.hdr.biClrImportant	= 256;

				for(int i=0; i<256; ++i) {
					bih.pal[i].rgbRed	= (uint8)(mSource.pixmap.palette[i] >> 16);
					bih.pal[i].rgbGreen	= (uint8)(mSource.pixmap.palette[i] >> 8);
					bih.pal[i].rgbBlue	= (uint8)mSource.pixmap.palette[i];
					bih.pal[i].rgbReserved = 0;
				}

				mhbm = CreateDIBSection(hdc, (const BITMAPINFO *)&bih, DIB_RGB_COLORS, &mpBitmapBits, mSource.pSharedObject, mSource.sharedOffset);
			} else {
				BITMAPV4HEADER bih = {0};

				bih.bV4Size				= sizeof(BITMAPINFOHEADER);
				bih.bV4Width			= mSource.pixmap.w;
				bih.bV4Height			= mSource.pixmap.h;
				bih.bV4Planes			= 1;
				bih.bV4V4Compression	= BI_RGB;
				bih.bV4BitCount			= (WORD)(mSource.bpp << 3);

				switch(mSource.pixmap.format) {
				case nsVDPixmap::kPixFormat_XRGB1555:
				case nsVDPixmap::kPixFormat_RGB888:
				case nsVDPixmap::kPixFormat_XRGB8888:
					break;
				default:
					switch(mScreenFormat) {
					case nsVDPixmap::kPixFormat_XRGB1555:
						bih.bV4BitCount			= 16;
						break;
					case nsVDPixmap::kPixFormat_RGB565:
						bih.bV4V4Compression	= BI_BITFIELDS;
						bih.bV4RedMask			= 0xf800;
						bih.bV4GreenMask		= 0x07e0;
						bih.bV4BlueMask			= 0x001f;
						bih.bV4BitCount			= 16;
						break;
					case nsVDPixmap::kPixFormat_RGB888:
						bih.bV4BitCount			= 24;
						break;
					case nsVDPixmap::kPixFormat_XRGB8888:
						bih.bV4BitCount			= 32;
						break;
					}
					mbConvertToScreenFormat = true;
					break;
				}

				mPitch = ((mSource.pixmap.w * bih.bV4BitCount + 31)>>5)*4;
				bih.bV4SizeImage		= mPitch * mSource.pixmap.h;
				mhbm = CreateDIBSection(hdc, (const BITMAPINFO *)&bih, DIB_RGB_COLORS, &mpBitmapBits, mSource.pSharedObject, mSource.sharedOffset);
			}

			if (mhbm) {
				mhbmOld = SelectObject(mhdc, mhbm);

				if (mhbmOld) {
					ReleaseDC(mhwnd, hdc);
					VDDEBUG_DISP("VideoDisplay: Using GDI for %dx%d %s display.\n", mSource.pixmap.w, mSource.pixmap.h, VDPixmapGetInfo(mSource.pixmap.format).name);
					mbValid = (mSource.pSharedObject != 0);

					mRenderer.Init();
					return true;
				}

				if (mSource.pSharedObject && mSource.sharedOffset >= 65536)
					UnmapViewOfFile(mpBitmapBits);		// Workaround for GDI memory leak in NT4

				DeleteObject(mhbm);
				mhbm = 0;
			}
			DeleteDC(mhdc);
			mhdc = 0;
		}

		ReleaseDC(mhwnd, hdc);
	}

	Shutdown();
	return false;
}

void VDVideoDisplayMinidriverGDI::Shutdown() {
	ShutdownCompositionBuffer();

	mRenderer.Shutdown();

	if (mhbm) {
		SelectObject(mhdc, mhbmOld);
		DeleteObject(mhbm);
		if (mSource.pSharedObject && mSource.sharedOffset >= 65536)
			UnmapViewOfFile(mpBitmapBits);		// Workaround for GDI memory leak in NT4
		mhbm = 0;
	}

	if (mhdc) {
		DeleteDC(mhdc);
		mhdc = 0;
	}

	mbValid = false;
}

bool VDVideoDisplayMinidriverGDI::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (!mhdc)
		return false;

	if (mSource.pSharedObject)
		return false;
	
	if (mSource.pixmap.w != info.pixmap.w || mSource.pixmap.h != info.pixmap.h || mSource.pixmap.pitch != info.pixmap.pitch)
		return false;

	const int prevFormat = mSource.pixmap.format;
	const int nextFormat = info.pixmap.format;
	if (prevFormat != nextFormat) {
		// Check for compatible formats.
		switch(prevFormat) {
			case nsVDPixmap::kPixFormat_YUV420it_Planar:
				if (nextFormat == nsVDPixmap::kPixFormat_YUV420ib_Planar)
					break;
				return false;

			case nsVDPixmap::kPixFormat_YUV420it_Planar_FR:
				if (nextFormat == nsVDPixmap::kPixFormat_YUV420ib_Planar_FR)
					break;
				return false;

			case nsVDPixmap::kPixFormat_YUV420it_Planar_709:
				if (nextFormat == nsVDPixmap::kPixFormat_YUV420ib_Planar_709)
					break;
				return false;

			case nsVDPixmap::kPixFormat_YUV420it_Planar_709_FR:
				if (nextFormat == nsVDPixmap::kPixFormat_YUV420ib_Planar_709_FR)
					break;
				return false;

			case nsVDPixmap::kPixFormat_YUV420ib_Planar:
				if (nextFormat == nsVDPixmap::kPixFormat_YUV420it_Planar)
					break;
				return false;

			case nsVDPixmap::kPixFormat_YUV420ib_Planar_FR:
				if (nextFormat == nsVDPixmap::kPixFormat_YUV420it_Planar_FR)
					break;
				return false;

			case nsVDPixmap::kPixFormat_YUV420ib_Planar_709:
				if (nextFormat == nsVDPixmap::kPixFormat_YUV420it_Planar_709)
					break;
				return false;

			case nsVDPixmap::kPixFormat_YUV420ib_Planar_709_FR:
				if (nextFormat == nsVDPixmap::kPixFormat_YUV420it_Planar_709_FR)
					break;
				return false;

			default:
				return false;
		}
	}

	mSource = info;
	return true;
}

void VDVideoDisplayMinidriverGDI::SetDestRect(const vdrect32 *r, uint32 color) {
	VDVideoDisplayMinidriver::SetDestRect(r, color);
	if (mhwnd)
		InvalidateRect(mhwnd, NULL, FALSE);
}

bool VDVideoDisplayMinidriverGDI::Update(UpdateMode mode) {
	if (!mSource.pixmap.data)
		return false;

	if (!mSource.pSharedObject) {
		GdiFlush();

		VDPixmap source(mSource.pixmap);

		char *dst = (char *)mpBitmapBits + mPitch*(source.h - 1);
		ptrdiff_t dstpitch = -mPitch;

		if (mSource.bInterlaced && (mode & kModeFieldMask) != kModeAllFields) {
			const bool oddField = ((mode & kModeFieldMask) == kModeOddField);
			source = VDPixmapExtractField(mSource.pixmap, oddField);
			if (oddField)
				dst += dstpitch;

			dstpitch += dstpitch;

			switch(source.format) {
				case nsVDPixmap::kPixFormat_YUV420i_Planar:
					if (oddField)
						source.format = nsVDPixmap::kPixFormat_YUV420ib_Planar;
					else
						source.format = nsVDPixmap::kPixFormat_YUV420it_Planar;
					break;

				case nsVDPixmap::kPixFormat_YUV420i_Planar_FR:
					if (oddField)
						source.format = nsVDPixmap::kPixFormat_YUV420ib_Planar_FR;
					else
						source.format = nsVDPixmap::kPixFormat_YUV420it_Planar_FR;
					break;

				case nsVDPixmap::kPixFormat_YUV420i_Planar_709:
					if (oddField)
						source.format = nsVDPixmap::kPixFormat_YUV420ib_Planar_709;
					else
						source.format = nsVDPixmap::kPixFormat_YUV420it_Planar_709;
					break;

				case nsVDPixmap::kPixFormat_YUV420i_Planar_709_FR:
					if (oddField)
						source.format = nsVDPixmap::kPixFormat_YUV420ib_Planar_709_FR;
					else
						source.format = nsVDPixmap::kPixFormat_YUV420it_Planar_709_FR;
					break;
			}
		}

		VDPixmap dstbm = { dst, NULL, source.w, source.h, dstpitch, source.format };

		if (mbPaletted) {
			dstbm.format = nsVDPixmap::kPixFormat_Pal8;

			VDPROFILEBEGINEX3("V-BlitDisplay",source.info.frame_num==-1 ? 0:(uint32)source.info.frame_num,0,"dither");
			VDDitherImage(dstbm, source, mIdentTab);
			VDPROFILEEND();
		} else {
			if (mbConvertToScreenFormat)
				dstbm.format = mScreenFormat;

			mCachedBlitter.Update(dstbm, source);
			VDPROFILEBEGINEX3("V-BlitDisplay",source.info.frame_num==-1 ? 0:(uint32)source.info.frame_num,0,mCachedBlitter.profiler_comment.c_str());
			mCachedBlitter.Blit(dstbm, source);
			VDPROFILEEND();
		}

		if (mbDisplayDebugInfo) {
			int saveIndex = SaveDC(mhdc);
			if (saveIndex) {
				SetTextColor(mhdc, RGB(255, 255, 0));
				SetBkColor(mhdc, RGB(0, 0, 0));
				SetBkMode(mhdc, OPAQUE);
				SetTextAlign(mhdc, TA_BOTTOM | TA_LEFT);
				SelectObject(mhdc, GetStockObject(DEFAULT_GUI_FONT));

				VDStringA desc;
				GetFormatString(mSource, desc);
				VDStringA s;
				s.sprintf("GDI minidriver - %s", desc.c_str());

				TextOut(mhdc, 10, source.h - 10, s.data(), s.size());
				RestoreDC(mhdc, saveIndex);
			}
		}

		mbValid = true;
	}

	return true;
}

void VDVideoDisplayMinidriverGDI::Refresh(UpdateMode mode) {
	if (mbValid) {
		if (HDC hdc = GetDC(mhwnd)) {
			RECT r;

			GetClientRect(mhwnd, &r);
			InternalRefresh(hdc, r, mode);
			ReleaseDC(mhwnd, hdc);
		}
	}
}

bool VDVideoDisplayMinidriverGDI::Paint(HDC hdc, const RECT& rClient, UpdateMode mode) {
	if (mBorderRectCount) {
		SetBkColor(hdc, VDSwizzleU32(mBackgroundColor) >> 8);
		SetBkMode(hdc, OPAQUE);

		for(int i=0; i<mBorderRectCount; ++i) {
			const vdrect32& rFill = mBorderRects[i];
			RECT rFill2 = { rFill.left, rFill.top, rFill.right, rFill.bottom };
			ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rFill2, "", 0, NULL);
		}
	}

	InternalRefresh(hdc, rClient, mode);
	return true;
}

bool VDVideoDisplayMinidriverGDI::SetSubrect(const vdrect32 *r) {
	if (r) {
		mbUseSubrect = true;
		mSubrect = *r;
	} else
		mbUseSubrect = false;

	return true;
}

void VDVideoDisplayMinidriverGDI::InternalRefresh(HDC hdc, const RECT& rClient, UpdateMode mode) {
	if (rClient.right <= 0 || rClient.bottom <= 0)
		return;

	const VDPixmap& source = mSource.pixmap;
	RECT rDst;
	rDst.left = mDrawRect.left;
	rDst.top = mDrawRect.top;
	rDst.right = mDrawRect.right;
	rDst.bottom = mDrawRect.bottom;

	if (rDst.right <= rDst.left || rDst.bottom <= rDst.top)
		return;

	HDC hdcComp = hdc;
	RECT rDstComp = rDst;

	if (mpCompositor) {
		if (!mhdcCompBuffer || rClient.right != mCompBufferWidth || rClient.bottom != mCompBufferHeight) {
			ShutdownCompositionBuffer();
			InitCompositionBuffer();
		}

		if (mhdcCompBuffer) {
			hdcComp = mhdcCompBuffer;
			OffsetRect(&rDstComp, -rDst.left, -rDst.top);
		}
	}

	if (mhClipRgn) SelectClipRgn(hdc,mhClipRgn);

	SetStretchBltMode(hdcComp, COLORONCOLOR);
	vdrect32 r;
	if (mbUseSubrect)
		r = mSubrect;
	else
		r.set(0, 0, source.w, source.h);

	if (mColorOverride) {
		SetBkColor(hdcComp, VDSwizzleU32(mColorOverride) >> 8);
		SetBkMode(hdcComp, OPAQUE);
		ExtTextOut(hdcComp, 0, 0, ETO_OPAQUE, &rDstComp, "", 0, NULL);
	} else if (mSource.bInterlaced) {
		const int w = rDstComp.right - rDstComp.left;
		const int h = rDstComp.bottom - rDstComp.top;

		int fieldMode = mode & kModeFieldMask;
		uint32 vinc		= (r.height() << 16) / h;
		uint32 vaccum	= (vinc >> 1) + (r.top << 16);
		uint32 vtlimit	= (((r.height() + 1) >> 1) << 17) - 1;
		int fieldbase	= (fieldMode == kModeOddField ? 1 : 0);
		int ystep		= (fieldMode == kModeAllFields) ? 1 : 2;

		vaccum += vinc*fieldbase;
		vinc *= ystep;

		for(int y = fieldbase; y < h; y += ystep) {
			int v;

			if (y & 1) {
				uint32 vt = vaccum < 0x8000 ? 0 : vaccum - 0x8000;

				v = (y&1) + ((vt>>16) & ~1);
			} else {
				uint32 vt = vaccum + 0x8000;

				if (vt > vtlimit)
					vt = vtlimit;

				v = (vt>>16) & ~1;
			}

			StretchBlt(hdcComp, rDstComp.left, rDstComp.top + y, w, 1, mhdc, r.left, v, r.width(), 1, SRCCOPY);
			vaccum += vinc;
		}
	} else {
		StretchBlt(hdcComp, rDstComp.left, rDstComp.top, rDstComp.right - rDstComp.left, rDstComp.bottom - rDstComp.top, mhdc, r.left, r.top, r.width(), r.height(), SRCCOPY);
	}

	if (mpCompositor) {
		if (mRenderer.Begin(hdcComp)) {
			mpCompositor->Composite(mRenderer);
			mRenderer.End();
		}
	}

	if (hdcComp != hdc) {
		BitBlt(hdc, rDst.left, rDst.top, rDst.right - rDst.left, rDst.bottom - rDst.top, hdcComp, rDstComp.left, rDstComp.top, SRCCOPY);
	}

	if (mhClipRgn) SelectClipRgn(hdc,0);
}

int VDVideoDisplayMinidriverGDI::GetScreenIntermediatePixmapFormat(HDC hdc) {
	int pxformat = 0;

	// First, get the depth of the screen and guess that way.
	int depth = GetDeviceCaps(hdc, BITSPIXEL);

	if (depth < 24)
		pxformat = nsVDPixmap::kPixFormat_RGB565;
	else if (depth < 32)
		pxformat = nsVDPixmap::kPixFormat_RGB888;
	else
		pxformat = nsVDPixmap::kPixFormat_XRGB8888;

	// If the depth is 16-bit, attempt to determine the exact format.
	if (HBITMAP hbm = CreateCompatibleBitmap(hdc, 1, 1)) {
		struct {
			BITMAPV5HEADER hdr;
			RGBQUAD buf[256];
		} format={0};

		if (GetDIBits(hdc, hbm, 0, 1, NULL, (LPBITMAPINFO)&format, DIB_RGB_COLORS)
			&& GetDIBits(hdc, hbm, 0, 1, NULL, (LPBITMAPINFO)&format, DIB_RGB_COLORS))
		{
			if (format.hdr.bV5Size >= sizeof(BITMAPINFOHEADER)) {
				const BITMAPV5HEADER& hdr = format.hdr;

				if (hdr.bV5Planes == 1) {
					if (hdr.bV5Compression == BI_BITFIELDS) {
						if (hdr.bV5BitCount == 16 && hdr.bV5RedMask == 0x7c00 && hdr.bV5GreenMask == 0x03e0 && hdr.bV5BlueMask == 0x7c00)
							pxformat = nsVDPixmap::kPixFormat_XRGB1555;
						else if (hdr.bV5BitCount == 16 && hdr.bV5RedMask == 0xf800 && hdr.bV5GreenMask == 0x07e0 && hdr.bV5BlueMask == 0x7c00)
							pxformat = nsVDPixmap::kPixFormat_RGB565;
						else if (hdr.bV5BitCount == 24 && hdr.bV5RedMask == 0xff0000 && hdr.bV5GreenMask == 0x00ff00 && hdr.bV5BlueMask == 0x0000ff)
							pxformat = nsVDPixmap::kPixFormat_RGB888;
						else if (hdr.bV5BitCount == 32 && hdr.bV5RedMask == 0x00ff0000 && hdr.bV5GreenMask == 0x0000ff00 && hdr.bV5BlueMask == 0x000000ff)
							pxformat = nsVDPixmap::kPixFormat_XRGB8888;
					} else if (hdr.bV5Compression == BI_RGB) {
						if (hdr.bV5BitCount == 16)
							pxformat = nsVDPixmap::kPixFormat_XRGB1555;
						else if (hdr.bV5BitCount == 24)
							pxformat = nsVDPixmap::kPixFormat_RGB888;
						else if (hdr.bV5BitCount == 32)
							pxformat = nsVDPixmap::kPixFormat_XRGB8888;
					}
				}
			}
		}

		DeleteObject(hbm);
	}

	return pxformat;
}

void VDVideoDisplayMinidriverGDI::InitCompositionBuffer() {
	const uint32 w = mClientRect.right;
	const uint32 h = mClientRect.bottom;

	if (!w || !h)
		return;

	HDC hdc = GetDC(NULL);
	if (!hdc)
		return;

	mhdcCompBuffer = CreateCompatibleDC(hdc);
	mhbmCompBuffer = CreateCompatibleBitmap(hdc, w, h);

	ReleaseDC(NULL, hdc);

	if (!mhdcCompBuffer || !mhbmCompBuffer) {
		ShutdownCompositionBuffer();
		return;
	}

	mhbmCompBufferOld = SelectObject(mhdcCompBuffer, mhbmCompBuffer);
	if (!mhbmCompBufferOld) {
		ShutdownCompositionBuffer();
		return;
	}

	mCompBufferWidth = w;
	mCompBufferHeight = h;
}

void VDVideoDisplayMinidriverGDI::ShutdownCompositionBuffer() {
	if (mhbmCompBufferOld) {
		SelectObject(mhdcCompBuffer, mhbmCompBufferOld);
		mhbmCompBufferOld = NULL;
	}

	if (mhbmCompBuffer) {
		DeleteObject(mhbmCompBuffer);
		mhbmCompBuffer = NULL;
	}

	if (mhdcCompBuffer) {
		DeleteDC(mhdcCompBuffer);
		mhdcCompBuffer = NULL;
	}

	mCompBufferWidth = 0;
	mCompBufferHeight = 0;
}

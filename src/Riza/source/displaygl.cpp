#include <windows.h>
#include <vd2/system/bitmath.h>
#include <vd2/system/profile.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/math.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/opengl.h>
#include <vd2/VDDisplay/compositor.h>
#include <vd2/VDDisplay/displaydrv.h>
#include <vd2/VDDisplay/renderer.h>

#define VDDEBUG_DISP (void)sizeof printf
//#define VDDEBUG_DISP VDDEBUG

///////////////////////////////////////////////////////////////////////////

struct VDDisplayRendererBaseOpenGL {
	vdfastvector<GLuint> mZombieTextures;
};

class VDDisplayCachedImageOpenGL : public vdrefcounted<IVDRefUnknown>, public vdlist_node {
	VDDisplayCachedImageOpenGL(const VDDisplayCachedImageOpenGL&);
	VDDisplayCachedImageOpenGL& operator=(const VDDisplayCachedImageOpenGL&);
public:
	enum { kTypeID = 'cimG' };

	VDDisplayCachedImageOpenGL();
	~VDDisplayCachedImageOpenGL();

	void *AsInterface(uint32 iid);

	bool Init(VDOpenGLBinding *pgl, VDDisplayRendererBaseOpenGL *owner, const VDDisplayImageView& imageView);
	void Shutdown();

	void Update(const VDDisplayImageView& imageView);

public:
	VDDisplayRendererBaseOpenGL *mpOwner;
	VDOpenGLBinding *mpGL;
	GLuint	mTexture;
	sint32	mWidth;
	sint32	mHeight;
	sint32	mTexWidth;
	sint32	mTexHeight;
	uint32	mUniquenessCounter;
};

VDDisplayCachedImageOpenGL::VDDisplayCachedImageOpenGL()
	: mTexture(0)
{
	mListNodePrev = NULL;
	mListNodeNext = NULL;
}

VDDisplayCachedImageOpenGL::~VDDisplayCachedImageOpenGL() {
	if (mListNodePrev)
		vdlist_base::unlink(*this);
}

void *VDDisplayCachedImageOpenGL::AsInterface(uint32 iid) {
	if (iid == kTypeID)
		return this;

	return NULL;
}

bool VDDisplayCachedImageOpenGL::Init(VDOpenGLBinding *pgl, VDDisplayRendererBaseOpenGL *owner, const VDDisplayImageView& imageView) {
	mpGL = pgl;

	const VDPixmap& px = imageView.GetImage();
	int w = VDCeilToPow2(px.w);
	int h = VDCeilToPow2(px.h);

	pgl->glGenTextures(1, &mTexture);
	pgl->glBindTexture(GL_TEXTURE_2D, mTexture);
	pgl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
	pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_EXT);
	pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_EXT);
	pgl->glBindTexture(GL_TEXTURE_2D, 0);
	VDASSERT(mpGL->glGetError() == GL_NO_ERROR);

	mWidth = px.w;
	mHeight = px.h;
	mTexWidth = w;
	mTexHeight = h;
	mpOwner = owner;

	Update(imageView);
	return true;
}

void VDDisplayCachedImageOpenGL::Shutdown() {
	if (mpOwner) {
		mpOwner->mZombieTextures.push_back(mTexture);
		mpOwner = NULL;
		mTexture = 0;
	}
}

void VDDisplayCachedImageOpenGL::Update(const VDDisplayImageView& imageView) {
	mUniquenessCounter = imageView.GetUniquenessCounter();

	if (mTexture) {
		const VDPixmap& px = imageView.GetImage();

		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, mWidth, mHeight, 4);
		VDPixmapLayoutFlipV(layout);

		VDPixmapBuffer buf;
		buf.init(layout);

		VDPixmapBlt(buf, px);

		mpGL->glBindTexture(GL_TEXTURE_2D, mTexture);
		mpGL->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		mpGL->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		mpGL->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buf.base());
		mpGL->glBindTexture(GL_TEXTURE_2D, 0);
		VDASSERT(mpGL->glGetError() == GL_NO_ERROR);
	}
}

///////////////////////////////////////////////////////////////////////////

class VDDisplayRendererOpenGL : public IVDDisplayRenderer, public VDDisplayRendererBaseOpenGL {
public:
	VDDisplayRendererOpenGL();

	void Init(VDOpenGLBinding *glbinding);
	void Shutdown();

	void Begin(sint32 w, sint32 h);
	void End();

	void SetColorRGB(uint32 color);
	void FillRect(sint32 x, sint32 y, sint32 w, sint32 h);
	void Blt(sint32 x, sint32 y, VDDisplayImageView& imageView);

protected:
	VDDisplayCachedImageOpenGL *GetCachedImage(VDDisplayImageView& imageView);
	void DeleteZombieTextures();

	VDOpenGLBinding *mpGL;
	float	mColorRed;
	float	mColorGreen;
	float	mColorBlue;

	vdlist<VDDisplayCachedImageOpenGL> mCachedImages;
};

VDDisplayRendererOpenGL::VDDisplayRendererOpenGL()
	: mpGL(NULL)
{
}

void VDDisplayRendererOpenGL::Init(VDOpenGLBinding *glbinding) {
	mpGL = glbinding;
}

void VDDisplayRendererOpenGL::Shutdown() {
	while(!mCachedImages.empty()) {
		VDDisplayCachedImageOpenGL *img = mCachedImages.front();
		mCachedImages.pop_front();

		img->mListNodePrev = NULL;
		img->mListNodeNext = NULL;

		img->Shutdown();
	}

	DeleteZombieTextures();

	mpGL = NULL;
}

void VDDisplayRendererOpenGL::Begin(sint32 w, sint32 h) {
	DeleteZombieTextures();

	VDASSERT(mpGL->glGetError() == GL_NO_ERROR);

	mpGL->glDisable(GL_BLEND);
	mpGL->glDisable(GL_CULL_FACE);
	mpGL->glDisable(GL_ALPHA_TEST);
	mpGL->glDisable(GL_DEPTH_TEST);
	mpGL->glDisable(GL_STENCIL_TEST);
	mpGL->glDisable(GL_LIGHTING);
	mpGL->glDisable(GL_TEXTURE_2D);
	VDASSERT(mpGL->glGetError() == GL_NO_ERROR);

	mpGL->glMatrixMode(GL_MODELVIEW);
	mpGL->glLoadIdentity();
	mpGL->glMatrixMode(GL_PROJECTION);
	mpGL->glLoadIdentity();
	mpGL->glOrtho(0, w, h, 0, 0, 1);
	VDASSERT(mpGL->glGetError() == GL_NO_ERROR);

	mpGL->glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	VDASSERT(mpGL->glGetError() == GL_NO_ERROR);

	mColorRed = 0;
	mColorGreen = 0;
	mColorBlue = 0;
}

void VDDisplayRendererOpenGL::End() {
	DeleteZombieTextures();
}

void VDDisplayRendererOpenGL::SetColorRGB(uint32 color) {
	mColorRed = ((color >> 16) & 0xff) / 255.0f;
	mColorGreen = ((color >> 8) & 0xff) / 255.0f;
	mColorBlue = ((color >> 0) & 0xff) / 255.0f;
}

void VDDisplayRendererOpenGL::FillRect(sint32 x, sint32 y, sint32 w, sint32 h) {
	mpGL->glDisable(GL_TEXTURE_2D);

	mpGL->glColor4f(mColorRed, mColorGreen, mColorBlue, 1.0f);
	mpGL->glBegin(GL_TRIANGLE_STRIP);
	mpGL->glVertex2i(x, y);
	mpGL->glVertex2i(x, y + h);
	mpGL->glVertex2i(x + w, y);
	mpGL->glVertex2i(x + w, y + h);
	mpGL->glEnd();
}

void VDDisplayRendererOpenGL::Blt(sint32 x, sint32 y, VDDisplayImageView& imageView) {
	VDDisplayCachedImageOpenGL *cachedImage = GetCachedImage(imageView);

	if (!cachedImage)
		return;

	const float u0 = 0;
	const float v0 = 0;
	const float u1 = (float)cachedImage->mWidth / (float)cachedImage->mTexWidth;
	const float v1 = (float)cachedImage->mHeight / (float)cachedImage->mTexHeight;

	mpGL->glEnable(GL_TEXTURE_2D);
	mpGL->glBindTexture(GL_TEXTURE_2D, cachedImage->mTexture);

	mpGL->glColor4f(1, 1, 1, 1);
	mpGL->glBegin(GL_TRIANGLE_STRIP);
	mpGL->glTexCoord2f(u0, v0);
	mpGL->glVertex2i(x, y);
	mpGL->glTexCoord2f(u0, v1);
	mpGL->glVertex2i(x, y + cachedImage->mHeight);
	mpGL->glTexCoord2f(u1, v0);
	mpGL->glVertex2i(x + cachedImage->mWidth, y);
	mpGL->glTexCoord2f(u1, v1);
	mpGL->glVertex2i(x + cachedImage->mWidth, y + cachedImage->mHeight);
	mpGL->glEnd();

	mpGL->glBindTexture(GL_TEXTURE_2D, 0);
}

VDDisplayCachedImageOpenGL *VDDisplayRendererOpenGL::GetCachedImage(VDDisplayImageView& imageView) {
	VDDisplayCachedImageOpenGL *cachedImage = vdpoly_cast<VDDisplayCachedImageOpenGL *>(imageView.GetCachedImage());

	if (cachedImage && cachedImage->mpOwner != this)
		cachedImage = NULL;

	if (!cachedImage) {
		DeleteZombieTextures();

		cachedImage = new_nothrow VDDisplayCachedImageOpenGL;

		if (!cachedImage)
			return NULL;
		
		cachedImage->AddRef();
		if (!cachedImage->Init(mpGL, this, imageView)) {
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

void VDDisplayRendererOpenGL::DeleteZombieTextures() {
	if (!mZombieTextures.empty()) {
		mpGL->glDeleteTextures(mZombieTextures.size(), mZombieTextures.data());
		mZombieTextures.clear();
	}
}

///////////////////////////////////////////////////////////////////////////

namespace {
	const char kFPCubic1[]=
		"!!ARBfp1.0\n"
#if 1
		"TEMP pix0;\n"
		"TEMP pix1;\n"
		"TEMP pix2;\n"
		"TEMP filt;\n"
		"TEMP tcen;\n"
		"TEMP r0;\n"
		"PARAM uvscale = program.local[0];\n"
		"PARAM scale = {-0.1875, 0.375, 0, 0};\n"
		"TEX filt, fragment.texcoord[3], texture[3], 2D;\n"
		"MAD tcen, filt.g, uvscale, fragment.texcoord[1];\n"
		"TEX pix0, fragment.texcoord[0], texture[0], 2D;\n"
		"TEX pix1, tcen, texture[1], 2D;\n"
		"TEX pix2, fragment.texcoord[2], texture[2], 2D;\n"

		// (pix0+pix2)*filt.b*0.75/4 + pix1*(filt.b*0.75/2 + 1)
		"MUL r0, pix0, scale.r;\n"
		"MAD r0, pix2, scale.r, r0;\n"
		"MAD r0, pix1, scale.g, r0;\n"
		"MAD result.color.rgb, r0, filt.r, pix1;\n"
		"MOV result.color.a, pix1.a;\n"
#else
		"PARAM one = {1,1,1,1};\n"
		"TEMP foo;\n"
		"TEX foo, fragment.texcoord[3], texture[3], 1D;\n"
		"ADD result.color, one, -foo;\n"
#endif
		"END\n";
}

///////////////////////////////////////////////////////////////////////////

class VDVideoTextureTilePatternOpenGL {
public:
	struct TileInfo {
		float	mInvU;
		float	mInvV;
		int		mSrcW;
		int		mSrcH;
	};

	VDVideoTextureTilePatternOpenGL() : mTexture(0), mbPhase(false) {}
	void Init(VDOpenGLBinding *pgl, int w, int h, bool bPackedPixelsSupported, bool bEdgeClampSupported);
	void Shutdown(VDOpenGLBinding *pgl);

	void ReinitFiltering(VDOpenGLBinding *pgl, IVDVideoDisplayMinidriver::FilterMode mode);

	bool IsInited() const { return mTexture != 0; }

	void Flip();
	GLuint GetTexture() const { return mTexture; }
	const TileInfo& GetTileInfo() const { return mTileInfo; }

protected:
	int			mTextureTilesW;
	int			mTextureTilesH;
	int			mTextureSize;
	double		mTextureSizeInv;
	int			mTextureLastW;
	int			mTextureLastH;
	double		mTextureLastWInvPow2;
	double		mTextureLastHInvPow2;
	GLuint		mTexture;
	TileInfo	mTileInfo;

	bool		mbPackedPixelsSupported;
	bool		mbEdgeClampSupported;
	bool		mbPhase;
};

void VDVideoTextureTilePatternOpenGL::Init(VDOpenGLBinding *pgl, int w, int h, bool bPackedPixelsSupported, bool bEdgeClampSupported) {
	mbPackedPixelsSupported		= bPackedPixelsSupported;
	mbEdgeClampSupported		= bEdgeClampSupported;

	GLint maxsize;
	pgl->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize);

	mTextureSize	= maxsize;
	mTextureSizeInv	= 1.0 / maxsize;
	mTextureTilesW	= 1;
	mTextureTilesH	= 1;

	int ntiles = 1;
	int xlast = w;
	int ylast = h;
	int xlasttex = 1;
	int ylasttex = 1;

	while(xlasttex < xlast)
		xlasttex += xlasttex;
	while(ylasttex < ylast)
		ylasttex += ylasttex;

	int largestW = xlasttex;
	int largestH = ylasttex;

	mTextureLastW = xlast;
	mTextureLastH = ylast;
	mTextureLastWInvPow2	= 1.0 / xlasttex;
	mTextureLastHInvPow2	= 1.0 / ylasttex;

	pgl->glGenTextures(1, &mTexture);

	vdautoblockptr zerobuffer(malloc(4 * largestW * largestH));
	memset(zerobuffer, 0, 4 * largestW * largestH);

	pgl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	pgl->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);


	int tile = 0;
	int y = 0;
	int x = 0;
	int texw = xlasttex;
	int texh = ylasttex;

	pgl->glBindTexture(GL_TEXTURE_2D, mTexture);

	if (mbEdgeClampSupported) {
		pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_EXT);
		pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_EXT);
	} else {
		pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		static const float black[4]={0.f,0.f,0.f,0.f};
		pgl->glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, black);
	}

	pgl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texw, texh, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);

	mTileInfo.mInvU		= 1.0f / texw;
	mTileInfo.mInvV		= 1.0f / texh;
	mTileInfo.mSrcW		= xlast;
	mTileInfo.mSrcH		= ylast;

	Flip();
}

void VDVideoTextureTilePatternOpenGL::Shutdown(VDOpenGLBinding *pgl) {
	if (mTexture) {
		pgl->glDeleteTextures(1, &mTexture);
		mTexture = 0;
	}
}

void VDVideoTextureTilePatternOpenGL::ReinitFiltering(VDOpenGLBinding *pgl, IVDVideoDisplayMinidriver::FilterMode mode) {
	pgl->glBindTexture(GL_TEXTURE_2D, mTexture);

	if (mode == IVDVideoDisplayMinidriver::kFilterPoint)
		pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	else
		pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	if (mode == IVDVideoDisplayMinidriver::kFilterPoint)
		pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	else
		pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void VDVideoTextureTilePatternOpenGL::Flip() {
	mbPhase = !mbPhase;
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayMinidriverOpenGL : public VDVideoDisplayMinidriver {
public:
	VDVideoDisplayMinidriverOpenGL();
	~VDVideoDisplayMinidriverOpenGL();

	bool Init(HWND hwnd, HMONITOR hmonitor, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid() { return mbValid; }
	void SetFilterMode(FilterMode mode);

	bool Resize(int w, int h);
	bool Update(UpdateMode);
	void Refresh(UpdateMode);
	bool Paint(HDC hdc, const RECT& rClient, UpdateMode mode) { return true; }

protected:
	void Upload(const VDPixmap& source, VDVideoTextureTilePatternOpenGL& texPattern);

	static ATOM Register();
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	bool OnOpenGLInit();
	void OnDestroy();
	void OnPaint();
	bool InitBicubic();
	void ShutdownBicubic();
	void UpdateCubicTextures(uint32 w, uint32 h);
	void UpdateCubicTexture(uint32 dw, uint32 sw);

	enum {
		kTimerId_Refresh = 100
	};
	
	HWND		mhwnd;
	HWND		mhwndOGL;
	bool		mbValid;
	bool		mbVsync;
	bool		mbFirstPresent;
	bool		mbVerticalFlip;
	UpdateMode	mRefreshMode;
	int			mRefreshIdleCount;
	bool		mbRefreshIdleTimerActive;
	bool		mbRefreshQueued;
	bool		mbCubicPossible;

	GLuint		mFPCubic;
	GLuint		mCubicFramebuffer;
	GLuint		mCubicFilterTempTex;
	GLuint		mCubicFilterTempTexWidth;
	GLuint		mCubicFilterTempTexHeight;
	GLuint		mCubicFilterH;
	uint32		mCubicFilterHSize;
	uint32		mCubicFilterHTexSize;
	GLuint		mCubicFilterV;
	uint32		mCubicFilterVSize;
	uint32		mCubicFilterVTexSize;

	GLuint		mFontBase;

	FilterMode	mPreferredFilter;
	VDVideoTextureTilePatternOpenGL		mTexPattern[2];
	VDVideoDisplaySourceInfo			mSource;

	VDDisplayRendererOpenGL	mRenderer;

	VDRTProfileChannel	mProfChan;
	VDOpenGLBinding	mGL;

	VDPixmapBuffer	mConversionBuffer;
};

#define MYWM_OGLINIT		(WM_USER + 0x180)

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverOpenGL() {
	return new VDVideoDisplayMinidriverOpenGL;
}

VDVideoDisplayMinidriverOpenGL::VDVideoDisplayMinidriverOpenGL()
	: mhwndOGL(0)
	, mbValid(false)
	, mbVsync(false)
	, mbFirstPresent(false)
	, mbVerticalFlip(false)
	, mRefreshMode(kModeNone)
	, mRefreshIdleCount(0)
	, mbRefreshIdleTimerActive(false)
	, mbRefreshQueued(false)
	, mFPCubic(0)
	, mCubicFramebuffer(0)
	, mCubicFilterTempTex(0)
	, mCubicFilterTempTexWidth(0)
	, mCubicFilterTempTexHeight(0)
	, mCubicFilterH(0)
	, mCubicFilterHSize(0)
	, mCubicFilterHTexSize(0)
	, mCubicFilterV(0)
	, mCubicFilterVTexSize(0)
	, mFontBase(0)
	, mPreferredFilter(kFilterAnySuitable)
	, mProfChan("GLDisplay")
{
	memset(&mSource, 0, sizeof mSource);
}

VDVideoDisplayMinidriverOpenGL::~VDVideoDisplayMinidriverOpenGL() {
}

bool VDVideoDisplayMinidriverOpenGL::Init(HWND hwnd, HMONITOR hmonitor, const VDVideoDisplaySourceInfo& info) {
	mSource = info;
	mhwnd = hwnd;

	// Format check....
	switch(info.pixmap.format) {
		case nsVDPixmap::kPixFormat_XRGB1555:
		case nsVDPixmap::kPixFormat_RGB565:
		case nsVDPixmap::kPixFormat_RGB888:
		case nsVDPixmap::kPixFormat_XRGB8888:
			break;

		default:
			if (!info.bAllowConversion)
				return false;

			mConversionBuffer.init(info.pixmap.w, info.pixmap.h, nsVDPixmap::kPixFormat_XRGB8888);
			break;
	}

	// OpenGL doesn't allow upside-down texture uploads, so we simply
	// upload the surface inverted and then reinvert on display.
	mbVerticalFlip = false;
	if (mSource.pixmap.pitch < 0) {
		mSource.pixmap.data = (char *)mSource.pixmap.data + mSource.pixmap.pitch*(mSource.pixmap.h - 1);
		mSource.pixmap.pitch = -mSource.pixmap.pitch;
		mbVerticalFlip = true;
	}

	RECT r;
	GetClientRect(mhwnd, &r);

	static ATOM wndClass = Register();

	if (!mGL.Init())
		return false;

	// We have to create a separate window because the NVIDIA driver subclasses the
	// window and doesn't unsubclass it even after the OpenGL context is deleted.
	// If we use the main window instead then the app will bomb the moment we unload
	// OpenGL.

	mhwndOGL = CreateWindowEx(WS_EX_TRANSPARENT, (LPCSTR)wndClass, "", WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, r.right, r.bottom, mhwnd, NULL, VDGetLocalModuleHandleW32(), this);
	if (!mhwndOGL)
		return false;

	if (!SendMessage(mhwndOGL, MYWM_OGLINIT, 0, 0)) {
		DestroyWindow(mhwndOGL);
		mhwndOGL = 0;
		return false;
	}

	mbValid = false;
	mbFirstPresent = true;
	return true;
}

void VDVideoDisplayMinidriverOpenGL::Shutdown() {
	ShutdownBicubic();

	mRenderer.Shutdown();

	if (mhwndOGL) {
		DestroyWindow(mhwndOGL);
		mhwndOGL = NULL;
	}

	mGL.Shutdown();
	mbValid = false;

	mRefreshIdleCount = 0;
	mbRefreshIdleTimerActive = false;
	mbRefreshQueued = false;
}

bool VDVideoDisplayMinidriverOpenGL::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (!mGL.IsInited())
		return false;

	if (info.pixmap.w == mSource.pixmap.w && info.pixmap.h == mSource.pixmap.h && info.pixmap.format == mSource.pixmap.format && info.bInterlaced == mSource.bInterlaced) {
		mSource = info;
		// OpenGL doesn't allow upside-down texture uploads, so we simply
		// upload the surface inverted and then reinvert on display.
		mbVerticalFlip = false;
		if (mSource.pixmap.pitch < 0) {
			mSource.pixmap.data = (char *)mSource.pixmap.data + mSource.pixmap.pitch*(mSource.pixmap.h - 1);
			mSource.pixmap.pitch = -mSource.pixmap.pitch;
			mbVerticalFlip = true;
		}
		return true;
	}

	return false;
}

void VDVideoDisplayMinidriverOpenGL::SetFilterMode(FilterMode mode) {
	if (mPreferredFilter == mode)
		return;

	mPreferredFilter = mode;

	if (mhwndOGL) {
		if (HDC hdc = GetDC(mhwndOGL)) {
			if (mGL.Begin(hdc)) {
				mTexPattern[0].ReinitFiltering(&mGL, mode);
				mTexPattern[1].ReinitFiltering(&mGL, mode);
				mGL.wglMakeCurrent(NULL, NULL);
			}
		}
	}
}

bool VDVideoDisplayMinidriverOpenGL::Resize(int w, int h) {
	if (mhwndOGL)
		SetWindowPos(mhwndOGL, 0, 0, 0, w, h, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);

	return true;
}

bool VDVideoDisplayMinidriverOpenGL::Update(UpdateMode mode) {
	if (!mGL.IsInited())
		return false;

	if (!mSource.pixmap.data)
		return false;

	if (HDC hdc = GetDC(mhwndOGL)) {
		if (mGL.Begin(hdc)) {
			VDASSERT(mGL.glGetError() == GL_NO_ERROR);

			if (mSource.bInterlaced) {
				uint32 fieldmode = (mode & kModeFieldMask);

				if (fieldmode == kModeAllFields || fieldmode == kModeEvenField) {
					VDPixmap evenFieldSrc(mSource.pixmap);

					evenFieldSrc.h = (evenFieldSrc.h+1) >> 1;
					evenFieldSrc.pitch += evenFieldSrc.pitch;

					Upload(evenFieldSrc, mTexPattern[0]);
				}
				if (fieldmode == kModeAllFields || fieldmode == kModeOddField) {
					VDPixmap oddFieldSrc(mSource.pixmap);

					oddFieldSrc.data = (char *)oddFieldSrc.data + oddFieldSrc.pitch;
					oddFieldSrc.h = (oddFieldSrc.h+1) >> 1;
					oddFieldSrc.pitch += oddFieldSrc.pitch;

					Upload(oddFieldSrc, mTexPattern[1]);
				}
			} else {
				Upload(mSource.pixmap, mTexPattern[0]);
			}

			VDASSERT(mGL.glGetError() == GL_NO_ERROR);

			mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			mGL.End();
		}

		mbValid = true;

		ReleaseDC(mhwndOGL, hdc);
	}

	return true;
}

void VDVideoDisplayMinidriverOpenGL::Refresh(UpdateMode updateMode) {
	if (mbValid) {
		InvalidateRect(mhwndOGL, NULL, FALSE);
		mRefreshMode = updateMode;
		mbRefreshQueued = true;
		mRefreshIdleCount = 0;
		if (!mbRefreshIdleTimerActive) {
			mbRefreshIdleTimerActive = true;

			VDVERIFY(SetTimer(mhwndOGL, kTimerId_Refresh, 1000, NULL));
		}

		PostMessage(mhwndOGL, WM_TIMER, kTimerId_Refresh, 0);
	}
}

void VDVideoDisplayMinidriverOpenGL::Upload(const VDPixmap& source, VDVideoTextureTilePatternOpenGL& texPattern) {
	mProfChan.Begin(0xe0e0e0, "Upload");

	mGL.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	switch(source.format) {
	case nsVDPixmap::kPixFormat_XRGB1555:
	case nsVDPixmap::kPixFormat_RGB565:
		mGL.glPixelStorei(GL_UNPACK_ROW_LENGTH, source.pitch >> 1);
		break;
	case nsVDPixmap::kPixFormat_RGB888:
		mGL.glPixelStorei(GL_UNPACK_ROW_LENGTH, source.pitch / 3);
		break;
	case nsVDPixmap::kPixFormat_XRGB8888:
		mGL.glPixelStorei(GL_UNPACK_ROW_LENGTH, source.pitch >> 2);
		break;
	}

	texPattern.Flip();

	const VDVideoTextureTilePatternOpenGL::TileInfo& tile = texPattern.GetTileInfo();

	mGL.glBindTexture(GL_TEXTURE_2D, texPattern.GetTexture());

	switch(source.format) {
	case nsVDPixmap::kPixFormat_XRGB1555:
		mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGRA_EXT, GL_UNSIGNED_SHORT_1_5_5_5_REV, (const char *)source.data);
		break;
	case nsVDPixmap::kPixFormat_RGB565:
		mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGR_EXT, GL_UNSIGNED_SHORT_5_6_5_REV, (const char *)source.data);
		break;
	case nsVDPixmap::kPixFormat_RGB888:
		mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGR_EXT, GL_UNSIGNED_BYTE, (const char *)source.data);
		break;
	case nsVDPixmap::kPixFormat_XRGB8888:
		mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const char *)source.data);
		break;
	default:
		VDPixmapBlt(mConversionBuffer, source);
		mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const char *)mConversionBuffer.data);
		break;
	}

	mProfChan.End();
}

///////////////////////////////////////////////////////////////////////////

ATOM VDVideoDisplayMinidriverOpenGL::Register() {
	WNDCLASS wc;

	wc.style			= CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= StaticWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoDisplayMinidriverOpenGL *);
	wc.hInstance		= VDGetLocalModuleHandleW32();
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= "phaeronOpenGLVideoDisplay";

	return RegisterClass(&wc);
}

LRESULT CALLBACK VDVideoDisplayMinidriverOpenGL::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVideoDisplayMinidriverOpenGL *pThis = (VDVideoDisplayMinidriverOpenGL *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		pThis = (VDVideoDisplayMinidriverOpenGL *)((LPCREATESTRUCT)lParam)->lpCreateParams;
		SetWindowLongPtr(hwnd, 0, (DWORD_PTR)pThis);
		pThis->mhwndOGL = hwnd;
		break;
	}

	return pThis ? pThis->WndProc(msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDVideoDisplayMinidriverOpenGL::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case MYWM_OGLINIT:
		return OnOpenGLInit();
	case WM_DESTROY:
		OnDestroy();
		break;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	case WM_TIMER:
		if (wParam == kTimerId_Refresh) {
			if (mbRefreshQueued) {
				mRefreshIdleCount = 0;
				mbRefreshQueued = false;
				UpdateWindow(mhwndOGL);
			} else if (++mRefreshIdleCount >= 5) {
				mRefreshIdleCount = 0;
				mbRefreshIdleTimerActive = false;
				VDVERIFY(KillTimer(mhwndOGL, kTimerId_Refresh));
			}
		}
		break;
	}

	return DefWindowProc(mhwndOGL, msg, wParam, lParam);
}

bool VDVideoDisplayMinidriverOpenGL::OnOpenGLInit() {
	if (HDC hdc = GetDC(mhwndOGL)) {
		if (mGL.Attach(hdc, 8, 0, 0, 0, true)) {
			if (mGL.Begin(hdc)) {
				VDDEBUG_DISP("VideoDisplay: OpenGL version string: [%s]\n", mGL.glGetString(GL_VERSION));

				const GLubyte *pExtensions = mGL.glGetString(GL_EXTENSIONS);

				vdfastvector<char> extstr(strlen((const char *)pExtensions)+1);
				std::copy(pExtensions, pExtensions + extstr.size(), extstr.data());

				char *s = extstr.data();

				bool bPackedPixelsSupported = false;
				bool bEdgeClampSupported = false;

				while(const char *tok = strtok(s, " ")) {
					if (!strcmp(tok, "GL_EXT_packed_pixels"))
						bPackedPixelsSupported = true;
					else if (!strcmp(tok, "GL_EXT_texture_edge_clamp"))
						bEdgeClampSupported = true;
					s = NULL;
				}

				if (mSource.bInterlaced) {
					mTexPattern[0].Init(&mGL, mSource.pixmap.w, (mSource.pixmap.h+1)>>1, bPackedPixelsSupported, bEdgeClampSupported);
					mTexPattern[1].Init(&mGL, mSource.pixmap.w, mSource.pixmap.h>>1, bPackedPixelsSupported, bEdgeClampSupported);
					mTexPattern[1].ReinitFiltering(&mGL, mPreferredFilter);
				} else
					mTexPattern[0].Init(&mGL, mSource.pixmap.w, mSource.pixmap.h, bPackedPixelsSupported, bEdgeClampSupported);
				mTexPattern[0].ReinitFiltering(&mGL, mPreferredFilter);

				VDASSERT(mGL.glGetError() == GL_NO_ERROR);

				mbCubicPossible = InitBicubic();
				if (!mbCubicPossible)
					ShutdownBicubic();

				mbVsync = false;
				if (mGL.EXT_swap_control)
					mGL.wglSwapIntervalEXT(0);

				mFontBase = mGL.glGenLists(96);

				SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
				mGL.wglUseFontBitmapsA(hdc, 32, 96, mFontBase);

				VDASSERT(mGL.glGetError() == GL_NO_ERROR);

				mGL.End();
				ReleaseDC(mhwndOGL, hdc);

				mRenderer.Init(&mGL);

				VDDEBUG_DISP("VideoDisplay: Using OpenGL for %dx%d display.\n", mSource.pixmap.w, mSource.pixmap.h);
				return true;
			}
			mGL.Detach();
		}

		ReleaseDC(mhwndOGL, hdc);
	}

	return false;
}

void VDVideoDisplayMinidriverOpenGL::OnDestroy() {
	if (mGL.IsInited()) {
		if (HDC hdc = GetDC(mhwndOGL)) {
			if (mGL.Begin(hdc)) {
				mTexPattern[0].Shutdown(&mGL);
				mTexPattern[1].Shutdown(&mGL);

				if (mFontBase) {
					mGL.glDeleteLists(96, mFontBase);
					mFontBase = 0;
				}

				if (mGL.ARB_fragment_program) {
					if (mFPCubic) {
						mGL.glDeleteProgramsARB(1, &mFPCubic);
						mFPCubic = 0;
					}
				}

				mGL.End();
			}
		}

		mGL.Detach();
	}
}

void VDVideoDisplayMinidriverOpenGL::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwndOGL, &ps);

	if (!hdc)
		return;

	float bobOffset;

	if (mRefreshMode & kModeBobEven)
		bobOffset = +1.0f;
	else if (mRefreshMode & kModeBobOdd)
		bobOffset = -1.0f;
	else
		bobOffset = 0.0f;

	RECT r;
	GetClientRect(mhwndOGL, &r);

	const int vph = r.bottom;

	FilterMode mode = mPreferredFilter;

	if (mode == kFilterAnySuitable)
		mode = kFilterBicubic;

	if (mode == kFilterBicubic && !mbCubicPossible)
		mode = kFilterBilinear;

	if (mGL.Begin(hdc)) {
		bool vsync = (mRefreshMode & kModeVSync) != 0;
		if (mbVsync != vsync) {
			mbVsync = vsync;

			if (mGL.EXT_swap_control)
				mGL.wglSwapIntervalEXT(vsync ? 1 : 0);
		}

		if (mode == kFilterBicubic)
			UpdateCubicTextures(r.right, r.bottom);

		mGL.glViewport(0, 0, r.right, r.bottom);
		mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		if (mbDestRectEnabled) {
			mGL.glClearColor(
				(float)(mBackgroundColor & 0x00ff0000) / (float)0x00ff0000,
				(float)(mBackgroundColor & 0x0000ff00) / (float)0x0000ff00,
				(float)(mBackgroundColor & 0x000000ff) / (float)0x000000ff,
				0.0f);

			mGL.glClear(GL_COLOR_BUFFER_BIT);

			if (r.left < mDestRect.left)
				r.left = mDestRect.left;

			if (r.top < mDestRect.top)
				r.top = mDestRect.top;

			if (r.right > mDestRect.right)
				r.right = mDestRect.right;

			if (r.bottom > mDestRect.bottom)
				r.bottom = mDestRect.bottom;

			if (r.right < r.left)
				r.right = r.left;

			if (r.bottom < r.top)
				r.bottom = r.top;

			mGL.glViewport(r.left, vph - r.bottom, r.right - r.left, r.bottom - r.top);
		}

		if (mColorOverride) {
			mGL.glClearColor(
				(float)(mColorOverride & 0x00ff0000) / (float)0x00ff0000,
				(float)(mColorOverride & 0x0000ff00) / (float)0x0000ff00,
				(float)(mColorOverride & 0x000000ff) / (float)0x000000ff,
				0.0f);
			mGL.glClear(GL_COLOR_BUFFER_BIT);
		} else if (r.right > r.left && r.bottom > r.top) {
			mGL.glMatrixMode(GL_PROJECTION);
			mGL.glLoadIdentity();
			mGL.glMatrixMode(GL_MODELVIEW);
			mGL.glLoadIdentity();

			mGL.glDisable(GL_ALPHA_TEST);
			mGL.glDisable(GL_DEPTH_TEST);
			mGL.glDisable(GL_STENCIL_TEST);
			mGL.glDisable(GL_BLEND);
			mGL.glDisable(GL_CULL_FACE);
			mGL.glEnable(GL_DITHER);
			mGL.glEnable(GL_TEXTURE_2D);

			if (mSource.bInterlaced) {
				const int dstH = r.bottom;

				mGL.glOrtho(0, r.right, mbVerticalFlip ? 0 : dstH, mbVerticalFlip ? dstH : 0, -1, 1);

				for(int field=0; field<2; ++field) {
					const VDVideoTextureTilePatternOpenGL::TileInfo& tile = mTexPattern[field].GetTileInfo();

					int		w = tile.mSrcW;
					int		h = tile.mSrcH;
					double	iw = tile.mInvU;
					double	ih = tile.mInvV;

					double	px1 = 0.0;
					double	py1 = 0.0;
					double	px2 = w;
					double	py2 = h;

					double	u1 = iw * px1;
					double	u2 = iw * px2;

					ih *= mSource.pixmap.h / (double)dstH * 0.5;

					mGL.glBindTexture(GL_TEXTURE_2D, mTexPattern[field].GetTexture());

					int ytop	= VDRoundToInt(ceil((py1*2 + field - 0.5) * (dstH / (double)mSource.pixmap.h) - 0.5));
					int ybottom	= VDRoundToInt(ceil((py2*2 + field - 0.5) * (dstH / (double)mSource.pixmap.h) - 0.5));

					if ((ytop^field) & 1)
						++ytop;

					mGL.glBegin(GL_QUADS);
					mGL.glColor4d(1.0f, 1.0f, 1.0f, 1.0f);

					for(int ydst = ytop; ydst < ybottom; ydst += 2) {
						mGL.glTexCoord2d(u1, (ydst  -field+0.5)*ih);		mGL.glVertex2d(px1, ydst);
						mGL.glTexCoord2d(u1, (ydst+1-field+0.5)*ih);		mGL.glVertex2d(px1, ydst+1);
						mGL.glTexCoord2d(u2, (ydst+1-field+0.5)*ih);		mGL.glVertex2d(px2, ydst+1);
						mGL.glTexCoord2d(u2, (ydst  -field+0.5)*ih);		mGL.glVertex2d(px2, ydst);
					}

					mGL.glEnd();
				}
			} else {
				const VDVideoTextureTilePatternOpenGL::TileInfo& tile = mTexPattern[0].GetTileInfo();

				int		w = tile.mSrcW;
				int		h = tile.mSrcH;
				float	iw = tile.mInvU;
				float	ih = tile.mInvV;

				GLuint texHandle = mTexPattern[0].GetTexture();
				mGL.glBindTexture(GL_TEXTURE_2D, texHandle);

				if (mode == kFilterBicubic) {
					float	px1 = 0;
					float	py1 = 0;
					float	px2 = (float)(r.right - r.left);
					float	py2 = (float)h;
					float	u1 = 0;
					float	v1 = 0.25f * ih * bobOffset;
					float	u2 = iw * w;
					float	v2 = ih * h;
					float	f1 = 0.0f;
					float	f2 = r.right / (float)mCubicFilterHTexSize;
					float	px3 = 0;
					float	py3 = 0;
					float	px4 = (float)(r.right - r.left);
					float	py4 = (float)(r.bottom - r.top);
					float	iw2 = 1.0f / mCubicFilterTempTexWidth;
					float	ih2 = 1.0f / mCubicFilterTempTexHeight;
					float	u3 = 0;
					float	v3 = 0;
					float	u4 = iw2 * (float)(r.right - r.left);
					float	v4 = ih2 * (float)h;
					float	f3 = 0.0f;
					float	f4 = r.bottom / (float)mCubicFilterVTexSize;

					mGL.glEnable(GL_FRAGMENT_PROGRAM_ARB);
					mGL.glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, mFPCubic);

					mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
					mGL.glEnable(GL_TEXTURE_2D);
					mGL.glBindTexture(GL_TEXTURE_2D, texHandle);
					mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
					mGL.glEnable(GL_TEXTURE_2D);
					mGL.glBindTexture(GL_TEXTURE_2D, texHandle);
					mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
					mGL.glEnable(GL_TEXTURE_2D);
					mGL.glBindTexture(GL_TEXTURE_2D, mCubicFilterH);

					mGL.glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, iw*0.5f, 0, 0, 0);

					mGL.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mCubicFramebuffer);
					GLenum foo = mGL.glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
					mGL.glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

					mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

					float du0 = -iw;
					float du1 = -0.25f * iw;
					float du2 = +iw;

					mGL.glLoadIdentity();
					mGL.glOrtho(0, mCubicFilterTempTexWidth, 0, mCubicFilterTempTexHeight, 0, 1);
					mGL.glViewport(0, 0, mCubicFilterTempTexWidth, mCubicFilterTempTexHeight);
					mGL.glClear(GL_COLOR_BUFFER_BIT);

					VDASSERT(mGL.glGetError() == GL_NO_ERROR);
					mGL.glBegin(GL_QUADS);

					mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1+du0, v1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1+du1, v1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u1+du2, v1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, f1, 0.0f);
					mGL.glVertex2f(px1, py1);

					mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1+du0, v2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1+du1, v2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u1+du2, v2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, f1, 0.0f);
					mGL.glVertex2f(px1, py2);

					mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u2+du0, v2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2+du1, v2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2+du2, v2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, f2, 0.0f);
					mGL.glVertex2f(px2, py2);

					mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u2+du0, v1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2+du1, v1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2+du2, v1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, f2, 0.0f);
					mGL.glVertex2f(px2, py1);

					mGL.glEnd();
					VDASSERT(mGL.glGetError() == GL_NO_ERROR);

					mGL.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
					VDASSERT(mGL.glGetError() == GL_NO_ERROR);
					mGL.glDrawBuffer(GL_BACK);
					VDASSERT(mGL.glGetError() == GL_NO_ERROR);

					mGL.glLoadIdentity();
					mGL.glOrtho(0, r.right - r.left, mbVerticalFlip ? 0 : r.bottom - r.top, mbVerticalFlip ? r.bottom - r.top : 0, -1, 1);

					mGL.glViewport(r.left, vph - r.bottom, r.right - r.left, r.bottom - r.top);
					mGL.glClear(GL_COLOR_BUFFER_BIT);
					mGL.glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, 0, ih2*0.5f, 0, 0);

					mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
					mGL.glEnable(GL_TEXTURE_2D);
					mGL.glBindTexture(GL_TEXTURE_2D, mCubicFilterTempTex);
					mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
					mGL.glEnable(GL_TEXTURE_2D);
					mGL.glBindTexture(GL_TEXTURE_2D, mCubicFilterTempTex);
					mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
					mGL.glEnable(GL_TEXTURE_2D);
					mGL.glBindTexture(GL_TEXTURE_2D, mCubicFilterTempTex);
					mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
					mGL.glEnable(GL_TEXTURE_2D);
					mGL.glBindTexture(GL_TEXTURE_2D, mCubicFilterV);

					float dv0 = -ih2;
					float dv1 = -0.25f * ih2;
					float dv2 = +ih2;

					mGL.glBegin(GL_QUADS);

					mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u3, v3+dv0);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v3+dv1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u3, v3+dv2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, f3, 0.0f);
					mGL.glVertex2f(px3, py3);

					mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u3, v4+dv0);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v4+dv1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u3, v4+dv2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, f4, 0.0f);
					mGL.glVertex2f(px3, py4);

					mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u4, v4+dv0);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u4, v4+dv1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v4+dv2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, f4, 0.0f);
					mGL.glVertex2f(px4, py4);

					mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u4, v3+dv0);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u4, v3+dv1);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v3+dv2);
					mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, f3, 0.0f);
					mGL.glVertex2f(px4, py3);

					mGL.glEnd();

					mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
					mGL.glDisable(GL_TEXTURE_2D);
					mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
					mGL.glDisable(GL_TEXTURE_2D);
					mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
					mGL.glDisable(GL_TEXTURE_2D);
					mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
					mGL.glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
					mGL.glDisable(GL_FRAGMENT_PROGRAM_ARB);
				} else {
					float	px1 = 0;
					float	py1 = 0;
					float	px2 = (float)r.right;
					float	py2 = (float)r.bottom;
					float	u1 = 0;
					float	v1 = 0.25f * ih * bobOffset;
					float	u2 = iw * w;
					float	v2 = ih * h;

					mGL.glOrtho(0, r.right, mbVerticalFlip ? 0 : r.bottom, mbVerticalFlip ? r.bottom : 0, -1, 1);
					mGL.glBegin(GL_QUADS);
					mGL.glColor4d(1.0f, 1.0f, 1.0f, 1.0f);
					mGL.glTexCoord2d(u1, v1);		mGL.glVertex2d(px1, py1);
					mGL.glTexCoord2d(u1, v2);		mGL.glVertex2d(px1, py2);
					mGL.glTexCoord2d(u2, v2);		mGL.glVertex2d(px2, py2);
					mGL.glTexCoord2d(u2, v1);		mGL.glVertex2d(px2, py1);
					mGL.glEnd();
				}
			}
		}

		if (mpCompositor) {
			mRenderer.Begin(r.right, r.bottom);
			mpCompositor->Composite(mRenderer);
			mRenderer.End();
		}

		VDASSERT(mGL.glGetError() == GL_NO_ERROR);

		mGL.glFlush();

		mProfChan.Begin(0xa0c0e0, "Flip");
		SwapBuffers(hdc);
		mProfChan.End();

		// Workaround for Windows Vista DWM composition chain not updating.
		if (mbFirstPresent) {
			SetWindowPos(mhwndOGL, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
			SetWindowPos(mhwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
			mbFirstPresent = false;
		}

		mGL.End();
	}

	EndPaint(mhwndOGL, &ps);
}

bool VDVideoDisplayMinidriverOpenGL::InitBicubic() {
	if (!mGL.ARB_fragment_program || !mGL.ARB_multitexture)
		return false;

	VDASSERT(!mFPCubic);

	mGL.glEnable(GL_FRAGMENT_PROGRAM_ARB);
	mGL.glGenProgramsARB(1, &mFPCubic);
	mGL.glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, mFPCubic);

	VDASSERT(mGL.glGetError() == GL_NO_ERROR);
	mGL.glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, sizeof kFPCubic1 - 1, kFPCubic1);
	if (mGL.glGetError()) {
		VDDEBUG_DISP("VideoDisplay: GL fragment shader compilation failed.\n%s\n", mGL.glGetString(GL_PROGRAM_ERROR_STRING_ARB));
		mGL.glDeleteProgramsARB(1, &mFPCubic);
		mFPCubic = 0;
	}
	mGL.glDisable(GL_FRAGMENT_PROGRAM_ARB);

	if (!mFPCubic)
		return false;

	return true;
}

void VDVideoDisplayMinidriverOpenGL::ShutdownBicubic() {
	if (mCubicFilterTempTex) {
		mGL.glDeleteTextures(1, &mCubicFilterTempTex);
		mCubicFilterTempTex = 0;
		mCubicFilterTempTexWidth = 0;
		mCubicFilterTempTexHeight = 0;
	}

	if (mCubicFramebuffer) {
		mGL.glDeleteFramebuffersEXT(1, &mCubicFramebuffer);
		mCubicFramebuffer = 0;
	}

	if (mCubicFilterH) {
		mGL.glDeleteTextures(1, &mCubicFilterH);
		mCubicFilterH = 0;
		mCubicFilterHSize = 0;
		mCubicFilterHTexSize = 0;
	}

	if (mCubicFilterV) {
		mGL.glDeleteTextures(1, &mCubicFilterV);
		mCubicFilterV = 0;
		mCubicFilterVSize = 0;
		mCubicFilterVTexSize = 0;
	}
}

void VDVideoDisplayMinidriverOpenGL::UpdateCubicTextures(uint32 w, uint32 h) {
	uint32 temptexw = 1;
	while(temptexw < w)
		temptexw += temptexw;

	uint32 temptexh = 1;
	while(temptexh < (uint32)mSource.pixmap.h)
		temptexh += temptexh;

	if (temptexw < 128)
		temptexw = 128;

	if (temptexh < 128)
		temptexh = 128;

	if (!mCubicFilterTempTex)
		mGL.glGenTextures(1, &mCubicFilterTempTex);

	if (mCubicFilterTempTex) {
		if (temptexw != mCubicFilterTempTexWidth || temptexh != mCubicFilterTempTexHeight) {
			mCubicFilterTempTexWidth = temptexw;
			mCubicFilterTempTexHeight = temptexh;

			mGL.glBindTexture(GL_TEXTURE_2D, mCubicFilterTempTex);
			mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, temptexw, temptexh, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
			mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			mGL.glBindTexture(GL_TEXTURE_2D, 0);
		}

		if (!mCubicFramebuffer) {
			mGL.glGenFramebuffersEXT(1, &mCubicFramebuffer);
			mGL.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mCubicFramebuffer);
			mGL.glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, mCubicFilterTempTex, 0);
			mGL.glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		}
	}

	if (mCubicFilterHSize != w) {
		mCubicFilterHSize = w;

		uint32 texw = 1;
		while(texw < w)
			texw += texw;

		if (!mCubicFilterH)
			mGL.glGenTextures(1, &mCubicFilterH);

		mGL.glBindTexture(GL_TEXTURE_2D, mCubicFilterH);

		if (mCubicFilterHTexSize != texw) {
			mCubicFilterHTexSize = texw;
			mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texw, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		}

		UpdateCubicTexture(w, mSource.pixmap.w);

		mGL.glBindTexture(GL_TEXTURE_2D, 0);
	}

	if (mCubicFilterVSize != h) {
		mCubicFilterVSize = h;

		uint32 texh = 1;
		while(texh < h)
			texh += texh;

		if (!mCubicFilterV)
			mGL.glGenTextures(1, &mCubicFilterV);

		mGL.glBindTexture(GL_TEXTURE_2D, mCubicFilterV);

		if (mCubicFilterVTexSize != texh) {
			mCubicFilterVTexSize = texh;
			mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texh, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		}

		UpdateCubicTexture(h, mSource.pixmap.h);

		mGL.glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void VDVideoDisplayMinidriverOpenGL::UpdateCubicTexture(uint32 dw, uint32 sw) {
	double dudx = (double)sw / (double)dw;
	double u = dudx * 0.5;

	vdfastvector<uint32> data(dw);

	for(int x = 0; x < (int)dw; ++x) {
		int ix = VDFloorToInt(u - 0.5);
		double d = u - ((double)ix + 0.5);

		static const double m = -0.75;
		double c0 = (( (m    )*d - 2.0*m    )*d +   m)*d;
		double c1 = (( (m+2.0)*d -     m-3.0)*d      )*d + 1.0;
		double c2 = ((-(m+2.0)*d + 2.0*m+3.0)*d -   m)*d;
		double c3 = ((-(m    )*d +     m    )*d      )*d;

		double k0 = d*(1-d)*m;
		double k2 = d*(1-d)*m;

		double c1bi = d*k0;
		double c2bi = (1-d)*k2;
		double c1ex = c1-c1bi;
		double c2ex = c2-c2bi;

		double o1 = c2ex/(c1ex+c2ex)-d;

		double blue		= d;							// bilinear offset - p0 and p3
		double green	= o1*4;							// bilinear offset - p1 and p2
		double red		= (d*(1-d))*4;					// shift factor between the two
		double alpha	= d;							// lerp constant between p0 and p3

		uint8 ib = VDClampedRoundFixedToUint8Fast((float)blue * 127.0f/255.0f + 128.0f/255.0f);
		uint8 ig = VDClampedRoundFixedToUint8Fast((float)green * 127.0f/255.0f + 128.0f/255.0f);
		uint8 ir = VDClampedRoundFixedToUint8Fast((float)red);
		uint8 ia = VDClampedRoundFixedToUint8Fast((float)alpha);

		data[x] = (uint32)ib + ((uint32)ig << 8) + ((uint32)ir << 16) + ((uint32)ia << 24);

#if 0
				double fb = ((int)ib - 128) / 127.0f;
				double fg = ((int)ig - 128) / 127.0f;
				double fr = (double)ir / 255.0f;
				double fa = (double)ia / 255.0f;

				double g0 = fr*0.25f*0.75f;
				double g1 = 2*(0.5f + fr*0.25f*0.75f);
				double d1 = 0.25f * fg + d;
				double g2 = fr*0.25f*0.75f;

				double cr0 = -g0*(1-d);
				double cr1 = -g0*d + g1*(1-d1);
				double cr2 = g1*d1 + -g2*(1-d);
				double cr3 = -g2*d;

				if (fabsf(cr0-c0) > 0.01f)
					__debugbreak();
				if (fabsf(cr1-c1) > 0.01f)
					__debugbreak();
				if (fabsf(cr2-c2) > 0.01f)
					__debugbreak();
				if (fabsf(cr3-c3) > 0.01f)
					__debugbreak();
#endif

		u += dudx;
	}

	mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, dw, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data.data());

	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

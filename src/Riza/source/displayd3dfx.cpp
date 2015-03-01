//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2005 Avery Lee
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

#include <d3d9.h>
#include <d3dx9.h>
#include <vd2/system/VDString.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/math.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>
#include <vd2/system/error.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/direct3d.h>
#include "d3dxfx.h"
#include <vd2/VDDisplay/displaydrv.h>
#include <vd2/Riza/displaydrvdx9.h>

///////////////////////////////////////////////////////////////////////////

class VDD3D9TextureGeneratorFullSizeRTT : public vdrefcounted<IVDD3D9TextureGenerator> {
public:
	bool GenerateTexture(VDD3D9Manager *pManager, IVDD3D9Texture *pTexture) {
		const D3DDISPLAYMODE& dmode = pManager->GetDisplayMode();

		int w = dmode.Width;
		int h = dmode.Height;

		pManager->AdjustTextureSize(w, h);

		IDirect3DDevice9 *dev = pManager->GetDevice();
		IDirect3DTexture9 *tex;
		HRESULT hr = dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL);
		if (FAILED(hr))
			return false;

		pTexture->SetD3DTexture(tex);
		tex->Release();
		return true;
	}
};

bool VDCreateD3D9TextureGeneratorFullSizeRTT(IVDD3D9TextureGenerator **ppGenerator) {
	*ppGenerator = new VDD3D9TextureGeneratorFullSizeRTT;
	if (!*ppGenerator)
		return false;
	(*ppGenerator)->AddRef();
	return true;
}

///////////////////////////////////////////////////////////////////////////

class VDD3D9TextureGeneratorFullSizeRTT16F : public vdrefcounted<IVDD3D9TextureGenerator> {
public:
	bool GenerateTexture(VDD3D9Manager *pManager, IVDD3D9Texture *pTexture) {
		const D3DDISPLAYMODE& dmode = pManager->GetDisplayMode();

		int w = dmode.Width;
		int h = dmode.Height;

		pManager->AdjustTextureSize(w, h);

		IDirect3DDevice9 *dev = pManager->GetDevice();
		IDirect3DTexture9 *tex;
		HRESULT hr = dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &tex, NULL);
		if (FAILED(hr))
			return false;

		pTexture->SetD3DTexture(tex);
		tex->Release();
		return true;
	}
};

bool VDCreateD3D9TextureGeneratorFullSizeRTT16F(IVDD3D9TextureGenerator **ppGenerator) {
	*ppGenerator = new VDD3D9TextureGeneratorFullSizeRTT16F;
	if (!*ppGenerator)
		return false;
	(*ppGenerator)->AddRef();
	return true;
}

///////////////////////////////////////////////////////////////////////////


#define D3D_AUTOBREAK_2(x) if (FAILED(hr = x)) { VDASSERT(!"VideoDriver/D3DFX: Direct3D call failed: "#x); goto d3d_failed; } else ((void)0)
#define D3D_AUTOBREAK(x) if ((hr = mpD3DDevice->x), FAILED(hr)) { VDASSERT(!"VideoDriver/D3DFX: Direct3D call failed: "#x); goto d3d_failed; } else ((void)0)

#define VDDEBUG_D3DFXDISP VDDEBUG

using namespace nsVDD3D9;

namespace {
	VDStringW g_VDVideoDisplayD3DFXEffectFileName;
}

namespace {
	typedef BOOL (WINAPI *tpD3DXCheckVersion)(UINT D3DSDKVersion, UINT D3DXSDKVersion);
	typedef HRESULT (WINAPI *tpD3DXCreateEffectCompilerFromFileA)(
				LPCSTR pSrcFile,
				const D3DXMACRO *pDefines,
				LPD3DXINCLUDE pInclude,
				DWORD Flags,
				ID3DXEffectCompilerVersion25 **ppEffect,
				ID3DXBuffer **ppCompilationErrors);
	typedef HRESULT (WINAPI *tpD3DXCreateEffectCompilerFromFileW)(
				LPCWSTR pSrcFile,
				const D3DXMACRO *pDefines,
				LPD3DXINCLUDE pInclude,
				DWORD Flags,
				ID3DXEffectCompilerVersion25 **ppEffect,
				ID3DXBuffer **ppCompilationErrors);
	typedef HRESULT (WINAPI *tpD3DXCreateEffect)(
				LPDIRECT3DDEVICE9 pDevice,
				LPCVOID pSrcData,
				UINT SrcDataLen,
				const D3DXMACRO *pDefines,
				LPD3DXINCLUDE pInclude,
				DWORD Flags,
				ID3DXEffectPool *pPool,
				ID3DXEffectVersion25 **ppEffect,
				ID3DXBuffer **ppCompilationErrors);
	typedef HRESULT (WINAPI *tpD3DXCreateTextureShader)(CONST DWORD *pFunction, LPD3DXTEXTURESHADER *ppTextureShader);
	typedef HRESULT (WINAPI *tpD3DXFillTextureTX)(LPDIRECT3DTEXTURE9 pTexture, LPD3DXTEXTURESHADER pTextureShader);
	typedef HRESULT (WINAPI *tpD3DXFillCubeTextureTX)(LPDIRECT3DCUBETEXTURE9 pCubeTexture, LPD3DXTEXTURESHADER pTextureShader);
	typedef HRESULT (WINAPI *tpD3DXFillVolumeTextureTX)(LPDIRECT3DVOLUMETEXTURE9 pVolumeTexture, LPD3DXTEXTURESHADER pTextureShader);

	struct StdParamData {
		float vpsize[4];			// (viewport size)			vpwidth, vpheight, 1/vpheight, 1/vpwidth
		float texsize[4];			// (texture size)			texwidth, texheight, 1/texheight, 1/texwidth
		float srcsize[4];			// (source size)			srcwidth, srcheight, 1/srcheight, 1/srcwidth
		float tempsize[4];			// (temp rtt size)			tempwidth, tempheight, 1/tempheight, 1/tempwidth
		float temp2size[4];			// (temp2 rtt size)			tempwidth, tempheight, 1/tempheight, 1/tempwidth
		float vpcorrect[4];			// (viewport correction)	2/vpwidth, 2/vpheight, -1/vpheight, 1/vpwidth
		float vpcorrect2[4];		// (viewport correction)	2/vpwidth, -2/vpheight, 1+1/vpheight, -1-1/vpwidth
		float tvpcorrect[4];		// (temp vp correction)		2/tvpwidth, 2/tvpheight, -1/tvpheight, 1/tvpwidth
		float tvpcorrect2[4];		// (temp vp correction)		2/tvpwidth, -2/tvpheight, 1+1/tvpheight, -1-1/tvpwidth
		float t2vpcorrect[4];		// (temp2 vp correction)	2/tvpwidth, 2/tvpheight, -1/tvpheight, 1/tvpwidth
		float t2vpcorrect2[4];		// (temp2 vp correction)	2/tvpwidth, -2/tvpheight, 1+1/tvpheight, -1-1/tvpwidth
		float time[4];				// (time)
	};

	static const struct StdParam {
		const char *name;
		int offset;
	} kStdParamInfo[]={
		{ "vd_vpsize",		offsetof(StdParamData, vpsize) },
		{ "vd_texsize",		offsetof(StdParamData, texsize) },
		{ "vd_srcsize",		offsetof(StdParamData, srcsize) },
		{ "vd_tempsize",	offsetof(StdParamData, tempsize) },
		{ "vd_temp2size",	offsetof(StdParamData, temp2size) },
		{ "vd_vpcorrect",	offsetof(StdParamData, vpcorrect) },
		{ "vd_vpcorrect2",	offsetof(StdParamData, vpcorrect2) },
		{ "vd_tvpcorrect",	offsetof(StdParamData, tvpcorrect) },
		{ "vd_tvpcorrect2",	offsetof(StdParamData, tvpcorrect2) },
		{ "vd_t2vpcorrect",		offsetof(StdParamData, t2vpcorrect) },
		{ "vd_t2vpcorrect2",	offsetof(StdParamData, t2vpcorrect2) },
		{ "vd_time",		offsetof(StdParamData, time) },
	};

	enum { kStdParamCount = sizeof kStdParamInfo / sizeof kStdParamInfo[0] };

	struct FormatInfo {
		const char *mpName;
		D3DFORMAT	mFormat;
	} kFormats[]={
#define X(name) { #name, D3DFMT_##name },
	X(R8G8B8)
	X(A8R8G8B8)
	X(X8R8G8B8)
	X(R5G6B5)
	X(X1R5G5B5)
	X(A1R5G5B5)
	X(A4R4G4B4)
	X(R3G3B2)
	X(A8)
	X(A8R3G3B2)
	X(X4R4G4B4)
	X(A2B10G10R10)
	X(A8B8G8R8)
	X(X8B8G8R8)
	X(G16R16)
	X(A2R10G10B10)
	X(A16B16G16R16)
	X(A8P8)
	X(P8)
	X(L8)
	X(A8L8)
	X(A4L4)
	X(V8U8)
	X(L6V5U5)
	X(X8L8V8U8)
	X(Q8W8V8U8)
	X(V16U16)
	X(A2W10V10U10)
	X(UYVY)
	X(R8G8_B8G8)
	X(YUY2)
	X(G8R8_G8B8)
	X(DXT1)
	X(DXT2)
	X(DXT3)
	X(DXT4)
	X(DXT5)
	X(D16_LOCKABLE)
	X(D32)
	X(D15S1)
	X(D24S8)
	X(D24X8)
	X(D24X4S4)
	X(D16)
	X(D32F_LOCKABLE)
	X(D24FS8)
	X(L16)
	X(VERTEXDATA)
	X(INDEX16)
	X(INDEX32)
	X(Q16W16V16U16)
	X(MULTI2_ARGB8)
	X(R16F)
	X(G16R16F)
	X(A16B16G16R16F)
	X(R32F)
	X(G32R32F)
	X(A32B32G32R32F)
	X(CxV8U8)
#undef X
	};
}

class VDVideoDisplayMinidriverD3DFX : public VDVideoDisplayMinidriver, protected VDD3D9Client {
public:
	VDVideoDisplayMinidriverD3DFX(bool clipToMonitor);
	~VDVideoDisplayMinidriverD3DFX();

protected:
	bool Init(HWND hwnd, HMONITOR hmonitor, const VDVideoDisplaySourceInfo& info);
	void ShutdownEffect();
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid();
	bool IsFramePending() { return mbSwapChainPresentPending; }
	void SetFilterMode(FilterMode mode);
	void SetFullScreen(bool fs, uint32 w, uint32 h, uint32 refresh);

	bool Tick(int id);
	void Poll();
	bool Resize(int width, int height);
	bool Update(UpdateMode);
	void Refresh(UpdateMode);
	bool Paint(HDC hdc, const RECT& rClient, UpdateMode mode);

	void SetLogicalPalette(const uint8 *pLogicalPalette);
	float GetSyncDelta() const { return mSyncDelta; }

protected:
	struct ParamBinding;

	typedef void (VDVideoDisplayMinidriverD3DFX::*ParamUploadMethod)(const ParamBinding& binding);

	struct ParamBinding {
		D3DXHANDLE	mhParam;
		ParamUploadMethod	mpFn;
	};

	void OnPreDeviceReset() {
		DestroyCustomTextures(true);

		mpSwapChain = NULL;
		mSwapChainW = 0;
		mSwapChainH = 0;

		if (mpEffect)
			mpEffect->OnLostDevice();

		mpD3DTempTexture = NULL;
		mpD3DTempSurface = NULL;
		mpD3DTempTexture2 = NULL;
		mpD3DTempSurface2 = NULL;
	}

	void OnPostDeviceReset() {
		if (mpEffect)
			mpEffect->OnResetDevice();

		if (mpTempTexture) {
			mpD3DTempTexture = mpTempTexture->GetD3DTexture();

			if (mpD3DTempTexture)
				mpD3DTempTexture->GetSurfaceLevel(0, ~mpD3DTempSurface);
		}

		if (mpTempTexture2) {
			mpD3DTempTexture2 = mpTempTexture2->GetD3DTexture();

			if (mpD3DTempTexture2)
				mpD3DTempTexture2->GetSurfaceLevel(0, ~mpD3DTempSurface2);
		}

		CreateCustomTextures(true, NULL);
	}

	bool UpdateBackbuffer(const RECT& rClient, UpdateMode updateMode);
	bool UpdateScreen(const RECT& rClient, UpdateMode updateMode, bool polling);
	void DisplayError();
	void PreprocessEffectParameters();
	void UpdateParam_RenderTargetDimensions(const ParamBinding& binding);
	void UpdateParam_ViewportPixelSize(const ParamBinding& binding);
	void UpdateEffectParameters(const D3DVIEWPORT9& vp, const D3DSURFACE_DESC& texdesc);

	void CreateCustomTextureBindings();
	bool CreateCustomTextures(bool vramOnly, const vdsize32 *vpsize);
	void DestroyCustomTextures(bool vramOnly);
	void DestroyCustomTextureBindings();

	HWND				mhwnd;
	HWND				mhwndError;
	HMODULE				mhmodD3DX;
	RECT				mrClient;

	VDD3D9Manager		*mpManager;
	IDirect3DDevice9	*mpD3DDevice;			// weak ref

	vdrefptr<IVDVideoUploadContextD3D9>	mpUploadContext;

	vdrefptr<IVDD3D9SwapChain>	mpSwapChain;
	int					mSwapChainW;
	int					mSwapChainH;
	bool				mbSwapChainPresentPending;
	bool				mbSwapChainPresentPolling;
	bool				mbSwapChainImageValid;
	bool				mbFirstPresent;
	bool				mbFullScreen;
	uint32				mFullScreenWidth;
	uint32				mFullScreenHeight;
	uint32				mFullScreenRefreshRate;
	bool				mbClipToMonitor;

	VDAtomicInt			mTickPending;

	vdrefptr<IVDD3D9Texture>	mpTempTexture;
	IDirect3DTexture9			*mpD3DTempTexture;		// weak ref
	vdrefptr<IDirect3DSurface9>	mpD3DTempSurface;

	vdrefptr<IVDD3D9Texture>	mpTempTexture2;
	IDirect3DTexture9			*mpD3DTempTexture2;		// weak ref
	vdrefptr<IDirect3DSurface9>	mpD3DTempSurface2;

	FilterMode			mPreferredFilter;

	ID3DXEffectVersion25 *mpEffect;
	ID3DXEffectCompilerVersion25	*mpEffectCompiler;

	VDVideoDisplaySourceInfo	mSource;
	bool				mbForceFrameUpload;

	float				mSyncDelta;
	VDD3DPresentHistory	mPresentHistory;

	VDStringA			mTimingString;
	int					mTimingStringCounter;
	float				mLastLongestPresentTime;
	float				mLastLongestFrameTime;
	uint64				mLastFrameTime;

	uint32				mLatencyFence;
	uint32				mLatencyFenceNext;

	VDStringW			mError;

	D3DXHANDLE			mhSrcTexture;
	D3DXHANDLE			mhPrevSrcTexture;
	D3DXHANDLE			mhPrevSrc2Texture;
	D3DXHANDLE			mhTempTexture;
	D3DXHANDLE			mhTempTexture2;
	D3DXHANDLE			mhTechniques[kFilterModeCount - 1];
	D3DXHANDLE			mhStdParamHandles[kStdParamCount];

	typedef vdfastvector<ParamBinding> ParamBindings;
	ParamBindings mParamBindings;

	struct TextureBinding {
		IDirect3DBaseTexture9 *mpTexture;
		ID3DXTextureShader *mpTextureShader;

		D3DXHANDLE		mhParam;
		D3DXHANDLE		mhSizeParam;
		D3DFORMAT		mFormat;
		D3DPOOL			mPool;
		DWORD			mUsage;
		D3DRESOURCETYPE	mResType;
		float			mViewportRatioW;
		float			mViewportRatioH;
		int				mWidth;
		int				mHeight;
		int				mDepth;
		int				mMipLevels;
	};

	typedef vdfastvector<TextureBinding> TextureBindings;
	TextureBindings	mTextureBindings;

	tpD3DXCreateEffect			mpD3DXCreateEffect;
	tpD3DXCreateTextureShader	mpD3DXCreateTextureShader;
	tpD3DXFillTextureTX			mpD3DXFillTextureTX;
	tpD3DXFillCubeTextureTX		mpD3DXFillCubeTextureTX;
	tpD3DXFillVolumeTextureTX	mpD3DXFillVolumeTextureTX;

	vdrefptr<IVDFontRendererD3D9>	mpFontRenderer;
};


IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverD3DFX(bool clipToMonitor) {
	return new VDVideoDisplayMinidriverD3DFX(clipToMonitor);
}

VDVideoDisplayMinidriverD3DFX::VDVideoDisplayMinidriverD3DFX(bool clipToMonitor)
	: mhwnd(NULL)
	, mhwndError(NULL)
	, mhmodD3DX(NULL)
	, mpManager(NULL)
	, mSwapChainW(0)
	, mSwapChainH(0)
	, mbSwapChainImageValid(false)
	, mbSwapChainPresentPending(false)
	, mbSwapChainPresentPolling(false)
	, mbFirstPresent(false)
	, mbFullScreen(false)
	, mFullScreenWidth(0)
	, mFullScreenHeight(0)
	, mFullScreenRefreshRate(0)
	, mbClipToMonitor(clipToMonitor)
	, mTickPending(0)
	, mpD3DTempTexture(NULL)
	, mpD3DTempTexture2(NULL)
	, mpD3DTempSurface(NULL)
	, mpD3DTempSurface2(NULL)
	, mpEffect(NULL)
	, mpEffectCompiler(NULL)
	, mPreferredFilter(kFilterAnySuitable)
	, mbForceFrameUpload(false)
	, mSyncDelta(0.0f)
	, mLastLongestPresentTime(0.0f)
	, mLastLongestFrameTime(0.0f)
	, mTimingStringCounter(0)
	, mLatencyFence(0)
	, mLatencyFenceNext(0)
{
	mrClient.top = mrClient.left = mrClient.right = mrClient.bottom = 0;
}

VDVideoDisplayMinidriverD3DFX::~VDVideoDisplayMinidriverD3DFX() {
}

bool VDVideoDisplayMinidriverD3DFX::Init(HWND hwnd, HMONITOR hmonitor, const VDVideoDisplaySourceInfo& info) {
	GetClientRect(hwnd, &mrClient);
	mhwnd = hwnd;
	mSource = info;
	mSyncDelta = 0.0f;

	mLastFrameTime = VDGetPreciseTick();

	// attempt to load d3dx9_25.dll
	mhmodD3DX = LoadLibrary("d3dx9_25.dll");

	if (!mhmodD3DX) {
		mError = L"Cannot initialize the Direct3D FX system: Unable to load d3dx9_25.dll.";
		DisplayError();
		return true;
	}

	// make sure we're using the right version
	tpD3DXCheckVersion pD3DXCheckVersion = (tpD3DXCheckVersion)GetProcAddress(mhmodD3DX, "D3DXCheckVersion");
	if (!pD3DXCheckVersion || !pD3DXCheckVersion(D3D_SDK_VERSION, 25)) {
		VDASSERT(!"Incorrect D3DX version.");
		Shutdown();
		return false;
	}

	// pull the effect compiler pointer
	tpD3DXCreateEffectCompilerFromFileA pD3DXCreateEffectCompilerFromFileA = NULL;
	tpD3DXCreateEffectCompilerFromFileW pD3DXCreateEffectCompilerFromFileW = NULL;

	if (VDIsWindowsNT())
		pD3DXCreateEffectCompilerFromFileW = (tpD3DXCreateEffectCompilerFromFileW)GetProcAddress(mhmodD3DX, "D3DXCreateEffectCompilerFromFileW");

	if (!pD3DXCreateEffectCompilerFromFileW) {
		pD3DXCreateEffectCompilerFromFileA = (tpD3DXCreateEffectCompilerFromFileA)GetProcAddress(mhmodD3DX, "D3DXCreateEffectCompilerFromFileA");

		if (!pD3DXCreateEffectCompilerFromFileA) {
			Shutdown();
			return false;
		}
	}

	mpD3DXCreateEffect			= (tpD3DXCreateEffect)			GetProcAddress(mhmodD3DX, "D3DXCreateEffect");
	mpD3DXCreateTextureShader	= (tpD3DXCreateTextureShader)	GetProcAddress(mhmodD3DX, "D3DXCreateTextureShader");
	mpD3DXFillTextureTX			= (tpD3DXFillTextureTX)			GetProcAddress(mhmodD3DX, "D3DXFillTextureTX");
	mpD3DXFillCubeTextureTX		= (tpD3DXFillCubeTextureTX)		GetProcAddress(mhmodD3DX, "D3DXFillCubeTextureTX");
	mpD3DXFillVolumeTextureTX	= (tpD3DXFillVolumeTextureTX)	GetProcAddress(mhmodD3DX, "D3DXFillVolumeTextureTX");

	if (!mpD3DXCreateEffect || !mpD3DXCreateTextureShader || !mpD3DXFillTextureTX || !mpD3DXFillCubeTextureTX || !mpD3DXFillVolumeTextureTX) {
		Shutdown();
		return false;
	}

	// attempt to initialize D3D9
	mpManager = VDInitDirect3D9(this, hmonitor, false);
	if (!mpManager) {
		Shutdown();
		return false;
	}

	if (mbFullScreen)
		mpManager->AdjustFullScreen(true, mFullScreenWidth, mFullScreenHeight, mFullScreenRefreshRate);

	mpD3DDevice = mpManager->GetDevice();

	// init font renderer
	if (mbDisplayDebugInfo) {
		if (!VDCreateFontRendererD3D9(~mpFontRenderer)) {
			Shutdown();
			return false;
		}

		mpFontRenderer->Init(mpManager);		// we explicitly allow this to fail
	}

	// attempt to compile effect
	ID3DXBuffer *pError = NULL;

	const VDStringW& fxfile = g_VDVideoDisplayD3DFXEffectFileName;

	VDStringW srcfile;

	if (fxfile.find(':') != VDStringW::npos || (fxfile.size() >= 2 && fxfile[0] == '\\' && fxfile[1] == '\\'))
		srcfile = fxfile;
	else
		srcfile = VDGetProgramPath() + fxfile;

	ID3DXBuffer *pEffectBuffer = NULL;

	HRESULT hr;
	if (pD3DXCreateEffectCompilerFromFileW)
		hr = pD3DXCreateEffectCompilerFromFileW(srcfile.c_str(), NULL, NULL, 0, &mpEffectCompiler, &pError);
	else
		hr = pD3DXCreateEffectCompilerFromFileA(VDTextWToA(srcfile).c_str(), NULL, NULL, 0, &mpEffectCompiler, &pError);

	if (SUCCEEDED(hr)) {
		if (pError) {
			pError->Release();
			pError = NULL;
		}

		// compile the effect
		hr = mpEffectCompiler->CompileEffect(0, &pEffectBuffer, &pError);

		if (SUCCEEDED(hr)) {
			if (pError) {
				pError->Release();
				pError = NULL;
			}

			// create the effect
			hr = mpD3DXCreateEffect(mpD3DDevice, pEffectBuffer->GetBufferPointer(), pEffectBuffer->GetBufferSize(), NULL, NULL, 0, NULL, &mpEffect, &pError);

			pEffectBuffer->Release();
		}
	}

	if (FAILED(hr)) {
		if (pError)
			mError.sprintf(L"Couldn't compile effect file %ls due to the following error:\r\n\r\n%hs\r\n\r\nIf you have Direct3D effects support enabled by mistake, it can be disabled under Options > Preferences > Display.", srcfile.c_str(), (const char *)pError->GetBufferPointer());
		else
			mError.sprintf(L"Couldn't compile effect file %ls.\r\n\r\nIf you have Direct3D effects support enabled by mistake, it can be disabled under Options > Preferences > Display.", srcfile.c_str());

		if (pError)
			pError->Release();

		ShutdownEffect();

		DisplayError();
		return true;
	}

	// scan for textures
	D3DXEFFECT_DESC effDesc;
	hr = mpEffect->GetDesc(&effDesc);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	// scan for standard parameter handles (ok for these to fail)
	for(int i=0; i<sizeof kStdParamInfo/sizeof kStdParamInfo[0]; ++i)
		mhStdParamHandles[i] = mpEffect->GetParameterByName(NULL, kStdParamInfo[i].name);

	// scan for standard techniques
	static const char *const kTechniqueNames[]={
		"point",
		"bilinear",
		"bicubic",
	};

	VDASSERTCT(sizeof kTechniqueNames / sizeof kTechniqueNames[0] == kFilterModeCount - 1);

	D3DXHANDLE hTechniqueLastValid = NULL;

	for(int i=kFilterModeCount - 2; i >= 0; --i) {
		const char *baseName = kTechniqueNames[i];
		size_t baseNameLen = strlen(baseName);

		mhTechniques[i] = NULL;

		for(UINT tech=0; tech<effDesc.Techniques; ++tech) {
			D3DXHANDLE hTechnique = mpEffect->GetTechnique(tech);

			if (!hTechnique)
				continue;

			D3DXTECHNIQUE_DESC techDesc;

			hr = mpEffect->GetTechniqueDesc(hTechnique, &techDesc);
			if (FAILED(hr))
				continue;

			hTechniqueLastValid = hTechnique;
			if (techDesc.Name && !strncmp(techDesc.Name, baseName, baseNameLen)) {
				char c = techDesc.Name[baseNameLen];

				if (!c || c=='_') {
					for(int j=i; j < kFilterModeCount - 1 && !mhTechniques[j]; ++j)
						mhTechniques[j] = hTechnique;
					break;
				}
			}
		}

		if (!mhTechniques[i])
			mhTechniques[i] = hTechniqueLastValid;
	}

	// check if we don't have any recognizable techniques
	if (!hTechniqueLastValid) {
		mError.sprintf(L"Couldn't find a valid technique in effect file %ls:\nMust be one of 'point', 'bilinear', or 'bicubic.'", srcfile.c_str());
		DisplayError();
		ShutdownEffect();
		return true;
	}

	// backfill lowest level techniques that may be missing
	for(int i=0; !mhTechniques[i]; ++i)
		mhTechniques[i] = hTechniqueLastValid;

	// check if we need the temp texture
	mhTempTexture = mpEffect->GetParameterByName(NULL, "vd_temptexture");
	if (mhTempTexture) {
		if (!mpManager->CreateSharedTexture<VDD3D9TextureGeneratorFullSizeRTT>("rtt1", ~mpTempTexture)) {
			mError = L"Unable to allocate temporary texture.";
			DisplayError();
			ShutdownEffect();
			return true;
		}

		mpD3DTempTexture = mpTempTexture->GetD3DTexture();

		hr = mpD3DTempTexture->GetSurfaceLevel(0, ~mpD3DTempSurface);
		if (FAILED(hr)) {
			Shutdown();
			return false;
		}
	}

	// check if we need the second temp texture
	mhTempTexture2 = mpEffect->GetParameterByName(NULL, "vd_temptexture2");
	if (!mhTempTexture2)
		mhTempTexture2 = mpEffect->GetParameterByName(NULL, "vd_temp2texture");
	if (mhTempTexture2) {
		if (!mpManager->CreateSharedTexture<VDD3D9TextureGeneratorFullSizeRTT>("rtt2", ~mpTempTexture2)) {
			mError = L"Unable to allocate second temporary texture.";
			DisplayError();
			ShutdownEffect();
			return true;
		}

		mpD3DTempTexture2 = mpTempTexture2->GetD3DTexture();

		hr = mpD3DTempTexture2->GetSurfaceLevel(0, ~mpD3DTempSurface2);
		if (FAILED(hr)) {
			Shutdown();
			return false;
		}
	}

	// get handle for src texture
	mhSrcTexture = mpEffect->GetParameterByName(NULL, "vd_srctexture");

	mbForceFrameUpload = false;
	if (mhSrcTexture) {
		D3DXHANDLE hForceFrameAnno = mpEffect->GetAnnotationByName(mhSrcTexture, "vd_forceframeupload");

		BOOL b;
		if (hForceFrameAnno && SUCCEEDED(mpEffect->GetBool(hForceFrameAnno, &b)) && b) {
			mbForceFrameUpload = true;
		}
	}

	// get handle for prev src texture
	mhPrevSrcTexture = mpEffect->GetParameterByName(NULL, "vd_prevsrctexture");
	mhPrevSrc2Texture = mpEffect->GetParameterByName(NULL, "vd_prevsrc2texture");

	try {
		PreprocessEffectParameters();
		CreateCustomTextureBindings();
		CreateCustomTextures(false, NULL);
	} catch(const MyError& e) {
		mError.sprintf(L"%hs", e.gets());
		DisplayError();
		ShutdownEffect();
		return true;		
	}

	if (mpEffectCompiler) {
		mpEffectCompiler->Release();
		mpEffectCompiler = NULL;
	}

	// create upload context
	if (!VDCreateVideoUploadContextD3D9(~mpUploadContext)) {
		Shutdown();
		return false;
	}

	if (!mpUploadContext->Init(hmonitor, false, info.pixmap, info.bAllowConversion, false, mhPrevSrc2Texture ? 3 : mhPrevSrcTexture ? 2 : 1)) {
		Shutdown();
		return false;
	}

	VDDEBUG_D3DFXDISP("VideoDisplay/D3DFX: Initialization successful for %dx%d source image.\n", mSource.pixmap.w, mSource.pixmap.h);

	mbFirstPresent = true;
	return true;
}

void VDVideoDisplayMinidriverD3DFX::ShutdownEffect() {
	mParamBindings.clear();
	DestroyCustomTextureBindings();

	mpSwapChain = NULL;
	mSwapChainW = 0;
	mSwapChainH = 0;

	if (mpEffectCompiler) {
		mpEffectCompiler->Release();
		mpEffectCompiler = NULL;
	}

	if (mpEffect) {
		mpEffect->Release();
		mpEffect = NULL;
	}
}

void VDVideoDisplayMinidriverD3DFX::Shutdown() {
	if (mhwndError) {
		DestroyWindow(mhwndError);
		mhwndError = NULL;
	}

	ShutdownEffect();

	if (mpFontRenderer) {
		mpFontRenderer->Shutdown();
		mpFontRenderer = NULL;
	}

	mpD3DTempSurface2 = NULL;
	mpD3DTempSurface = NULL;

	mpD3DTempTexture2 = NULL;
	mpTempTexture2 = NULL;

	mpD3DTempTexture = NULL;
	mpTempTexture = NULL;

	mpUploadContext = NULL;

	if (mpManager) {
		if (mbFullScreen)
			mpManager->AdjustFullScreen(false, 0, 0, 0);
		VDDeinitDirect3D9(mpManager, this);
		mpManager = NULL;
	}

	if (mhmodD3DX) {
		FreeLibrary(mhmodD3DX);
		mhmodD3DX = NULL;
	}
}

bool VDVideoDisplayMinidriverD3DFX::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (mSource.pixmap.w == info.pixmap.w && mSource.pixmap.h == info.pixmap.h && mSource.pixmap.format == info.pixmap.format && mSource.pixmap.pitch == info.pixmap.pitch) {
		mSource = info;
		return true;
	}
	return false;
}

bool VDVideoDisplayMinidriverD3DFX::IsValid() {
	return mpD3DDevice != 0;
}

void VDVideoDisplayMinidriverD3DFX::SetFilterMode(FilterMode mode) {
	mPreferredFilter = mode;
}

void VDVideoDisplayMinidriverD3DFX::SetFullScreen(bool fs, uint32 w, uint32 h, uint32 refresh) {
	if (mbFullScreen != fs) {
		mbFullScreen = fs;
		mFullScreenWidth = w;
		mFullScreenHeight = h;
		mFullScreenRefreshRate = refresh;

		if (mpManager)
			mpManager->AdjustFullScreen(fs, w, h, refresh);
	}
}

bool VDVideoDisplayMinidriverD3DFX::Tick(int id) {
	return true;
}

void VDVideoDisplayMinidriverD3DFX::Poll() {
	if (mbSwapChainPresentPending) {
		UpdateScreen(mrClient, kModeVSync, true);
	}
}

bool VDVideoDisplayMinidriverD3DFX::Resize(int width, int height) {
	mbSwapChainImageValid = false;
	mbSwapChainPresentPending = false;
	mbSwapChainPresentPolling = false;

	mrClient.right = width;
	mrClient.bottom = height;

	if (mhwndError)
		SetWindowPos(mhwndError, NULL, 0, 0, width, height, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);

	return true;
}

bool VDVideoDisplayMinidriverD3DFX::Update(UpdateMode mode) {
	if (!mpEffect)
		return true;

	uint32 fieldMode = mode & kModeFieldMask;
	if (mbForceFrameUpload) {
		if (mode & kModeFirstField)
			fieldMode = kModeAllFields;
		else if ((mode & kModeFieldMask) != kModeAllFields) {
			UpdateBackbuffer(mrClient, mode);
			mSource.mpCB->ReleaseActiveFrame();
			return true;
		}
	}

	int fieldMask = 3;

	switch(fieldMode) {
		case kModeEvenField:
			fieldMask = 1;
			break;

		case kModeOddField:
			fieldMask = 2;
			break;

		case kModeAllFields:
			break;
	}

	bool success = mpUploadContext->Update(mSource.pixmap, fieldMask);

	mSource.mpCB->ReleaseActiveFrame();

	if (!success)
		return false;

	UpdateBackbuffer(mrClient, mode);

	return true;
}

void VDVideoDisplayMinidriverD3DFX::Refresh(UpdateMode mode) {
	RECT r;
	GetClientRect(mhwnd, &r);
	if (r.right > 0 && r.bottom > 0) {
		Paint(NULL, r, mode);
	}
}

bool VDVideoDisplayMinidriverD3DFX::Paint(HDC hdc, const RECT& rClient, UpdateMode updateMode) {
	if (!mpEffect) {
		mSource.mpCB->ReleaseActiveFrame();
		return true;
	}

	return (mbSwapChainImageValid || UpdateBackbuffer(rClient, updateMode)) && UpdateScreen(rClient, updateMode, (updateMode & kModeVSync) != 0);
}

void VDVideoDisplayMinidriverD3DFX::SetLogicalPalette(const uint8 *pLogicalPalette) {
}

bool VDVideoDisplayMinidriverD3DFX::UpdateBackbuffer(const RECT& rClient0, UpdateMode updateMode) {
	VDASSERT(!mbSwapChainPresentPending);
	uint64 startTime = VDGetPreciseTick();
	RECT rClient = rClient0;

	// Exit immediately if nothing to do.
	if (!rClient.right || !rClient.bottom)
		return true;

	// Make sure the device is sane.
	if (!mpManager->CheckDevice())
		return false;

	// Check if we need to create or resize the swap chain.
	if (!mbFullScreen) {
		const D3DDISPLAYMODE& dm = mpManager->GetDisplayMode();
		const int dw = std::min<int>(rClient.right, dm.Width);
		const int dh = std::min<int>(rClient.bottom, dm.Height);

		if (mpManager->GetDeviceEx()) {
			if (mSwapChainW != dw || mSwapChainH != dh) {
				mpSwapChain = NULL;
				mSwapChainW = 0;
				mSwapChainH = 0;
			}

			if (!mpSwapChain || mSwapChainW != dw || mSwapChainH != dh) {
				int scw = dw;
				int sch = dh;

				VDDEBUG("Resizing swap chain to %dx%d\n", scw, sch);

				if (!mpManager->CreateSwapChain(mhwnd, scw, sch, mbClipToMonitor, ~mpSwapChain))
					return false;

				mSwapChainW = scw;
				mSwapChainH = sch;
			}
		} else {
			if (mSwapChainW >= dw + 128 || mSwapChainH >= dh + 128) {
				mpSwapChain = NULL;
				mSwapChainW = 0;
				mSwapChainH = 0;
			}

			if ((!mpSwapChain || mSwapChainW < dw || mSwapChainH < dh)) {
				int scw = std::min<int>((dw + 127) & ~127, dm.Width);
				int sch = std::min<int>((dh + 127) & ~127, dm.Height);

				VDDEBUG("Resizing swap chain to %dx%d\n", scw, sch);

				if (!mpManager->CreateSwapChain(mhwnd, scw, sch, mbClipToMonitor, ~mpSwapChain))
					return false;

				mSwapChainW = scw;
				mSwapChainH = sch;
			}
		}
	}

	int rtw;
	int rth;

	if (mbFullScreen) {
		rClient.right  = rtw = mpManager->GetMainRTWidth();
		rClient.bottom = rth = mpManager->GetMainRTHeight();
	} else {
		rtw = mSwapChainW;
		rth = mSwapChainH;
	}

	RECT rClippedClient={0,0,std::min<int>(rClient.right, rtw), std::min<int>(rClient.bottom, rth)};

	vdsize32 vpsize(rClippedClient.right, rClippedClient.bottom);
	CreateCustomTextures(false, &vpsize);

	IDirect3DTexture9 *tex = mpUploadContext->GetD3DTexture(0);
	D3DSURFACE_DESC texdesc;
	HRESULT hr = tex->GetLevelDesc(0, &texdesc);
	if (FAILED(hr))
		return false;

	// Do we need to switch bicubic modes?
	FilterMode mode = mPreferredFilter;

	if (mode == kFilterAnySuitable)
		mode = kFilterBicubic;

	// begin rendering
//	mLatencyFence = mLatencyFenceNext;
//	mLatencyFenceNext = mpManager->InsertFence();
//	mLatencyFence = mpManager->InsertFence();

	D3D_AUTOBREAK(SetRenderState(D3DRS_COLORWRITEENABLE, 15));
	D3D_AUTOBREAK(SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE));

	if (mColorOverride) {
		mpManager->SetSwapChainActive(mpSwapChain);

		D3DRECT rClear;
		rClear.x1 = rClient.left;
		rClear.y1 = rClient.top;
		rClear.x2 = rClient.right;
		rClear.y2 = rClient.bottom;

		HRESULT hr = mpD3DDevice->Clear(1, &rClear, D3DCLEAR_TARGET, mColorOverride, 0.0f, 0);
		mpManager->SetSwapChainActive(NULL);

		if (FAILED(hr))
			return false;
	} else {
		D3D_AUTOBREAK(SetStreamSource(0, mpManager->GetVertexBuffer(), 0, sizeof(Vertex)));
		D3D_AUTOBREAK(SetIndices(mpManager->GetIndexBuffer()));
		D3D_AUTOBREAK(SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2));
		D3D_AUTOBREAK(SetRenderState(D3DRS_LIGHTING, FALSE));
		D3D_AUTOBREAK(SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
		D3D_AUTOBREAK(SetRenderState(D3DRS_ZENABLE, FALSE));
		D3D_AUTOBREAK(SetRenderState(D3DRS_ALPHATESTENABLE, FALSE));
		D3D_AUTOBREAK(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
		D3D_AUTOBREAK(SetRenderState(D3DRS_STENCILENABLE, FALSE));
		D3D_AUTOBREAK(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));

		D3DRECT rects[4];
		D3DRECT *nextRect = rects;
		RECT rDest = rClippedClient;

		if (mbDestRectEnabled) {
			// clip client rect to dest rect
			if (rDest.left < mDestRect.left)
				rDest.left = mDestRect.left;

			if (rDest.top < mDestRect.top)
				rDest.top = mDestRect.top;

			if (rDest.right > mDestRect.right)
				rDest.right = mDestRect.right;

			if (rDest.bottom > mDestRect.bottom)
				rDest.bottom = mDestRect.bottom;

			// fix rect in case dest rect lies entirely outside of client rect
			if (rDest.left > rClippedClient.right)
				rDest.left = rClippedClient.right;

			if (rDest.top > rClippedClient.bottom)
				rDest.top = rClippedClient.bottom;

			if (rDest.right < rDest.left)
				rDest.right = rDest.left;

			if (rDest.bottom < rDest.top)
				rDest.bottom = rDest.top;
		}

		if (rDest.right <= rDest.left || rDest.bottom <= rDest.top) {
			mpManager->SetSwapChainActive(mpSwapChain);

			D3DRECT r;
			r.x1 = rClippedClient.left;
			r.y1 = rClippedClient.top;
			r.x2 = rClippedClient.right;
			r.y2 = rClippedClient.bottom;

			HRESULT hr = mpD3DDevice->Clear(1, &r, D3DCLEAR_TARGET, mBackgroundColor, 0.0f, 0);
			if (FAILED(hr))
				return false;
		} else {
			if (rDest.top > rClippedClient.top) {
				nextRect->x1 = rClippedClient.left;
				nextRect->y1 = rClippedClient.top;
				nextRect->x2 = rClippedClient.right;
				nextRect->y2 = rDest.top;
				++nextRect;
			}

			if (rDest.left > rClippedClient.left) {
				nextRect->x1 = rClippedClient.left;
				nextRect->y1 = rDest.top;
				nextRect->x2 = rDest.left;
				nextRect->y2 = rDest.bottom;
				++nextRect;
			}

			if (rDest.right < rClippedClient.right) {
				nextRect->x1 = rDest.right;
				nextRect->y1 = rDest.top;
				nextRect->x2 = rClippedClient.right;
				nextRect->y2 = rDest.bottom;
				++nextRect;
			}

			if (rDest.bottom < rClippedClient.bottom) {
				nextRect->x1 = rClippedClient.left;
				nextRect->y1 = rDest.bottom;
				nextRect->x2 = rClippedClient.right;
				nextRect->y2 = rClippedClient.bottom;
				++nextRect;
			}

			HRESULT hr;
			if (nextRect > rects) {
				mpManager->SetSwapChainActive(mpSwapChain);

				hr = mpD3DDevice->Clear(nextRect - rects, rects, D3DCLEAR_TARGET, mBackgroundColor, 0.0f, 0);
				if (FAILED(hr))
					return false;
			}

			vdrefptr<IDirect3DSurface9> pRTMain;

			if (mpSwapChain) {
				IDirect3DSwapChain9 *sc = mpSwapChain->GetD3DSwapChain();
				hr = sc->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, ~pRTMain);
				if (FAILED(hr))
					return false;
			} else {
				mpManager->SetSwapChainActive(NULL);
				mpD3DDevice->GetRenderTarget(0, ~pRTMain);
			}

			mbSwapChainImageValid = false;

			D3D_AUTOBREAK(SetRenderTarget(0, pRTMain));

			if (!mpManager->BeginScene())
				goto d3d_failed;

			D3DVIEWPORT9 vp = {
				rDest.left,
				rDest.top,
				rDest.right - rDest.left,
				rDest.bottom - rDest.top,
				0.f,
				1.f
			};

			D3D_AUTOBREAK(SetViewport(&vp));

			// fill vertex/index buffers
			if (Vertex *pvx = mpManager->LockVertices(4)) {
				float umax = (float)mSource.pixmap.w / (float)(int)texdesc.Width;
				float vmax = (float)mSource.pixmap.h / (float)(int)texdesc.Height;
				float x0 = -1.f - 1.f/rClippedClient.right;
				float x1 = x0 + 2.0f*(rClient.right / rClippedClient.right);
				float y0 = 1.f + 1.f/rClippedClient.bottom;
				float y1 = y0 - 2.0f*(rClient.bottom / rClippedClient.bottom);

				pvx[0].SetFF2(x0, y0, 0, 0, 0, 0, 0);
				pvx[1].SetFF2(x0, y1, 0, 0, vmax, 0, 1);
				pvx[2].SetFF2(x1, y0, 0, umax, 0, 1, 0);
				pvx[3].SetFF2(x1, y1, 0, umax, vmax, 1, 1);

				mpManager->UnlockVertices();
			}

			if (uint16 *dst = mpManager->LockIndices(6)) {
				dst[0] = 0;
				dst[1] = 1;
				dst[2] = 2;
				dst[3] = 2;
				dst[4] = 1;
				dst[5] = 3;

				mpManager->UnlockIndices();
			}

			UpdateEffectParameters(vp, texdesc);

			UINT passes;
			D3DXHANDLE hTechnique = mhTechniques[mode - 1];
			D3D_AUTOBREAK_2(mpEffect->SetTechnique(hTechnique));
			D3D_AUTOBREAK_2(mpEffect->Begin(&passes, 0));

			IDirect3DSurface9 *pLastRT = pRTMain;

			for(UINT pass=0; pass<passes; ++pass) {
				D3DXHANDLE hPass = mpEffect->GetPass(hTechnique, pass);

				D3DXHANDLE hFieldAnno = mpEffect->GetAnnotationByName(hPass, "vd_fieldmask");
				if (hFieldAnno) {
					INT fieldMask;

					if (SUCCEEDED(mpEffect->GetInt(hFieldAnno, &fieldMask))) {
						const uint32 fieldMode = updateMode & kModeFieldMask;

						if (!(fieldMode & fieldMask))
							continue;
					}
				}

				vdrefptr<IDirect3DSurface9> pNewRTRef;
				IDirect3DSurface9 *pNewRT = NULL;

				if (D3DXHANDLE hTarget = mpEffect->GetAnnotationByName(hPass, "vd_target")) {
					const char *s;

					if (SUCCEEDED(mpEffect->GetString(hTarget, &s)) && s) {
						if (!strcmp(s, "temp"))
							pNewRT = mpD3DTempSurface;
						else if (!strcmp(s, "temp2"))
							pNewRT = mpD3DTempSurface2;
						else if (!*s)
							pNewRT = pRTMain;
						else {
							D3DXHANDLE hTextureParam = mpEffect->GetParameterByName(NULL, s);
							if (SUCCEEDED(hr)) {
								vdrefptr<IDirect3DBaseTexture9> pBaseTex;
								hr = mpEffect->GetTexture(hTextureParam, ~pBaseTex);
								if (SUCCEEDED(hr) && pBaseTex) {
									switch(pBaseTex->GetType()) {
										case D3DRTYPE_TEXTURE:
											{
												vdrefptr<IDirect3DTexture9> pTex;
												hr = pBaseTex->QueryInterface(IID_IDirect3DTexture9, (void **)~pTex);
												if (SUCCEEDED(hr))
													hr = pTex->GetSurfaceLevel(0, ~pNewRTRef);
											}
											break;
										case D3DRTYPE_CUBETEXTURE:
											{
												vdrefptr<IDirect3DCubeTexture9> pTex;
												hr = pBaseTex->QueryInterface(IID_IDirect3DCubeTexture9, (void **)~pTex);
												if (SUCCEEDED(hr))
													hr = pTex->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, ~pNewRTRef);
											}
											break;
									}

									pNewRT = pNewRTRef;
								}
							}
						}
					}
				}

				if (pNewRT && pLastRT != pNewRT) {
					pLastRT = pNewRT;

					D3D_AUTOBREAK(SetRenderTarget(0, pNewRT));

					if (pNewRT == pRTMain)
						D3D_AUTOBREAK(SetViewport(&vp));
				}

				if (D3DXHANDLE hClear = mpEffect->GetAnnotationByName(hPass, "vd_clear")) {
					float clearColor[4];

					if (SUCCEEDED(mpEffect->GetVector(hClear, (D3DXVECTOR4 *)clearColor))) {
						int r = VDRoundToInt(clearColor[0]);
						int g = VDRoundToInt(clearColor[1]);
						int b = VDRoundToInt(clearColor[2]);
						int a = VDRoundToInt(clearColor[3]);

						if ((unsigned)r >= 256) r = (~r >> 31) & 255;
						if ((unsigned)g >= 256) g = (~g >> 31) & 255;
						if ((unsigned)b >= 256) b = (~b >> 31) & 255;
						if ((unsigned)a >= 256) a = (~a >> 31) & 255;

						D3DCOLOR clearColor = (a<<24) + (r<<16) + (g<<8) + b;

						D3D_AUTOBREAK(Clear(0, NULL, D3DCLEAR_TARGET, clearColor, 1.f, 0));
					}
				}

				D3D_AUTOBREAK_2(mpEffect->BeginPass(pass));

				mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

				D3D_AUTOBREAK_2(mpEffect->EndPass());
			}

			D3D_AUTOBREAK_2(mpEffect->End());

			if (mbDisplayDebugInfo && mpFontRenderer) {
				if (!(++mTimingStringCounter & 63)) {
					mTimingString.sprintf("Longest present: %4.2fms  Longest frame: %4.2fms", mLastLongestPresentTime * 1000.0f, mLastLongestFrameTime * 1000.0f);
					mLastLongestPresentTime = 0.0f;
					mLastLongestFrameTime = 0.0f;
				}

				if (mpFontRenderer->Begin()) {
					VDStringA s;

					VDStringA desc;
					GetFormatString(mSource, desc);
					s.sprintf("D3DFX minidriver - %s", desc.c_str());
					mpFontRenderer->DrawTextLine(10, rClient.bottom - 40, 0xFFFFFF00, 0, s.c_str());

					mpFontRenderer->DrawTextLine(10, rClient.bottom - 20, 0xFFFFFF00, 0, mTimingString.c_str());
					mpFontRenderer->End();
				}
			}

			if (!mpManager->EndScene())
				goto d3d_failed;
		}	
	}

	mpManager->Flush();
	mpManager->SetSwapChainActive(NULL);
	mbSwapChainImageValid = true;
	mbSwapChainPresentPending = true;
	mbSwapChainPresentPolling = false;
//	mLatencyFence = mLatencyFenceNext;
//	mLatencyFenceNext = mpManager->InsertFence();

d3d_failed:
	return true;
}

bool VDVideoDisplayMinidriverD3DFX::UpdateScreen(const RECT& rClient, UpdateMode updateMode, bool polling) {
	if (!mbSwapChainImageValid || (!mbFullScreen && !mpSwapChain))
		return false;

	if (polling) {
		if (mLatencyFence) {
			if (!mpManager->IsFencePassed(mLatencyFence))
				return true;

			mLatencyFence = 0;
		}
	}

	HRESULT hr;
	if (mbFullScreen) {
		uint64 tstart = VDGetPreciseTick();
		hr = mpManager->PresentFullScreen(!polling);

		float tdelta = (float)((VDGetPreciseTick() - tstart) * VDGetPreciseSecondsPerTick());

		if (tdelta > mLastLongestPresentTime)
			mLastLongestPresentTime = tdelta;
	} else {
		hr = mpManager->PresentSwapChain(mpSwapChain, &rClient, mhwnd, (updateMode & kModeVSync) != 0, !polling || !mbSwapChainPresentPolling, polling, mSyncDelta, mPresentHistory);
		mbSwapChainPresentPolling = true;
	}

	bool dec = false;
	if (hr == S_FALSE)
		return true;
	else if (hr == S_OK) {
		uint64 curTime = VDGetPreciseTick();
		float ft = (float)((curTime - mLastFrameTime) * VDGetPreciseSecondsPerTick());
		if (mLastLongestFrameTime < ft)
			mLastLongestFrameTime = ft;
		mLastFrameTime = curTime;
	}

	if (mbSwapChainPresentPending) {
		mbSwapChainPresentPending = false;
		mbSwapChainPresentPolling = false;
		dec = true;
	}

	mLatencyFence = mLatencyFenceNext;
	mLatencyFenceNext = 0;

	// Workaround for Windows Vista DWM composition chain not updating.
	if (!mbFullScreen && mbFirstPresent) {
		SetWindowPos(mhwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
		mbFirstPresent = false;
	}

	if (FAILED(hr)) {
		VDDEBUG("VideoDisplay/D3DFX: Render failed -- applying boot to the head.\n");

		if (!mpManager->Reset())
			return false;
	} else if (dec) {
		mSource.mpCB->RequestNextFrame();
	}

	return true;
}

void VDVideoDisplayMinidriverD3DFX::DisplayError() {
	if (mhwndError) {
		DestroyWindow(mhwndError);
		mhwndError = NULL;
	}

	HINSTANCE hInst = VDGetLocalModuleHandleW32();
	if (VDIsWindowsNT())
		mhwndError = CreateWindowW(L"EDIT", mError.c_str(), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0, 0, mrClient.right, mrClient.bottom, mhwnd, NULL, hInst, NULL);
	else
		mhwndError = CreateWindowA("EDIT", VDTextWToA(mError).c_str(), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0, 0, mrClient.right, mrClient.bottom, mhwnd, NULL, hInst, NULL);

	if (mhwndError)
		SendMessage(mhwndError, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
}

void VDVideoDisplayMinidriverD3DFX::PreprocessEffectParameters() {
	mParamBindings.clear();

	D3DXEFFECT_DESC effdesc;
	HRESULT hr = mpEffect->GetDesc(&effdesc);

	ParamBinding binding={};
	if (VDINLINEASSERT(SUCCEEDED(hr))) {
		for(UINT parmidx = 0; parmidx < effdesc.Parameters; ++parmidx) {
			D3DXHANDLE hparm = mpEffect->GetParameter(NULL, parmidx);
			if (!hparm)
				continue;

			D3DXPARAMETER_DESC parmdesc;
			hr = mpEffect->GetParameterDesc(hparm, &parmdesc);
			if (VDINLINEASSERTFALSE(FAILED(hr)))
				continue;

			static const struct ParamInfo {
				D3DXPARAMETER_CLASS	mClass;
				D3DXPARAMETER_TYPE	mType;
				const char			*mpSemantic;
				ParamUploadMethod	mpMethod;
			} kParamInfo[]={
				{ D3DXPC_VECTOR, D3DXPT_FLOAT,	"RenderTargetDimensions",	&VDVideoDisplayMinidriverD3DFX::UpdateParam_RenderTargetDimensions },
				{ D3DXPC_VECTOR, D3DXPT_FLOAT,	"ViewportPixelSize",		&VDVideoDisplayMinidriverD3DFX::UpdateParam_ViewportPixelSize }
			};

			binding.mpFn = NULL;
			for(int i=0; i<sizeof(kParamInfo)/sizeof(kParamInfo[0]); ++i) {
				const ParamInfo& pi = kParamInfo[i];

				if (parmdesc.Class == pi.mClass && parmdesc.Type == pi.mType && parmdesc.Semantic && !_stricmp(parmdesc.Semantic, pi.mpSemantic)) {
					binding.mpFn = pi.mpMethod;
					break;
				}
			}

			if (binding.mpFn) {
				binding.mhParam = hparm;
				mParamBindings.push_back(binding);
			}
		}
	}
}

void VDVideoDisplayMinidriverD3DFX::UpdateParam_RenderTargetDimensions(const ParamBinding& binding) {
	vdrefptr<IDirect3DSurface9> rt;
	HRESULT hr = mpD3DDevice->GetRenderTarget(0, ~rt);
	if (SUCCEEDED(hr)) {
		D3DSURFACE_DESC surfdesc;
		hr = rt->GetDesc(&surfdesc);
		if (SUCCEEDED(hr)) {
			D3DXVECTOR4 v4;
			v4.x = (float)surfdesc.Width;
			v4.y = (float)surfdesc.Height;
			v4.z = 0.0f;
			v4.w = 1.0f;

			mpEffect->SetVector(binding.mhParam, &v4);
		}
	}
}

void VDVideoDisplayMinidriverD3DFX::UpdateParam_ViewportPixelSize(const ParamBinding& binding) {
	D3DVIEWPORT9 vp;
	HRESULT hr = mpD3DDevice->GetViewport(&vp);

	if (SUCCEEDED(hr)) {
		D3DXVECTOR4 v4;
		v4.x = (float)vp.Width;
		v4.y = (float)vp.Height;
		v4.z = 0.0f;
		v4.w = 1.0f;
	}
}

void VDVideoDisplayMinidriverD3DFX::UpdateEffectParameters(const D3DVIEWPORT9& vp, const D3DSURFACE_DESC& texdesc) {
	// render the effect
	StdParamData data;

	data.vpsize[0] = (float)vp.Width;
	data.vpsize[1] = (float)vp.Height;
	data.vpsize[2] = 1.0f / (float)vp.Height;
	data.vpsize[3] = 1.0f / (float)vp.Width;
	data.texsize[0] = (float)(int)texdesc.Width;
	data.texsize[1] = (float)(int)texdesc.Height;
	data.texsize[2] = 1.0f / (float)(int)texdesc.Height;
	data.texsize[3] = 1.0f / (float)(int)texdesc.Width;
	data.srcsize[0] = (float)mSource.pixmap.w;
	data.srcsize[1] = (float)mSource.pixmap.h;
	data.srcsize[2] = 1.0f / (float)mSource.pixmap.h;
	data.srcsize[3] = 1.0f / (float)mSource.pixmap.w;
	data.tempsize[0] = 1.f;
	data.tempsize[1] = 1.f;
	data.tempsize[2] = 1.f;
	data.tempsize[3] = 1.f;
	data.temp2size[0] = 1.f;
	data.temp2size[1] = 1.f;
	data.temp2size[2] = 1.f;
	data.temp2size[3] = 1.f;
	data.vpcorrect[0] = 2.0f / vp.Width;
	data.vpcorrect[1] = 2.0f / vp.Height;
	data.vpcorrect[2] = -1.0f / (float)vp.Height;
	data.vpcorrect[3] = 1.0f / (float)vp.Width;
	data.vpcorrect2[0] = 2.0f / vp.Width;
	data.vpcorrect2[1] = -2.0f / vp.Height;
	data.vpcorrect2[2] = 1.0f + 1.0f / (float)vp.Height;
	data.vpcorrect2[3] = -1.0f - 1.0f / (float)vp.Width;
	data.tvpcorrect[0] = 2.0f;
	data.tvpcorrect[1] = 2.0f;
	data.tvpcorrect[2] = -1.0f;
	data.tvpcorrect[3] = 1.0f;
	data.tvpcorrect2[0] = 2.0f;
	data.tvpcorrect2[1] = -2.0f;
	data.tvpcorrect2[2] = 0.f;
	data.tvpcorrect2[3] = 2.0f;
	data.t2vpcorrect[0] = 2.0f;
	data.t2vpcorrect[1] = 2.0f;
	data.t2vpcorrect[2] = -1.0f;
	data.t2vpcorrect[3] = 1.0f;
	data.t2vpcorrect2[0] = 2.0f;
	data.t2vpcorrect2[1] = -2.0f;
	data.t2vpcorrect2[2] = 0.f;
	data.t2vpcorrect2[3] = 2.0f;
	data.time[0] = (GetTickCount() % 30000) / 30000.0f;
	data.time[1] = 1;
	data.time[2] = 2;
	data.time[3] = 3;

	if (mhSrcTexture)
		mpEffect->SetTexture(mhSrcTexture, mpUploadContext->GetD3DTexture(0));

	if (mhPrevSrcTexture)
		mpEffect->SetTexture(mhPrevSrcTexture, mpUploadContext->GetD3DTexture(1));

	if (mhPrevSrc2Texture)
		mpEffect->SetTexture(mhPrevSrc2Texture, mpUploadContext->GetD3DTexture(2));

	if (mhTempTexture) {
		mpEffect->SetTexture(mhTempTexture, mpD3DTempTexture);

		float tempw = (float)mpTempTexture->GetWidth();
		float temph = (float)mpTempTexture->GetHeight();

		data.tempsize[0] = tempw;
		data.tempsize[1] = temph;
		data.tempsize[2] = 1.0f / temph;
		data.tempsize[3] = 1.0f / tempw;
		data.tvpcorrect[0] = 2.0f * data.tempsize[3];
		data.tvpcorrect[1] = 2.0f * data.tempsize[2];
		data.tvpcorrect[2] = -data.tempsize[2];
		data.tvpcorrect[3] = data.tempsize[3];
		data.tvpcorrect2[0] = 2.0f * data.tempsize[3];
		data.tvpcorrect2[1] = -2.0f * data.tempsize[2];
		data.tvpcorrect2[2] = 1.0f + data.tempsize[2];
		data.tvpcorrect2[3] = -1.0f - data.tempsize[3];
	}

	if (mhTempTexture2) {
		mpEffect->SetTexture(mhTempTexture2, mpD3DTempTexture2);

		float temp2w = (float)mpTempTexture2->GetWidth();
		float temp2h = (float)mpTempTexture2->GetHeight();

		data.temp2size[0] = temp2w;
		data.temp2size[1] = temp2h;
		data.temp2size[2] = 1.0f / temp2h;
		data.temp2size[3] = 1.0f / temp2w;
		data.t2vpcorrect[0] = 2.0f * data.temp2size[3];
		data.t2vpcorrect[1] = 2.0f * data.temp2size[2];
		data.t2vpcorrect[2] = -data.temp2size[2];
		data.t2vpcorrect[3] = data.temp2size[3];
		data.t2vpcorrect2[0] = 2.0f * data.temp2size[3];
		data.t2vpcorrect2[1] = -2.0f * data.temp2size[2];
		data.t2vpcorrect2[2] = 1.0f + data.temp2size[2];
		data.t2vpcorrect2[3] = -1.0f - data.temp2size[3];
	}

	for(int i=0; i<kStdParamCount; ++i) {
		D3DXHANDLE h = mhStdParamHandles[i];

		if (h)
			mpEffect->SetVector(h, (const D3DXVECTOR4 *)((const char *)&data + kStdParamInfo[i].offset));
	}

	ParamBindings::const_iterator it(mParamBindings.begin()), itEnd(mParamBindings.end());
	for(; it!=itEnd; ++it) {
		const ParamBinding& binding = *it;

		(this->*binding.mpFn)(binding);
	}
}

void VDVideoDisplayMinidriverD3DFX::CreateCustomTextureBindings() {
	mParamBindings.clear();

	D3DXEFFECT_DESC effdesc;
	HRESULT hr = mpEffect->GetDesc(&effdesc);

	TextureBinding binding = {};
	VDStringA	sizename;
	if (VDINLINEASSERT(SUCCEEDED(hr))) {
		for(UINT parmidx = 0; parmidx < effdesc.Parameters; ++parmidx) {
			D3DXHANDLE hparm = mpEffect->GetParameter(NULL, parmidx);
			if (!hparm)
				continue;

			D3DXPARAMETER_DESC parmdesc;
			hr = mpEffect->GetParameterDesc(hparm, &parmdesc);
			if (VDINLINEASSERTFALSE(FAILED(hr)))
				continue;

			switch(parmdesc.Type) {
				case D3DXPT_TEXTURE:
				case D3DXPT_TEXTURE1D:
				case D3DXPT_TEXTURE2D:
					binding.mResType = D3DRTYPE_TEXTURE;
					break;
				case D3DXPT_TEXTURE3D:
					binding.mResType = D3DRTYPE_VOLUMETEXTURE;
					break;
				case D3DXPT_TEXTURECUBE:
					binding.mResType = D3DRTYPE_CUBETEXTURE;
					break;
				default:
					continue;
			}

			D3DXHANDLE hAnnoResourceType	= mpEffect->GetAnnotationByName(hparm, "ResourceType");
			D3DXHANDLE hAnnoDimensions		= mpEffect->GetAnnotationByName(hparm, "Dimensions");
			D3DXHANDLE hAnnoFormat			= mpEffect->GetAnnotationByName(hparm, "Format");
			D3DXHANDLE hAnnoFunction		= mpEffect->GetAnnotationByName(hparm, "Function");
			D3DXHANDLE hAnnoMIPLevels		= mpEffect->GetAnnotationByName(hparm, "MIPLevels");
			D3DXHANDLE hAnnoViewportRatio	= mpEffect->GetAnnotationByName(hparm, "ViewportRatio");

			D3DXHANDLE hAnnoWidth			= mpEffect->GetAnnotationByName(hparm, "width");
			D3DXHANDLE hAnnoHeight			= mpEffect->GetAnnotationByName(hparm, "height");

			if (!hAnnoFormat)
				hAnnoFormat = mpEffect->GetAnnotationByName(hparm, "format");

			if (!hAnnoFunction)
				hAnnoFunction = mpEffect->GetAnnotationByName(hparm, "function");

			binding.mpTexture		= NULL;
			binding.mpTextureShader	= NULL;
			binding.mhParam			= hparm;
			binding.mFormat			= D3DFMT_A8R8G8B8;
			binding.mPool			= D3DPOOL_MANAGED;
			binding.mUsage			= 0;
			binding.mWidth			= 64;
			binding.mHeight			= 64;
			binding.mDepth			= 1;
			binding.mMipLevels		= 1;
			binding.mViewportRatioW	= 0;
			binding.mViewportRatioH	= 0;

			if (parmdesc.Semantic) {
				if (!_stricmp(parmdesc.Semantic, "RenderColorTarget")) {
					binding.mUsage	= D3DUSAGE_RENDERTARGET;
					binding.mPool	= D3DPOOL_DEFAULT;
				} else if (!_stricmp(parmdesc.Semantic, "RenderDepthStencilTarget")) {
					binding.mUsage	= D3DUSAGE_DEPTHSTENCIL;
					binding.mPool	= D3DPOOL_DEFAULT;
				}
			}

			if (hAnnoResourceType) {
				LPCSTR s;

				hr = mpEffect->GetString(hAnnoResourceType, &s);
				if (SUCCEEDED(hr)) {
					if (!_stricmp(s, "1D"))
						binding.mResType = D3DRTYPE_TEXTURE;
					else if (!_stricmp(s, "2D"))
						binding.mResType = D3DRTYPE_TEXTURE;
					else if (!_stricmp(s, "3D"))
						binding.mResType = D3DRTYPE_VOLUMETEXTURE;
					else if (!_stricmp(s, "cube"))
						binding.mResType = D3DRTYPE_CUBETEXTURE;
					else
						throw MyError("Texture parameter %s has an unrecognizable resource type: %s", parmdesc.Name, s);
				}
			}

			if (hAnnoDimensions) {
				D3DXVECTOR4 v4dim;

				hr = mpEffect->GetVector(hAnnoDimensions, &v4dim);
				if (SUCCEEDED(hr)) {
					binding.mWidth	= VDRoundToInt(v4dim.x);
					binding.mHeight	= VDRoundToInt(v4dim.y);
					binding.mDepth	= VDRoundToInt(v4dim.z);
				}
			} else if (hAnnoWidth && hAnnoHeight) {
				int w;
				int h;
				hr = mpEffect->GetInt(hAnnoWidth, &w);
				if (SUCCEEDED(hr)) {
					hr = mpEffect->GetInt(hAnnoHeight, &h);
					if (SUCCEEDED(hr)) {
						binding.mWidth = w;
						binding.mHeight = h;
					}
				}
			}

			if (hAnnoFormat) {
				LPCSTR s;

				hr = mpEffect->GetString(hAnnoFormat, &s);
				if (SUCCEEDED(hr)) {
					for(int fidx=0; fidx<sizeof(kFormats)/sizeof(kFormats[0]); ++fidx) {
						if (!_stricmp(s, kFormats[fidx].mpName)) {
							binding.mFormat = kFormats[fidx].mFormat;
							break;
						}
					}
				}
			}

			if (hAnnoFunction) {
				LPCSTR name;

				hr = mpEffect->GetString(hAnnoFunction, &name);
				if (SUCCEEDED(hr)) {
					const char *profile = "tx_1_0";

					D3DXHANDLE hAnno;
					if (hAnno = mpEffect->GetAnnotationByName(hparm, "target"))
						mpEffect->GetString(hAnno, &profile);

					// check that the function exists (CompileShader just gives us the dreaded INVALIDCALL
					// error in this case)
		#if 0
					if (!mpEffect->GetFunctionByName(pName)) {
						mError.sprintf(L"Couldn't create procedural texture '%hs' in effect file %ls:\nUnknown function '%hs'", parmDesc.Name, srcfile.c_str(), pName);
						if (pError)
							pError->Release();
						ShutdownEffect();
						return true;
					}
		#endif

					// attempt to compile the texture shader
					vdrefptr<ID3DXBuffer> pEffectBuffer;
					vdrefptr<ID3DXBuffer> pError;
					hr = mpEffectCompiler->CompileShader(name, profile, 0, ~pEffectBuffer, ~pError, NULL);
					if (FAILED(hr))
						throw MyError("Couldn't compile texture shader '%hs':\n%hs", name, pError ? (const char *)pError->GetBufferPointer() : "unknown error");

					// create the texture shader
					hr = mpD3DXCreateTextureShader((const DWORD *)pEffectBuffer->GetBufferPointer(), &binding.mpTextureShader);
					if (FAILED(hr))
						throw MyError("Couldn't create texture shader '%hs':\n%hs", name, pError ? (const char *)pError->GetBufferPointer() : "unknown error");
				}
			}

			if (hAnnoMIPLevels) {
				INT mipcount;

				hr = mpEffect->GetInt(hAnnoMIPLevels, &mipcount);
				if (SUCCEEDED(hr)) {
					binding.mMipLevels = mipcount;
				}
			}

			if (hAnnoViewportRatio) {
				D3DXVECTOR4 v4ratio;

				hr = mpEffect->GetVector(hAnnoViewportRatio, &v4ratio);
				if (SUCCEEDED(hr)) {
					binding.mViewportRatioW = v4ratio.x;
					binding.mViewportRatioH = v4ratio.y;
				}
			}

			binding.mhSizeParam = NULL;

			sizename = parmdesc.Name;
			sizename += "_size";

			D3DXHANDLE hsizeparm = mpEffect->GetParameterByName(NULL, sizename.c_str());
			if (hsizeparm) {
				D3DXPARAMETER_DESC sizedesc;
				hr = mpEffect->GetParameterDesc(hsizeparm, &sizedesc);
				if (SUCCEEDED(hr)) {
					if (sizedesc.Class == D3DXPC_VECTOR && sizedesc.Type == D3DXPT_FLOAT)
						binding.mhSizeParam = hsizeparm;
				}
			}

			mTextureBindings.push_back(binding);
		}
	}	
}

bool VDVideoDisplayMinidriverD3DFX::CreateCustomTextures(bool vramOnly, const vdsize32 *vpsize) {
	TextureBindings::iterator it(mTextureBindings.begin()), itEnd(mTextureBindings.end());
	bool success = true;

	for(; it!=itEnd; ++it) {
		TextureBinding& binding = *it;

		if (binding.mPool == D3DPOOL_DEFAULT || !vramOnly) {
			HRESULT hr = E_FAIL;

			if (binding.mViewportRatioW > 0 || binding.mViewportRatioH > 0) {
				if (!vpsize)
					continue;

				int w = binding.mWidth;
				int h = binding.mHeight;

				if (binding.mViewportRatioW > 0)
					w = VDCeilToInt((float)vpsize->w * binding.mViewportRatioW);
				if (binding.mViewportRatioH > 0)
					h = VDCeilToInt((float)vpsize->h * binding.mViewportRatioH);

				if (binding.mpTexture) {
					if (w == binding.mWidth && h == binding.mHeight)
						continue;

					binding.mpTexture->Release();
					binding.mpTexture = NULL;
				}

				binding.mWidth = w;
				binding.mHeight = h;
			} else {
				if (vpsize)
					continue;

				if (binding.mpTexture)
					continue;
			}
			
			switch(binding.mResType) {
			case D3DRTYPE_TEXTURE:
				{
					IDirect3DTexture9 *pTexture;
					hr = mpD3DDevice->CreateTexture(binding.mWidth, binding.mHeight, binding.mMipLevels, binding.mUsage, binding.mFormat, binding.mPool, &pTexture, NULL);
					if (SUCCEEDED(hr)) {
						binding.mpTexture = pTexture;
						if (binding.mpTextureShader) {
							// fill the texture
							hr = mpD3DXFillTextureTX(pTexture, binding.mpTextureShader);

							if (FAILED(hr))
								success = false;
						}
					} else
						success = false;
				}
				break;
			case D3DRTYPE_CUBETEXTURE:
				{
					IDirect3DCubeTexture9 *pCubeTexture;
					hr = mpD3DDevice->CreateCubeTexture(binding.mWidth, binding.mMipLevels, binding.mUsage, binding.mFormat, binding.mPool, &pCubeTexture, NULL);
					if (SUCCEEDED(hr)) {
						binding.mpTexture = pCubeTexture;
						if (binding.mpTextureShader) {
							// fill the texture
							hr = mpD3DXFillCubeTextureTX(pCubeTexture, binding.mpTextureShader);

							if (FAILED(hr))
								success = false;
						}
					} else
						success = false;
				}
				break;
			case D3DRTYPE_VOLUMETEXTURE:
				{
					IDirect3DVolumeTexture9 *pVolumeTexture;
					hr = mpD3DDevice->CreateVolumeTexture(binding.mWidth, binding.mHeight, binding.mDepth, binding.mMipLevels, binding.mUsage, binding.mFormat, binding.mPool, &pVolumeTexture, NULL);
					if (SUCCEEDED(hr)) {
						binding.mpTexture = pVolumeTexture;
						if (binding.mpTextureShader) {
							// fill the texture
							hr = mpD3DXFillVolumeTextureTX(pVolumeTexture, binding.mpTextureShader);

							if (FAILED(hr))
								success = false;
						}
					} else
						success = false;
				}
				break;
			}

			mpEffect->SetTexture(binding.mhParam, binding.mpTexture);

			if (binding.mhSizeParam) {
				D3DXVECTOR4 v4size;
				v4size.x = (float)binding.mWidth;
				v4size.y = (float)binding.mHeight;
				v4size.z = 1.0f / (float)binding.mHeight;
				v4size.w = 1.0f / (float)binding.mWidth;

				mpEffect->SetVector(binding.mhSizeParam, &v4size);
			}
		}
	}

	return success;
}

void VDVideoDisplayMinidriverD3DFX::DestroyCustomTextures(bool vramOnly) {
	TextureBindings::iterator it(mTextureBindings.begin()), itEnd(mTextureBindings.end());
	for(; it!=itEnd; ++it) {
		TextureBinding& binding = *it;

		if (binding.mPool == D3DPOOL_DEFAULT || !vramOnly) {
			mpEffect->SetTexture(binding.mhParam, NULL);

			if (binding.mpTexture) {
				binding.mpTexture->Release();
				binding.mpTexture = NULL;
			}
		}
	}
}

void VDVideoDisplayMinidriverD3DFX::DestroyCustomTextureBindings() {
	while(!mTextureBindings.empty()) {
		TextureBinding& binding = mTextureBindings.back();

		if (binding.mpTextureShader)
			binding.mpTextureShader->Release();

		if (binding.mpTexture)
			binding.mpTexture->Release();

		mTextureBindings.pop_back();
	}
}

///////////////////////////////////////////////////////////////////////////

void VDVideoDisplaySetD3DFXFileName(const wchar_t *fileName) {
	g_VDVideoDisplayD3DFXEffectFileName = fileName;
}

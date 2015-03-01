//	Asuka - VirtualDub Build/Post-Mortem Utility
//	Copyright (C) 2005 Avery Lee
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
#include <vector>
#include <list>
#include <string>
#include <d3d9.h>
#include <d3dx9.h>
#include <objbase.h>
#include <vd2/system/refcount.h>
#include <vd2/system/filesys.h>
#include <vd2/system/math.h>
#include <vd2/system/vdstl.h>

#pragma comment(lib, "d3dx9")

#define NOT_IMPLEMENTED() puts(__FUNCTION__); return E_NOTIMPL
#define NOT_IMPLEMENTED_VOID()

namespace
{
	static const char *const kTextureNames[]={
		"vd_srctexture",
		"vd_src2atexture",
		"vd_src2btexture",
		"vd_src2ctexture",
		"vd_src2dtexture",
		"vd_srcpaltexture",
		"vd_temptexture",
		"vd_temp2texture",
		"vd_cubictexture",
		"vd_hevenoddtexture",
		"vd_dithertexture",
		"vd_interphtexture",
		"vd_interpvtexture",
	};

	static const char *const kParameterNames[]={
		"vd_vpsize",
		"vd_cvpsize",
		"vd_texsize",
		"vd_tex2size",
		"vd_srcsize",
		"vd_tempsize",
		"vd_temp2size",
		"vd_vpcorrect",
		"vd_vpcorrect2",
		"vd_tvpcorrect",
		"vd_tvpcorrect2",
		"vd_t2vpcorrect",
		"vd_t2vpcorrect2",
		"vd_time",
		"vd_interphtexsize",
		"vd_interpvtexsize",
		"vd_fieldinfo",
		"vd_chromauvscale",
		"vd_chromauvoffset",
		"vd_pixelsharpness",
	};
}

namespace
{
	class DummyD3DVertexShader : public IDirect3DVertexShader9 {
	public:
		DummyD3DVertexShader(IDirect3DDevice9 *pDevice, const DWORD *pByteCode) : mRefCount(0), mpDevice(pDevice), mByteCode(pByteCode, pByteCode + (D3DXGetShaderSize(pByteCode) >> 2)) {}

		/*** IUnknown methods ***/
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) {
			if (riid == IID_IDirect3DVertexShader9)
				*ppvObj = static_cast<IDirect3DVertexShader9 *>(this);
			else if (riid == IID_IUnknown)
				*ppvObj = static_cast<IUnknown *>(this);
			else {
				*ppvObj = NULL;
				return E_NOINTERFACE;
			}

			AddRef();
			return S_OK;
		}

		ULONG	STDMETHODCALLTYPE AddRef() {
			return (ULONG)InterlockedIncrement(&mRefCount);
		}
		ULONG	STDMETHODCALLTYPE Release() {
			ULONG rv = (ULONG)InterlockedDecrement(&mRefCount);
			if (!rv)
				delete this;
			return rv;
		}

		/*** IDirect3DVertexShader9 methods ***/
		HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) {
			*ppDevice = mpDevice;
			(*ppDevice)->AddRef();
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE GetFunction(void* p, UINT* pSizeOfData) {
			UINT bytes = (UINT)(mByteCode.size() << 2);

			if (!p)
				*pSizeOfData = bytes;
			else if (*pSizeOfData < bytes)
				return D3DERR_INVALIDCALL;
			else {
				*pSizeOfData = bytes;
				memcpy(p, &mByteCode[0], bytes);
			}

			return S_OK;
		}

	protected:
		volatile LONG mRefCount;
		vdrefptr<IDirect3DDevice9> mpDevice;
		std::vector<DWORD> mByteCode;
	};

	class DummyD3DPixelShader : public IDirect3DPixelShader9 {
	public:
		DummyD3DPixelShader(IDirect3DDevice9 *pDevice, const DWORD *pByteCode) : mRefCount(0), mpDevice(pDevice), mByteCode(pByteCode, pByteCode + (D3DXGetShaderSize(pByteCode) >> 2)) {}

		/*** IUnknown methods ***/
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) {
			if (riid == IID_IDirect3DPixelShader9)
				*ppvObj = static_cast<IDirect3DPixelShader9 *>(this);
			else if (riid == IID_IUnknown)
				*ppvObj = static_cast<IUnknown *>(this);
			else {
				*ppvObj = NULL;
				return E_NOINTERFACE;
			}

			AddRef();
			return S_OK;
		}

		ULONG	STDMETHODCALLTYPE AddRef() {
			return (ULONG)InterlockedIncrement(&mRefCount);
		}
		ULONG	STDMETHODCALLTYPE Release() {
			ULONG rv = (ULONG)InterlockedDecrement(&mRefCount);
			if (!rv)
				delete this;
			return rv;
		}

		/*** IDirect3DPixelShader9 methods ***/
		HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) {
			*ppDevice = mpDevice;
			(*ppDevice)->AddRef();
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE GetFunction(void* p, UINT* pSizeOfData) {
			UINT bytes = (UINT)(mByteCode.size() << 2);

			if (!p)
				*pSizeOfData = bytes;
			else if (*pSizeOfData < bytes)
				return D3DERR_INVALIDCALL;
			else {
				*pSizeOfData = bytes;
				memcpy(p, &mByteCode[0], bytes);
			}

			return S_OK;
		}

	protected:
		volatile LONG mRefCount;
		vdrefptr<IDirect3DDevice9> mpDevice;
		std::vector<DWORD> mByteCode;
	};

	class DummyD3DBaseTexture : public IDirect3DBaseTexture9
	{
	public:
		DummyD3DBaseTexture(IDirect3DDevice9 *pDevice, uint32 id) : mRefCount(0), mpDevice(pDevice), mId(id) {}

		/*** IUnknown methods ***/
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) {
			if (riid == IID_IDirect3DBaseTexture9)
				*ppvObj = static_cast<IDirect3DBaseTexture9 *>(this);
			else if (riid == IID_IDirect3DResource9)
				*ppvObj = static_cast<IDirect3DResource9 *>(this);
			else if (riid == IID_IUnknown)
				*ppvObj = static_cast<IUnknown *>(this);
			else {
				*ppvObj = NULL;
				return E_NOINTERFACE;
			}

			AddRef();
			return S_OK;
		}

		ULONG	STDMETHODCALLTYPE AddRef() {
			return (ULONG)InterlockedIncrement(&mRefCount);
		}
		ULONG	STDMETHODCALLTYPE Release() {
			ULONG rv = (ULONG)InterlockedDecrement(&mRefCount);
			if (!rv)
				delete this;
			return rv;
		}

		/*** IDirect3DResource9 methods ***/
		HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) {
			*ppDevice = mpDevice;
			(*ppDevice)->AddRef();
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid,void* pData,DWORD* pSizeOfData) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) { NOT_IMPLEMENTED(); }
		DWORD	STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) { NOT_IMPLEMENTED_VOID(); return 0; }
		DWORD	STDMETHODCALLTYPE GetPriority() { return 0; }
		void	STDMETHODCALLTYPE PreLoad() {}
		D3DRESOURCETYPE STDMETHODCALLTYPE GetType() {
			return D3DRTYPE_TEXTURE;
		}
		DWORD	STDMETHODCALLTYPE SetLOD(DWORD LODNew) { NOT_IMPLEMENTED_VOID(); return 0; }
		DWORD	STDMETHODCALLTYPE GetLOD() { return 0; }
		DWORD	STDMETHODCALLTYPE GetLevelCount() { return 1; }
		HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) { NOT_IMPLEMENTED(); }
		D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() {
			return D3DTEXF_LINEAR;
		}
		void	STDMETHODCALLTYPE GenerateMipSubLevels() {}

	public:
		uint32 GetId() const { return mId; }

	protected:
		volatile LONG mRefCount;
		vdrefptr<IDirect3DDevice9> mpDevice;
		uint32 mId;
	};

	class DummyD3DDevice : public IDirect3DDevice9 {
	public:
		DummyD3DDevice() : mRefCount(0) {}

		/*** IUnknown methods ***/
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) {
			if (riid == IID_IDirect3DDevice9)
				*ppvObj = static_cast<IDirect3DDevice9 *>(this);
			else if (riid == IID_IUnknown)
				*ppvObj = static_cast<IUnknown *>(this);
			else {
				*ppvObj = NULL;
				return E_NOINTERFACE;
			}

			AddRef();
			return S_OK;
		}

		ULONG	STDMETHODCALLTYPE AddRef() {
			return (ULONG)InterlockedIncrement(&mRefCount);
		}
		ULONG	STDMETHODCALLTYPE Release() {
			ULONG rv = (ULONG)InterlockedDecrement(&mRefCount);
			if (!rv)
				delete this;
			return rv;
		}

		/*** IDirect3DDevice9 methods ***/
		HRESULT STDMETHODCALLTYPE TestCooperativeLevel() { NOT_IMPLEMENTED(); }
		UINT	STDMETHODCALLTYPE GetAvailableTextureMem() { return 16777216; }
		HRESULT STDMETHODCALLTYPE EvictManagedResources() { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9** ppD3D9) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9* pCaps) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT iSwapChain,D3DDISPLAYMODE* pMode) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface9* pCursorBitmap) { NOT_IMPLEMENTED(); }
		void	STDMETHODCALLTYPE SetCursorPosition(int X,int Y,DWORD Flags) {}
		BOOL	STDMETHODCALLTYPE ShowCursor(BOOL bShow) { return TRUE; }
		HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain9** pSwapChain) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetSwapChain(UINT iSwapChain,IDirect3DSwapChain9** pSwapChain) { NOT_IMPLEMENTED(); }
		UINT	STDMETHODCALLTYPE GetNumberOfSwapChains() { return 1; }
		HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iSwapChain,UINT iBackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9** ppBackBuffer) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT iSwapChain,D3DRASTER_STATUS* pRasterStatus) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL bEnableDialogs) { NOT_IMPLEMENTED(); }
		void	STDMETHODCALLTYPE SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp) { }
		void	STDMETHODCALLTYPE GetGammaRamp(UINT iSwapChain,D3DGAMMARAMP* pRamp) { NOT_IMPLEMENTED_VOID(); }
		HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,HANDLE* pSharedHandle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,HANDLE* pSharedHandle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestinationSurface,CONST POINT* pDestPoint) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture9* pSourceTexture,IDirect3DBaseTexture9* pDestinationTexture) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetFrontBufferData(UINT iSwapChain,IDirect3DSurface9* pDestSurface) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DSurface9* pSurface,CONST RECT* pRect,D3DCOLOR color) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE BeginScene() { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE EndScene() { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE,CONST D3DMATRIX*) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetViewport(CONST D3DVIEWPORT9* pViewport) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9* pViewport) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetMaterial(CONST D3DMATERIAL9* pMaterial) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL9* pMaterial) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetLight(DWORD Index,CONST D3DLIGHT9*) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetLight(DWORD Index,D3DLIGHT9*) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index,BOOL Enable) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index,BOOL* pEnable) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index,CONST float* pPlane) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index,float* pPlane) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State,DWORD* pValue) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type,IDirect3DStateBlock9** ppSB) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE BeginStateBlock() { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9** ppSB) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9* pClipStatus) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage,IDirect3DBaseTexture9** ppTexture) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD* pValue) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* pNumPasses) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber,CONST PALETTEENTRY* pEntries) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT *PaletteNumber) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetScissorRect(CONST RECT* pRect) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetScissorRect(RECT* pRect) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL bSoftware) { NOT_IMPLEMENTED(); }
		BOOL	STDMETHODCALLTYPE GetSoftwareVertexProcessing() { return FALSE; }
		HRESULT STDMETHODCALLTYPE SetNPatchMode(float nSegments) { NOT_IMPLEMENTED(); }
		float	STDMETHODCALLTYPE GetNPatchMode() { return 0; }
		HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer9* pDestBuffer,IDirect3DVertexDeclaration9* pVertexDecl,DWORD Flags) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetFVF(DWORD FVF) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetFVF(DWORD* pFVF) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9** ppShader) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(UINT StartRegister,float* pConstantData,UINT Vector4fCount) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(UINT StartRegister,int* pConstantData,UINT Vector4iCount) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT StartRegister,BOOL* pConstantData,UINT BoolCount) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9** ppStreamData,UINT* pOffsetInBytes,UINT* pStride) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT StreamNumber,UINT Setting) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT StreamNumber,UINT* pSetting) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9* pIndexData) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer9** ppIndexData) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9** ppShader) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(UINT StartRegister,int* pConstantData,UINT Vector4iCount) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(UINT StartRegister,float* pConstantData,UINT Vector4fCount) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT StartRegister,BOOL* pConstantData,UINT BoolCount) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle) { NOT_IMPLEMENTED(); }
		HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery) { NOT_IMPLEMENTED(); }

		HRESULT STDMETHODCALLTYPE CreateVertexShader(CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader) {
			*ppShader = new DummyD3DVertexShader(this, pFunction);
			(*ppShader)->AddRef();
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader) {
			*ppShader = new DummyD3DPixelShader(this, pFunction);
			(*ppShader)->AddRef();
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* pShader) { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount) { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount) { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount) { return S_OK; }

		HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9* pShader) { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount) { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount) { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount) { return S_OK; }

		// We need to record these.
		HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage,IDirect3DBaseTexture9* pTexture) {
			if (pTexture && mRequiredCaps.MaxSimultaneousTextures <= Stage)
				mRequiredCaps.MaxSimultaneousTextures = Stage + 1;

			uint32 token = 0x30000000 + (Stage << 12);

			if (pTexture)
				token += static_cast<DummyD3DBaseTexture *>(pTexture)->GetId();

			mAccumulatedStates.push_back(token);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value) {
			if ((Type == D3DTSS_COLOROP || Type == D3DTSS_ALPHAOP) && Value != D3DTOP_DISABLE) {
				if (mRequiredCaps.MaxTextureBlendStages <= Stage)
					mRequiredCaps.MaxTextureBlendStages = Stage + 1;

				switch(Value) {
					case D3DTOP_ADD:						mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_ADD; break;
					case D3DTOP_ADDSIGNED:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_ADDSIGNED; break;
					case D3DTOP_ADDSIGNED2X:				mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_ADDSIGNED2X; break;
					case D3DTOP_ADDSMOOTH:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_ADDSMOOTH; break;
					case D3DTOP_BLENDCURRENTALPHA:			mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_BLENDCURRENTALPHA; break;
					case D3DTOP_BLENDDIFFUSEALPHA:			mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_BLENDDIFFUSEALPHA; break;
					case D3DTOP_BLENDFACTORALPHA:			mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_BLENDFACTORALPHA; break;
					case D3DTOP_BLENDTEXTUREALPHA:			mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_BLENDTEXTUREALPHA; break;
					case D3DTOP_BLENDTEXTUREALPHAPM:		mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_BLENDTEXTUREALPHAPM; break;
					case D3DTOP_BUMPENVMAP:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_BUMPENVMAP; break;
					case D3DTOP_BUMPENVMAPLUMINANCE:		mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_BUMPENVMAPLUMINANCE; break;
					case D3DTOP_DISABLE:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_DISABLE; break;
					case D3DTOP_DOTPRODUCT3:				mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_DOTPRODUCT3; break;
					case D3DTOP_LERP:						mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_LERP; break;
					case D3DTOP_MODULATE:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_MODULATE; break;
					case D3DTOP_MODULATE2X:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_MODULATE2X; break;
					case D3DTOP_MODULATE4X:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_MODULATE4X; break;
					case D3DTOP_MODULATEALPHA_ADDCOLOR:		mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR; break;
					case D3DTOP_MODULATECOLOR_ADDALPHA:		mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA; break;
					case D3DTOP_MODULATEINVALPHA_ADDCOLOR:	mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR; break;
					case D3DTOP_MODULATEINVCOLOR_ADDALPHA:	mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA; break;
					case D3DTOP_MULTIPLYADD:				mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_MULTIPLYADD; break;
					case D3DTOP_PREMODULATE:				mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_PREMODULATE; break;
					case D3DTOP_SELECTARG1:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_SELECTARG1; break;
					case D3DTOP_SELECTARG2:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_SELECTARG2; break;
					case D3DTOP_SUBTRACT:					mRequiredCaps.TextureOpCaps |= D3DTEXOPCAPS_SUBTRACT; break;
				}
			}

			uint32 token = 0x10000000 + (Stage << 24) + (Type << 12);

			if (Value < 0xFFF) {
				mAccumulatedStates.push_back(token + Value);
			} else {
				mAccumulatedStates.push_back(token + 0xFFF);
				mAccumulatedStates.push_back(Value);
			}
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State,DWORD Value) {
			switch(State) {
				case D3DRS_BLENDOP:
					if (Value != D3DBLENDOP_ADD)
						mRequiredCaps.PrimitiveMiscCaps |= D3DPMISCCAPS_BLENDOP;
					break;

				case D3DRS_SRCBLEND:
				case D3DRS_DESTBLEND:
					{
						DWORD blendCap = 0;

						switch(Value) {
							case D3DBLEND_BLENDFACTOR:
								blendCap = D3DPBLENDCAPS_BLENDFACTOR;
								break;
							case D3DBLEND_BOTHINVSRCALPHA:
								blendCap = D3DPBLENDCAPS_BOTHINVSRCALPHA;
								break;
							case D3DBLEND_BOTHSRCALPHA:
								blendCap = D3DPBLENDCAPS_BOTHSRCALPHA;
								break;
							case D3DBLEND_DESTALPHA:
								blendCap = D3DPBLENDCAPS_DESTALPHA;
								break;
							case D3DBLEND_DESTCOLOR:
								blendCap = D3DPBLENDCAPS_DESTCOLOR;
								break;
							case D3DBLEND_INVDESTALPHA:
								blendCap = D3DPBLENDCAPS_INVDESTALPHA;
								break;
							case D3DBLEND_INVDESTCOLOR:
								blendCap = D3DPBLENDCAPS_INVDESTCOLOR;
								break;
							case D3DBLEND_INVSRCALPHA:
								blendCap = D3DPBLENDCAPS_INVSRCALPHA;
								break;
							case D3DBLEND_INVSRCCOLOR:
								blendCap = D3DPBLENDCAPS_INVSRCCOLOR;
								break;
							case D3DBLEND_ONE:
								blendCap = D3DPBLENDCAPS_ONE;
								break;
							case D3DBLEND_SRCALPHA:
								blendCap = D3DPBLENDCAPS_SRCALPHA;
								break;
							case D3DBLEND_SRCALPHASAT:
								blendCap = D3DPBLENDCAPS_SRCALPHASAT;
								break;
							case D3DBLEND_SRCCOLOR:
								blendCap = D3DPBLENDCAPS_SRCCOLOR;
								break;
							case D3DBLEND_ZERO:
								blendCap = D3DPBLENDCAPS_ZERO;
								break;
						}

						if (State == D3DRS_SRCBLEND)
							mRequiredCaps.SrcBlendCaps |= blendCap;
						else
							mRequiredCaps.DestBlendCaps |= blendCap;
					}
					break;
			}

			if (Value < 0xFFF) {
				mAccumulatedStates.push_back((State << 12) + Value);
			} else {
				mAccumulatedStates.push_back((State << 12) + 0xFFF);
				mAccumulatedStates.push_back(Value);
			}
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value) {
			uint32 token = 0x20000000 + (Sampler << 24) + (Type << 12);

			if (Value < 0xFFF) {
				mAccumulatedStates.push_back(token + Value);
			} else {
				mAccumulatedStates.push_back(token + 0xFFF);
				mAccumulatedStates.push_back(Value);
			}
			return S_OK;
		}

	public:
		const uint32 *GetStates() const { return &mAccumulatedStates[0]; }
		int GetStateCount() const { return (int)mAccumulatedStates.size(); }

		void ClearStates() { mAccumulatedStates.clear(); }

		const D3DCAPS9& GetRequiredCaps() const { return mRequiredCaps; }
		void ClearRequiredCaps() {
			memset(&mRequiredCaps, 0, sizeof mRequiredCaps);
		}

	protected:
		volatile LONG mRefCount;

		D3DCAPS9 mRequiredCaps;

		vdfastvector<uint32> mAccumulatedStates;
	};
}

namespace {
	struct PassInfo {
		int mVertexShaderIndex;
		int mPixelShaderIndex;
		int mStateStart;
		int mStateEnd;
		int mRenderTarget;
		uint8 mViewportW;
		uint8 mViewportH;
		uint8 mBumpEnvScale;
		uint8 mTileMode;
		bool mbClipPosition;
		bool mbRTDoClear;
		uint32 mRTClearColor;
	};

	void DeleteShaderConstantTable(std::vector<uint32>& shader) {
		LPCVOID data;
		UINT size;
		if (D3D_OK == D3DXFindShaderComment((const DWORD *)&shader[0], MAKEFOURCC('C', 'T', 'A', 'B'), &data, &size)) {
			ptrdiff_t offset = (char *)data - (char *)&shader[0];

			VDASSERT(!(offset & 3));
			VDASSERT(offset >= 8);

			// convert to dword offset
			offset >>= 2;
			size = (size + 3) >> 2;

			VDASSERT(offset + size <= shader.size());

			// erase comment token, fourcc, and comment data
			shader.erase(shader.begin() + (offset - 2), shader.begin() + offset + size);
		}
	}

	void EmitShaderConstants(vdfastvector<uint32>& states, ID3DXConstantTable *pConstants, bool pixelShader) {
		D3DXCONSTANTTABLE_DESC desc;
		HRESULT hr;
		hr = pConstants->GetDesc(&desc);
		if (FAILED(hr)) {
			printf("ID3DXConstantTable::GetDesc() failed, HRESULT=%08x\n", hr);
			exit(20);
		}

		D3DXCONSTANT_DESC descArray[16];
		uint32 baseOffset = pixelShader ? 0x40000000 : 0x00000000;

		for(UINT i=0; i<desc.Constants; ++i) {
			D3DXHANDLE hConstant = pConstants->GetConstant(NULL, i);
			UINT count = 16;

			hr = pConstants->GetConstantDesc(hConstant, descArray, &count);
			if (FAILED(hr)) {
				printf("ID3DXConstantTable::GetConstantDesc() failed, HRESULT=%08x\n", hr);
				exit(20);
			}

			for(UINT dc=0; dc<count; ++dc) {
				const D3DXCONSTANT_DESC& desc = descArray[dc];

				switch(desc.RegisterSet) {
					case D3DXRS_BOOL:
					case D3DXRS_INT4:
					case D3DXRS_FLOAT4:
						{
							for(int index=0; index<sizeof(kParameterNames)/sizeof(kParameterNames[0]); ++index) {
								if (!strcmp(kParameterNames[index], desc.Name)) {
									switch(desc.RegisterSet) {
									case D3DXRS_BOOL:
										states.push_back(baseOffset + 0x80000000 + (desc.RegisterIndex << 12) + index);
										break;
									case D3DXRS_INT4:
										states.push_back(baseOffset + 0x90000000 + (desc.RegisterIndex << 12) + index);
										break;
									case D3DXRS_FLOAT4:
										states.push_back(baseOffset + 0xA0000000 + (desc.RegisterIndex << 12) + index);
										break;
									}
									goto param_found;
								}
							}

							printf("Error: Unknown constant: %s\n", desc.Name);
							exit(10);
						}
param_found:
						break;
					case D3DXRS_SAMPLER:
						if (!strncmp(desc.Name, "vd_", 3)) {
							for(int index=0; index<sizeof(kTextureNames)/sizeof(kTextureNames[0]); ++index) {
								if (!strcmp(kTextureNames[index], desc.Name)) {
									UINT samplerIndex = pConstants->GetSamplerIndex(hConstant);
									states.push_back(0x30000000 + (samplerIndex << 24) + index + 1);
									goto texture_found;
								}
							}

							printf("Error: Unknown texture: %s\n", desc.Name);
							exit(10);
						}
texture_found:
						break;
				}
			}
		}
	}
}

void tool_fxc(const vdfastvector<const char *>& args, const vdfastvector<const char *>& switches, bool amd64) {
	if (args.size() != 2) {
		puts("usage: asuka fxc source.fx target.cpp");
		exit(5);
	}

	const char *filename = args[0];

	printf("Asuka: Compiling effect file (Direct3D): %s -> %s.\n", filename, args[1]);

	vdrefptr<DummyD3DDevice> pDevice(new DummyD3DDevice);

	vdrefptr<ID3DXEffect> pEffect;
	vdrefptr<ID3DXBuffer> pErrors;

	DWORD flags = D3DXSHADER_NO_PRESHADER;

#ifdef D3DXSHADER_USE_LEGACY_D3DX9_31_DLL
	// The V32 compiler seems to be broken in that it thinks "point" and "linear" are
	// keywords.
	flags |= D3DXSHADER_USE_LEGACY_D3DX9_31_DLL;
#endif

	HRESULT hr = D3DXCreateEffectFromFile(pDevice, filename, NULL, NULL, flags, NULL, ~pEffect, ~pErrors);

	if (FAILED(hr)) {
		printf("Effect compilation failed for \"%s\"\n", filename);

		if (pErrors)
			puts((const char *)pErrors->GetBufferPointer());

		exit(10);
	}

	pErrors.clear();

#if 0
	vdrefptr<ID3DXBuffer> pDisasm;
	hr = D3DXDisassembleEffect(pEffect, FALSE, ~pDisasm);

	if (SUCCEEDED(hr))
		puts((const char *)pDisasm->GetBufferPointer());

	pDisasm.clear();
#endif

	// dump the effect
	D3DXEFFECT_DESC desc;
	pEffect->GetDesc(&desc);

	vdrefptr<DummyD3DBaseTexture> pDummyTextures[sizeof(kTextureNames) / sizeof(kTextureNames[0])];

	for(int i=0; i<sizeof(kTextureNames) / sizeof(kTextureNames[0]); ++i) {
		pDummyTextures[i] = new DummyD3DBaseTexture(pDevice, i+1);

		pEffect->SetTexture(kTextureNames[i], pDummyTextures[i]);
	}

	FILE *f = fopen(args[1], "w");
	if (!f) {
		printf("Couldn't open %s for write\n", args[1]);
		exit(10);
	}

	fprintf(f, "// Effect data auto-generated by Asuka from %s. DO NOT EDIT!\n", VDFileSplitPath(filename));

	fprintf(f, "\n");
	fprintf(f, "struct PassInfo {\n");
	fprintf(f, "\tint mVertexShaderIndex;\n");
	fprintf(f, "\tint mPixelShaderIndex;\n");
	fprintf(f, "\tint mStateStart;\n");
	fprintf(f, "\tint mStateEnd;\n");
	fprintf(f, "\tint mRenderTarget;\n");
	fprintf(f, "\tuint8 mViewportW;\n");
	fprintf(f, "\tuint8 mViewportH;\n");
	fprintf(f, "\tuint8 mBumpEnvScale;\n");
	fprintf(f, "\tuint8 mTileMode;\n");
	fprintf(f, "\tbool mbClipPosition;\n");
	fprintf(f, "\tbool mbRTDoClear;\n");
	fprintf(f, "\tuint32 mRTClearColor;\n");
	fprintf(f, "};\n");

	fprintf(f, "\n");
	fprintf(f, "struct TechniqueInfo {\n");
	fprintf(f, "\tconst PassInfo *mpPasses;\n");
	fprintf(f, "\tuint32 mPassCount;\n");
	fprintf(f, "\tuint32 mPSVersionRequired;\n");
	fprintf(f, "\tuint32 mVSVersionRequired;\n");
	fprintf(f, "\tuint32 mPrimitiveMiscCaps;\n");
	fprintf(f, "\tuint32 mMaxSimultaneousTextures;\n");
	fprintf(f, "\tuint32 mMaxTextureBlendStages;\n");
	fprintf(f, "\tuint32 mSrcBlendCaps;\n");
	fprintf(f, "\tuint32 mDestBlendCaps;\n");
	fprintf(f, "\tuint32 mTextureOpCaps;\n");
	fprintf(f, "\tfloat mPixelShader1xMaxValue;\n");
	fprintf(f, "};\n");

	fprintf(f, "\n");
	fprintf(f, "struct EffectInfo {\n");
	fprintf(f, "\tconst uint32 *mpShaderData;\n");
	fprintf(f, "\tconst uint32 *mVertexShaderOffsets;\n");
	fprintf(f, "\tuint32 mVertexShaderCount;\n");
	fprintf(f, "\tconst uint32 *mPixelShaderOffsets;\n");
	fprintf(f, "\tuint32 mPixelShaderCount;\n");
	fprintf(f, "};\n");

	std::list<std::vector<uint32> > mVertexShaders;
	std::list<std::vector<uint32> > mPixelShaders;
	vdfastvector<uint32> mStates;

	pDevice->ClearStates();

	for(int technique=0; technique<(int)desc.Techniques; ++technique) {
		D3DXHANDLE hTechnique = pEffect->GetTechnique(technique);
		D3DXTECHNIQUE_DESC techDesc;

		pEffect->GetTechniqueDesc(hTechnique, &techDesc);
		pEffect->SetTechnique(hTechnique);

		pDevice->ClearRequiredCaps();

		UINT passCount = 0;
		if (FAILED(pEffect->Begin(&passCount, D3DXFX_DONOTSAVESTATE))) {
			puts("Begin failed!");
			exit(20);
		}

		vdfastvector<PassInfo> mPasses;
		uint32 maxVSVersion = 0;
		uint32 maxPSVersion = 0;

		for(int pass=0; pass<(int)passCount; ++pass) {
			D3DXHANDLE hPass = pEffect->GetPass(hTechnique, pass);
			D3DXPASS_DESC passDesc;

			pEffect->GetPassDesc(hPass, &passDesc);

			int stateStart = mStates.size();

			int psIndex = -1;
			if (passDesc.pPixelShaderFunction) {
				std::vector<uint32> ps(passDesc.pPixelShaderFunction, passDesc.pPixelShaderFunction + (D3DXGetShaderSize(passDesc.pPixelShaderFunction) >> 2));

				DeleteShaderConstantTable(ps);

				// slow... fix if perf bottleneck
				int psIndex2 = 0;
				for(std::list<std::vector<uint32> >::const_iterator it(mPixelShaders.begin()), itEnd(mPixelShaders.end()); it!=itEnd; ++it, ++psIndex2) {
					if (*it == ps) {
						psIndex = psIndex2;
						break;
					}
				}

				if (psIndex < 0) {
					mPixelShaders.push_back(std::vector<uint32>());
					mPixelShaders.back().swap(ps);
					psIndex = mPixelShaders.size() - 1;
				}

				vdrefptr<ID3DXConstantTable> pConstantTable;
				if (D3D_OK == D3DXGetShaderConstantTable(passDesc.pPixelShaderFunction, ~pConstantTable))
					EmitShaderConstants(mStates, pConstantTable, true);

				uint32 psVersion = passDesc.pPixelShaderFunction[0] & 0xffff;
				if (psVersion > maxPSVersion)
					maxPSVersion = psVersion;
			}

			int vsIndex = -1;
			if (passDesc.pVertexShaderFunction) {
				std::vector<uint32> vs(passDesc.pVertexShaderFunction, passDesc.pVertexShaderFunction + (D3DXGetShaderSize(passDesc.pVertexShaderFunction) >> 2));

				DeleteShaderConstantTable(vs);

				// slow... fix if perf bottleneck
				int vsIndex2 = 0;
				for(std::list<std::vector<uint32> >::const_iterator it(mVertexShaders.begin()), itEnd(mVertexShaders.end()); it!=itEnd; ++it, ++vsIndex2) {
					if (*it == vs) {
						vsIndex = vsIndex2;
						break;
					}
				}

				if (vsIndex < 0) {
					mVertexShaders.push_back(std::vector<uint32>());
					mVertexShaders.back().swap(vs);
					vsIndex = mVertexShaders.size() - 1;
				}

				vdrefptr<ID3DXConstantTable> pConstantTable;
				if (D3D_OK == D3DXGetShaderConstantTable(passDesc.pVertexShaderFunction, ~pConstantTable))
					EmitShaderConstants(mStates, pConstantTable, false);

				uint32 vsVersion = passDesc.pVertexShaderFunction[0] & 0xffff;
				if (vsVersion > maxVSVersion)
					maxVSVersion = vsVersion;
			}

			hr = pEffect->BeginPass(pass);
			if (FAILED(hr)) {
				puts("BeginPass failed!");
				exit(20);
			}

			int stateCount = pDevice->GetStateCount();
			if (stateCount > 0) {
				const uint32 *p = pDevice->GetStates();
				mStates.insert(mStates.end(), p, p+stateCount);
			}
			int stateEnd = mStates.size();

			pDevice->ClearStates();

			PassInfo pi;
			pi.mVertexShaderIndex = vsIndex;
			pi.mPixelShaderIndex = psIndex;
			pi.mStateStart = stateStart;
			pi.mStateEnd = stateEnd;
			pi.mRenderTarget = -1;
			pi.mViewportW = 0;
			pi.mViewportH = 0;

			// force first pass to always default to main RT
			if (!pass) {
				pi.mRenderTarget = 0;
				pi.mViewportW = 2;
				pi.mViewportH = 2;
			}

			D3DXHANDLE hRTAnno = pEffect->GetAnnotationByName(hPass, "vd_target");
			if (hRTAnno) {
				LPCSTR s;
				if (SUCCEEDED(pEffect->GetString(hRTAnno, &s))) {
					if (!*s) {
						pi.mRenderTarget = 0;

						// main RT defaults to out,out instead of full,full
						pi.mViewportW = 2;
						pi.mViewportH = 2;
					} else if (!strcmp(s, "temp"))
						pi.mRenderTarget = 1;
					else if (!strcmp(s, "temp2"))
						pi.mRenderTarget = 2;
					else {
						printf("Error: Unknown render target: %s\n", s);
						exit(10);
					}
				}
			}

			D3DXHANDLE hVPAnno = pEffect->GetAnnotationByName(hPass, "vd_viewport");
			if (hVPAnno) {
				LPCSTR s;
				if (SUCCEEDED(pEffect->GetString(hVPAnno, &s))) {
					bool valid = true;

					const char *brk = strchr(s, ',');
					if (!brk)
						valid = false;

					if (valid) {
						std::string hvp(s, brk);

						if (hvp == "src")
							pi.mViewportW = 1;
						else if (hvp == "out")
							pi.mViewportW = 2;
						else if (hvp == "unclipped")
							pi.mViewportW = 3;
						else if (hvp == "full")
							pi.mViewportW = 0;
						else
							valid = false;
					}

					if (valid) {
						++brk;
						while(*brk == ' ')
							++brk;

						if (!strcmp(brk, "src"))
							pi.mViewportH = 1;
						else if (!strcmp(brk, "out"))
							pi.mViewportH = 2;
						else if (!strcmp(brk, "unclipped"))
							pi.mViewportH = 3;
						else if (!strcmp(brk, "full"))
							pi.mViewportH = 0;
						else
							valid = false;
					}

					if (!valid) {
						printf("Error: Invalid viewport annotation: %s\n", s);
						exit(10);
					}
				}
			}

			pi.mbClipPosition = false;
			D3DXHANDLE hClipPosAnno = pEffect->GetAnnotationByName(hPass, "vd_clippos");
			if (hClipPosAnno) {
				BOOL b;
				if (SUCCEEDED(pEffect->GetBool(hClipPosAnno, &b))) {
					if (b)
						pi.mbClipPosition = true;
				}
			}

			pi.mbRTDoClear = false;
			pi.mRTClearColor = 0;
			D3DXHANDLE hRTClearAnno = pEffect->GetAnnotationByName(hPass, "vd_clear");
			if (hRTClearAnno) {
				D3DXVECTOR4 v;
				if (SUCCEEDED(pEffect->GetVector(hRTClearAnno, &v))) {
					pi.mRTClearColor	= (VDClampedRoundFixedToUint8Fast(v.x) << 16)
										+ (VDClampedRoundFixedToUint8Fast(v.y) <<  8)
										+ (VDClampedRoundFixedToUint8Fast(v.z) <<  0)
										+ (VDClampedRoundFixedToUint8Fast(v.w) << 24);

					pi.mbRTDoClear = true;
				}
			}

			pi.mBumpEnvScale = 0;
			D3DXHANDLE hBESAnno = pEffect->GetAnnotationByName(hPass, "vd_bumpenvscale");
			if (hBESAnno) {
				LPCSTR s;
				if (SUCCEEDED(pEffect->GetString(hBESAnno, &s))) {
					for(int i=0; i<sizeof(kParameterNames)/sizeof(kParameterNames[0]); ++i) {
						if (!strcmp(s, kParameterNames[i])) {
							pi.mBumpEnvScale = i+1;
							break;
						}
					}

					if (!pi.mBumpEnvScale) {
						printf("Error: Unknown source for bump-map environment matrix scale: %s\n", s);
						exit(10);
					}
				}
			}

			pi.mTileMode = 0;
			D3DXHANDLE hTMAnno = pEffect->GetAnnotationByName(hPass, "vd_tilemode");
			if (hTMAnno) {
				INT val;

				if (SUCCEEDED(pEffect->GetInt(hTMAnno, &val))) {
					pi.mTileMode = val;
				}
			}

			mPasses.push_back(pi);

			pEffect->EndPass();
		}

		pEffect->End();

		// emit passes
		fprintf(f, "static const PassInfo g_technique_%s_passes[]={\n", techDesc.Name);
		for(int i=0; i<(int)passCount; ++i) {
			const PassInfo& pi = mPasses[i];
			fprintf(f, "\t{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, %s, 0x%08x },\n"
				, pi.mVertexShaderIndex
				, pi.mPixelShaderIndex
				, pi.mStateStart
				, pi.mStateEnd
				, pi.mRenderTarget
				, pi.mViewportW
				, pi.mViewportH
				, pi.mBumpEnvScale
				, pi.mTileMode
				, pi.mbClipPosition ? "true" : "false"
				, pi.mbRTDoClear ? "true" : "false"
				, pi.mRTClearColor);
		}
		fprintf(f, "};\n");

		const D3DCAPS9& caps = pDevice->GetRequiredCaps();

		fprintf(f, "static const TechniqueInfo g_technique_%s={\n", techDesc.Name);
		fprintf(f, "\tg_technique_%s_passes, %d,\n", techDesc.Name, passCount);
		fprintf(f, "\tD3DPS_VERSION(%d,%d),\n", maxPSVersion >> 8, maxPSVersion & 255);
		fprintf(f, "\tD3DVS_VERSION(%d,%d),\n", maxVSVersion >> 8, maxVSVersion & 255);
		fprintf(f, "\t0x%08x,\n", caps.PrimitiveMiscCaps);
		fprintf(f, "\t0x%08x,\n", caps.MaxSimultaneousTextures);
		fprintf(f, "\t0x%08x,\n", caps.MaxTextureBlendStages);
		fprintf(f, "\t0x%08x,\n", caps.SrcBlendCaps);
		fprintf(f, "\t0x%08x,\n", caps.DestBlendCaps);
		fprintf(f, "\t0x%08x,\n", caps.TextureOpCaps);
		fprintf(f, "\t%d.f,\n", maxPSVersion >= 0x0104 ? 2 : maxPSVersion >= 0x0101 ? 1 : 0);
		fprintf(f, "};\n");
	}

	// Emit state array.
	const int stateCount = mStates.size();
	if (stateCount > 0) {
		fprintf(f, "static const uint32 g_states[]={\n");
		for(int i=0; i<stateCount; i+=8) {
			fprintf(f, "\t");
			for(int j=i; j<i+8 && j<stateCount; ++j) {
				fprintf(f, "0x%08x,", mStates[j]);
			}
			fprintf(f, "\n");
		}
		fprintf(f, "};\n");
	}

	// Emit all vertex and pixel shaders
	std::list<std::vector<uint32> >::iterator it, itEnd;

	std::vector<uint32> mShaderData;
	vdfastvector<int> mVertexShaderOffsets;
	vdfastvector<int> mPixelShaderOffsets;

	for(it=mVertexShaders.begin(), itEnd=mVertexShaders.end(); it!=itEnd; ++it) {
		mVertexShaderOffsets.push_back(mShaderData.size());
		mShaderData.insert(mShaderData.end(), it->begin(), it->end());
	}
	mVertexShaderOffsets.push_back(mShaderData.size());

	for(it=mPixelShaders.begin(), itEnd=mPixelShaders.end(); it!=itEnd; ++it) {
		mPixelShaderOffsets.push_back(mShaderData.size());
		mShaderData.insert(mShaderData.end(), it->begin(), it->end());
	}
	mPixelShaderOffsets.push_back(mShaderData.size());

	fprintf(f, "static const uint32 g_shaderData[]={\n");
	for(int i=0, count=mShaderData.size(); i<count; i+=8) {
		fprintf(f, "\t");
		for(int j=i; j<i+8 && j<count; ++j) {
			fprintf(f, "0x%08x,", mShaderData[j]);
		}
		fprintf(f, "\n");
	}
	fprintf(f, "};\n");

	// output shader list
	fprintf(f, "static const uint32 g_shaderOffsets[]={\n");
	fprintf(f, "\t");
	for(int i=0; i<(int)mVertexShaderOffsets.size(); ++i)
		fprintf(f, "%d,", mVertexShaderOffsets[i]);
	fprintf(f, "\n");
	fprintf(f, "\t");
	for(int i=0; i<(int)mPixelShaderOffsets.size(); ++i)
		fprintf(f, "%d,", mPixelShaderOffsets[i]);
	fprintf(f, "\n");
	fprintf(f, "};\n");

	// output effect data
	fprintf(f, "static const EffectInfo g_effect={\n");
		fprintf(f, "\tg_shaderData,\n");
		fprintf(f, "\tg_shaderOffsets+0, %d,\n", mVertexShaderOffsets.size() - 1);
		fprintf(f, "\tg_shaderOffsets+%d, %d\n", mVertexShaderOffsets.size(), mPixelShaderOffsets.size() - 1);
	fprintf(f, "};\n");
	fclose(f);

	printf("Asuka: %d techniques, %d shader bytes, %d state bytes.\n", desc.Techniques, mShaderData.size()*4, mStates.size()*4);
	printf("Asuka: Compilation was successful.\n");
}

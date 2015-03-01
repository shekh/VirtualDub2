#include "stdafx.h"
#include <D3D11.h>
#include <vd2/system/bitmath.h>
#include <vd2/system/w32assist.h>
#include <vd2/Tessa/Context.h>
#include <vd2/Tessa/Format.h>
#include "D3D11/Context_D3D11.h"
#include "D3D11/FenceManager_D3D11.h"
#include "Program.h"

namespace {
	DXGI_FORMAT GetSurfaceFormatD3D11(VDTFormat format) {
		switch(format) {
			case kVDTF_B8G8R8A8:
				return DXGI_FORMAT_B8G8R8A8_UNORM;

			case kVDTF_R8G8B8A8:
				return DXGI_FORMAT_R8G8B8A8_UNORM;

			case kVDTF_U8V8:
				return DXGI_FORMAT_R8G8_SNORM;

			case kVDTF_R8G8:
				return DXGI_FORMAT_R8G8_UNORM;

			case kVDTF_R8:
				return DXGI_FORMAT_R8_UNORM;

			default:
				return DXGI_FORMAT_UNKNOWN;
		}
	}

	VDTFormat GetSurfaceFormatFromD3D11(DXGI_FORMAT format) {
		switch(format) {
			case DXGI_FORMAT_B8G8R8A8_UNORM:
				return kVDTF_B8G8R8A8;

			case DXGI_FORMAT_R8G8B8A8_UNORM:
				return kVDTF_R8G8B8A8;

			case DXGI_FORMAT_R8G8_SNORM:
				return kVDTF_U8V8;

			case DXGI_FORMAT_R8G8_UNORM:
				return kVDTF_R8G8;

			case DXGI_FORMAT_R8_UNORM:
				return kVDTF_R8;

			default:
				return kVDTF_Unknown;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

VDTResourceD3D11::VDTResourceD3D11() {
	mListNodePrev = NULL;
	mpParent = NULL;
}

VDTResourceD3D11::~VDTResourceD3D11() {
}

void VDTResourceD3D11::Shutdown() {
	if (mListNodePrev)
		mpParent->RemoveResource(this);
}

void VDTResourceManagerD3D11::AddResource(VDTResourceD3D11 *res) {
	VDASSERT(!res->mListNodePrev);

	mResources.push_back(res);
	res->mpParent = this;
}

void VDTResourceManagerD3D11::RemoveResource(VDTResourceD3D11 *res) {
	VDASSERT(res->mListNodePrev);

	mResources.erase(res);
	res->mListNodePrev = NULL;
}

void VDTResourceManagerD3D11::ShutdownAllResources() {
	while(!mResources.empty()) {
		VDTResourceD3D11 *res = mResources.back();
		mResources.pop_back();

		res->mListNodePrev = NULL;
		res->Shutdown();
	}
}

///////////////////////////////////////////////////////////////////////////////

VDTReadbackBufferD3D11::VDTReadbackBufferD3D11()
	: mpSurface(NULL)
{
}

VDTReadbackBufferD3D11::~VDTReadbackBufferD3D11() {
	Shutdown();
}

bool VDTReadbackBufferD3D11::Init(VDTContextD3D11 *parent, uint32 width, uint32 height, VDTFormat format) {
	ID3D11Device *dev = parent->GetDeviceD3D11();

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;

	HRESULT hr = dev->CreateTexture2D(&desc, NULL, &mpSurface);

	parent->AddResource(this);
	return SUCCEEDED(hr);
}

void VDTReadbackBufferD3D11::Shutdown() {
	if (mpSurface) {
		mpSurface->Release();
		mpSurface = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTReadbackBufferD3D11::Lock(VDTLockData2D& lockData) {
	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();

	D3D11_MAPPED_SUBRESOURCE info;
	HRESULT hr = devctx->Map(mpSurface, 0, D3D11_MAP_READ, 0, &info);

	if (FAILED(hr)) {
		lockData.mpData = NULL;
		lockData.mPitch = 0;
		return false;
	}

	lockData.mpData = info.pData;
	lockData.mPitch = info.RowPitch;
	return true;
}

void VDTReadbackBufferD3D11::Unlock() {
	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();

	devctx->Unmap(mpSurface, 0);
}

bool VDTReadbackBufferD3D11::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTSurfaceD3D11::VDTSurfaceD3D11()
	: mpTexture(NULL)
	, mpTextureSys(NULL)
	, mpRTView(NULL)
{
}

VDTSurfaceD3D11::~VDTSurfaceD3D11() {
	Shutdown();
}

bool VDTSurfaceD3D11::Init(VDTContextD3D11 *parent, uint32 width, uint32 height, VDTFormat format, VDTUsage usage) {
	DXGI_FORMAT dxgiFormat = GetSurfaceFormatD3D11(format);

	if (!dxgiFormat)
		return false;

	ID3D11Device *dev = parent->GetDeviceD3D11();
	HRESULT hr;
	
	mDesc.mWidth = width;
	mDesc.mHeight = height;
	mDesc.mFormat = format;

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = dxgiFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	switch(usage) {
		case kVDTUsage_Default:
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			break;

		case kVDTUsage_Render:
			desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			break;
	}

	hr = dev->CreateTexture2D(&desc, NULL, &mpTexture);
	if (FAILED(hr))
		return false;

	if (usage == kVDTUsage_Render) {
		D3D11_RENDER_TARGET_VIEW_DESC rtvdesc = {};
		rtvdesc.Format = desc.Format;
		rtvdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvdesc.Texture2D.MipSlice = 0;

		hr = dev->CreateRenderTargetView(mpTexture, &rtvdesc, &mpRTView);
		if (FAILED(hr)) {
			Shutdown();
			return false;
		}
	}

	parent->AddResource(this);

	return SUCCEEDED(hr);
}

bool VDTSurfaceD3D11::Init(VDTContextD3D11 *parent, ID3D11Texture2D *tex, ID3D11Texture2D *texsys, uint32 mipLevel, bool rt) {
	D3D11_TEXTURE2D_DESC desc = {};

	tex->GetDesc(&desc);

	mMipLevel = mipLevel;

	mDesc.mWidth = desc.Width;
	mDesc.mHeight = desc.Height;
	mDesc.mFormat = GetSurfaceFormatFromD3D11(desc.Format);

	if (rt) {
		D3D11_RENDER_TARGET_VIEW_DESC rtvdesc = {};
		rtvdesc.Format = desc.Format;
		rtvdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvdesc.Texture2D.MipSlice = mipLevel;

		HRESULT hr = parent->GetDeviceD3D11()->CreateRenderTargetView(tex, &rtvdesc, &mpRTView);
		if (FAILED(hr))
			return false;
	}

	parent->AddResource(this);

	mpTexture = tex;
	mpTexture->AddRef();

	mpTextureSys = texsys;
	if (mpTextureSys)
		mpTextureSys->AddRef();
	return true;
}

void VDTSurfaceD3D11::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D11 *>(mpParent)->UnsetRenderTarget(this);

	vdsaferelease <<= mpRTView, mpTextureSys, mpTexture;

	VDTResourceD3D11::Shutdown();
}

bool VDTSurfaceD3D11::Restore() {
	return true;
}

bool VDTSurfaceD3D11::Readback(IVDTReadbackBuffer *target) {
	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();
	VDTReadbackBufferD3D11 *targetD3D11 = static_cast<VDTReadbackBufferD3D11 *>(target);

	devctx->CopyResource(targetD3D11->mpSurface, mpTexture);
	return true;
}

void VDTSurfaceD3D11::Load(uint32 dx, uint32 dy, const VDTInitData2D& srcData, uint32 w, uint32 h) {
	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();

	D3D11_MAPPED_SUBRESOURCE info;
	HRESULT hr = devctx->Map(mpTextureSys, mMipLevel, D3D11_MAP_WRITE, 0, &info);
	if (FAILED(hr)) {
		parent->ProcessHRESULT(hr);
		return;
	}

	const uint32 bpr = VDTGetBytesPerBlockRow(mDesc.mFormat, w);
	const uint32 bh = VDTGetNumBlockRows(mDesc.mFormat, h);

	VDMemcpyRect((char *)info.pData + info.RowPitch * dy, info.RowPitch, srcData.mpData, srcData.mPitch, bpr, bh);

	devctx->Unmap(mpTextureSys, mMipLevel);
	devctx->CopySubresourceRegion(mpTexture, mMipLevel, 0, 0, 0, mpTextureSys, mMipLevel, NULL);
}

void VDTSurfaceD3D11::Copy(uint32 dx, uint32 dy, IVDTSurface *src0, uint32 sx, uint32 sy, uint32 w, uint32 h) {
	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();
	VDTSurfaceD3D11 *src = static_cast<VDTSurfaceD3D11 *>(src0);

	D3D11_BOX box;
	box.left = sx;
	box.right = sx + w;
	box.top = sy;
	box.bottom = sy+h;
	box.front = 0;
	box.back = 1;
	devctx->CopySubresourceRegion(mpTexture, mMipLevel, dx, dy, 0, src->mpTexture, src->mMipLevel, &box);
}

void VDTSurfaceD3D11::GetDesc(VDTSurfaceDesc& desc) {
	desc = mDesc;
}

bool VDTSurfaceD3D11::Lock(const vdrect32 *r, VDTLockData2D& lockData) {
	if (!mpTextureSys)
		return false;

	RECT r2;
	const RECT *pr = NULL;
	if (r) {
		r2.left = r->left;
		r2.top = r->top;
		r2.right = r->right;
		r2.bottom = r->bottom;
		pr = &r2;
	}

	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = devctx->Map(mpTextureSys, mMipLevel, D3D11_MAP_READ_WRITE, 0, &mapped);
	if (FAILED(hr)) {
		VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
		parent->ProcessHRESULT(hr);
		return false;
	}

	lockData.mpData = mapped.pData;
	lockData.mPitch = mapped.RowPitch;

	return true;
}

void VDTSurfaceD3D11::Unlock() {
	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();

	devctx->Unmap(mpTextureSys, mMipLevel);
	devctx->CopySubresourceRegion(mpTexture, mMipLevel, 0, 0, 0, mpTextureSys, mMipLevel, NULL);
}

///////////////////////////////////////////////////////////////////////////////

VDTTexture2DD3D11::VDTTexture2DD3D11()
	: mpTexture(NULL)
	, mpTextureSys(NULL)
	, mpShaderResView(NULL)
{
}

VDTTexture2DD3D11::~VDTTexture2DD3D11() {
	Shutdown();
}

void *VDTTexture2DD3D11::AsInterface(uint32 id) {
	if (id == kTypeD3DShaderResView)
		return mpShaderResView;

	if (id == IVDTTexture2D::kTypeID)
		return static_cast<IVDTTexture2D *>(this);

	return NULL;
}

bool VDTTexture2DD3D11::Init(VDTContextD3D11 *parent, uint32 width, uint32 height, VDTFormat format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData) {
	parent->AddResource(this);

	if (!mipcount) {
		uint32 mask = (width - 1) | (height - 1);

		mipcount = VDFindHighestSetBit(mask) + 1;
	}

	DXGI_FORMAT dxgiFormat = GetSurfaceFormatD3D11(format);
	if (!dxgiFormat)
		return false;

	mWidth = width;
	mHeight = height;
	mMipCount = mipcount;
	mUsage = usage;
	mFormat = format;

	if (mpTexture)
		return true;

	ID3D11Device *dev = parent->GetDeviceD3D11();
	if (!dev)
		return false;

	D3D11_TEXTURE2D_DESC desc;
    desc.Width = mWidth;
    desc.Height = mHeight;
    desc.MipLevels = mMipCount;
    desc.ArraySize = 1;
	desc.Format = dxgiFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = initData ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
	desc.BindFlags = usage == kVDTUsage_Render ? D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE : D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

	vdfastvector<D3D11_SUBRESOURCE_DATA> subResData;

	if (initData) {
		subResData.resize(mipcount);

		for(uint32 i=0; i<mipcount; ++i) {
			D3D11_SUBRESOURCE_DATA& dst = subResData[i];
			const VDTInitData2D& src = initData[i];

			dst.pSysMem = src.mpData;
			dst.SysMemPitch = (UINT)src.mPitch;
			dst.SysMemSlicePitch = 0;
		}
	}

	HRESULT hr = dev->CreateTexture2D(&desc, initData ? subResData.data() : NULL, &mpTexture);
	
	if (FAILED(hr))
		return false;

	if (!initData && usage != kVDTUsage_Render) {
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		hr = dev->CreateTexture2D(&desc, NULL, &mpTextureSys);

		if (FAILED(hr))
			return false;
	}

	hr = dev->CreateShaderResourceView(mpTexture, NULL, &mpShaderResView);

	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	mMipmaps.reserve(mipcount);

	for(uint32 i=0; i<mipcount; ++i) {
		vdrefptr<VDTSurfaceD3D11> surf(new VDTSurfaceD3D11);

		surf->Init(parent, mpTexture, mpTextureSys, i, usage == kVDTUsage_Render);

		mMipmaps.push_back(surf.release());
	}

	return true;
}

bool VDTTexture2DD3D11::Init(VDTContextD3D11 *parent, ID3D11Texture2D *tex, ID3D11Texture2D *texsys) {
	parent->AddResource(this);

	D3D11_TEXTURE2D_DESC desc;
	tex->GetDesc(&desc);

	mWidth = desc.Width;
	mHeight = desc.Height;
	mMipCount = desc.MipLevels;
	mUsage = (desc.BindFlags & D3D11_BIND_RENDER_TARGET) ? kVDTUsage_Render : kVDTUsage_Default;
	mFormat = GetSurfaceFormatFromD3D11(desc.Format);

	mpTexture = tex;
	tex->AddRef();

	mpTextureSys = texsys;
	if (texsys)
		texsys->AddRef();

	ID3D11Device *dev = parent->GetDeviceD3D11();
	if (!dev)
		return false;

	if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
		HRESULT hr = dev->CreateShaderResourceView(mpTexture, NULL, &mpShaderResView);

		if (FAILED(hr)) {
			Shutdown();
			return false;
		}
	}

	mMipmaps.reserve(mMipCount);

	for(uint32 i=0; i<mMipCount; ++i) {
		vdrefptr<VDTSurfaceD3D11> surf(new VDTSurfaceD3D11);

		if (!surf->Init(parent, mpTexture, mpTextureSys, i, mUsage == kVDTUsage_Render)) {
			Shutdown();
			return false;
		}

		mMipmaps.push_back(surf);
		surf.release();
	}

	return true;
}

void VDTTexture2DD3D11::Shutdown() {
	while(!mMipmaps.empty()) {
		VDTSurfaceD3D11 *surf = mMipmaps.back();
		mMipmaps.pop_back();

		surf->Shutdown();
		surf->Release();
	}

	vdsaferelease <<= mpShaderResView, mpTextureSys;

	if (mpTexture) {
		if (mpParent)
			static_cast<VDTContextD3D11 *>(mpParent)->UnsetTexture(this);

		mpTexture->Release();
		mpTexture = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTTexture2DD3D11::Restore() {
	return true;
}

IVDTSurface *VDTTexture2DD3D11::GetLevelSurface(uint32 level) {
	return mMipmaps[level];
}

void VDTTexture2DD3D11::GetDesc(VDTTextureDesc& desc) {
	desc.mWidth = mWidth;
	desc.mHeight = mHeight;
	desc.mMipCount = mMipCount;
	desc.mFormat = mFormat;
}

void VDTTexture2DD3D11::Load(uint32 mip, uint32 x, uint32 y, const VDTInitData2D& srcData, uint32 w, uint32 h) {
	mMipmaps[mip]->Load(x, y, srcData, w, h);
}

bool VDTTexture2DD3D11::Lock(uint32 mip, const vdrect32 *r, VDTLockData2D& lockData) {
	return mMipmaps[mip]->Lock(r, lockData);
}

void VDTTexture2DD3D11::Unlock(uint32 mip) {
	mMipmaps[mip]->Unlock();
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexBufferD3D11::VDTVertexBufferD3D11()
	: mpVB(NULL)
{
}

VDTVertexBufferD3D11::~VDTVertexBufferD3D11() {
	Shutdown();
}

bool VDTVertexBufferD3D11::Init(VDTContextD3D11 *parent, uint32 size, bool dynamic, const void *initData) {
	parent->AddResource(this);

	mbDynamic = dynamic;
	mByteSize = size;

	if (mpVB)
		return true;

	ID3D11Device *dev = static_cast<VDTContextD3D11 *>(mpParent)->GetDeviceD3D11();
	if (!dev)
		return false;

	D3D11_BUFFER_DESC desc;
    desc.ByteWidth = mByteSize;
	desc.Usage = mbDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = mbDynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA srdata = {};
	srdata.pSysMem = initData;

	HRESULT hr = dev->CreateBuffer(&desc, initData ? &srdata : NULL, &mpVB);

	if (FAILED(hr))
		return false;

	return true;
}

void VDTVertexBufferD3D11::Shutdown() {
	if (mpVB) {
		if (mpParent)
			static_cast<VDTContextD3D11 *>(mpParent)->UnsetVertexBuffer(this);

		mpVB->Release();
		mpVB = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTVertexBufferD3D11::Restore() {
	return true;
}

bool VDTVertexBufferD3D11::Load(uint32 offset, uint32 size, const void *data) {
	if (!size)
		return true;

	if (offset > mByteSize || mByteSize - offset < size)
		return false;

	D3D11_MAP flags = D3D11_MAP_WRITE;

	if (mbDynamic) {
		if (offset)
			flags = D3D11_MAP_WRITE_NO_OVERWRITE;
		else
			flags = D3D11_MAP_WRITE_DISCARD;
	}

	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();

	D3D11_MAPPED_SUBRESOURCE info;
	HRESULT hr = devctx->Map(mpVB, 0, flags, 0, &info);
	if (FAILED(hr)) {
		static_cast<VDTContextD3D11 *>(mpParent)->ProcessHRESULT(hr);
		return false;
	}

	bool success = true;

	if (mbDynamic)
		success = VDMemcpyGuarded(info.pData, data, size);
	else
		memcpy(info.pData, data, size);

	devctx->Unmap(mpVB, 0);
	return success;
}

///////////////////////////////////////////////////////////////////////////////

VDTIndexBufferD3D11::VDTIndexBufferD3D11()
	: mpIB(NULL)
{
}

VDTIndexBufferD3D11::~VDTIndexBufferD3D11() {
	Shutdown();
}

bool VDTIndexBufferD3D11::Init(VDTContextD3D11 *parent, uint32 size, bool index32, bool dynamic, const void *initData) {
	mByteSize = index32 ? size << 2 : size << 1;
	mbDynamic = dynamic;
	mbIndex32 = index32;

	if (mpIB)
		return true;

	ID3D11Device *dev = static_cast<VDTContextD3D11 *>(parent)->GetDeviceD3D11();
	if (!dev)
		return false;

	D3D11_BUFFER_DESC desc;
    desc.ByteWidth = mByteSize;
	desc.Usage = mbDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.CPUAccessFlags = mbDynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA srd;

	srd.pSysMem = initData;
	srd.SysMemPitch = 0;
	srd.SysMemSlicePitch = 0;

	HRESULT hr = dev->CreateBuffer(&desc, initData ? &srd : NULL, &mpIB);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTIndexBufferD3D11::Shutdown() {
	if (mpIB) {
		if (mpParent)
			static_cast<VDTContextD3D11 *>(mpParent)->UnsetIndexBuffer(this);

		mpIB->Release();
		mpIB = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTIndexBufferD3D11::Restore() {
	return true;
}

bool VDTIndexBufferD3D11::Load(uint32 offset, uint32 size, const void *data) {
	if (!size)
		return true;

	VDTContextD3D11 *parent = static_cast<VDTContextD3D11 *>(mpParent);
	ID3D11DeviceContext *devctx = parent->GetDeviceContextD3D11();

	D3D11_MAPPED_SUBRESOURCE info;
	HRESULT hr = devctx->Map(mpIB, 0, D3D11_MAP_WRITE, 0, &info);
	if (FAILED(hr)) {
		static_cast<VDTContextD3D11 *>(mpParent)->ProcessHRESULT(hr);
		return false;
	}

	bool success = true;
	if (mbDynamic)
		success = VDMemcpyGuarded(info.pData, data, size);
	else
		memcpy(info.pData, data, size);

	devctx->Unmap(mpIB, 0);
	return success;
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexFormatD3D11::VDTVertexFormatD3D11()
	: mpVF(NULL)
{
}

VDTVertexFormatD3D11::~VDTVertexFormatD3D11() {
	Shutdown();
}

bool VDTVertexFormatD3D11::Init(VDTContextD3D11 *parent, const VDTVertexElement *elements, uint32 count, VDTVertexProgramD3D11 *vp) {
	static const char *const kSemanticD3D11[]={
		"position",
		"blendweight",
		"blendindices",
		"normal",
		"texcoord",
		"tangent",
		"binormal",
		"color"
	};

	static const DXGI_FORMAT kFormatD3D11[]={
		DXGI_FORMAT_R32_FLOAT,
		DXGI_FORMAT_R32G32_FLOAT,
		DXGI_FORMAT_R32G32B32_FLOAT,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		DXGI_FORMAT_R8G8B8A8_UNORM
	};

	if (count >= 16) {
		VDASSERT(!"Too many vertex elements.");
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC vxe[16];
	for(uint32 i=0; i<count; ++i) {
		D3D11_INPUT_ELEMENT_DESC& dst = vxe[i];
		const VDTVertexElement& src = elements[i];

		dst.SemanticName = kSemanticD3D11[src.mUsage];
		dst.SemanticIndex = src.mUsageIndex;
		dst.Format = kFormatD3D11[src.mType];
		dst.InputSlot = 0;
		dst.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		dst.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		dst.InstanceDataStepRate = 0;
	}

	HRESULT hr = parent->GetDeviceD3D11()->CreateInputLayout(vxe, count, vp->mByteCode.data(), vp->mByteCode.size(), &mpVF);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTVertexFormatD3D11::Shutdown() {
	if (mpVF) {
		if (mpParent)
			static_cast<VDTContextD3D11 *>(mpParent)->UnsetVertexFormat(this);

		mpVF->Release();
		mpVF = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTVertexFormatD3D11::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexProgramD3D11::VDTVertexProgramD3D11()
	: mpVS(NULL)
{
}

VDTVertexProgramD3D11::~VDTVertexProgramD3D11() {
	Shutdown();
}

bool VDTVertexProgramD3D11::Init(VDTContextD3D11 *parent, VDTProgramFormat format, const void *data, uint32 size) {
	HRESULT hr = parent->GetDeviceD3D11()->CreateVertexShader(data, size, NULL, &mpVS);

	if (FAILED(hr))
		return false;

	mByteCode.assign((const uint8 *)data, (const uint8 *)data + size);

	parent->AddResource(this);
	return true;
}

void VDTVertexProgramD3D11::Shutdown() {
	if (mpVS) {
		if (mpParent)
			static_cast<VDTContextD3D11 *>(mpParent)->UnsetVertexProgram(this);

		mpVS->Release();
		mpVS = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTVertexProgramD3D11::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTFragmentProgramD3D11::VDTFragmentProgramD3D11()
	: mpPS(NULL)
{
}

VDTFragmentProgramD3D11::~VDTFragmentProgramD3D11() {
	Shutdown();
}

bool VDTFragmentProgramD3D11::Init(VDTContextD3D11 *parent, VDTProgramFormat format, const void *data, uint32 size) {
	HRESULT hr = parent->GetDeviceD3D11()->CreatePixelShader(data, size, NULL, &mpPS);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTFragmentProgramD3D11::Shutdown() {
	if (mpPS) {
		if (mpParent)
			static_cast<VDTContextD3D11 *>(mpParent)->UnsetFragmentProgram(this);

		mpPS->Release();
		mpPS = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTFragmentProgramD3D11::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTBlendStateD3D11::VDTBlendStateD3D11()
	: mpBlendState(NULL)
{
}

VDTBlendStateD3D11::~VDTBlendStateD3D11() {
	Shutdown();
}

bool VDTBlendStateD3D11::Init(VDTContextD3D11 *parent, const VDTBlendStateDesc& desc) {
	mDesc = desc;

	static const D3D11_BLEND kD3DBlendStateLookup[]={
		D3D11_BLEND_ZERO,
		D3D11_BLEND_ONE,
		D3D11_BLEND_SRC_COLOR,
		D3D11_BLEND_INV_SRC_COLOR,
		D3D11_BLEND_SRC_ALPHA,
		D3D11_BLEND_INV_SRC_ALPHA,
		D3D11_BLEND_DEST_ALPHA,
		D3D11_BLEND_INV_DEST_ALPHA,
		D3D11_BLEND_DEST_COLOR,
		D3D11_BLEND_INV_DEST_COLOR
	};

	static const D3D11_BLEND_OP kD3DBlendOpLookup[]={
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_SUBTRACT,
		D3D11_BLEND_OP_REV_SUBTRACT,
		D3D11_BLEND_OP_MIN,
		D3D11_BLEND_OP_MAX
	};

	D3D11_BLEND_DESC d3ddesc = {};
	d3ddesc.RenderTarget[0].BlendEnable = desc.mbEnable;
	d3ddesc.RenderTarget[0].SrcBlend = kD3DBlendStateLookup[desc.mSrc];
	d3ddesc.RenderTarget[0].DestBlend = kD3DBlendStateLookup[desc.mDst];
    d3ddesc.RenderTarget[0].BlendOp = kD3DBlendOpLookup[desc.mOp];
	d3ddesc.RenderTarget[0].SrcBlendAlpha = d3ddesc.RenderTarget[0].SrcBlend;
    d3ddesc.RenderTarget[0].DestBlendAlpha = d3ddesc.RenderTarget[0].DestBlend;
    d3ddesc.RenderTarget[0].BlendOpAlpha = d3ddesc.RenderTarget[0].BlendOp;
	d3ddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	HRESULT hr = parent->GetDeviceD3D11()->CreateBlendState(&d3ddesc, &mpBlendState);

	if (!hr) {
		parent->ProcessHRESULT(hr);
		return false;
	}

	parent->AddResource(this);
	return true;
}

void VDTBlendStateD3D11::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D11 *>(mpParent)->UnsetBlendState(this);

	if (mpBlendState) {
		mpBlendState->Release();
		mpBlendState = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTBlendStateD3D11::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTRasterizerStateD3D11::VDTRasterizerStateD3D11()
	: mpRastState(NULL)
{
}

VDTRasterizerStateD3D11::~VDTRasterizerStateD3D11() {
	Shutdown();
}

bool VDTRasterizerStateD3D11::Init(VDTContextD3D11 *parent, const VDTRasterizerStateDesc& desc) {
	mDesc = desc;

	D3D11_RASTERIZER_DESC d3ddesc = {};
	d3ddesc.FillMode = D3D11_FILL_SOLID;
    d3ddesc.DepthBias = 0;
    d3ddesc.DepthBiasClamp = 0;
    d3ddesc.SlopeScaledDepthBias = 0;
    d3ddesc.DepthClipEnable = TRUE;
    d3ddesc.ScissorEnable = desc.mbEnableScissor;
    d3ddesc.MultisampleEnable = FALSE;
    d3ddesc.AntialiasedLineEnable = FALSE;

	switch(desc.mCullMode) {
		case kVDTCull_None:
			d3ddesc.CullMode = D3D11_CULL_NONE;
			break;

		case kVDTCull_Front:
			d3ddesc.CullMode = D3D11_CULL_FRONT;
			break;

		case kVDTCull_Back:
			d3ddesc.CullMode = D3D11_CULL_BACK;
			break;
	}

	d3ddesc.FrontCounterClockwise = desc.mbFrontIsCCW;

	HRESULT hr = parent->GetDeviceD3D11()->CreateRasterizerState(&d3ddesc, &mpRastState);
	if (FAILED(hr)) {
		parent->ProcessHRESULT(hr);
		return false;
	}

	parent->AddResource(this);
	return true;
}

void VDTRasterizerStateD3D11::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D11 *>(mpParent)->UnsetRasterizerState(this);

	if (mpRastState) {
		mpRastState->Release();
		mpRastState = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTRasterizerStateD3D11::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTSamplerStateD3D11::VDTSamplerStateD3D11()
	: mpSamplerState(NULL)
{
}

VDTSamplerStateD3D11::~VDTSamplerStateD3D11() {
	Shutdown();
}

bool VDTSamplerStateD3D11::Init(VDTContextD3D11 *parent, const VDTSamplerStateDesc& desc) {
	mDesc = desc;

	static const D3D11_FILTER kD3DFilterLookup[]={
		D3D11_FILTER_MIN_MAG_MIP_POINT,
		D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		D3D11_FILTER_ANISOTROPIC
	};

	static const D3D11_TEXTURE_ADDRESS_MODE kD3DAddressLookup[]={
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_TEXTURE_ADDRESS_WRAP
	};

	D3D11_SAMPLER_DESC d3ddesc = {};
	d3ddesc.Filter = kD3DFilterLookup[desc.mFilterMode];
	d3ddesc.AddressU = kD3DAddressLookup[desc.mAddressU];
    d3ddesc.AddressV = kD3DAddressLookup[desc.mAddressV];
    d3ddesc.AddressW = kD3DAddressLookup[desc.mAddressW];
    d3ddesc.MipLODBias = 0;
    d3ddesc.MaxAnisotropy = 0;
	d3ddesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    d3ddesc.MinLOD = 0;
    d3ddesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT hr = parent->GetDeviceD3D11()->CreateSamplerState(&d3ddesc, &mpSamplerState);
	if (FAILED(hr)) {
		parent->ProcessHRESULT(hr);
		return false;
	}

	parent->AddResource(this);
	return true;
}

void VDTSamplerStateD3D11::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D11 *>(mpParent)->UnsetSamplerState(this);

	if (mpSamplerState) {
		mpSamplerState->Release();
		mpSamplerState = NULL;
	}

	VDTResourceD3D11::Shutdown();
}

bool VDTSamplerStateD3D11::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTSwapChainD3D11::VDTSwapChainD3D11()
	: mpSwapChain(NULL)
	, mpTexture(NULL)
{
}

VDTSwapChainD3D11::~VDTSwapChainD3D11() {
	Shutdown();
}

bool VDTSwapChainD3D11::Init(VDTContextD3D11 *parent, const VDTSwapChainDesc& desc) {
	mDesc = desc;

	DXGI_SWAP_CHAIN_DESC scdesc = {};
    scdesc.BufferDesc.Width = desc.mWidth;
    scdesc.BufferDesc.Height = desc.mHeight;
	scdesc.BufferDesc.RefreshRate.Numerator = 0;
	scdesc.BufferDesc.RefreshRate.Denominator = 0;
	scdesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scdesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scdesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scdesc.SampleDesc.Count = 1;
	scdesc.SampleDesc.Quality = 0;
	scdesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scdesc.BufferCount = 1;
    scdesc.OutputWindow = (HWND)mDesc.mhWindow;
    scdesc.Windowed = TRUE;
	scdesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scdesc.Flags = 0;

	HRESULT hr = parent->GetDXGIFactory()->CreateSwapChain(parent->GetDeviceD3D11(), &scdesc, &mpSwapChain);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	vdrefptr<ID3D11Texture2D> d3dtex;
	hr = mpSwapChain->GetBuffer(0, IID_ID3D11Texture2D, (void **)~d3dtex);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	vdrefptr<VDTTexture2DD3D11> tex(new VDTTexture2DD3D11);
	if (!tex->Init(parent, d3dtex, NULL)) {
		Shutdown();
		return false;
	}

	mpTexture = tex.release();
	return true;
}

void VDTSwapChainD3D11::Shutdown() {
	vdsaferelease <<= mpTexture, mpSwapChain;
}

void VDTSwapChainD3D11::GetDesc(VDTSwapChainDesc& desc) {
	desc = mDesc;
}

IVDTSurface *VDTSwapChainD3D11::GetBackBuffer() {
	return mpTexture ? mpTexture->GetLevelSurface(0) : NULL;
}

void VDTSwapChainD3D11::Present() {
	if (!mpSwapChain)
		return;

	mpSwapChain->Present(0, 0);
}

///////////////////////////////////////////////////////////////////////////////

struct VDTContextD3D11::PrivateData {
	HMODULE mhmodD3D11;

	VDTFenceManagerD3D11 mFenceManager;
};

VDTContextD3D11::VDTContextD3D11()
	: mRefCount(0)
	, mpData(NULL)
	, mpD3DHolder(NULL)
	, mpDXGIFactory(NULL)
	, mpD3DDevice(NULL)
	, mpD3DDeviceContext(NULL)
	, mpVSConstBuffer(NULL)
	, mpPSConstBuffer(NULL)
	, mpVSConstBufferSys(NULL)
	, mpPSConstBufferSys(NULL)
	, mpSwapChain(NULL)
	, mpCurrentRT(NULL)
	, mpCurrentVB(NULL)
	, mCurrentVBOffset(NULL)
	, mCurrentVBStride(NULL)
	, mpCurrentIB(NULL)
	, mpCurrentVP(NULL)
	, mpCurrentFP(NULL)
	, mpCurrentVF(NULL)
	, mpCurrentBS(NULL)
	, mpCurrentRS(NULL)
	, mpDefaultBS(NULL)
	, mpDefaultRS(NULL)
	, mpDefaultSS(NULL)
	, mProfChan("Filter 3D accel")
{
	memset(mpCurrentSamplerStates, 0, sizeof mpCurrentSamplerStates);
	memset(mpCurrentTextures, 0, sizeof mpCurrentTextures);
}

VDTContextD3D11::~VDTContextD3D11() {
	Shutdown();
}

int VDTContextD3D11::AddRef() {
	return ++mRefCount;
}

int VDTContextD3D11::Release() {
	int rc = --mRefCount;

	if (!rc)
		delete this;

	return rc;
}

void *VDTContextD3D11::AsInterface(uint32 iid) {
	if (iid == IVDTProfiler::kTypeID)
		return static_cast<IVDTProfiler *>(this);

	return NULL;
}

bool VDTContextD3D11::Init(ID3D11Device *dev, ID3D11DeviceContext *devctx, IDXGIFactory *factory, IVDRefUnknown *pD3DHolder) {
	mpData = new PrivateData;
	mpData->mhmodD3D11 = VDLoadSystemLibraryW32("D3D11");

	if (mpData->mhmodD3D11) {
		mpBeginEvent = GetProcAddress(mpData->mhmodD3D11, "D3DPERF_BeginEvent");
		mpEndEvent = GetProcAddress(mpData->mhmodD3D11, "D3DPERF_EndEvent");
	}

	switch(dev->GetFeatureLevel()) {
		case D3D_FEATURE_LEVEL_11_0:
			mCaps.mMaxTextureWidth = 16384;
			mCaps.mMaxTextureHeight = 16384;
			break;

		case D3D_FEATURE_LEVEL_10_1:
		case D3D_FEATURE_LEVEL_10_0:
			mCaps.mMaxTextureWidth = 8192;
			mCaps.mMaxTextureHeight = 8192;
			break;

		case D3D_FEATURE_LEVEL_9_3:
			mCaps.mMaxTextureWidth = 4096;
			mCaps.mMaxTextureHeight = 4096;
			break;

		case D3D_FEATURE_LEVEL_9_2:
		case D3D_FEATURE_LEVEL_9_1:
		default:
			mCaps.mMaxTextureWidth = 2048;
			mCaps.mMaxTextureHeight = 2048;
			break;
	}

	mpD3DHolder = pD3DHolder;
	if (mpD3DHolder)
		mpD3DHolder->AddRef();

	mpDXGIFactory = factory;
	mpDXGIFactory->AddRef();

	mpD3DDevice = dev;
	mpD3DDevice->AddRef();

	mpD3DDeviceContext = devctx;
	mpD3DDeviceContext->AddRef();

	mpData->mFenceManager.Init(mpD3DDevice, mpD3DDeviceContext);

	mpDefaultBS = new VDTBlendStateD3D11;
	mpDefaultBS->AddRef();
	mpDefaultBS->Init(this, VDTBlendStateDesc());

	mpDefaultRS = new VDTRasterizerStateD3D11;
	mpDefaultRS->AddRef();
	mpDefaultRS->Init(this, VDTRasterizerStateDesc());

	mpDefaultSS = new VDTSamplerStateD3D11;
	mpDefaultSS->AddRef();
	mpDefaultSS->Init(this, VDTSamplerStateDesc());

	mpCurrentBS = NULL;
	mpCurrentRS = NULL;
	mpCurrentRT = NULL;

	for(int i=0; i<16; ++i)
		mpCurrentSamplerStates[i] = mpDefaultSS;

	D3D11_BUFFER_DESC bufdesc = {};
    bufdesc.ByteWidth = 256 * sizeof(float) * 4;
	bufdesc.Usage = D3D11_USAGE_DEFAULT;
	bufdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufdesc.CPUAccessFlags = 0;
    bufdesc.MiscFlags = 0;
    bufdesc.StructureByteStride = 0;
	mpD3DDevice->CreateBuffer(&bufdesc, NULL, &mpVSConstBuffer);
	mpD3DDevice->CreateBuffer(&bufdesc, NULL, &mpPSConstBuffer);

	bufdesc.Usage = D3D11_USAGE_STAGING;
	bufdesc.BindFlags = 0;
	bufdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	mpD3DDevice->CreateBuffer(&bufdesc, NULL, &mpVSConstBufferSys);
	mpD3DDevice->CreateBuffer(&bufdesc, NULL, &mpPSConstBufferSys);

	mpD3DDeviceContext->VSSetConstantBuffers(0, 1, &mpVSConstBuffer);
	mpD3DDeviceContext->PSSetConstantBuffers(0, 1, &mpPSConstBuffer);

	SetBlendState(NULL);
	SetRasterizerState(NULL);

	D3D11_VIEWPORT vp = {0,0,0,0,0,0};
	mpD3DDeviceContext->RSSetViewports(1, &vp);

	memset(&mViewport, 0, sizeof mViewport);

	return true;
}

void VDTContextD3D11::Shutdown() {
	if (mpD3DDeviceContext)
		mpD3DDeviceContext->ClearState();

	// We need to drop the implicit swap chain first so we don't attempt to re-set it.
	// However, the swap chain itself will trigger an unset, so we must do a swaparoo here.
	IVDTSwapChain *sc = mpSwapChain;
	mpSwapChain = NULL;

	vdsaferelease <<= sc;

	ShutdownAllResources();

	vdsaferelease <<= mpDefaultSS, mpDefaultRS, mpDefaultBS, mpPSConstBuffer, mpVSConstBuffer, mpPSConstBufferSys, mpVSConstBufferSys;

	mpData->mFenceManager.Shutdown();

	vdsaferelease <<= mpDXGIFactory, mpD3DDeviceContext, mpD3DDevice, mpD3DHolder;

	if (mpData) {
		if (mpData->mhmodD3D11)
			FreeLibrary(mpData->mhmodD3D11);

		delete mpData;
		mpData = NULL;
	}

	mpBeginEvent = NULL;
	mpEndEvent = NULL;
}

void VDTContextD3D11::SetImplicitSwapChain(VDTSwapChainD3D11 *sc) {
	mpSwapChain = sc;
	sc->AddRef();

	SetRenderTarget(0, NULL);
}

bool VDTContextD3D11::IsFormatSupportedTexture2D(VDTFormat format) {
	DXGI_FORMAT dxgiFormat = GetSurfaceFormatD3D11(format);
	UINT support = 0;

	HRESULT hr = mpD3DDevice->CheckFormatSupport(dxgiFormat, &support);
	return SUCCEEDED(hr) && (support & D3D11_FORMAT_SUPPORT_TEXTURE2D);
}

bool VDTContextD3D11::CreateReadbackBuffer(uint32 width, uint32 height, VDTFormat format, IVDTReadbackBuffer **ppbuffer) {
	vdrefptr<VDTReadbackBufferD3D11> surf(new VDTReadbackBufferD3D11);

	if (!surf->Init(this, width, height, format))
		return false;

	*ppbuffer = surf.release();
	return true;
}

bool VDTContextD3D11::CreateSurface(uint32 width, uint32 height, VDTFormat format, VDTUsage usage, IVDTSurface **ppsurface) {
	vdrefptr<VDTSurfaceD3D11> surf(new VDTSurfaceD3D11);

	if (!surf->Init(this, width, height, format, usage))
		return false;

	*ppsurface = surf.release();
	return true;
}

bool VDTContextD3D11::CreateTexture2D(uint32 width, uint32 height, VDTFormat format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData, IVDTTexture2D **pptex) {
	vdrefptr<VDTTexture2DD3D11> tex(new VDTTexture2DD3D11);

	if (!tex->Init(this, width, height, format, mipcount, usage, initData))
		return false;

	*pptex = tex.release();
	return true;
}

bool VDTContextD3D11::CreateVertexProgram(VDTProgramFormat format, VDTData data, IVDTVertexProgram **program) {
	vdrefptr<VDTVertexProgramD3D11> vp(new VDTVertexProgramD3D11);

	if (format == kVDTPF_MultiTarget) {
		static const uint32 kVSTargets[]={
			0x1b39ae13,		// vs_4_0_level_9_1
			0x1b39ae11,		// vs_4_0_level_9_3
			0x62d902b4,		// vs_4_0
			0
		};

		if (!VDTExtractMultiTargetProgram(data, kVSTargets, data))
			return false;

		format = kVDTPF_D3D11ByteCode;
	} else if (format != kVDTPF_D3D11ByteCode)
		return false;

	if (!vp->Init(this, format, data.mpData, data.mLength))
		return false;

	*program = vp.release();
	return true;
}

bool VDTContextD3D11::CreateFragmentProgram(VDTProgramFormat format, VDTData data, IVDTFragmentProgram **program) {
	vdrefptr<VDTFragmentProgramD3D11> fp(new VDTFragmentProgramD3D11);

	if (format == kVDTPF_MultiTarget) {
		static const uint32 kPSTargets[]={
			0x301bfb89,		// ps_4_0_level_9_1
			0x301bfb8b,		// ps_4_0_level_9_3
			0x4657d92a,		// ps_4_0
			0
		};

		if (!VDTExtractMultiTargetProgram(data, kPSTargets, data))
			return false;
	} else if (format != kVDTPF_D3D11ByteCode)
		return false;

	if (!fp->Init(this, format, data.mpData, data.mLength))
		return false;

	*program = fp.release();
	return true;
}

bool VDTContextD3D11::CreateVertexFormat(const VDTVertexElement *elements, uint32 count, IVDTVertexProgram *vp, IVDTVertexFormat **format) {
	VDASSERT(vp);

	vdrefptr<VDTVertexFormatD3D11> vf(new VDTVertexFormatD3D11);

	if (!vf->Init(this, elements, count, static_cast<VDTVertexProgramD3D11 *>(vp)))
		return false;

	*format = vf.release();
	return true;
}

bool VDTContextD3D11::CreateVertexBuffer(uint32 size, bool dynamic, const void *initData, IVDTVertexBuffer **ppbuffer) {
	vdrefptr<VDTVertexBufferD3D11> vb(new VDTVertexBufferD3D11);

	if (!vb->Init(this, size, dynamic, initData))
		return false;

	*ppbuffer = vb.release();
	return true;
}

bool VDTContextD3D11::CreateIndexBuffer(uint32 size, bool index32, bool dynamic, const void *initData, IVDTIndexBuffer **ppbuffer) {
	vdrefptr<VDTIndexBufferD3D11> ib(new VDTIndexBufferD3D11);

	if (!ib->Init(this, size, index32, dynamic, initData))
		return false;

	*ppbuffer = ib.release();
	return true;
}

bool VDTContextD3D11::CreateBlendState(const VDTBlendStateDesc& desc, IVDTBlendState **state) {
	vdrefptr<VDTBlendStateD3D11> bs(new VDTBlendStateD3D11);

	if (!bs->Init(this, desc))
		return false;

	*state = bs.release();
	return true;
}

bool VDTContextD3D11::CreateRasterizerState(const VDTRasterizerStateDesc& desc, IVDTRasterizerState **state) {
	vdrefptr<VDTRasterizerStateD3D11> rs(new VDTRasterizerStateD3D11);

	if (!rs->Init(this, desc))
		return false;

	*state = rs.release();
	return true;
}

bool VDTContextD3D11::CreateSamplerState(const VDTSamplerStateDesc& desc, IVDTSamplerState **state) {
	vdrefptr<VDTSamplerStateD3D11> ss(new VDTSamplerStateD3D11);

	if (!ss->Init(this, desc))
		return false;

	*state = ss.release();
	return true;
}

bool VDTContextD3D11::CreateSwapChain(const VDTSwapChainDesc& desc, IVDTSwapChain **swapChain) {
	vdrefptr<VDTSwapChainD3D11> sc(new VDTSwapChainD3D11);

	if (!sc->Init(this, desc))
		return false;

	*swapChain = sc.release();
	return true;
}

IVDTSurface *VDTContextD3D11::GetRenderTarget(uint32 index) const {
	return index ? NULL : mpCurrentRT;
}

void VDTContextD3D11::SetVertexFormat(IVDTVertexFormat *format) {
	if (format == mpCurrentVF)
		return;

	mpCurrentVF = static_cast<VDTVertexFormatD3D11 *>(format);

	mpD3DDeviceContext->IASetInputLayout(mpCurrentVF ? mpCurrentVF->mpVF : NULL);
}

void VDTContextD3D11::SetVertexProgram(IVDTVertexProgram *program) {
	if (program == mpCurrentVP)
		return;

	mpCurrentVP = static_cast<VDTVertexProgramD3D11 *>(program);

	mpD3DDeviceContext->VSSetShader(mpCurrentVP ? mpCurrentVP->mpVS : NULL, NULL, 0);
}

void VDTContextD3D11::SetFragmentProgram(IVDTFragmentProgram *program) {
	if (program == mpCurrentFP)
		return;

	mpCurrentFP = static_cast<VDTFragmentProgramD3D11 *>(program);

	mpD3DDeviceContext->PSSetShader(mpCurrentFP ? mpCurrentFP->mpPS : NULL, NULL, 0);
}

void VDTContextD3D11::SetVertexStream(uint32 index, IVDTVertexBuffer *buffer, uint32 offset, uint32 stride) {
	VDASSERT(index == 0);

	if (buffer == mpCurrentVB && offset == mCurrentVBOffset && offset == mCurrentVBStride)
		return;

	mpCurrentVB = static_cast<VDTVertexBufferD3D11 *>(buffer);
	mCurrentVBOffset = offset;
	mCurrentVBStride = stride;

	ID3D11Buffer *pBuffer = mpCurrentVB ? mpCurrentVB->mpVB : NULL;
	UINT d3doffset = offset;
	UINT d3dstride = stride;
	mpD3DDeviceContext->IASetVertexBuffers(index, 1, &pBuffer, &d3dstride, &d3doffset);
}

void VDTContextD3D11::SetIndexStream(IVDTIndexBuffer *buffer) {
	if (buffer == mpCurrentIB)
		return;

	mpCurrentIB = static_cast<VDTIndexBufferD3D11 *>(buffer);

	mpD3DDeviceContext->IASetIndexBuffer(mpCurrentIB ? mpCurrentIB->mpIB : NULL, mpCurrentIB->mbIndex32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT, 0);
}

void VDTContextD3D11::SetRenderTarget(uint32 index, IVDTSurface *surface) {
	VDASSERT(index == 0);

	if (index == 0 && !surface && mpSwapChain)
		surface = mpSwapChain->GetBackBuffer();

	if (mpCurrentRT == surface)
		return;

	mpCurrentRT = static_cast<VDTSurfaceD3D11 *>(surface);

	ID3D11RenderTargetView *rtv = NULL;
	uint32 w = 1;
	uint32 h = 1;

	if (mpCurrentRT) {
		rtv = mpCurrentRT->mpRTView;

		VDASSERT(rtv);

		VDTSurfaceDesc desc;
		mpCurrentRT->GetDesc(desc);

		w = desc.mWidth;
		h = desc.mHeight;
	}

	mpD3DDeviceContext->OMSetRenderTargets(1, &rtv, NULL);
}

void VDTContextD3D11::SetBlendState(IVDTBlendState *state) {
	if (!state)
		state = mpDefaultBS;

	if (mpCurrentBS == state)
		return;

	mpCurrentBS = static_cast<VDTBlendStateD3D11 *>(state);

	float blendfactor[4] = {0};
	mpD3DDeviceContext->OMSetBlendState(mpCurrentBS->mpBlendState, blendfactor, 0xFFFFFFFF);
}

void VDTContextD3D11::SetSamplerStates(uint32 baseIndex, uint32 count, IVDTSamplerState *const *states) {
	VDASSERT(baseIndex <= 16 && 16 - baseIndex >= count);

	for(uint32 i=0; i<count; ++i) {
		uint32 stage = baseIndex + i;
		VDTSamplerStateD3D11 *state = static_cast<VDTSamplerStateD3D11 *>(states[i]);
		if (!state)
			state = mpDefaultSS;

		if (mpCurrentSamplerStates[stage] != state) {
			mpCurrentSamplerStates[stage] = state;
			
			ID3D11SamplerState *d3dss = state->mpSamplerState;
			mpD3DDeviceContext->PSSetSamplers(stage, 1, &d3dss);
		}
	}
}

void VDTContextD3D11::SetTextures(uint32 baseIndex, uint32 count, IVDTTexture *const *textures) {
	VDASSERT(baseIndex <= 16 && 16 - baseIndex >= count);

	for(uint32 i=0; i<count; ++i) {
		uint32 stage = baseIndex + i;
		IVDTTexture *tex = textures[i];

		if (mpCurrentTextures[stage] != tex) {
			mpCurrentTextures[stage] = tex;

			ID3D11ShaderResourceView *pSRV = tex ? (ID3D11ShaderResourceView *)tex->AsInterface(VDTTextureD3D11::kTypeD3DShaderResView) : NULL;
			mpD3DDeviceContext->PSSetShaderResources(stage, 1, &pSRV);
		}
	}
}

void VDTContextD3D11::SetRasterizerState(IVDTRasterizerState *state) {
	if (!state)
		state = mpDefaultRS;

	if (mpCurrentRS == state)
		return;

	mpCurrentRS = static_cast<VDTRasterizerStateD3D11 *>(state);

	mpD3DDeviceContext->RSSetState(mpCurrentRS->mpRastState);
}

VDTViewport VDTContextD3D11::GetViewport() {
	return mViewport;
}

void VDTContextD3D11::SetViewport(const VDTViewport& vp) {
	mViewport = vp;

	D3D11_VIEWPORT d3dvp;
	d3dvp.TopLeftX = (float)vp.mX;
	d3dvp.TopLeftY = (float)vp.mY;
	d3dvp.Width = (float)vp.mWidth;
	d3dvp.Height = (float)vp.mHeight;
	d3dvp.MinDepth = vp.mMinZ;
	d3dvp.MaxDepth = vp.mMaxZ;

	mpD3DDeviceContext->RSSetViewports(1, &d3dvp);
}

void VDTContextD3D11::SetVertexProgramConstF(uint32 baseIndex, uint32 count, const float *data) {
	D3D11_MAPPED_SUBRESOURCE info;
	HRESULT hr = mpD3DDeviceContext->Map(mpVSConstBufferSys, 0, D3D11_MAP_WRITE, 0, &info);

	if (FAILED(hr))
		ProcessHRESULT(hr);
	else {
		memcpy((char *)info.pData + baseIndex*16, data, count*16);
		mpD3DDeviceContext->Unmap(mpVSConstBufferSys, 0);

		const D3D11_BOX box = { baseIndex * 16, 0, 0, (baseIndex + count) * 16, 1, 1 };
		mpD3DDeviceContext->CopySubresourceRegion(mpVSConstBuffer, 0, 0, 0, 0, mpVSConstBufferSys, 0, &box);
	}
}

void VDTContextD3D11::SetFragmentProgramConstF(uint32 baseIndex, uint32 count, const float *data) {
	D3D11_MAPPED_SUBRESOURCE info;
	HRESULT hr = mpD3DDeviceContext->Map(mpPSConstBufferSys, 0, D3D11_MAP_WRITE, 0, &info);

	if (FAILED(hr))
		ProcessHRESULT(hr);
	else {
		memcpy((char *)info.pData + baseIndex*16, data, count*16);
		mpD3DDeviceContext->Unmap(mpPSConstBufferSys, 0);

		const D3D11_BOX box = { baseIndex * 16, 0, 0, (baseIndex + count) * 16, 1, 1 };
		mpD3DDeviceContext->CopySubresourceRegion(mpPSConstBuffer, 0, 0, 0, 0, mpPSConstBufferSys, 0, &box);
	}
}

void VDTContextD3D11::Clear(VDTClearFlags clearFlags, uint32 color, float depth, uint32 stencil) {
	DWORD flags = 0;

	if (clearFlags & kVDTClear_Color) {
		float color32[4];

		color32[0] = (int)(color >> 16) / 255.0f;
		color32[1] = (int)(color >>  8) / 255.0f;
		color32[2] = (int)(color >>  0) / 255.0f;
		color32[3] = (int)(color >> 24) / 255.0f;

		mpD3DDeviceContext->ClearRenderTargetView(mpCurrentRT->mpRTView, color32);
	}
}

namespace {
	const D3D11_PRIMITIVE_TOPOLOGY kPTLookup[]={
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
	};
}

void VDTContextD3D11::DrawPrimitive(VDTPrimitiveType type, uint32 startVertex, uint32 primitiveCount) {
	mpD3DDeviceContext->IASetPrimitiveTopology(kPTLookup[type]);

	switch(type) {
		case kVDTPT_Triangles:
			mpD3DDeviceContext->Draw(primitiveCount * 3, startVertex);
			break;

		case kVDTPT_TriangleStrip:
			mpD3DDeviceContext->Draw(primitiveCount + 2, startVertex);
			break;
	}
}

void VDTContextD3D11::DrawIndexedPrimitive(VDTPrimitiveType type, uint32 baseVertexIndex, uint32 minVertex, uint32 vertexCount, uint32 startIndex, uint32 primitiveCount) {
	mpD3DDeviceContext->IASetPrimitiveTopology(kPTLookup[type]);

	uint32 indexCount = 0;
	switch(type) {
		case kVDTPT_Triangles:
			indexCount = primitiveCount * 3;
			break;

		case kVDTPT_TriangleStrip:
			indexCount = primitiveCount + 2;
			break;
	}

	mpD3DDeviceContext->DrawIndexed(indexCount, startIndex, baseVertexIndex);
}

uint32 VDTContextD3D11::InsertFence() {
	return mpData->mFenceManager.InsertFence();
}

bool VDTContextD3D11::CheckFence(uint32 id) {
	return mpData->mFenceManager.CheckFence(id);
}

bool VDTContextD3D11::RecoverDevice() {
	return true;
}

bool VDTContextD3D11::OpenScene() {
	return true;
}

bool VDTContextD3D11::CloseScene() {
	return true;
}

uint32 VDTContextD3D11::GetDeviceLossCounter() const {
	return 0;
}

void VDTContextD3D11::Present() {
	if (!mpSwapChain)
		return;

	mpSwapChain->Present();
}

void VDTContextD3D11::BeginScope(uint32 color, const char *message) {
	mProfChan.Begin(color, message);
}

void VDTContextD3D11::EndScope() {
	mProfChan.End();
}

VDRTProfileChannel *VDTContextD3D11::GetProfileChannel() {
	return &mProfChan;
}

void VDTContextD3D11::UnsetVertexFormat(IVDTVertexFormat *format) {
	if (mpCurrentVF == format)
		SetVertexFormat(NULL);
}

void VDTContextD3D11::UnsetVertexProgram(IVDTVertexProgram *program) {
	if (mpCurrentVP == program)
		SetVertexProgram(NULL);
}

void VDTContextD3D11::UnsetFragmentProgram(IVDTFragmentProgram *program) {
	if (mpCurrentFP == program)
		SetFragmentProgram(NULL);
}

void VDTContextD3D11::UnsetVertexBuffer(IVDTVertexBuffer *buffer) {
	if (mpCurrentVB == buffer)
		SetVertexStream(0, NULL, 0, 0);
}

void VDTContextD3D11::UnsetIndexBuffer(IVDTIndexBuffer *buffer) {
	if (mpCurrentIB == buffer)
		SetIndexStream(NULL);
}

void VDTContextD3D11::UnsetRenderTarget(IVDTSurface *surface) {
	if (mpCurrentRT == surface)
		SetRenderTarget(0, NULL);
}

void VDTContextD3D11::UnsetBlendState(IVDTBlendState *state) {
	if (mpCurrentBS == state && state != mpDefaultBS)
		SetBlendState(NULL);
}

void VDTContextD3D11::UnsetRasterizerState(IVDTRasterizerState *state) {
	if (mpCurrentRS == state && state != mpDefaultRS)
		SetRasterizerState(NULL);
}

void VDTContextD3D11::UnsetSamplerState(IVDTSamplerState *state) {
	if (state == mpDefaultSS)
		return;

	for(int i=0; i<16; ++i) {
		if (mpCurrentSamplerStates[i] == state) {
			IVDTSamplerState *ssnull = NULL;
			SetSamplerStates(i, 1, &ssnull);
		}
	}
}

void VDTContextD3D11::UnsetTexture(IVDTTexture *tex) {
	for(int i=0; i<16; ++i) {
		if (mpCurrentTextures[i] == tex) {
			IVDTTexture *tex = NULL;
			SetTextures(i, 1, &tex);
		}
	}
}

void VDTContextD3D11::ProcessHRESULT(uint32 hr) {
}

///////////////////////////////////////////////////////////////////////////////

class VDD3D11Holder : public vdrefcounted<IVDRefUnknown> {
public:
	VDD3D11Holder();
	~VDD3D11Holder();

	void *AsInterface(uint32 iid);

	typedef HRESULT (APIENTRY *CreateDeviceFn)(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext);
	typedef HRESULT (APIENTRY *CreateDXGIFactoryFn)(REFIID riid, void **ppFactory);

	CreateDeviceFn GetCreateDeviceFn() const { return mpCreateDeviceFn; }
	CreateDXGIFactoryFn GetCreateDXGIFactoryFn() const { return mpCreateDXGIFactoryFn; }

	bool Init();
	void Shutdown();

protected:
	HMODULE mhmodDXGI;
	HMODULE mhmodD3D11;
	CreateDXGIFactoryFn mpCreateDXGIFactoryFn;
	CreateDeviceFn mpCreateDeviceFn;
};

VDD3D11Holder::VDD3D11Holder()
	: mhmodDXGI(NULL)
	, mhmodD3D11(NULL)
	, mpCreateDXGIFactoryFn(NULL)
	, mpCreateDeviceFn(NULL)
{
}

VDD3D11Holder::~VDD3D11Holder() {
	Shutdown();
}

void *VDD3D11Holder::AsInterface(uint32 iid) {
	return NULL;
}

bool VDD3D11Holder::Init() {
	if (!mhmodDXGI) {
		mhmodDXGI = VDLoadSystemLibraryW32("dxgi");

		if (!mhmodDXGI) {
			Shutdown();
			return false;
		}
	}

	if (!mpCreateDXGIFactoryFn) {
		mpCreateDXGIFactoryFn = (CreateDXGIFactoryFn)GetProcAddress(mhmodDXGI, "CreateDXGIFactory1");

		if (!mpCreateDXGIFactoryFn) {
			Shutdown();
			return false;
		}
	}

	if (!mhmodD3D11) {
		mhmodD3D11 = VDLoadSystemLibraryW32("D3D11");

		if (!mhmodD3D11) {
			Shutdown();
			return false;
		}
	}

	if (!mpCreateDeviceFn) {
		mpCreateDeviceFn = (CreateDeviceFn)GetProcAddress(mhmodD3D11, "D3D11CreateDevice");
		if (!mpCreateDeviceFn) {
			Shutdown();
			return false;
		}
	}

	return true;
}

void VDD3D11Holder::Shutdown() {
	mpCreateDeviceFn = NULL;

	if (mhmodD3D11) {
		FreeLibrary(mhmodD3D11);
		mhmodD3D11 = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

bool VDTCreateContextD3D11(IVDTContext **ppctx) {
	vdrefptr<VDD3D11Holder> holder(new VDD3D11Holder);

	if (!holder->Init())
		return false;

	vdrefptr<IDXGIFactory1> factory;
	HRESULT hr = holder->GetCreateDXGIFactoryFn()(IID_IDXGIFactory1, (void **)~factory);

	if (FAILED(hr))
		return false;

	vdrefptr<IDXGIAdapter1> adapter;
	hr = factory->EnumAdapters1(0, ~adapter);
	if (FAILED(hr))
		return false;

	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_UNKNOWN;

	vdrefptr<ID3D11Device> dev;
	vdrefptr<ID3D11DeviceContext> devctx;
	UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL actualLevel;
	hr = holder->GetCreateDeviceFn()(adapter, driverType, NULL, flags, NULL, 0, D3D11_SDK_VERSION, ~dev, &actualLevel, ~devctx);
	if (FAILED(hr))
		return false;

	vdrefptr<VDTContextD3D11> ctx(new VDTContextD3D11);
	if (!ctx->Init(dev, devctx, factory, holder))
		return false;

	*ppctx = ctx.release();
	return true;
}

bool VDTCreateContextD3D11(int width, int height, int refresh, bool fullscreen, bool vsync, void *hwnd, IVDTContext **ppctx) {
	
	vdrefptr<IVDTContext> ctx;
	if (!VDTCreateContextD3D11(~ctx))
		return false;

	VDTSwapChainDesc desc = {};
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mhWindow = hwnd;

	vdrefptr<IVDTSwapChain> sc;
	if (!ctx->CreateSwapChain(desc, ~sc))
		return false;

	static_cast<VDTContextD3D11 *>(&*ctx)->SetImplicitSwapChain(static_cast<VDTSwapChainD3D11 *>(&*sc));

	*ppctx = ctx.release();
	return true;
}

bool VDTCreateContextD3D11(ID3D11Device *dev, ID3D11DeviceContext *devctx, IDXGIFactory *factory, IVDTContext **ppctx) {
	vdrefptr<VDTContextD3D11> ctx(new VDTContextD3D11);

	if (!ctx->Init(dev, devctx, factory, NULL))
		return false;

	*ppctx = ctx.release();
	return true;
}

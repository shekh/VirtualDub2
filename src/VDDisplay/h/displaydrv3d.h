#ifndef f_VD2_VDDISPLAY_DISPLAYDRV3D_H
#define f_VD2_VDDISPLAY_DISPLAYDRV3D_H

#include <vd2/VDDisplay/displaydrv.h>

struct VDPixmap;
class IVDTContext;
class IVDTTexture2D;

struct VDDisplayVertex3D {
	float x;
	float y;
	float z;
	float u;
	float v;
};

struct VDDisplayVertex2T3D {
	float x;
	float y;
	float z;
	float u0;
	float v0;
	float u1;
	float v1;
};

struct VDDisplayVertex3T3D {
	float x;
	float y;
	float z;
	float u0;
	float v0;
	float u1;
	float v1;
	float u2;
	float v2;
};

///////////////////////////////////////////////////////////////////////////

class VDDisplayNodeContext3D {
	VDDisplayNodeContext3D(const VDDisplayNodeContext3D&);
	VDDisplayNodeContext3D& operator=(const VDDisplayNodeContext3D&);
public:
	VDDisplayNodeContext3D();
	~VDDisplayNodeContext3D();

	bool Init(IVDTContext& ctx);
	void Shutdown();

public:
	IVDTVertexFormat *mpVFTexture;
	IVDTVertexFormat *mpVFTexture2T;
	IVDTVertexFormat *mpVFTexture3T;
	IVDTVertexProgram *mpVPTexture;
	IVDTVertexProgram *mpVPTexture2T;
	IVDTVertexProgram *mpVPTexture3T;
	IVDTFragmentProgram *mpFPBlit;
	IVDTSamplerState *mpSSPoint;
	IVDTSamplerState *mpSSBilinear;

	VDTFormat mBGRAFormat;
};

///////////////////////////////////////////////////////////////////////////

class VDDisplayNode3D : public vdrefcount {
public:
	virtual ~VDDisplayNode3D();

	virtual void Draw(IVDTContext& ctx, VDDisplayNodeContext3D& dctx) = 0;
};

///////////////////////////////////////////////////////////////////////////

class VDDisplayImageNode3D : public VDDisplayNode3D {
public:
	VDDisplayImageNode3D();
	~VDDisplayImageNode3D();

	bool CanStretch() const;
	void SetBilinear(bool enabled) { mbBilinear = enabled; }

	bool Init(IVDTContext& ctx, VDDisplayNodeContext3D& dctx, uint32 w, uint32 h, uint32 format);
	void Shutdown();

	void Load(const VDPixmap& px);

	void Draw(IVDTContext& ctx, VDDisplayNodeContext3D& dctx);

private:
	enum RenderMode {
		kRenderMode_Blit,
		kRenderMode_BlitY,
		kRenderMode_BlitYCbCr,
		kRenderMode_BlitPal8,
		kRenderMode_BlitUYVY,
		kRenderMode_BlitRGB16,
		kRenderMode_BlitRGB16Direct,
		kRenderMode_BlitRGB24
	};

	IVDTTexture2D *mpImageTex[3];
	IVDTTexture2D *mpPaletteTex;
	IVDTVertexFormat *mpVF;
	IVDTVertexProgram *mpVP;
	IVDTFragmentProgram *mpFP;
	IVDTVertexBuffer *mpVB;

	RenderMode	mRenderMode;
	bool	mbRenderSwapRB;
	bool	mbRender2T;
	bool	mbBilinear;
	uint32	mTexWidth;
	uint32	mTexHeight;
	uint32	mTex2Width;
	uint32	mTex2Height;
};

///////////////////////////////////////////////////////////////////////////

class VDDisplayBufferNode3D : public VDDisplayNode3D {
public:
	VDDisplayBufferNode3D();
	~VDDisplayBufferNode3D();

	bool Init(IVDTContext& ctx, VDDisplayNodeContext3D& dctx, uint32 w, uint32 h, bool linear, VDDisplayNode3D *child);
	void Shutdown();

	void Draw(IVDTContext& ctx, VDDisplayNodeContext3D& dctx);

private:
	struct Vertex;

	IVDTVertexBuffer *mpVB;
	IVDTTexture2D *mpRTT;
	VDDisplayNode3D *mpChildNode;
	bool mbLinear;
};

///////////////////////////////////////////////////////////////////////////

class VDDisplayStretchNode3D : public VDDisplayNode3D {
public:
	VDDisplayStretchNode3D();
	~VDDisplayStretchNode3D();

	bool Init(IVDTContext& ctx, VDDisplayNodeContext3D& dctx, uint32 srcw, uint32 srch, uint32 dstw, uint32 dsth, VDDisplayNode3D *child);
	void Shutdown();

	void Draw(IVDTContext& ctx, VDDisplayNodeContext3D& dctx);

private:
	struct Vertex;

	IVDTVertexProgram *mpVP;
	IVDTFragmentProgram *mpFP;
	IVDTVertexFormat *mpVF;
	IVDTVertexBuffer *mpVB;

	IVDTTexture2D *mpRTTChild;
	IVDTTexture2D *mpRTTHoriz;
	IVDTTexture2D *mpFilterTex;
	VDDisplayNode3D *mpChildNode;

	uint32	mSrcW;
	uint32	mSrcH;
	uint32	mDstW;
	uint32	mDstH;
};

///////////////////////////////////////////////////////////////////////////

class VDDisplayDriver3D : public VDVideoDisplayMinidriver {
	VDDisplayDriver3D(const VDDisplayDriver3D&);
	VDDisplayDriver3D& operator=(const VDDisplayDriver3D&);
public:
	VDDisplayDriver3D();
	~VDDisplayDriver3D();

	virtual bool Init(HWND hwnd, HMONITOR hmonitor, const VDVideoDisplaySourceInfo& info);
	virtual void Shutdown();

	virtual bool ModifySource(const VDVideoDisplaySourceInfo& info);

	virtual void SetFilterMode(FilterMode mode);

	virtual bool IsValid();
	virtual bool Resize(int w, int h);
	virtual bool Update(UpdateMode);
	virtual void Refresh(UpdateMode);
	virtual bool Paint(HDC hdc, const RECT& rClient, UpdateMode lastUpdateMode);

private:
	bool CreateImageNode();
	void DestroyImageNode();
	bool RebuildTree();

	HWND mhwnd;
	IVDTContext *mpContext;
	IVDTSwapChain *mpSwapChain;
	VDDisplayImageNode3D *mpImageNode;
	VDDisplayNode3D *mpRootNode;

	FilterMode mFilterMode;
	bool mbCompositionTreeDirty;

	VDVideoDisplaySourceInfo mSource;

	VDDisplayNodeContext3D mDisplayNodeContext;
};

#endif

#ifndef f_VD2_VDCAPTURE_SCREENGRABBERDXGI12_H
#define f_VD2_VDCAPTURE_SCREENGRABBERDXGI12_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <d3d11.h>
#include <vd2/VDCapture/win32/api_dxgi.h>
#include <vd2/VDCapture/ScreenGrabber.h>

class IVDResamplerFilter;

struct VDScreenGrabberDXGI12 : public IVDScreenGrabber {
public:
	VDScreenGrabberDXGI12();
	~VDScreenGrabberDXGI12();

	bool Init(IVDScreenGrabberCallback *cb);
	void Shutdown();

	bool InitCapture(uint32 srcw, uint32 srch, uint32 dstw, uint32 dsth, VDScreenGrabberFormat format);
	void ShutdownCapture();

	void SetCaptureOffset(int x, int y);
	void SetCapturePointer(bool enable);

	uint64 GetCurrentTimestamp();
	sint64 ConvertTimestampDelta(uint64 t, uint64 base);

	bool InitDisplay(HWND hwndParent, bool preview);
	void ShutdownDisplay();

	void SetDisplayVisible(bool visible);
	void SetDisplayArea(const vdrect32& area);

	bool AcquireFrame(bool dispatch);

protected:
	void UpdateFrame(const DXGI_OUTDUPL_FRAME_INFO& frameInfo, int x, int y, bool dispatch);

	bool InitDXGI();
	void ShutdownDXGI();

	bool InitDevice();
	void ShutdownDevice();

	bool InitRescale(uint32 srcw, uint32 srch, uint32 dstw, uint32 dsth);
	void ShutdownRescale();

	void CreateFilter(uint32 *dst, uint32 dstw, uint32 srcw, const IVDResamplerFilter& filter);

	bool InitPointer();
	void ShutdownPointer();
	void UpdatePointer();
	void RenderPointer(int x, int y);

	bool InitDisplayInternal(VDZHWND hwndParent);
	void ShutdownDisplayInternal();
	bool InitDisplayBuffers();
	void ShutdownDisplayBuffers();
	void ResizeDisplaySwapChain(int w, int h);
	void Display();

	LRESULT DisplayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	IVDScreenGrabberCallback *mpCB;

	// x
	sint32 mOutputW;
	sint32 mOutputH;
	sint32 mSrcW;
	sint32 mSrcH;
	sint32 mDstW;
	sint32 mDstH;
	VDScreenGrabberFormat mCaptureFormat;
	sint32	mCaptureX;
	sint32	mCaptureY;

	sint32	mPointerX;
	sint32	mPointerY;
	bool	mbPointerVisible;
	bool	mbCapturePointer;

	// inited with DXGI
	HMODULE mhmodDXGI;
	HMODULE mhmodD3D11;
	IDXGIFactory1 *mpFactory;
	IDXGIAdapter1 *mpAdapter;

	// inited with device
	IDXGIOutputDuplication *mpOutDup;
	ID3D11Device *mpDevice;
	ID3D11DeviceContext *mpDevCtx;
	ID3D11SamplerState *mpDownscaleSamplerState;
	ID3D11VertexShader *mpDownscaleVS;
	ID3D11InputLayout *mpGeometryIL;
	ID3D11Buffer *mpGeometryVB;
	ID3D11BlendState *mpBlendStateDefault;
	ID3D11RasterizerState *mpRasterizerStateDefault;
	ID3D11DepthStencilState *mpDepthStencilStateDefault;

	// source
	ID3D11Texture2D *mpImageTex;
	ID3D11ShaderResourceView *mpImageTexSRV;
	ID3D11RenderTargetView *mpImageTexRTV;

	// pointer rendering
	ID3D11Texture2D *mpPointerImageTex;
	ID3D11ShaderResourceView *mpPointerImageSRV;
	DXGI_OUTDUPL_POINTER_SHAPE_INFO mPointerShapeInfo;
	ID3D11Buffer *mpPointerConstBuf;
	ID3D11BlendState *mpPointerBlendState;
	ID3D11BlendState *mpPointerMaskBlendState;
	ID3D11VertexShader *mpPointerVS;
	ID3D11PixelShader *mpPointerPSBlend;
	ID3D11PixelShader *mpPointerPSMaskA0;
	ID3D11PixelShader *mpPointerPSMaskA1;

	// rescale
	ID3D11PixelShader *mpDownscaleHorizPS;
	ID3D11PixelShader *mpDownscaleVertPS;
	ID3D11RenderTargetView *mpDownscaleIntRTV;
	ID3D11ShaderResourceView *mpDownscaleIntSRV;
	ID3D11Texture2D *mpDownscaleIntTex;
	ID3D11RenderTargetView *mpDownscaleResultRTV;
	ID3D11ShaderResourceView *mpDownscaleResultSRV;
	ID3D11Texture2D *mpDownscaleResultTex;
	ID3D11Texture2D *mpDownscaleHorizCoeffTex;
	ID3D11ShaderResourceView *mpDownscaleHorizCoeffSRV;
	ID3D11Texture2D *mpDownscaleVertCoeffTex;
	ID3D11ShaderResourceView *mpDownscaleVertCoeffSRV;
	ID3D11Buffer *mpDownscaleConstBufHoriz;
	ID3D11Buffer *mpDownscaleConstBufVert;

	// color conversion
	ID3D11SamplerState *mpColorConvSamplerState;
	ID3D11VertexShader *mpColorConvVSLuma;
	ID3D11PixelShader *mpColorConvPSLuma;
	ID3D11VertexShader *mpColorConvVSChroma;
	ID3D11PixelShader *mpColorConvPSChroma;
	ID3D11Buffer *mpColorConvConstBufY;
	ID3D11Buffer *mpColorConvConstBufCb;
	ID3D11Buffer *mpColorConvConstBufCr;
	ID3D11Texture2D *mpColorConvResultTex;
	ID3D11ShaderResourceView *mpColorConvResultSRV;
	ID3D11RenderTargetView *mpColorConvResultRTV;
	ID3D11Buffer *mpColorConvVB;
	ID3D11Buffer *mpColorConvIB;

	// readback
	ID3D11Texture2D *mpReadbackTex;

	vdblock<uint8> mPointerShapeBuffer;
	vdblock<uint32> mPointerImageBuffer;

	// display
	ATOM mDisplayWndClass;
	WNDPROC mDisplayWndProc;
	HWND mhwndDisplay;
	HWND mhwndDisplayParent;
	vdrect32 mDisplayArea;
	bool mbDisplayVisible;
	ID3D11InputLayout *mpDisplayIL;
	ID3D11Buffer *mpDisplayVB;
	ID3D11Texture2D *mpDisplaySrcTex;
	ID3D11ShaderResourceView *mpDisplaySrcTexSRV;
	ID3D11SamplerState *mpDisplaySamplerState;
	ID3D11RasterizerState *mpDisplayRasterizerState;
	ID3D11PixelShader *mpDisplayPS;
	ID3D11VertexShader *mpDisplayVS;
	ID3D11Texture2D *mpDisplayRT;
	ID3D11RenderTargetView *mpDisplayRTV;
	IDXGISwapChain *mpDisplaySwapChain;

	bool mbDisplayFramePending;
};

#endif

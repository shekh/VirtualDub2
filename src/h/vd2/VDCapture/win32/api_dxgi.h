#ifndef f_VD2_VDCAPTURE_WIN32_API_DXGI_H
#define f_VD2_VDCAPTURE_WIN32_API_DXGI_H

#include <dxgi.h>

///////////////////////////////////////////////////////////////////////////

struct IDXGIOutputDuplication;

///////////////////////////////////////////////////////////////////////////
//
// DXGI 1.2 API
//
///////////////////////////////////////////////////////////////////////////

#define DXGI_ERROR_WAIT_TIMEOUT (0x887A0027)

typedef struct _DXGI_MODE_DESC1 {
	UINT Width;
	UINT Height;
	DXGI_RATIONAL RefreshRate;
	DXGI_FORMAT Format;
	DXGI_MODE_SCANLINE_ORDER ScanlineOrdering;
	DXGI_MODE_SCALING Scaling;
	BOOL Stereo;
} DXGI_MODE_DESC1;

// {00cddea8-939b-4b83-a340-a685226666cc}
DEFINE_GUID(IID_IDXGIOutput1, 0x00cddea8, 0x939b, 0x4b83, 0xa3, 0x40, 0xa6, 0x85, 0x22, 0x66, 0x66, 0xcc);

struct IDXGIOutput1 : public IDXGIOutput {
public:
	virtual HRESULT STDMETHODCALLTYPE GetDisplayModeList1(DXGI_FORMAT EnumFormat, UINT Flags, UINT *pNumModes, DXGI_MODE_DESC1 *pDesc) = 0;
	virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode1(const DXGI_MODE_DESC1 *pModeToMatch, DXGI_MODE_DESC1 *pClosestMatch, IUnknown *pConcernedDevice) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData1(IDXGIResource *pDestination) = 0;
	virtual HRESULT STDMETHODCALLTYPE DuplicateOutput(IUnknown *pDevice, IDXGIOutputDuplication **ppOutputDuplication) = 0;
};

///////////////////////////////////////////////////////////////////////////
//
// DXGI 1.2 Output Duplication API
//
///////////////////////////////////////////////////////////////////////////

typedef enum _DXGI_OUTDUPL_POINTER_SHAPE_TYPE {
	DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME = 0x1,
	DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR = 0x2,
	DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR = 0x4
} DXGI_OUTDUPL_POINTER_SHAPE_TYPE;

typedef struct _DXGI_OUTDUPL_POINTER_POSITION {
	POINT Position;
	BOOL Visible;
} DXGI_OUTDUPL_POINTER_POSITION;

typedef struct _DXGI_OUTDUPL_POINTER_SHAPE_INFO {
	UINT Type;
	UINT Width;
	UINT Height;
	UINT Pitch;
	POINT HotSpot;
} DXGI_OUTDUPL_POINTER_SHAPE_INFO;

typedef struct _DXGI_OUTDUPL_FRAME_INFO {
	LARGE_INTEGER LastPresentTime;
	LARGE_INTEGER LastMouseUpdateTime;
	UINT AccumulatedFrames;
	BOOL RectsCoalesced;
	BOOL ProtectedContextMaskedOut;
	DXGI_OUTDUPL_POINTER_POSITION PointerPosition;
	UINT TotalMetadataBufferSize;
	UINT PointerShapeBufferSize;
} DXGI_OUTDUPL_FRAME_INFO;

typedef struct _DXGI_OUTDUPL_DESC {
	DXGI_MODE_DESC ModeDesc;
	DXGI_MODE_ROTATION Rotation;
	BOOL DesktopImageInSystemMemory;
} DXGI_OUTDUPL_DESC;

typedef struct _DXGI_OUTDUPL_MOVE_RECT {
	POINT SourcePoint;
	RECT DestinationRect;
} DXGI_OUTDUPL_MOVE_RECT;

struct IDXGIOutputDuplication : public IDXGIObject {
public:
	virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTDUPL_DESC *pDesc) = 0;
	virtual HRESULT STDMETHODCALLTYPE AcquireNextFrame(UINT TimeoutInMilliseconds, DXGI_OUTDUPL_FRAME_INFO *pFrameInfo, IDXGIResource **ppDesktopResource) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFrameDirtyRects(UINT DirtyRectsBufferSize, RECT *pDirtyRectsBuffer, UINT *pDirtyRectsBufferSizeRequired) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFrameMoveRects(UINT MoveRectsBufferSize, DXGI_OUTDUPL_MOVE_RECT *pMoveRectBuffer, UINT *pMoveRectsBufferSizeRequired) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFramePointerShape(UINT PointerShapeBufferSize, void *pPointerShapeBuffer, UINT *pPointerShapeBufferSizeRequired, DXGI_OUTDUPL_POINTER_SHAPE_INFO *pPointerShapeInfo) = 0;
	virtual HRESULT STDMETHODCALLTYPE MapDesktopSurface(DXGI_MAPPED_RECT *pLockedRect) = 0;
	virtual HRESULT STDMETHODCALLTYPE UnMapDesktopSurface() = 0;
	virtual HRESULT STDMETHODCALLTYPE ReleaseFrame() = 0;
};

#endif

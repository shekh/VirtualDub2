#ifndef f_VD2_FILTERACCELENGINE_H
#define f_VD2_FILTERACCELENGINE_H

#include <vd2/system/refcount.h>
#include <vd2/system/thread.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/vectors.h>
#include <vd2/system/win32/miniwindows.h>
#include <vd2/plugin/vdvideoaccel.h>

class IVDTContext;
class IVDTProfiler;
class IVDTSurface;
class IVDTReadbackBuffer;
class IVDTFragmentProgram;
class IVDTVertexProgram;
class IVDTVertexFormat;
class IVDTVertexBuffer;
class IVDTIndexBuffer;
class IVDTRasterizerState;
class IVDTBlendState;
class IVDTSamplerState;
class VDFilterFrameBuffer;
class VDFilterFrameBufferAccel;
struct VDPixmapLayout;

class VDFilterAccelEngine;
class VDFilterAccelEngineDispatchQueue;
class VDFilterAccelReadbackBuffer;
class VDRTProfileChannel;

enum VDFilterAccelStatus {
	kVDFilterAccelStatus_OK,
	kVDFilterAccelStatus_Failed,
	kVDFilterAccelStatus_DeviceLost
};

struct VDFilterAccelVertex {
	float x;
	float y;
	float z;
	float uv[8][4];
};

struct VDFilterAccelClearVertex {
	float x;
	float y;
	uint8 c[4];
};

class VDFilterAccelEngineMessage : public vdlist_node {
public:
	VDFilterAccelEngineMessage();

	void (*mpCallback)(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);

	typedef void (*CleanupFn)(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	CleanupFn mpCleanup;
	bool mbRePoll;
	volatile bool mbCompleted;
	VDSignal *mpCompleteSignal;
};

struct VDFilterAccelEngineDownloadMsg : public VDFilterAccelEngineMessage {
	VDFilterAccelEngine *mpThis;
	VDFilterAccelReadbackBuffer *mpRB;
	VDFilterFrameBuffer *mpDstBuffer;
	VDFilterFrameBufferAccel *mpSrc;
	char *mpDst[3];
	ptrdiff_t mDstPitch[3];
	uint32 mWidth;
	uint32 mHeight;
	bool mbSrcYUV;
	bool mbDstYUV;
	bool mbWaitingForRender;
	bool mbSuccess;
	bool mbDeviceLost;
	uint32 mFence;
};

class VDFilterAccelEngineDispatchQueue {
public:
	typedef VDFilterAccelEngineMessage Message;

	VDFilterAccelEngineDispatchQueue();

	void Shutdown();

	void SetProfilingChannel(VDRTProfileChannel *chan);

	void Run(VDScheduler *scheduler);
	void Post(Message *msg);
	void Wait(Message *msg, VDFilterAccelEngineDispatchQueue& localQueue);
	void Send(Message *msg, VDFilterAccelEngineDispatchQueue& localQueue);

protected:
	VDAtomicInt mbActive;
	VDRTProfileChannel *mpProfChan;

	typedef vdfastdeque<Message *> Queue;
	Queue mQueue;
	Queue mPollQueue;
	VDSignal mMessagesReady;
	VDSignal mSyncMessageCompleted;

	VDCriticalSection mMutex;
};

class VDFilterAccelEngine : VDThread, public vdrefcounted<IVDRefCount> {
public:
	VDFilterAccelEngine();
	~VDFilterAccelEngine();

	IVDTContext *GetContext() const { return mpTC; }
	VDScheduler *GetScheduler() { return &mScheduler; }

	bool Init(bool visibleDebugWindow);
	void Shutdown();

	void SyncCall(VDFilterAccelEngineMessage *message);
	void PostCall(VDFilterAccelEngineMessage *message);
	void WaitForCall(VDFilterAccelEngineMessage *message);

	bool CommitBuffer(VDFilterFrameBufferAccel *buf, bool renderable);
	void DecommitBuffer(VDFilterFrameBufferAccel *buf);
	void Upload(VDFilterFrameBufferAccel *dst, VDFilterFrameBuffer *src, const VDPixmapLayout& srcLayout);
	void Upload(VDFilterFrameBufferAccel *dst, const void *srcp, const VDPixmapLayout& srcLayout);

	VDFilterAccelReadbackBuffer *CreateReadbackBuffer(uint32 width, uint32 height, int format);
	void DestroyReadbackBuffer(VDFilterAccelReadbackBuffer *rb);

	bool Download(VDFilterFrameBuffer *dst, const VDPixmapLayout& dstLayout, VDFilterFrameBufferAccel *src, bool srcYUV, VDFilterAccelReadbackBuffer *rb);
	bool BeginDownload(VDFilterAccelEngineDownloadMsg *msg, VDFilterFrameBuffer *dst, const VDPixmapLayout& dstLayout, VDFilterFrameBufferAccel *src, bool srcYUV, VDFilterAccelReadbackBuffer *rb);
	VDFilterAccelStatus EndDownload(VDFilterAccelEngineDownloadMsg *msg);

	bool Convert(VDFilterFrameBufferAccel *dst, const VDPixmapLayout& dstLayout, VDFilterFrameBufferAccel *src, const VDPixmapLayout& srcLayout, const vdrect32& srcRect);

	void UpdateProfilingDisplay();
	void SetSamplerState(uint32 index, bool wrap, VDXAFilterMode filter);

	enum QuadPattern {
		kQuadPattern_1,
		kQuadPattern_3x3,
		kQuadPattern_4x4
	};

	bool DrawQuads(IVDTVertexFormat *vf, IVDTFragmentProgram *fp, const void *vxs, uint32 vxcount, uint32 vxstride, QuadPattern pattern);
	bool FillRects(uint32 rectCount, const VDXRect *rects, uint32 colorARGB, uint32 w, uint32 h, uint32 bw, uint32 bh);

protected:
	struct InitMsg;
	struct DecommitMsg;
	typedef VDFilterAccelEngineDownloadMsg DownloadMsg;
	struct UploadMsg;
	struct ConvertMsg;

	enum { kVBSize = 1024 * sizeof(VDFilterAccelVertex) };

	static void InitCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	bool InitCallback2(bool visibleDebugWindow);
	static void ShutdownCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	void ShutdownCallback2();
	static void DecommitCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	static void UploadCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	static void DownloadCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	void DownloadCallback2a(DownloadMsg& msg);
	void DownloadCallback2b(DownloadMsg& msg);
	static void ConvertCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	void ConvertCallback2(ConvertMsg& msg);

	void ThreadRun();
	static VDZLPARAM VDZCALLBACK StaticWndProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	IVDTContext		*mpTC;
	IVDTProfiler	*mpTP;

	IVDTFragmentProgram *mpFPConvertRGBToYUV;
	IVDTFragmentProgram *mpFPConvertYUVToRGB;
	IVDTFragmentProgram *mpFPNull;
	IVDTFragmentProgram *mpFPExtractPlane;
	IVDTFragmentProgram *mpFPClear;
	IVDTVertexProgram *mpVP;
	IVDTVertexProgram *mpVPClear;
	IVDTVertexFormat *mpVF;
	IVDTVertexFormat *mpVFC;
	IVDTVertexFormat *mpVFCClear;
	IVDTVertexBuffer *mpVB;
	IVDTIndexBuffer *mpIB;
	IVDTRasterizerState *mpRS;
	IVDTBlendState *mpBS;
	uint32 mVBPos;

	IVDTSamplerState *mpSamplerStates[2][kVDXAFiltCount];

	VDFilterAccelEngineDispatchQueue	mWorkerQueue;
	VDFilterAccelEngineDispatchQueue	mCallbackQueue;

	VDZATOM		mWndClass;
	VDZHWND		mhwnd;
	bool		mbVisualDebugEnabled;

	VDScheduler	mScheduler;
	VDSignal	mSchedulerSignal;
};

#endif

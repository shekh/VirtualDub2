#ifndef f_VD2_FILTERACCELREADBACKBUFFER_H
#define f_VD2_FILTERACCELREADBACKBUFFER_H

class VDFilterAccelReadbackBuffer {
	VDFilterAccelReadbackBuffer(const VDFilterAccelReadbackBuffer&);
	VDFilterAccelReadbackBuffer& operator=(const VDFilterAccelReadbackBuffer&);
public:
	VDFilterAccelReadbackBuffer();
	~VDFilterAccelReadbackBuffer();

	IVDTSurface *GetRenderTarget() const { return mpReadbackRT; }
	IVDTReadbackBuffer *GetReadbackBuffer() const { return mpReadbackBuffer; }

	bool Init(IVDTContext *ctx, uint32 w, uint32 h, bool yuvMode);
	void Shutdown();

protected:
	IVDTSurface *mpReadbackRT;
	IVDTReadbackBuffer *mpReadbackBuffer;
};

#endif	// f_VD2_FILTERACCELREADBACKBUFFER_H

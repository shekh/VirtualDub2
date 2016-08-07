//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#ifndef f_VD2_RIZA_VIDEOCODEC_H
#define f_VD2_RIZA_VIDEOCODEC_H

#include <vd2/system/unknown.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/fraction.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/VDString.h>
#include <windows.h>
#include <vfw.h>

struct tagBITMAPINFOHEADER;
struct FilterModPixmapInfo;
struct VDPixmapLayout;

class IVDVideoCompressor;

class IVDVideoCompressorDesc : public IVDRefUnknown {
public:
	virtual void CreateInstance(IVDVideoCompressor **pp) = 0;
};

class VDINTERFACE IVDVideoCompressor {
public:
	virtual ~IVDVideoCompressor() {}

	virtual bool IsKeyFrameOnly() = 0;

	virtual int QueryInputFormat(FilterModPixmapInfo* info) = 0;
	virtual int GetInputFormat(FilterModPixmapInfo* info) = 0;
	virtual bool Query(const void *inputFormat, const void *outputFormat = NULL) = 0;
	virtual bool Query(const VDPixmapLayout *inputFormat, const void *outputFormat = NULL) { return false; }
	virtual void GetOutputFormat(const void *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat) = 0;
	virtual void GetOutputFormat(const VDPixmapLayout *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat){}
	virtual const void *GetOutputFormat() = 0;
	virtual uint32 GetOutputFormatSize() = 0;
	virtual uint32 GetMaxOutputSize() = 0;
	virtual void Start(const void *inputFormat, uint32 inputFormatSize, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount) = 0;
	virtual void Start(const VDPixmapLayout& inputFormat, FilterModPixmapInfo& info, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount) = 0;
	virtual void Restart() = 0;
	virtual void SkipFrame() = 0;
	virtual void DropFrame() = 0;
	virtual bool CompressFrame(void *dst, const void *src, bool& keyframe, uint32& size) = 0;
	virtual void Stop() = 0;

	virtual void Clone(IVDVideoCompressor **vc) = 0;
};

IVDVideoCompressor *VDCreateVideoCompressorVCM(struct EncoderHIC *pHIC, uint32 kilobytesPerSecond, long quality, long keyrate, bool ownHandle);

class VDINTERFACE IVDVideoDecompressor {
public:
	virtual ~IVDVideoDecompressor() {}
	virtual bool QueryTargetFormat(int format) = 0;
	virtual bool QueryTargetFormat(const void *format) = 0;
	virtual bool SetTargetFormat(int format) = 0;
	virtual bool SetTargetFormat(const void *format) = 0;
	virtual int GetTargetFormat() = 0;
	virtual int GetTargetFormatVariant() = 0;
	virtual const uint32 *GetTargetFormatPalette() = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) = 0;
	virtual const void *GetRawCodecHandlePtr() = 0;		// (HIC *) on Win32
	virtual const wchar_t *GetName() = 0;
	virtual bool GetAlpha(){ return 0; }
};

IVDVideoDecompressor *VDCreateVideoDecompressorVCM(const void *srcFormat, uint32 srcFormatSize, const void *pHIC);
IVDVideoDecompressor *VDCreateVideoDecompressorDV(int w, int h);
IVDVideoDecompressor *VDCreateVideoDecompressorHuffyuv(uint32 w, uint32 h, uint32 depth, const uint8 *extradata, uint32 extralen);

IVDVideoDecompressor *VDFindVideoDecompressor(uint32 preferredCodec, const void *srcFormat, uint32 srcFormatSize);

class VDINTERFACE IVDVideoCodecBugTrap {
public:
	virtual void OnCodecRenamingDetected(const wchar_t *pName) = 0;	// Called when codec modifies input format.
	virtual void OnAcceptedBS(const wchar_t *pName) = 0;			// Called when codec accepts BS FOURCC.
	virtual void OnCodecModifiedInput(const wchar_t *pName) = 0;	// Called when codec modifies input buffer.
};

void VDSetVideoCodecBugTrap(IVDVideoCodecBugTrap *);

typedef LRESULT (WINAPI DriverProc)(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2);

struct EncoderHIC{
	VDStringW path;
	HMODULE module;
	DriverProc* proc;
	DriverProc* vdproc;
	DWORD_PTR obj;
	HIC hic;

	EncoderHIC(){
		module = 0;
		obj = 0;
		proc = 0;
		vdproc = 0;
		hic = 0;
	}

	~EncoderHIC(){
		close();
		if(module) FreeLibrary(module);
	}

	static EncoderHIC* load(const VDStringW& path, DWORD type, DWORD handler, DWORD flags);
	static EncoderHIC* open(DWORD type, DWORD handler, DWORD flags);

	int getInfo(ICINFO& info);
	int compressQuery(void* src, void* dst, const VDPixmapLayout* pxsrc=0);
	int queryInputFormat(FilterModPixmapInfo* info);
	int sendMessage(int msg, LPARAM p1, LPARAM p2);
	void close();
	void configure(HWND hwnd);
	void about(HWND hwnd);
	bool queryConfigure();
	bool queryAbout();
	void setState(void* data, int size);
	int getState(void* data, int size);
	int getStateSize(){ return getState(0,0); }
	int compressGetFormat(BITMAPINFO* b1, BITMAPINFO* b2, const VDPixmapLayout* pxsrc=0);
	int compressGetFormatSize(BITMAPINFO* b1, const VDPixmapLayout* pxsrc=0){ return compressGetFormat(b1,0,pxsrc); }
	int compressGetSize(BITMAPINFO* b1, BITMAPINFO* b2, const VDPixmapLayout* pxsrc=0);
	int compressBegin(BITMAPINFO* b1, BITMAPINFO* b2, const VDPixmapLayout* pxsrc=0);
	int compressEnd(){ return sendMessage(ICM_COMPRESS_END,0,0); }
	int decompressBegin(BITMAPINFO* b1, BITMAPINFO* b2){ return sendMessage(ICM_DECOMPRESS_BEGIN,(LPARAM)b1,(LPARAM)b2); }
	int decompressEnd(){ return sendMessage(ICM_DECOMPRESS_END,0,0); }

	DWORD compress(
		DWORD               dwFlags,
		LPBITMAPINFOHEADER  lpbiOutput,
		LPVOID              lpData,
		LPBITMAPINFOHEADER  lpbiInput,
		LPVOID              lpBits,
		LPDWORD             lpckid,
		LPDWORD             lpdwFlags,
		LONG                lFrameNum,
		DWORD               dwFrameSize,
		DWORD               dwQuality,
		LPBITMAPINFOHEADER  lpbiPrev,
		LPVOID              lpPrev,
		const VDPixmapLayout* pxsrc=0
	);

	DWORD decompress(
		DWORD               dwFlags, 
		LPBITMAPINFOHEADER  lpbiFormat, 
		LPVOID              lpData, 
		LPBITMAPINFOHEADER  lpbi, 
		LPVOID              lpBits
	);
};

struct COMPVARS2: public COMPVARS{
	EncoderHIC* driver;

	COMPVARS2(){ driver=0; }
	~COMPVARS2(){ delete driver; }
	void clear() {
		memset(this, 0, sizeof COMPVARS);
	}
};

void FreeCompressor(COMPVARS2 *pCompVars);

#endif

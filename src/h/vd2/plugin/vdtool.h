#ifndef f_VD2_PLUGIN_VDTOOL_H
#define f_VD2_PLUGIN_VDTOOL_H

#include "vdplugin.h"

class IVDXTool : public IVDXUnknown {
public:
	enum { kIID = VDXMAKEFOURCC('X', 'g', 't', '1') };

	virtual bool VDXAPIENTRY GetMenuInfo(int id, char* name, int name_size, bool* enabled) = 0;
	virtual bool VDXAPIENTRY GetCommandId(int id, char* name, int name_size) = 0;
	virtual bool VDXAPIENTRY ExecuteMenu(int id, VDXHWND hwndParent) = 0;
	virtual bool VDXAPIENTRY TranslateMessage(MSG& msg) = 0;
	virtual bool VDXAPIENTRY HandleError(const char* s, int source, void* userData) = 0;
	virtual bool VDXAPIENTRY HandleFileOpen(const wchar_t* fileName, const wchar_t* driverName, VDXHWND hwndParent) = 0;
	virtual void VDXAPIENTRY Attach(VDXHWND hwndParent) = 0;
	virtual void VDXAPIENTRY Detach(VDXHWND hwndParent) = 0;

	// version 2
	virtual bool VDXAPIENTRY HandleFileOpenError(const wchar_t* fileName, const wchar_t* driverName, VDXHWND hwndParent, const char* s, int source) = 0;
};

class IVDTimeline {
public:
	virtual int64 GetTimelinePos()=0;
	virtual void GetSelection(int64& start, int64& end)=0;
	virtual int GetSubsetCount()=0;
	virtual void GetSubsetRange(int i, int64& start, int64& end)=0;
	virtual void SetTimelinePos(int64 pos)=0;
};

class IVDToolCallbacks {
public:
	virtual size_t GetFileName(wchar_t* buf, size_t n) = 0;
	virtual void SetFileName(const wchar_t* fileName, const wchar_t* driverName, void* userData) = 0;
	virtual void Reopen(void* userData) = 0;
	virtual IVDTimeline* GetTimeline() = 0;

	// version 2
	virtual void Reopen(const wchar_t* fileName, const wchar_t* driverName, void* userData) = 0;
};

struct VDXToolContext {
	uint32	mAPIVersion;
	IVDToolCallbacks *mpCallbacks;
};

typedef bool (VDXAPIENTRY *VDXToolCreateProc)(const VDXToolContext *pContext, IVDXTool **);

struct VDXToolDefinition {
	uint32		mSize;				// size of this structure in bytes

	VDXToolCreateProc		mpCreate;
};

enum {
	kVDXPlugin_ToolAPIVersion = 2
};

#endif

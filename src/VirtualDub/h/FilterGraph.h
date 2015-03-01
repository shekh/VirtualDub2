#ifndef f_FILTERGRAPH_H
#define f_FILTERGRAPH_H

#include <windows.h>

#include <vector>

#define FILTERGRAPHCONTROLCLASS (g_szFilterGraphControlName)

#ifndef f_FILTERGRAPH_CPP
extern const char g_szFilterGraphControlName[];
#endif

ATOM RegisterFilterGraphControl();

class IVDRefCount;

struct VDFilterGraphNode {
	const wchar_t *name;
	int inputs;
	int outputs;
	IVDRefCount *pInstance;
};

struct VDFilterGraphConnection {
	int srcfilt;
	int srcpin;
};

class IVDFilterGraphControlCallback {
public:
	virtual void SelectionChanged(IVDRefCount *pNewSelection) = 0;
	virtual bool Configure(VDGUIHandle hParent, IVDRefCount *pInstance) = 0;
	virtual bool RequeryFormats() = 0;
};


class IVDFilterGraphControl {
public:
	virtual void SetCallback(IVDFilterGraphControlCallback *pCB) = 0;
	virtual void Arrange() = 0;
	virtual void ConfigureSelection() = 0;
	virtual void DeleteSelection() = 0;
	virtual void EnableAutoArrange(bool) = 0;
	virtual void EnableAutoConnect(bool) = 0;
	virtual void AddFilter(const wchar_t *name, int inpins, int outpins, bool bProtected, IVDRefCount *pInstance) = 0;
	virtual void GetFilterGraph(std::vector<VDFilterGraphNode>& filters, std::vector<VDFilterGraphConnection>& connections) = 0;
	virtual void SetFilterGraph(const std::vector<VDFilterGraphNode>& filters, const std::vector<VDFilterGraphConnection>& connections) = 0;
	virtual void SetConnectionLabel(IVDRefCount *pInstance, int outpin, const wchar_t *pLabel) = 0;

	virtual IVDRefCount *GetSelection() = 0;
};

IVDFilterGraphControl *VDGetIFilterGraphControl(HWND hwnd);

#endif

#ifndef TOOL_H
#define TOOL_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstring.h>

struct FileNameCommand{
	VDStringW fileName;
	VDStringW driverName;
	void* object;
	void* userData;
	bool reopen;

	FileNameCommand(){ object=0; userData=0; reopen=0; }
};

void VDToolExecuteCommand(int id, HWND parent);
void VDToolInsertMenu(HMENU menu, int pos);
bool VDToolCatchError(FileNameCommand* cmd, const MyError& e);
void VDToolsHandleFileOpen(const wchar_t* fname, IVDInputDriver *pDriver);
void VDToolsAttach(HWND hwnd);
void VDToolsDetach(HWND hwnd);

#endif
